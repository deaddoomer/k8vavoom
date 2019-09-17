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
#include "net_local.h"


static const char *cli_Port = nullptr;
//static int cli_Listen = 0;

/*static*/ bool cliRegister_netmain_args =
  VParsedArgs::RegisterStringOption("-port", "explicitly set your host port (default is 26000)", &cli_Port)
  /*&& VParsedArgs::RegisterFlagSet("-listen", nullptr, &cli_Listen)*/;


// ////////////////////////////////////////////////////////////////////////// //
class VNetwork : public VNetworkLocal {
private:
  VNetPollProcedure SlistSendProcedure;
  VNetPollProcedure SlistPollProcedure;
  VNetPollProcedure MasterListSendProcedure;
  VNetPollProcedure MasterListPollProcedure;

  bool SlistInProgress;
  bool SlistSilent;
  bool SlistLocal;
  bool SlistSorted;
  bool SlistMaster;
  double SlistStartTime;
  int SlistLastShown;

  slist_t slist;

  VNetPollProcedure *PollProcedureList;

public:
  // public API
  VNetwork ();
  virtual ~VNetwork () override;

  virtual void Init () override;
  virtual void Shutdown () override;
  virtual VSocketPublic *Connect (const char *) override;
  virtual VSocketPublic *CheckNewConnections () override;
  virtual void Poll () override;
  virtual void StartSearch (bool) override;
  virtual slist_t *GetSlist () override;
  virtual void UpdateMaster () override;
  virtual void QuitMaster () override;

  // API only for network drivers!
  virtual double SetNetTime () override;
  virtual void SchedulePollProcedure (VNetPollProcedure *, double) override;

  void MasterList ();
  virtual void Slist () override;

private:
  static void Slist_Send (void *);
  static void Slist_Poll (void *);
  void Slist_Send ();
  void Slist_Poll ();
  static void MasterList_Send (void *);
  static void MasterList_Poll (void *);
  void MasterList_Send ();
  void MasterList_Poll ();
  void PrintSlistHeader ();
  void PrintSlist ();
  void PrintSlistTrailer ();
};


// ////////////////////////////////////////////////////////////////////////// //
VNetworkPublic *GNet;

VCvarS VNetworkLocal::HostName("hostname", "UNNAMED", "Name of this host.");
// k8: dunno, it was 300. but why? we can have big hiccups on level loading,
//     and when sound is not cached, for example. it should be safe to
//     set this to 3 seconds: the only consequence is longer timeout before
//     detecting disconnects. not a big deal, i think.
VCvarF VNetworkPublic::MessageTimeOut("net_messagetimeout", "3000", "Network timeout value in milliseconds.");

VNetDriver *VNetworkLocal::Drivers[MAX_NET_DRIVERS];
int VNetworkLocal::NumDrivers = 0;

VNetLanDriver *VNetworkLocal::LanDrivers[MAX_NET_DRIVERS];
int VNetworkLocal::NumLanDrivers = 0;


//==========================================================================
//
//  VNetworkPublic::Create
//
//==========================================================================
VNetworkPublic *VNetworkPublic::Create () {
  return new VNetwork();
}


//==========================================================================
//
//  VNetworkPublic::VNetworkPublic
//
//==========================================================================
VNetworkPublic::VNetworkPublic ()
  : NetTime(0.0)
  , MessagesSent(0)
  , MessagesReceived(0)
  , UnreliableMessagesSent(0)
  , UnreliableMessagesReceived(0)
  , packetsSent(0)
  , packetsReSent(0)
  , packetsReceived(0)
  , receivedDuplicateCount(0)
  , shortPacketCount(0)
  , droppedDatagrams(0)
{
}


//==========================================================================
//
//  VNetworkLocal::VNetworkLocal
//
//==========================================================================
VNetworkLocal::VNetworkLocal ()
  : ActiveSockets(nullptr)
  , HostCacheCount(0)
  , HostPort(0)
  , DefaultHostPort(26000)
  , IpAvailable(false)
  , Listening(false)
{
  MyIpAddress[0] = 0;
  ReturnReason[0] = 0;
}


