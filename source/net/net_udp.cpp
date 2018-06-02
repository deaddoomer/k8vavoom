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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>

#include "gamedefs.h"
#include "net_local.h"


// ////////////////////////////////////////////////////////////////////////// //
class VUdpDriver : public VNetLanDriver {
public:
  enum { MAXHOSTNAMELEN = 256 };

  VUdpDriver ();
  virtual int Init () override;
  virtual void Shutdown () override;
  virtual void Listen (bool state) override;
  virtual int OpenSocket (int) override;
  virtual int CloseSocket (int) override;
  virtual int Connect (int, sockaddr_t *) override;
  virtual int CheckNewConnections () override;
  virtual int Read (int, vuint8 *, int, sockaddr_t*) override;
  virtual int Write (int, const vuint8 *, int, sockaddr_t *) override;
  virtual int Broadcast (int, const vuint8 *, int) override;
  virtual char *AddrToString (sockaddr_t *) override;
  virtual int StringToAddr (const char *, sockaddr_t *) override;
  virtual int GetSocketAddr (int, sockaddr_t *) override;
  virtual VStr GetNameFromAddr (sockaddr_t *) override;
  virtual int GetAddrFromName (const char *, sockaddr_t *, int) override;
  virtual int AddrCompare (const sockaddr_t *, const sockaddr_t *) override;
  virtual int GetSocketPort (const sockaddr_t *) override;
  virtual int SetSocketPort (sockaddr_t *, int) override;

  int PartialIPAddress (const char *, sockaddr_t *, int);
};


static VUdpDriver Impl;


//==========================================================================
//
//  VUdpDriver::VUdpDriver
//
//==========================================================================
VUdpDriver::VUdpDriver () : VNetLanDriver(0, "UDP") {
}


