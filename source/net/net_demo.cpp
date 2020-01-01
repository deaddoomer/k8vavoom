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
//
//  DEMO CODE
//
// When a demo is playing back, all NET_SendMessages are skipped, and
// NET_GetMessages are read from the demo file.
//
// Whenever cl->time gets past the last received message, another message
// is read from the demo file.
//
#include "gamedefs.h"
#include "network.h"

static VCvarB demo_flush_each_packet("demo_flush_each_packet", false, "Flush file after each written demo packet?", CVAR_PreInit|CVAR_Archive);


//==========================================================================
//
//  VDemoPlaybackNetConnection::VDemoPlaybackNetConnection
//
//==========================================================================
VDemoPlaybackNetConnection::VDemoPlaybackNetConnection (VNetContext *AContext, VBasePlayer *AOwner, VStream *AStrm, bool ATimeDemo)
  : VNetConnection(nullptr, AContext, AOwner)
  , NextPacketTime(0)
  , bTimeDemo(ATimeDemo)
  , Strm(AStrm)
  , td_lastframe(0)
  , td_startframe(0)
  , td_starttime(0)
  , inIntermission(false)
{
  AutoAck = true;
  *Strm << NextPacketTime;

  if (bTimeDemo) {
    // cls.td_starttime will be grabbed at the second frame of the demo,
    // so all the loading time doesn't get counted
    td_startframe = host_framecount;
    td_lastframe = -1; // get a new message this frame
  }
}


//==========================================================================
//
//  VDemoPlaybackNetConnection::~VDemoPlaybackNetConnection
//
//==========================================================================
VDemoPlaybackNetConnection::~VDemoPlaybackNetConnection () {
  delete Strm;
  Strm = nullptr;
#ifdef CLIENT
  cls.demoplayback = false;
#endif
  if (bTimeDemo) {
    // the first frame didn't count
    int frames = (host_framecount-td_startframe)-1;
    float time = systime-td_starttime;
    if (!time) time = 1;
    GCon->Logf(NAME_DevNet, "%d frames %f seconds %f fps", frames, time, frames/time);
  }
}


//==========================================================================
//
//  VDemoPlaybackNetConnection::GetRawPacket
//
//  Handles recording and playback of demos, on top of NET_ code
//
//==========================================================================
int VDemoPlaybackNetConnection::GetRawPacket (TArray<vuint8> &Data) {
  //Driver->SetNetTime();
  //LastMessageTime = Driver->NetTime;

  // decide if it is time to grab the next message
  if (Owner->MO) { // always grab until fully connected
    if (bTimeDemo) {
      if (host_framecount == td_lastframe) return 0; // allready read this frame's message
      td_lastframe = host_framecount;
      // if this is the second frame, grab the real  cls.td_starttime
      // so the bogus time on the first frame doesn't count
      if (host_framecount == td_startframe+1) td_starttime = systime;
    } else if (GClLevel->Time < NextPacketTime) {
      return 0; // don't need another message yet
    }
  } else {
    /*
    if (GClLevel) {
      GCon->Logf("no owner (level: %s; ltime=%g)", *GClLevel->MapName, GClLevel->Time);
    } else {
      GCon->Logf("no owner");
    }
    */
    if (GClLevel && GClLevel->Time == 0.0f && NextPacketTime != 0.0f) {
      //GCon->Logf("no owner (level: %s; ltime=%g; NextPacketTime=%g)", *GClLevel->MapName, GClLevel->Time, NextPacketTime);
      //return 0;
    }
  }

  if (Strm->AtEnd()) {
    //GCon->Logf("*** EOF ***");
    State = NETCON_Closed;
    return 0;
  }

  // get the next message
  vint32 MsgSize;
  *Strm << MsgSize;
  *Strm << Owner->ViewAngles;

  if (MsgSize > OUT_MESSAGE_SIZE) Sys_Error("Demo message > MAX_MSGLEN");
  Data.SetNum(MsgSize);
  Strm->Serialise(Data.Ptr(), MsgSize);
  if (Strm->IsError()) {
    State = NETCON_Closed;
    return 0;
  }

  if (!Strm->AtEnd()) {
    *Strm << NextPacketTime;
    //GCon->Logf("*** NEXTPKT: %g", NextPacketTime);
  }

  /*
  static float ptt = 0;
  GCon->Logf("demo packet: size=%d; pktime=%f; msdiff=%d", MsgSize, NextPacketTime, (int)((NextPacketTime-ptt)*1000));
  ptt = NextPacketTime;
  */

  return 1;
}