//==========================================================================
//
//  VNetwork::VNetwork
//
//==========================================================================
VNetwork::VNetwork()
  : SlistSendProcedure(Slist_Send, this)
  , SlistPollProcedure(Slist_Poll, this)
  , MasterListSendProcedure(MasterList_Send, this)
  , MasterListPollProcedure(MasterList_Poll, this)
  , SlistInProgress(false)
  , SlistSilent(false)
  , SlistLocal(true)
  , SlistSorted(true)
  , SlistMaster(false)
  , SlistStartTime(0.0)
  , SlistLastShown(0)
  , PollProcedureList(nullptr)
{
}


//==========================================================================
//
//  VNetwork::~VNetwork
//
//==========================================================================
VNetwork::~VNetwork () {
  Shutdown();
}


//==========================================================================
//
//  VNetwork::Init
//
//==========================================================================
void VNetwork::Init () {
  const char *p = cli_Port;
  if (p && p[0]) {
    DefaultHostPort = VStr::atoi(p);
    if (DefaultHostPort < 1 || DefaultHostPort > 65535) DefaultHostPort = 26000;
  }
  HostPort = DefaultHostPort;

#ifdef CLIENT
  //if (cli_Listen || cls.state == ca_dedicated) Listening = true;
#else
  Listening = true;
#endif
  SetNetTime();

  // initialise all the drivers
  for (int i = 0; i < NumDrivers; ++i) {
    Drivers[i]->Net = this;
    if (Drivers[i]->Init() != -1) {
      Drivers[i]->initialised = true;
      if (Listening) Drivers[i]->Listen(true);
    }
  }

  if (developer && *MyIpAddress) GCon->Logf(NAME_DevNet, "TCP/IP address %s", MyIpAddress);
}


//==========================================================================
//
//  VNetwork::Shutdown
//
//==========================================================================
void VNetwork::Shutdown () {
  SetNetTime();

  while (ActiveSockets) {
    delete ActiveSockets;
    ActiveSockets = nullptr;
  }

  // shutdown the drivers
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised) {
      Drivers[i]->Shutdown();
      Drivers[i]->initialised = false;
    }
  }
}


//==========================================================================
//
//  VNetwork::SetNetTime
//
//==========================================================================
double VNetwork::SetNetTime () {
  NetTime = Sys_Time();
  //GCon->Logf("*** VNetwork::SetNetTime: %g ***", NetTime);
  return NetTime;
}


//==========================================================================
//
//  VNetwork::Poll
//
//==========================================================================
void VNetwork::Poll () {
  SetNetTime();
  for (VNetPollProcedure *pp = PollProcedureList; pp; pp = pp->next) {
    if (pp->nextTime > NetTime) break;
    PollProcedureList = pp->next;
    pp->procedure(pp->arg);
  }
}


//==========================================================================
//
//  VNetwork::SchedulePollProcedure
//
//==========================================================================
void VNetwork::SchedulePollProcedure (VNetPollProcedure *proc, double timeOffset) {
  VNetPollProcedure *pp, *prev;

  proc->nextTime = Sys_Time()+timeOffset;
  for (pp = PollProcedureList, prev = nullptr; pp; pp = pp->next) {
    if (pp->nextTime >= proc->nextTime) break;
    prev = pp;
  }

  if (prev == nullptr) {
    proc->next = PollProcedureList;
    PollProcedureList = proc;
  } else {
    proc->next = pp;
    prev->next = proc;
  }
}


//==========================================================================
//
//  VNetwork::Slist
//
//==========================================================================
void VNetwork::Slist () {
  if (SlistInProgress) return;

  if (!SlistSilent) {
    GCon->Log(NAME_DevNet, "Looking for k8vavoom servers...");
    PrintSlistHeader();
  }

  SlistMaster = false;
  SlistInProgress = true;
  SlistStartTime = Sys_Time();

  SchedulePollProcedure(&SlistSendProcedure, 0.0);
  SchedulePollProcedure(&SlistPollProcedure, 0.1);

  HostCacheCount = 0;
}


//==========================================================================
//
//  VNetwork::Slist_Send
//
//==========================================================================
void VNetwork::Slist_Send (void *Arg) {
  ((VNetwork *)Arg)->Slist_Send();
}


//==========================================================================
//
//  VNetwork::Slist_Poll
//
//==========================================================================
void VNetwork::Slist_Poll (void *Arg) {
  ((VNetwork *)Arg)->Slist_Poll();
}