//==========================================================================
//
//  VUdpDriver::Init
//
//==========================================================================
int VUdpDriver::Init () {
  guard(VUdpDriver::Init);

  hostent* local;
  char buff[MAXHOSTNAMELEN];
  sockaddr_t addr;
  char* colon;

  if (GArgs.CheckParm("-noudp")) return -1;

  // determine my name & address
  gethostname(buff, MAXHOSTNAMELEN);
  GCon->Logf(NAME_Init, "Host name: %s", buff);
  local = gethostbyname(buff);
  if (!local) Sys_Error("UDP_Init: Couldn't get local host by name %s,\nCheck your /etc/hosts file.", buff);
  myAddr = *(int *)local->h_addr_list[0];

  // if the Vavoom hostname isn't set, set it to the machine name
  if (VStr::Cmp(Net->HostName, "UNNAMED") == 0) {
    buff[15] = 0;
    Net->HostName = buff;
  }

  if ((net_controlsocket = OpenSocket(0)) == -1) Sys_Error("UDP_Init: Unable to open control socket\n");

  ((sockaddr_in *)&broadcastaddr)->sin_family = AF_INET;
  ((sockaddr_in *)&broadcastaddr)->sin_addr.s_addr = INADDR_BROADCAST;
  ((sockaddr_in *)&broadcastaddr)->sin_port = htons(Net->HostPort);

  GetSocketAddr(net_controlsocket, &addr);
  VStr::Cpy(Net->MyIpAddress, AddrToString(&addr));
  colon = strrchr(Net->MyIpAddress, ':');
  if (colon) *colon = 0;
  GCon->Logf(NAME_Init, "My IP address: %s", Net->MyIpAddress);

  GCon->Log(NAME_Init, "UDP Initialised");
  Net->IpAvailable = true;

  return net_controlsocket;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::Shutdown
//
//==========================================================================
void VUdpDriver::Shutdown () {
  guard(VUdpDriver::Shutdown);
  Listen(false);
  CloseSocket(net_controlsocket);
  unguard;
}


//==========================================================================
//
//  VUdpDriver::Listen
//
//==========================================================================
void VUdpDriver::Listen (bool state) {
  guard(VUdpDriver::Listen);
  if (state) {
    // enable listening
    if (net_acceptsocket == -1) {
      net_acceptsocket = OpenSocket(Net->HostPort);
      if (net_acceptsocket == -1) Sys_Error("UDP_Listen: Unable to open accept socket\n");
    }
  } else {
    // disable listening
    if (net_acceptsocket != -1) {
      CloseSocket(net_acceptsocket);
      net_acceptsocket = -1;
    }
  }
  unguard;
}


//==========================================================================
//
//  VUdpDriver::OpenSocket
//
//==========================================================================
int VUdpDriver::OpenSocket (int port) {
  guard(UDP_OpenSocket);

  int newsocket;
  sockaddr_in address;
  int trueval = 1;

  newsocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (newsocket == -1) return -1;

  if (ioctl(newsocket, FIONBIO, (char*)&trueval) == -1) {
    close(newsocket);
    return -1;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  if (bind(newsocket, (sockaddr*)&address, sizeof(address)) == -1) {
    close(newsocket);
    return -1;
  }

  return newsocket;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::CloseSocket
//
//==========================================================================
int VUdpDriver::CloseSocket (int socket) {
  guard(VUdpDriver::CloseSocket);
  if (socket == net_broadcastsocket) net_broadcastsocket = 0;
  return close(socket);
  unguard;
}


//==========================================================================
//
//  VUdpDriver::Connect
//
//==========================================================================
int VUdpDriver::Connect (int, sockaddr_t *) {
  return 0;
}


//==========================================================================
//
//  VUdpDriver::CheckNewConnections
//
//==========================================================================
int VUdpDriver::CheckNewConnections () {
  guard(VUdpDriver::CheckNewConnections);
  char buf[4096];
  if (net_acceptsocket == -1) return -1;
  if (recvfrom(net_acceptsocket, buf, sizeof(buf), MSG_PEEK, nullptr, nullptr) >= 0) {
    return net_acceptsocket;
  }
  return -1;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::Read
//
//==========================================================================
int VUdpDriver::Read (int socket, vuint8 *buf, int len, sockaddr_t *addr) {
  guard(VUdpDriver::Read);
  socklen_t addrlen = sizeof(sockaddr_t);
  int ret = recvfrom(socket, buf, len, 0, (sockaddr*)addr, &addrlen);
  if (ret == -1 && (errno == EWOULDBLOCK || errno == ECONNREFUSED)) return 0;
  return ret;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::Write
//
//==========================================================================
int VUdpDriver::Write (int socket, const vuint8 *buf, int len, sockaddr_t *addr) {
  guard(VUdpDriver::Write);
  int ret = sendto(socket, buf, len, 0, (sockaddr *)addr, sizeof(sockaddr));
  if (ret == -1 && errno == EWOULDBLOCK) return 0;
  return ret;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::Broadcast
//
//==========================================================================
int VUdpDriver::Broadcast (int socket, const vuint8 *buf, int len) {
  guard(VUdpDriver::Broadcast);
  int i = 1;
  if (socket != net_broadcastsocket) {
    if (net_broadcastsocket != 0) Sys_Error("Attempted to use multiple broadcasts sockets\n");
    // make this socket broadcast capable
    if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i)) < 0) {
      GCon->Log(NAME_DevNet, "Unable to make socket broadcast capable");
      return -1;
    }
    net_broadcastsocket = socket;
  }
  return Write(socket, buf, len, &broadcastaddr);
  unguard;
}


//==========================================================================
//
//  VUdpDriver::AddrToString
//
//==========================================================================
char *VUdpDriver::AddrToString (sockaddr_t *addr) {
  guard(VUdpDriver::AddrToString);
  static char buffer[32];
  int haddr;

  haddr = ntohl(((sockaddr_in *)addr)->sin_addr.s_addr);
  snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d:%d", (haddr>>24)&0xff,
    (haddr>>16)&0xff, (haddr>>8)&0xff, haddr&0xff,
    ntohs(((sockaddr_in *)addr)->sin_port));
  return buffer;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::StringToAddr
//
//==========================================================================
int VUdpDriver::StringToAddr (const char *string, sockaddr_t *addr) {
  guard(VUdpDriver::StringToAddr);
  int ha1, ha2, ha3, ha4, hp;
  int ipaddr;

  sscanf(string, "%d.%d.%d.%d:%d", &ha1, &ha2, &ha3, &ha4, &hp);
  ipaddr = (ha1<<24)|(ha2<<16)|(ha3<<8)|ha4;

  addr->sa_family = AF_INET;
  ((sockaddr_in *)addr)->sin_addr.s_addr = htonl(ipaddr);
  ((sockaddr_in *)addr)->sin_port = htons(hp);
  return 0;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::GetSocketAddr
//
//==========================================================================
int VUdpDriver::GetSocketAddr (int socket, sockaddr_t *addr) {
  guard(VUdpDriver::GetSocketAddr);
  socklen_t addrlen = sizeof(sockaddr_t);
  vuint32 a;

  memset(addr, 0, sizeof(sockaddr_t));
  getsockname(socket, (sockaddr *)addr, &addrlen);
  a = ((sockaddr_in *)addr)->sin_addr.s_addr;
  if (a == 0 || a == inet_addr("127.0.0.1")) ((sockaddr_in *)addr)->sin_addr.s_addr = myAddr;

  return 0;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::GetNameFromAddr
//
//==========================================================================
VStr VUdpDriver::GetNameFromAddr (sockaddr_t *addr) {
  guard(VUdpDriver::GetNameFromAddr);
  hostent *hostentry = gethostbyaddr((char*)&((sockaddr_in*)addr)->sin_addr, sizeof(in_addr), AF_INET);
  if (hostentry) return (char *)hostentry->h_name;
  return AddrToString(addr);
  unguard;
}


//==========================================================================
//
//  VUdpDriver::PartialIPAddress
//
// This lets you type only as much of the net address as required, using
// the local network components to fill in the rest
//
//==========================================================================
int VUdpDriver::PartialIPAddress (const char *in, sockaddr_t *hostaddr, int DefaultPort) {
  guard(VUdpDriver::PartialIPAddress);
  char buff[256];
  char *b;
  int addr;
  int num;
  int mask;
  int run;
  int port;

  buff[0] = '.';
  b = buff;
  VStr::Cpy(buff+1, in);
  if (buff[1] == '.') ++b;

  addr = 0;
  mask = -1;
  while (*b == '.') {
    ++b;
    num = 0;
    run = 0;
    while (!(*b < '0' || *b > '9')) {
      num = num*10 + *b++ - '0';
      if (++run > 3) return -1;
    }
    if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0) return -1;
    if (num < 0 || num > 255) return -1;
    mask <<= 8;
    addr = (addr << 8) + num;
  }

  if (*b++ == ':') port = atoi(b); else port = DefaultPort;

  hostaddr->sa_family = AF_INET;
  ((sockaddr_in *)hostaddr)->sin_port = htons((short)port);
  ((sockaddr_in *)hostaddr)->sin_addr.s_addr = (myAddr & htonl(mask)) | htonl(addr);

  return 0;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::GetAddrFromName
//
//==========================================================================
int VUdpDriver::GetAddrFromName (const char *name, sockaddr_t *addr,int DefaultPort) {
  guard(VUdpDriver::GetAddrFromName);
  hostent *hostentry;

  if (name[0] >= '0' && name[0] <= '9') return PartialIPAddress(name, addr, DefaultPort);

  hostentry = gethostbyname(name);
  if (!hostentry) return -1;

  addr->sa_family = AF_INET;
  ((sockaddr_in*)addr)->sin_port = htons(DefaultPort);
  ((sockaddr_in*)addr)->sin_addr.s_addr = *(int*)hostentry->h_addr_list[0];

  return 0;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::AddrCompare
//
//==========================================================================
int VUdpDriver::AddrCompare (const sockaddr_t *addr1, const sockaddr_t *addr2) {
  guard(VUdpDriver::AddrCompare);
  if (addr1->sa_family != addr2->sa_family) return -1;
  if (((const sockaddr_in *)addr1)->sin_addr.s_addr != ((const sockaddr_in *)addr2)->sin_addr.s_addr) return -1;
  if (((const sockaddr_in *)addr1)->sin_port != ((const sockaddr_in *)addr2)->sin_port) return 1;
  return 0;
  unguard;
}


//==========================================================================
//
//  VUdpDriver::GetSocketPort
//
//==========================================================================
int VUdpDriver::GetSocketPort (const sockaddr_t *addr) {
  guard(VUdpDriver::GetSocketPort);
  return ntohs(((const sockaddr_in *)addr)->sin_port);
  unguard;
}


//==========================================================================
//
//  VUdpDriver::SetSocketPort
//
//==========================================================================
int VUdpDriver::SetSocketPort (sockaddr_t *addr, int port) {
  guard(VUdpDriver::SetSocketPort);
  ((sockaddr_in *)addr)->sin_port = htons(port);
  return 0;
  unguard;
}
