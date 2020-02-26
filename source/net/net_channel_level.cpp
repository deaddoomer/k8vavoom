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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
  CMD_ServerInfo,
  CMD_ServerInfoEnd,
  CMD_PreRender,
  CMD_Line,
  CMD_CamTex,
  CMD_LevelTrans,
  CMD_BodyQueueTrans,

  CMD_ResetLevel,

  CMD_StaticLightSpot,

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
  serverInfoBuf.clear();
  severInfoPacketCount = 0;
  severInfoCurrPacket = -1;
  csi.mapname.clear();
  csi.sinfo.clear();
  csi.maxclients = 1;
  csi.deathmatch = 0;
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
//  VLevelChannel::Suicide
//
//==========================================================================
void VLevelChannel::Suicide () {
  #ifdef VAVOOM_EXCESSIVE_NETWORK_DEBUG_LOGS
  GCon->Logf(NAME_Debug, "VLevelChannel::Suicide:%p (#%d)", this, Index);
  #endif
  VChannel::Suicide();
  Closing = true; // just in case
  ClearAllQueues();
  if (Index >= 0 && Index < MAX_CHANNELS && Connection) {
    Connection->UnregisterChannel(this);
    Index = -1; // just in case
  }
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
//  VLevelChannel::ResetLevel
//
//==========================================================================
void VLevelChannel::ResetLevel () {
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
    Level = nullptr;
    serverInfoBuf.clear();
    severInfoPacketCount = 0;
    severInfoCurrPacket = -1;
    csi.mapname.clear();
    csi.sinfo.clear();
    csi.maxclients = 1;
    csi.deathmatch = 0;
  }
}


//==========================================================================
//
//  VLevelChannel::SendNewLevel
//
//==========================================================================
void VLevelChannel::SendNewLevel () {
  VStr sinfo = svs.serverinfo;
  {
    VMessageOut Msg(this);
    Msg.bReliable = true;
    Msg.WriteInt(CMD_NewLevel/*, CMD_MAX*/);
    Msg.WriteInt(NETWORK_PROTO_VERSION);
    VStr MapName = *Level->MapName;
    vassert(!Msg.IsLoading());
    //Msg << svs.serverinfo << MapName;
    Msg << MapName;
    Msg.WriteInt(svs.max_clients/*, MAXPLAYERS+1*/);
    Msg.WriteInt(deathmatch/*, 256*/);
    Msg.WriteInt((int)((sinfo.length()+999)/1000)); // number of packets in server info
    SendMessage(&Msg);
  }

  // send server info
  if (sinfo.length()) {
    int pktnum = 0;
    while (sinfo.length() > 0) {
      VMessageOut Msg(this);
      Msg.bReliable = true;
      Msg.WriteInt(CMD_ServerInfo/*, CMD_MAX*/);
      Msg.WriteInt(pktnum); // sequence number
      VStr s = sinfo;
      if (s.length() > 1000) {
        s.chopRight(s.length()-1000);
        sinfo.chopLeft(1000);
      } else {
        sinfo.clear();
      }
      Msg << s;
      SendMessage(&Msg);
      ++pktnum;
    }

    {
      VMessageOut Msg(this);
      Msg.bReliable = true;
      Msg.WriteInt(CMD_ServerInfoEnd/*, CMD_MAX*/);
      Msg.WriteInt(pktnum); // sequence number
      SendMessage(&Msg);
    }
  }

  SendStaticLights();

  {
    VMessageOut Msg(this);
    Msg.bReliable = true;
    Msg.WriteInt(CMD_PreRender/*, CMD_MAX*/);
    SendMessage(&Msg);
  }

  //GCon->Log(NAME_DevNet, "VLevelChannel::SendNewLevel");
}