//==========================================================================
//
//  VNetwork::Slist_Send
//
//==========================================================================
void VNetwork::Slist_Send () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (!SlistLocal && i == 0) continue;
    if (Drivers[i]->initialised == false) continue;
    Drivers[i]->SearchForHosts(true, SlistMaster);
  }

  if ((Sys_Time()-SlistStartTime) < 0.5) SchedulePollProcedure(&SlistSendProcedure, 0.75);
}


//==========================================================================
//
//  VNetwork::Slist_Poll
//
//==========================================================================
void VNetwork::Slist_Poll () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (!SlistLocal && i == 0) continue;
    if (Drivers[i]->initialised == false) continue;
    Drivers[i]->SearchForHosts(false, SlistMaster);
  }

  if (!SlistSilent) PrintSlist();

  if ((Sys_Time()-SlistStartTime) < 1.5) {
    SchedulePollProcedure(&SlistPollProcedure, 0.1);
    return;
  }

  if (!SlistSilent) PrintSlistTrailer();
  SlistInProgress = false;
  SlistSilent = false;
  SlistLocal = true;
  SlistSorted = false;
}


//==========================================================================
//
//  VNetwork::MasterList
//
//==========================================================================
void VNetwork::MasterList () {
  if (SlistInProgress) return;

  if (!SlistSilent) {
    GCon->Log(NAME_DevNet, "Looking for k8vavoom servers...");
    PrintSlistHeader();
  }

  SlistMaster = true;
  SlistInProgress = true;
  SlistStartTime = Sys_Time();

  SchedulePollProcedure(&MasterListSendProcedure, 0.0);
  SchedulePollProcedure(&MasterListPollProcedure, 0.1);

  HostCacheCount = 0;
}


//==========================================================================
//
//  VNetwork::MasterList_Send
//
//==========================================================================
void VNetwork::MasterList_Send (void *Arg) {
  ((VNetwork *)Arg)->MasterList_Send();
}


//==========================================================================
//
//  VNetwork::MasterList_Poll
//
//==========================================================================
void VNetwork::MasterList_Poll (void *Arg) {
  ((VNetwork *)Arg)->MasterList_Poll();
}


//==========================================================================
//
//  VNetwork::MasterList_Send
//
//==========================================================================
void VNetwork::MasterList_Send () {
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised == false) continue;
    Drivers[i]->QueryMaster(true);
  }

  if ((Sys_Time()-SlistStartTime) < 0.5) SchedulePollProcedure(&MasterListSendProcedure, 0.75);
}


//==========================================================================
//
//  VNetwork::MasterList_Poll
//
//==========================================================================
void VNetwork::MasterList_Poll () {
  // check for reply from master server
  bool GotList = false;
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised == false) continue;
    if (Drivers[i]->QueryMaster(false)) GotList = true;
  }

  // if no reply, try again
  if (!GotList && (Sys_Time()-SlistStartTime) < 1.5) {
    SchedulePollProcedure(&MasterListPollProcedure, 0.1);
    return;
  }

  // close socket for communicating with master server
  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised) Drivers[i]->EndQueryMaster();
  }

  // if we got list, server info command has been sent to all servers, so start listening for their replies
  if (GotList) {
    SlistStartTime = Sys_Time();
    SchedulePollProcedure(&SlistPollProcedure, 0.1);
    return;
  }

  // could not connect to the master server
  if (!SlistSilent) {
    GCon->Log(NAME_DevNet, "Could not connect to the master server.");
    GCon->Log(NAME_DevNet, "");
  }

  SlistInProgress = false;
  SlistSilent = false;
  SlistLocal = true;
  SlistSorted = false;
}


//==========================================================================
//
//  VNetwork::PrintSlistHeader
//
//==========================================================================
void VNetwork::PrintSlistHeader () {
  GCon->Log(NAME_DevNet, "Server          Map             Users");
  GCon->Log(NAME_DevNet, "--------------- --------------- -----");
  SlistLastShown = 0;
}


//==========================================================================
//
//  VNetwork::PrintSlist
//
//==========================================================================
void VNetwork::PrintSlist () {
  int n;
  for (n = SlistLastShown; n < HostCacheCount; ++n) {
    if (HostCache[n].MaxUsers) {
      GCon->Logf(NAME_DevNet, "%-15s %-15s %2d/%2d", *HostCache[n].Name, *HostCache[n].Map, HostCache[n].Users, HostCache[n].MaxUsers);
    } else {
      GCon->Logf(NAME_DevNet, "%-15s %-15s", *HostCache[n].Name, *HostCache[n].Map);
    }
  }
  SlistLastShown = n;
}