//==========================================================================
//
//  VDemoPlaybackNetConnection::SendRawMessage
//
//==========================================================================
void VDemoPlaybackNetConnection::SendRawMessage (VMessageOut &) {
  //Driver->SetNetTime();
}


//==========================================================================
//
//  VDemoPlaybackNetConnection::Intermission
//
//==========================================================================
void VDemoPlaybackNetConnection::Intermission (bool active) {
  //if (inIntermission == active) return;
  //inIntermission = active;
}


#ifdef CLIENT

//==========================================================================
//
//  VDemoRecordingNetConnection::VDemoRecordingNetConnection
//
//==========================================================================
VDemoRecordingNetConnection::VDemoRecordingNetConnection (VSocketPublic *Sock, VNetContext *AContext, VBasePlayer *AOwner)
  : VNetConnection(Sock, AContext, AOwner)
  , inIntermission(false)
{
}


//==========================================================================
//
//  VDemoPlaybackNetConnection::Intermission
//
//==========================================================================
void VDemoRecordingNetConnection::Intermission (bool active) {
  if (inIntermission == active) return;
  inIntermission = active;
  /*
  GCon->Logf("**** IMISS=%d", (int)active);
  if (cls.demorecording) {
    float Time = (active ? -4.0f : -6.0f);
    *cls.demofile << Time;
  }
  */
}


//==========================================================================
//
//  VDemoRecordingNetConnection::GetRawPacket
//
//  Handles recording and playback of demos, on top of NET_ code
//
//==========================================================================
int VDemoRecordingNetConnection::GetRawPacket (TArray<vuint8> &Data) {
  //Driver->SetNetTime();
  int r = VNetConnection::GetRawPacket(Data);
  if (r == 1 && cls.demorecording) {
    // dumps the current net message, prefixed by the length and view angles
    float Time = (GClLevel ? GClLevel->Time : 0.0f);
    *cls.demofile << Time;
    vint32 MsgSize = Data.Num();
    *cls.demofile << MsgSize;
    *cls.demofile << cl->ViewAngles;
    cls.demofile->Serialise(Data.Ptr(), Data.Num());
    if (demo_flush_each_packet) cls.demofile->Flush();
  }
  return r;
}


//==========================================================================
//
//  VDemoRecordingSocket::IsLocalConnection
//
//==========================================================================
bool VDemoRecordingSocket::IsLocalConnection () {
  return false;
}


//==========================================================================
//
//  VDemoRecordingSocket::GetMessage
//
//==========================================================================
int VDemoRecordingSocket::GetMessage (TArray<vuint8> &) {
  //LastMessageTime = Sys_Time();
  LastMessageTime = GNet->NetTime;
  return 0;
}


//==========================================================================
//
//  VDemoRecordingSocket::SendMessage
//
//==========================================================================
int VDemoRecordingSocket::SendMessage (const vuint8 *Msg, vuint32 MsgSize) {
  //LastMessageTime = Sys_Time();
  LastMessageTime = GNet->NetTime;
  if (cls.demorecording) {
    /*
    if (cl) {
      if (GClLevel) {
        GCon->Logf("SRV: no owner (level: %s; ltime=%g)", *GClLevel->MapName, GClLevel->Time);
      } else {
        GCon->Logf("SRV: no owner");
      }
    }
    */
    // dumps the current net message, prefixed by the length and view angles
    float Time = (GClLevel ? GClLevel->Time : 0.0f);
    *cls.demofile << Time;
    *cls.demofile << MsgSize;
    if (cl) {
      *cls.demofile << cl->ViewAngles;
    } else {
      TAVec A(0, 0, 0);
      *cls.demofile << A;
    }
    cls.demofile->Serialise(Msg, MsgSize);
    if (demo_flush_each_packet) cls.demofile->Flush();
  }
  return 1;
}

#endif
