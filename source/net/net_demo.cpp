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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "../gamedefs.h"
#include "../host.h"
#include "../psim/p_player.h"
#ifdef CLIENT
# include "../client/client.h"
#endif
#include "network.h"

static VCvarB demo_flush_each_packet("demo_flush_each_packet", false, "Flush file after each written demo packet?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);


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
{
  Strm->Serialise(AuthKey, VNetUtils::ChaCha20KeySize);
  #if 0
  GCon->Logf(NAME_Debug, "read auth key...");
  VStr s;
  for (int f = 0; f < VNetUtils::ChaCha20KeySize; ++f) { char tmp[8]; snprintf(tmp, sizeof(tmp), " %02X", AuthKey[f]&0xff); s += tmp; }
  GCon->Logf(NAME_Debug, "%s", *s);
  #endif

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
  VStream::Destroy(Strm);
#ifdef CLIENT
  cls.demoplayback = false;
#endif
  if (bTimeDemo) {
    // the first frame didn't count
    int frames = (host_framecount-td_startframe)-1;
    float time = host_systime-td_starttime;
    if (!time) time = 1;
    GCon->Logf(NAME_DevNet, "%d frames %f seconds %f fps", frames, time, frames/time);
  }
}


//==========================================================================
//
//  VDemoPlaybackNetConnection::GetRawPacket
//
//  handles recording and playback of demos
//
//==========================================================================
int VDemoPlaybackNetConnection::GetRawPacket (void *dest, size_t destSize) {
  // decide if it is time to grab the next message
  if (Owner->MO) { // always grab until fully connected
    if (bTimeDemo) {
      if (host_framecount == td_lastframe) return 0; // allready read this frame's message
      td_lastframe = host_framecount;
      // if this is the second frame, grab the real  cls.td_starttime
      // so the bogus time on the first frame doesn't count
      if (host_framecount == td_startframe+1) td_starttime = host_systime;
    } else if (GClLevel->Time < NextPacketTime) {
      return 0; // don't need another message yet
    }
  } else {
    /*
    if (GClLevel && GClLevel->Time == 0.0f && NextPacketTime != 0.0f) {
      //GCon->Logf("no owner (level: %s; ltime=%g; NextPacketTime=%g)", *GClLevel->MapName, GClLevel->Time, NextPacketTime);
      //return 0;
    }
    */
  }

  if (Strm->AtEnd()) {
    //GCon->Logf("*** EOF ***");
    Close();
    return 0;
  }

  // get the next message
  vint32 MsgSize;
  *Strm << MsgSize;
  *Strm << Owner->ViewAngles;

  if (MsgSize < 0 || MsgSize > (int)destSize) {
    GCon->Logf(NAME_Error, "demo message corrupted (size is %d)", MsgSize);
    Close();
    return 0;
  }

  //if (MsgSize > OUT_MESSAGE_SIZE) Sys_Error("Demo message > MAX_MSGLEN");
  Strm->Serialise(dest, MsgSize);
  if (Strm->IsError()) {
    Close();
    return 0;
  }

  if (!Strm->AtEnd()) {
    *Strm << NextPacketTime;
    //GCon->Logf("*** NEXTPKT: %g", NextPacketTime);
  }

  return MsgSize;
}


//==========================================================================
//
//  VDemoPlaybackNetConnection::SendMessage
//
//==========================================================================
void VDemoPlaybackNetConnection::SendMessage (VMessageOut * /*Msg*/) {
}



#ifdef CLIENT
//==========================================================================
//
//  VDemoRecordingNetConnection::VDemoRecordingNetConnection
//
//==========================================================================
VDemoRecordingNetConnection::VDemoRecordingNetConnection (VSocketPublic *Sock, VNetContext *AContext, VBasePlayer *AOwner)
  : VNetConnection(Sock, AContext, AOwner)
{
  vassert(cls.demorecording);
  vassert(cls.demofile);
  #if 0
  GCon->Logf(NAME_Debug, "writing auth key...");
  VStr s;
  for (int f = 0; f < VNetUtils::ChaCha20KeySize; ++f) { char tmp[8]; snprintf(tmp, sizeof(tmp), " %02X", Sock->AuthKey[f]&0xff); s += tmp; }
  GCon->Logf(NAME_Debug, "%s", *s);
  #endif
  cls.demofile->Serialise(Sock->AuthKey, VNetUtils::ChaCha20KeySize);
  AutoAck = true;
}


//==========================================================================
//
//  VDemoRecordingNetConnection::GetRawPacket
//
//  handles recording and playback of demos
//
//==========================================================================
int VDemoRecordingNetConnection::GetRawPacket (void *dest, size_t destSize) {
  // demo writer has no incoming packets
/*
  int len = VNetConnection::GetRawPacket(dest, destSize);
  if (len > 0 && cls.demorecording) {
    // dumps the current net message, prefixed by the length and view angles
    float Time = (GClLevel ? GClLevel->Time : 0.0f);
    *cls.demofile << Time;
    vint32 MsgSize = (int)len;
    *cls.demofile << MsgSize;
    *cls.demofile << cl->ViewAngles;
    cls.demofile->Serialise(dest, len);
    if (demo_flush_each_packet) cls.demofile->Flush();
  }
  return len;
*/
  return 0;
}


//**************************************************************************
//
// VDemoRecordingSocket
//
//**************************************************************************

//==========================================================================
//
//  VDemoRecordingSocket::VDemoRecordingSocket
//
//==========================================================================
VDemoRecordingSocket::VDemoRecordingSocket ()
  : VSocketPublic()
{
  VNetUtils::GenerateKey(AuthKey);
  memcpy(ClientKey, AuthKey, sizeof(ClientKey));
}


//==========================================================================
//
//  VDemoRecordingSocket::IsLocalConnection
//
//==========================================================================
bool VDemoRecordingSocket::IsLocalConnection () const noexcept {
  return false;
}


//==========================================================================
//
//  VDemoRecordingSocket::GetMessage
//
//==========================================================================
int VDemoRecordingSocket::GetMessage (void * /*dest*/, size_t /*destSize*/) {
  GNet->UpdateNetTime();
  LastMessageTime = GNet->GetNetTime();
  return 0;
}


//==========================================================================
//
//  VDemoRecordingSocket::SendMessage
//
//==========================================================================
int VDemoRecordingSocket::SendMessage (const vuint8 *Msg, vuint32 MsgSize) {
  GNet->UpdateNetTime();
  LastMessageTime = GNet->GetNetTime();
  if (cls.demorecording) {
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