//==========================================================================
//
//  VNetwork::PrintSlistTrailer
//
//==========================================================================
void VNetwork::PrintSlistTrailer () {
  if (HostCacheCount) {
    GCon->Log(NAME_DevNet, "== end list ==");
  } else {
    GCon->Log(NAME_DevNet, "No k8vavoom servers found.");
  }
  GCon->Log(NAME_DevNet, "");
}


//==========================================================================
//
//  VNetwork::StartSearch
//
//==========================================================================
void VNetwork::StartSearch (bool Master) {
  SlistSilent = true;
  SlistLocal = false;
  if (Master) {
    MasterList();
  } else {
    Slist();
  }
}


//==========================================================================
//
//  VNetwork::GetSlist
//
//==========================================================================
slist_t *VNetwork::GetSlist () {
  if (!SlistSorted) {
    if (HostCacheCount > 1) {
      vuint8 temp[sizeof(hostcache_t)];
      for (int i = 0; i < HostCacheCount; ++i) {
        for (int j = i+1; j < HostCacheCount; ++j) {
          if (HostCache[j].Name.Cmp(HostCache[i].Name) < 0) {
            memcpy(&temp, (void *)(&HostCache[j]), sizeof(hostcache_t));
            memcpy((void *)(&HostCache[j]), (void *)(&HostCache[i]), sizeof(hostcache_t));
            memcpy((void *)(&HostCache[i]), &temp, sizeof(hostcache_t));
          }
        }
      }
    }
    SlistSorted = true;
    memset(ReturnReason, 0, sizeof(ReturnReason));
  }

  if (SlistInProgress) {
    slist.Flags |= slist_t::SF_InProgress;
  } else {
    slist.Flags &= ~slist_t::SF_InProgress;
  }
  slist.Count = HostCacheCount;
  slist.Cache = HostCache;
  slist.ReturnReason = ReturnReason;
  return &slist;
}


//==========================================================================
//
//  VNetwork::Connect
//
//==========================================================================
VSocketPublic *VNetwork::Connect (const char *InHost) {
  VStr host = InHost;
  VSocket *ret;
  int numdrivers = NumDrivers;
  int n;

  SetNetTime();

  if (host.IsNotEmpty()) {
    if (host == "local") {
      numdrivers = 1;
      goto JustDoIt;
    }

    if (HostCacheCount) {
      for (n = 0; n < HostCacheCount; ++n) {
        if (HostCache[n].Name.ICmp(host) == 0) {
          host = HostCache[n].CName;
          break;
        }
      }
      if (n < HostCacheCount) goto JustDoIt;
    }
  }

  SlistSilent = host.IsNotEmpty();
  Slist();

  while (SlistInProgress) Poll();

  if (host.IsEmpty()) {
    if (HostCacheCount != 1) return nullptr;
    host = HostCache[0].CName;
    GCon->Log(NAME_DevNet, "Connecting to...");
    GCon->Logf(NAME_DevNet, "%s @ %s", *HostCache[0].Name, *host);
    GCon->Log(NAME_DevNet, "");
  }

  if (HostCacheCount) {
    for (n = 0; n < HostCacheCount; ++n) {
      if (HostCache[n].Name.ICmp(host) == 0) {
        host = HostCache[n].CName;
        break;
      }
    }
  }

JustDoIt:
  for (int i = 0; i < numdrivers; ++i) {
    if (Drivers[i]->initialised == false) continue;
    ret = Drivers[i]->Connect(*host);
    if (ret) return ret;
  }

  if (host.IsNotEmpty()) {
    PrintSlistHeader();
    PrintSlist();
    PrintSlistTrailer();
  }

  return nullptr;
}


//==========================================================================
//
//  VNetwork::CheckNewConnections
//
//==========================================================================
VSocketPublic *VNetwork::CheckNewConnections () {
  SetNetTime();

  for (int i = 0; i < NumDrivers; ++i) {
    if (Drivers[i]->initialised == false) continue;
    if (i && Listening == false) continue;
    VSocket *ret = Drivers[i]->CheckNewConnections();
    if (ret) return ret;
  }

  return nullptr;
}