//==========================================================================
//
//  VLevelChannel::Update
//
//==========================================================================
void VLevelChannel::Update () {
  //GCon->Log(NAME_DevNet, "VLevelChannel::Update: enter");

  VMessageOut Msg(this, true);
  //Msg.bReliable = true; // autoreliable

  //GCon->Log(NAME_DevNet, "VLevelChannel::Update -- Lines");
  for (int i = 0; i < Level->NumLines; ++i) {
    line_t *Line = &Level->Lines[i];
    //if (!Connection->SecCheckFatPVS(Line->sector)) continue;

    rep_line_t *RepLine = &Lines[i];
    if (Line->alpha == RepLine->alpha) continue;

    Msg.SetMark();
    Msg.WriteInt(CMD_Line/*, CMD_MAX*/);
    Msg.WriteInt(i/*, Level->NumLines*/);
    Msg.WriteBit(Line->alpha != RepLine->alpha);
    if (Line->alpha != RepLine->alpha) {
      Msg << Line->alpha;
      Msg.WriteBit(!!(Line->flags&ML_ADDITIVE));
      RepLine->alpha = Line->alpha;
    }
    if (Msg.NeedSplit()) Msg.SendSplitMessage();
  }

  //GCon->Log(NAME_DevNet, "VLevelChannel::Update -- Sides");
  for (int i = 0; i < Level->NumSides; ++i) {
    side_t *Side = &Level->Sides[i];
    if (!Connection->SecCheckFatPVS(Side->Sector)) continue;

    rep_side_t *RepSide = &Sides[i];
    if (Side->TopTexture == RepSide->TopTexture &&
        Side->BottomTexture == RepSide->BottomTexture &&
        Side->MidTexture == RepSide->MidTexture &&
        Side->Top.TextureOffset == RepSide->Top.TextureOffset &&
        Side->Bot.TextureOffset == RepSide->Bot.TextureOffset &&
        Side->Mid.TextureOffset == RepSide->Mid.TextureOffset &&
        Side->Top.RowOffset == RepSide->Top.RowOffset &&
        Side->Bot.RowOffset == RepSide->Bot.RowOffset &&
        Side->Mid.RowOffset == RepSide->Mid.RowOffset &&
        Side->Top.ScaleX == RepSide->Top.ScaleX &&
        Side->Top.ScaleY == RepSide->Top.ScaleY &&
        Side->Bot.ScaleX == RepSide->Bot.ScaleX &&
        Side->Bot.ScaleY == RepSide->Bot.ScaleY &&
        Side->Mid.ScaleX == RepSide->Mid.ScaleX &&
        Side->Mid.ScaleY == RepSide->Mid.ScaleY &&
        Side->Flags == RepSide->Flags &&
        Side->Light == RepSide->Light)
    {
      continue;
    }

    //GCon->Logf(NAME_DevNet, "VLevelChannel::Update: Side #%d (pos=%d; max=%d; err=%d)", i, Msg.GetPos(), Msg.GetNum(), (int)Msg.IsError());

    Msg.SetMark();
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
    Msg.WriteBit(Side->Top.TextureOffset != RepSide->Top.TextureOffset);
    if (Side->Top.TextureOffset != RepSide->Top.TextureOffset) {
      Msg << Side->Top.TextureOffset;
      RepSide->Top.TextureOffset = Side->Top.TextureOffset;
    }
    Msg.WriteBit(Side->Bot.TextureOffset != RepSide->Bot.TextureOffset);
    if (Side->Bot.TextureOffset != RepSide->Bot.TextureOffset) {
      Msg << Side->Bot.TextureOffset;
      RepSide->Bot.TextureOffset = Side->Bot.TextureOffset;
    }
    Msg.WriteBit(Side->Mid.TextureOffset != RepSide->Mid.TextureOffset);
    if (Side->Mid.TextureOffset != RepSide->Mid.TextureOffset) {
      Msg << Side->Mid.TextureOffset;
      RepSide->Mid.TextureOffset = Side->Mid.TextureOffset;
    }
    Msg.WriteBit(Side->Top.RowOffset != RepSide->Top.RowOffset);
    if (Side->Top.RowOffset != RepSide->Top.RowOffset) {
      Msg << Side->Top.RowOffset;
      RepSide->Top.RowOffset = Side->Top.RowOffset;
    }
    Msg.WriteBit(Side->Bot.RowOffset != RepSide->Bot.RowOffset);
    if (Side->Bot.RowOffset != RepSide->Bot.RowOffset) {
      Msg << Side->Bot.RowOffset;
      RepSide->Bot.RowOffset = Side->Bot.RowOffset;
    }
    Msg.WriteBit(Side->Mid.RowOffset != RepSide->Mid.RowOffset);
    if (Side->Mid.RowOffset != RepSide->Mid.RowOffset) {
      Msg << Side->Mid.RowOffset;
      RepSide->Mid.RowOffset = Side->Mid.RowOffset;
    }
    Msg.WriteBit(Side->Top.ScaleX != RepSide->Top.ScaleX);
    if (Side->Top.ScaleX != RepSide->Top.ScaleX) {
      Msg << Side->Top.ScaleX;
      RepSide->Top.ScaleX = Side->Top.ScaleX;
    }
    Msg.WriteBit(Side->Top.ScaleY != RepSide->Top.ScaleY);
    if (Side->Top.ScaleY != RepSide->Top.ScaleY) {
      Msg << Side->Top.ScaleY;
      RepSide->Top.ScaleY = Side->Top.ScaleY;
    }
    Msg.WriteBit(Side->Bot.ScaleX != RepSide->Bot.ScaleX);
    if (Side->Bot.ScaleX != RepSide->Bot.ScaleX) {
      Msg << Side->Bot.ScaleX;
      RepSide->Bot.ScaleX = Side->Bot.ScaleX;
    }
    Msg.WriteBit(Side->Bot.ScaleY != RepSide->Bot.ScaleY);
    if (Side->Bot.ScaleY != RepSide->Bot.ScaleY) {
      Msg << Side->Bot.ScaleY;
      RepSide->Bot.ScaleY = Side->Bot.ScaleY;
    }
    Msg.WriteBit(Side->Mid.ScaleX != RepSide->Mid.ScaleX);
    if (Side->Mid.ScaleX != RepSide->Mid.ScaleX) {
      Msg << Side->Mid.ScaleX;
      RepSide->Mid.ScaleX = Side->Mid.ScaleX;
    }
    Msg.WriteBit(Side->Mid.ScaleY != RepSide->Mid.ScaleY);
    if (Side->Mid.ScaleY != RepSide->Mid.ScaleY) {
      Msg << Side->Mid.ScaleY;
      RepSide->Mid.ScaleY = Side->Mid.ScaleY;
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
    if (Msg.NeedSplit()) Msg.SendSplitMessage();
  }

  //GCon->Log(NAME_DevNet, "VLevelChannel::Update -- Sectors");
  for (int i = 0; i < Level->NumSectors; ++i) {
    sector_t *Sec = &Level->Sectors[i];
    //bool forced = false;

    if (!Connection->SecCheckFatPVS(Sec) &&
        !(Sec->SectorFlags&sector_t::SF_ExtrafloorSource) &&
        !(Sec->SectorFlags&sector_t::SF_TransferSource))
    {
      // inner door sector is invisible if closed
      // this may miss door update, so force updating
      // TODO: better solution is checking both remembered and current height in clipper
      // DONE!
      /*
      if (Sec->floor.minz == Sec->ceiling.maxz) {
        //GCon->Logf("*** closed sector #%d changed, and not visible; force update", (int)(ptrdiff_t)(Sec-&Level->Sectors[0]));
        //forced = true;
      } else
      */
      {
        continue;
      }
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

    //if (forced) GCon->Logf("*** closed sector #%d changed, and not visible; force update", (int)(ptrdiff_t)(Sec-&Level->Sectors[0]));
    /*
    if (!Connection->SecCheckFatPVS(Sec) &&
        !(Sec->SectorFlags&sector_t::SF_ExtrafloorSource) &&
        !(Sec->SectorFlags&sector_t::SF_TransferSource)) {
      GCon->Logf("*** sector #%d changed, but not visible", (int)(ptrdiff_t)(Sec-&Level->Sectors[0]));
      continue;
    } else {
      GCon->Logf("!!! sector #%d changed", (int)(ptrdiff_t)(Sec-&Level->Sectors[0]));
    }
    */

    Msg.SetMark();
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
    if (Msg.NeedSplit()) Msg.SendSplitMessage();

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

  //GCon->Log(NAME_DevNet, "VLevelChannel::Update -- Polys");
  for (int i = 0; i < Level->NumPolyObjs; ++i) {
    polyobj_t *Po = Level->PolyObjs[i];
    if (!Connection->CheckFatPVS(Po->GetSubsector())) continue;

    rep_polyobj_t *RepPo = &PolyObjs[i];
    if (RepPo->startSpot.x == Po->startSpot.x &&
        RepPo->startSpot.y == Po->startSpot.y &&
        RepPo->angle == Po->angle)
    {
      continue;
    }

    Msg.SetMark();
    Msg.WriteInt(CMD_PolyObj/*, CMD_MAX*/);
    Msg.WriteInt(i/*, Level->NumPolyObjs*/);
    Msg.WriteBit(RepPo->startSpot.x != Po->startSpot.x);
    if (RepPo->startSpot.x != Po->startSpot.x) Msg << Po->startSpot.x;
    Msg.WriteBit(RepPo->startSpot.y != Po->startSpot.y);
    if (RepPo->startSpot.y != Po->startSpot.y) Msg << Po->startSpot.y;
    Msg.WriteBit(RepPo->angle != Po->angle);
    if (RepPo->angle != Po->angle) Msg << Po->angle;
    if (Msg.NeedSplit()) Msg.SendSplitMessage();

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
    Msg.SetMark();
    Msg.WriteInt(CMD_CamTex/*, CMD_MAX*/);
    Msg.WriteInt(i/*, 0xff*/);
    Connection->ObjMap->SerialiseObject(Msg, *(VObject **)&CamEnt);
    Msg.WriteInt(Cam.TexNum/*, 0xffff*/);
    Msg.WriteInt(Cam.FOV/*, 360*/);
    if (Msg.NeedSplit()) Msg.SendSplitMessage();

    // update replication info
    RepCam.Camera = CamEnt;
    RepCam.TexNum = Cam.TexNum;
    RepCam.FOV = Cam.FOV;
  }

  //GCon->Log(NAME_DevNet, "VLevelChannel::Update -- Trans");
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
    Msg.SetMark();
    Msg.WriteInt(CMD_LevelTrans/*, CMD_MAX*/);
    Msg.WriteInt(i/*, MAX_LEVEL_TRANSLATIONS*/);
    Msg.WriteInt(Tr->Commands.Num()/*, 0xff*/);
    Rep.SetNum(Tr->Commands.Num());
    for (int j = 0; j < Tr->Commands.Num(); ++j) {
      VTextureTranslation::VTransCmd &C = Tr->Commands[j];
      Msg.WriteInt(C.Type/*, 2*/);
           if (C.Type == 0) Msg << C.Start << C.End << C.R1 << C.R2;
      else if (C.Type == 1) Msg << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2 << C.G2 << C.B2;
      else if (C.Type == 2) Msg << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2 << C.G2 << C.B2;
      else if (C.Type == 3) Msg << C.Start << C.End << C.R1 << C.G1 << C.B1;
      else if (C.Type == 4) Msg << C.Start << C.End << C.R1 << C.G1 << C.B1 << C.R2;
      Rep[j] = C;
    }
    if (Msg.NeedSplit()) Msg.SendSplitMessage();
  }

  //GCon->Log(NAME_DevNet, "VLevelChannel::Update -- Bodies");
  for (int i = 0; i < Level->BodyQueueTrans.Num(); ++i) {
    // grow replication array if needed
    if (BodyQueueTrans.Num() == i) BodyQueueTrans.Alloc().TranslStart = 0;
    if (!Level->BodyQueueTrans[i]) continue;
    VTextureTranslation *Tr = Level->BodyQueueTrans[i];
    if (!Tr->TranslStart) continue;
    VBodyQueueTrInfo &Rep = BodyQueueTrans[i];
    if (Tr->TranslStart == Rep.TranslStart && Tr->TranslEnd == Rep.TranslEnd && Tr->Color == Rep.Color) continue;

    // send message
    Msg.SetMark();
    Msg.WriteInt(CMD_BodyQueueTrans/*, CMD_MAX*/);
    Msg.WriteInt(i/*, MAX_BODY_QUEUE_TRANSLATIONS*/);
    Msg << Tr->TranslStart << Tr->TranslEnd;
    Msg.WriteInt(Tr->Color/*, 0x00ffffff*/);
    if (Msg.NeedSplit()) Msg.SendSplitMessage();

    Rep.TranslStart = Tr->TranslStart;
    Rep.TranslEnd = Tr->TranslEnd;
    Rep.Color = Tr->Color;
  }

  if (Msg.GetNumBits()) {
    //GCon->Logf(NAME_DevNet, "VLevelChannel::Update: sending... (pos=%d; max=%d; err=%d)", Msg.GetPos(), Msg.GetNum(), (int)Msg.IsError());
    SendMessage(&Msg);
    //GCon->Log(NAME_DevNet, "VLevelChannel::Update: sent.");
  }
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
    if (L.ConeAngle) {
      Msg.WriteInt(CMD_StaticLightSpot/*, CMD_MAX*/);
      Msg << L.Origin << L.Radius << L.Color << L.ConeDir << L.ConeAngle;
    } else {
      Msg.WriteInt(CMD_StaticLight/*, CMD_MAX*/);
      Msg << L.Origin << L.Radius << L.Color;
    }
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
          if (Msg.ReadBit()) Msg << Side->Top.TextureOffset;
          if (Msg.ReadBit()) Msg << Side->Bot.TextureOffset;
          if (Msg.ReadBit()) Msg << Side->Mid.TextureOffset;
          if (Msg.ReadBit()) Msg << Side->Top.RowOffset;
          if (Msg.ReadBit()) Msg << Side->Bot.RowOffset;
          if (Msg.ReadBit()) Msg << Side->Mid.RowOffset;
          if (Msg.ReadBit()) Msg << Side->Top.ScaleX;
          if (Msg.ReadBit()) Msg << Side->Top.ScaleY;
          if (Msg.ReadBit()) Msg << Side->Bot.ScaleX;
          if (Msg.ReadBit()) Msg << Side->Bot.ScaleY;
          if (Msg.ReadBit()) Msg << Side->Mid.ScaleX;
          if (Msg.ReadBit()) Msg << Side->Mid.ScaleY;
          if (Msg.ReadBit()) Side->Flags = Msg.ReadInt(/*0x000f*/);
          if (Msg.ReadBit()) Msg << Side->Light;
        }
        break;
      case CMD_Sector:
        {
          sector_t *Sec = &Level->Sectors[Msg.ReadInt(/*Level->NumSectors*/)];
          const float PrevFloorDist = Sec->floor.dist;
          const float PrevCeilDist = Sec->ceiling.dist;
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
          if (PrevFloorDist != Sec->floor.dist || PrevCeilDist != Sec->ceiling.dist) {
            //GCon->Logf("updating sector #%d", (int)(ptrdiff_t)(Sec-&GClLevel->Sectors[0]));
            Level->CalcSecMinMaxs(Sec);
          }
        }
        break;
      case CMD_PolyObj:
        {
          polyobj_t *Po = Level->PolyObjs[Msg.ReadInt(/*Level->NumPolyObjs*/)];
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
          //FIXME: owner!
          TVec Origin;
          float Radius;
          vuint32 Color;
          Msg << Origin << Radius << Color;
          Level->Renderer->AddStaticLightRGB(nullptr, Origin, Radius, Color);
        }
        break;
      case CMD_StaticLightSpot:
        {
          //FIXME: owner!
          TVec Origin;
          float Radius;
          vuint32 Color;
          TVec ConeDir;
          float ConeAngle;
          Msg << Origin << Radius << Color << ConeDir << ConeAngle;
          Level->Renderer->AddStaticLightRGB(nullptr, Origin, Radius, Color, ConeDir, ConeAngle);
        }
        break;
      case CMD_NewLevel:
#ifdef CLIENT
        {
          int ver = Msg.ReadInt();
          if (Msg.IsError()) Host_Error("Cannot read network protocol version");
          if (ver != NETWORK_PROTO_VERSION) Host_Error("Invalid network protocol version: expected %d, got %d", ver, NETWORK_PROTO_VERSION);
        }
        Msg << csi.mapname;
        csi.maxclients = Msg.ReadInt();
        csi.deathmatch = Msg.ReadInt();
        if (Msg.IsError() || csi.maxclients < 1 || csi.maxclients > MAXPLAYERS) Host_Error("Invalid level handshake sequence");
        // prepare serveinfo buffer
        serverInfoBuf.clear();
        severInfoPacketCount = Msg.ReadInt();
        if (Msg.IsError() || severInfoPacketCount < 0 || severInfoPacketCount > 1024) Host_Error("Invalid server info size");
        severInfoCurrPacket = (severInfoPacketCount ? 0 : -2);
        //CL_ParseServerInfo(Msg);
#endif
        break;
      case CMD_ServerInfo:
#ifdef CLIENT
        if (severInfoCurrPacket < 0) Host_Error("Invalid level handshake sequence");
        if (severInfoCurrPacket >= severInfoPacketCount) Host_Error("Invalid level handshake sequence");
        {
          int sq = Msg.ReadInt();
          if (Msg.IsError() || sq != severInfoCurrPacket) Host_Error("Invalid level handshake sequence");
          VStr s;
          Msg << s;
          if (Msg.IsError()) Host_Error("Invalid level handshake sequence");
          serverInfoBuf += s;
          ++severInfoCurrPacket;
        }
#endif
        break;
      case CMD_ServerInfoEnd:
#ifdef CLIENT
        if (severInfoCurrPacket == -2) Host_Error("Invalid level handshake sequence");
        {
          int sq = Msg.ReadInt();
          if (Msg.IsError() || sq != severInfoPacketCount) Host_Error("Invalid level handshake sequence");
          severInfoCurrPacket = -2;
        }
        csi.sinfo = serverInfoBuf;
        serverInfoBuf.clear();
        CL_ParseServerInfo(&csi);
        csi.mapname.clear();
        csi.sinfo.clear();
#endif
        break;
      case CMD_PreRender:
#ifdef CLIENT
        if (severInfoCurrPacket != -2) Host_Error("Invalid level handshake sequence");
#endif
        Level->Renderer->PreRender();
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
              Tr->MapToColors(Start, End, R1, G1, B1, R2, G2, B2);
            } else if (Type == 2) {
              vuint8 Start;
              vuint8 End;
              vuint8 R1;
              vuint8 G1;
              vuint8 B1;
              vuint8 R2;
              vuint8 G2;
              vuint8 B2;
              Msg << Start << End << R1 << G1 << B1 << R2 << G2 << B2;
              Tr->MapDesaturated(Start, End, R1/128.0f, G1/128.0f, B1/128.0f, R2/128.0f, G2/128.0f, B2/128.0f);
            } else if (Type == 3) {
              vuint8 Start;
              vuint8 End;
              vuint8 R1;
              vuint8 G1;
              vuint8 B1;
              Msg << Start << End << R1 << G1 << B1;
              Tr->MapBlended(Start, End, R1, G1, B1);
            } else if (Type == 4) {
              vuint8 Start;
              vuint8 End;
              vuint8 R1;
              vuint8 G1;
              vuint8 B1;
              vuint8 Amount;
              Msg << Start << End << R1 << G1 << B1 << Amount;
              Tr->MapTinted(Start, End, R1, G1, B1, Amount);
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
      case CMD_ResetLevel:
         ResetLevel();
         break;
      default:
        Sys_Error("Invalid level update command %d", Cmd);
    }
  }
}
