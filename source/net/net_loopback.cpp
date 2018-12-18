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
#include "net_local.h"


// ////////////////////////////////////////////////////////////////////////// //
struct VLoopbackMessage {
  TArray<vuint8> Data;
};


// ////////////////////////////////////////////////////////////////////////// //
class VLoopbackSocket : public VSocket {
public:
  VLoopbackSocket *OtherSock;
  TArray<VLoopbackMessage> LoopbackMessages;

public:
  VLoopbackSocket (VNetDriver *Drv) : VSocket(Drv), OtherSock(nullptr) {}
  virtual ~VLoopbackSocket () override;

  virtual int GetMessage (TArray<vuint8> &) override;
  virtual int SendMessage (const vuint8 *, vuint32) override;
  virtual bool IsLocalConnection () override;
};


// ////////////////////////////////////////////////////////////////////////// //
class VLoopbackDriver : public VNetDriver {
public:
  bool localconnectpending;
  VLoopbackSocket *loop_client;
  VLoopbackSocket *loop_server;

public:
  VLoopbackDriver ();

  virtual int Init () override;
  virtual void Listen (bool) override;
  virtual void SearchForHosts (bool, bool) override;
  virtual VSocket *Connect (const char *) override;
  virtual VSocket *CheckNewConnections () override;
  virtual void UpdateMaster () override;
  virtual void QuitMaster () override;
  virtual bool QueryMaster (bool) override;
  virtual void EndQueryMaster () override;
  virtual void Shutdown () override;

  static int IntAlign (int);
};


static VLoopbackDriver  Impl;


//==========================================================================
//
//  VLoopbackDriver::VLoopbackDriver
//
//==========================================================================
VLoopbackDriver::VLoopbackDriver ()
  : VNetDriver(0, "Loopback")
  , localconnectpending(false)
  , loop_client(nullptr)
  , loop_server(nullptr)
{
}


//==========================================================================
//
//  VLoopbackDriver::Init
//
//==========================================================================
int VLoopbackDriver::Init () {
#ifdef CLIENT
  if (GGameInfo && GGameInfo->NetMode == NM_DedicatedServer) return -1;
  return 0;
#else
  return -1;
#endif
}


//==========================================================================
//
//  VLoopbackDriver::Listen
//
//==========================================================================
void VLoopbackDriver::Listen (bool) {
}


//==========================================================================
//
//  VLoopbackDriver::SearchForHosts
//
//==========================================================================
void VLoopbackDriver::SearchForHosts (bool, bool ForMaster) {
  guard(VLoopbackDriver::SearchForHosts);
#ifdef SERVER
  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client || ForMaster) return;
  Net->HostCacheCount = 1;
  if (VStr::Cmp(Net->HostName, "UNNAMED") == 0) {
    Net->HostCache[0].Name = "local";
  } else {
    Net->HostCache[0].Name = Net->HostName;
  }
  Net->HostCache[0].Map = VStr(GLevel->MapName);
  Net->HostCache[0].Users = svs.num_connected;
  Net->HostCache[0].MaxUsers = svs.max_clients;
  Net->HostCache[0].CName = "local";
#endif
  unguard;
}


//==========================================================================
//
//  VLoopbackDriver::Connect
//
//==========================================================================
VSocket *VLoopbackDriver::Connect (const char *host) {
  guard(VLoopbackDriver::Connect);
  if (VStr::Cmp(host, "local") != 0) return nullptr;

  localconnectpending = true;

  if (!loop_client) {
    loop_client = new VLoopbackSocket(this);
    loop_client->Address = "localhost";
  }

  if (!loop_server) {
    loop_server = new VLoopbackSocket(this);
    loop_server->Address = "LOCAL";
  }

  loop_client->OtherSock = loop_server;
  loop_server->OtherSock = loop_client;

  return loop_client;
  unguard;
}


//==========================================================================
//
//  VLoopbackDriver::CheckNewConnections
//
//==========================================================================
VSocket *VLoopbackDriver::CheckNewConnections () {
  guard(VLoopbackDriver::CheckNewConnections);
  if (!localconnectpending) return nullptr;
  localconnectpending = false;
  return loop_server;
  unguard;
}


//==========================================================================
//
//  VLoopbackDriver::IntAlign
//
//==========================================================================
int VLoopbackDriver::IntAlign (int value) {
  return (value+(sizeof(int)-1))&(~(sizeof(int)-1));
}


//==========================================================================
//
//  VLoopbackDriver::UpdateMaster
//
//==========================================================================
void VLoopbackDriver::UpdateMaster () {
}


//==========================================================================
//
//  VLoopbackDriver::QuitMaster
//
//==========================================================================
void VLoopbackDriver::QuitMaster () {
}


//==========================================================================
//
//  VLoopbackDriver::QueryMaster
//
//==========================================================================
bool VLoopbackDriver::QueryMaster (bool) {
  return false;
}


//==========================================================================
//
//  VLoopbackDriver::EndQueryMaster
//
//==========================================================================
void VLoopbackDriver::EndQueryMaster () {
}


//==========================================================================
//
//  VLoopbackDriver::Shutdown
//
//==========================================================================
void VLoopbackDriver::Shutdown () {
}


//==========================================================================
//
//  VLoopbackSocket::~VLoopbackSocket
//
//==========================================================================
VLoopbackSocket::~VLoopbackSocket () {
  if (OtherSock) OtherSock->OtherSock = nullptr;
  if (this == ((VLoopbackDriver*)Driver)->loop_client) {
    ((VLoopbackDriver*)Driver)->loop_client = nullptr;
  } else {
    ((VLoopbackDriver*)Driver)->loop_server = nullptr;
  }
}


//==========================================================================
//
//  VLoopbackSocket::GetMessage
//
// If there is a packet, return it.
//
// returns 0 if no data is waiting
// returns 1 if a packet was received
// returns -1 if connection is invalid
//
//==========================================================================
int VLoopbackSocket::GetMessage (TArray<vuint8> &Data) {
  guard(VLoopbackSocket::GetMessage);
  if (!LoopbackMessages.Num()) return 0;
  Data = LoopbackMessages[0].Data;
  LoopbackMessages.RemoveIndex(0);
  return 1;
  unguard;
}


//==========================================================================
//
//  VLoopbackSocket::SendMessage
//
// Send a packet over the net connection.
// returns 1 if the packet was sent properly
// returns -1 if the connection died
//
//==========================================================================
int VLoopbackSocket::SendMessage (const vuint8 *Data, vuint32 Length) {
  guard(VLoopbackSocket::SendMessage);
  if (!OtherSock) return -1;
  VLoopbackMessage &Msg = OtherSock->LoopbackMessages.Alloc();
  Msg.Data.SetNum(Length);
  memcpy(Msg.Data.Ptr(), Data, Length);
  return 1;
  unguard;
}


//==========================================================================
//
//  VLoopbackSocket::IsLocalConnection
//
//==========================================================================
bool VLoopbackSocket::IsLocalConnection () {
  return true;
}