//==========================================================================
//
//  VNetwork::UpdateMaster
//
//==========================================================================
void VNetwork::UpdateMaster () {
  SetNetTime();
  for (int i = 0; i < NumDrivers; ++i) {
    if (!Drivers[i]->initialised) continue;
    Drivers[i]->UpdateMaster();
  }
}


//==========================================================================
//
//  VNetwork::QuitMaster
//
//==========================================================================
void VNetwork::QuitMaster () {
  SetNetTime();
  for (int i = 0; i < NumDrivers; ++i) {
    if (!Drivers[i]->initialised) continue;
    Drivers[i]->QuitMaster();
  }
}


//==========================================================================
//
//  VSocket::VSocket
//
//==========================================================================
VSocket::VSocket (VNetDriver *Drv) : Driver(Drv) {
  // add it to active list
  Next = Driver->Net->ActiveSockets;
  Driver->Net->ActiveSockets = this;

  ConnectTime = Driver->Net->NetTime;
  Address = "UNSET ADDRESS";
  LastMessageTime = Driver->Net->NetTime;
}


//==========================================================================
//
//  VSocket::~VSocket
//
//==========================================================================
VSocket::~VSocket () {
  // remove it from active list
  if (this == Driver->Net->ActiveSockets) {
    Driver->Net->ActiveSockets = Driver->Net->ActiveSockets->Next;
  } else {
    VSocket *s = nullptr;
    for (s = Driver->Net->ActiveSockets; s; s = s->Next) {
      if (s->Next == this) {
        s->Next = this->Next;
        break;
      }
    }
    if (!s) Sys_Error("NET_FreeQSocket: not active\n");
  }
}


//==========================================================================
//
//  VNetDriver::VNetDriver
//
//==========================================================================
VNetDriver::VNetDriver (int Level, const char *AName) : name(AName), initialised(false) {
  VNetwork::Drivers[Level] = this;
  if (VNetwork::NumDrivers <= Level) VNetwork::NumDrivers = Level+1;
}


//==========================================================================
//
//  VNetLanDriver::VNetLanDriver
//
//==========================================================================
VNetLanDriver::VNetLanDriver (int Level, const char *AName)
  : name(AName)
  , initialised(false)
  , controlSock(0)
  , MasterQuerySocket(-1)
  , net_acceptsocket(-1)
  , net_controlsocket(0)
  , net_broadcastsocket(0)
  , myAddr(0)
{
  memset(&broadcastaddr, 0, sizeof(broadcastaddr));
  VNetwork::LanDrivers[Level] = this;
  if (VNetwork::NumLanDrivers <= Level) VNetwork::NumLanDrivers = Level+1;
}


#if defined(CLIENT) && defined(SERVER) /* I think like this */
//==========================================================================
//
//  COMMAND Listen
//
//==========================================================================
COMMAND(Listen) {
  VNetwork *Net = (VNetwork *)GNet;
  if (Args.Num() != 2) {
    GCon->Logf(NAME_DevNet, "\"listen\" is \"%d\"", Net->Listening ? 1 : 0);
    return;
  }
  Net->Listening = (VStr::atoi(*Args[1]) ? true : false);
  for (int i = 0; i < VNetwork::NumDrivers; ++i) {
    if (VNetwork::Drivers[i]->initialised == false) continue;
    VNetwork::Drivers[i]->Listen(Net->Listening);
  }
}
#endif


//==========================================================================
//
//  COMMAND Port
//
//==========================================================================
COMMAND(Port) {
  int n;

  VNetwork *Net = (VNetwork *)GNet;
  if (Args.Num() != 2) {
    GCon->Logf(NAME_DevNet, "\"port\" is \"%d\"", Net->HostPort);
    return;
  }

  n = VStr::atoi(*Args[1]);
  if (n < 1 || n > 65534) {
    GCon->Log(NAME_Error, "Bad value, must be between 1 and 65534");
    return;
  }

  Net->DefaultHostPort = n;
  Net->HostPort = n;

  if (Net->Listening) {
    // force a change to the new port
    GCmdBuf << "listen 0\n";
    GCmdBuf << "listen 1\n";
  }
}


//==========================================================================
//
//  COMMAND Slist
//
//==========================================================================
COMMAND(Slist) {
  ((VNetwork *)GNet)->Slist();
}


//==========================================================================
//
//  COMMAND MasterList
//
//==========================================================================
COMMAND(MasterList) {
  ((VNetwork *)GNet)->MasterList();
}
