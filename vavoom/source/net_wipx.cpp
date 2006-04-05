//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2002 J�nis Legzdi��
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "winlocal.h"
#include <wsipx.h>
#include "gamedefs.h"
#include "net_loc.h"
#include "net_wipx.h"

// MACROS ------------------------------------------------------------------

#define MAXHOSTNAMELEN		256
#define IPXSOCKETS			18

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean		winsock_initialized;
extern WSADATA		winsockdata;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int			net_acceptsocket = -1;		// socket for fielding new connections
static int			net_controlsocket;
static sockaddr_t	broadcastaddr;

static int			ipxsocket[IPXSOCKETS];
static int			sequence[IPXSOCKETS];

static char			packetBuffer[NET_DATAGRAMSIZE + 4];

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  WIPX_Init
//
//==========================================================================

int WIPX_Init()
{
	guard(WIPX_Init);
	int			i;
	char		buff[MAXHOSTNAMELEN];
	sockaddr_t	addr;
	char		*p;
	int			r;

	if (GArgs.CheckParm("-noipx"))
		return -1;

	if (winsock_initialized == 0)
	{
		r = WSAStartup(MAKEWORD(1, 1), &winsockdata);

		if (r)
		{
			GCon->Log(NAME_Init, "Winsock initialization failed.");
			return -1;
		}
	}
	winsock_initialized++;

	for (i = 0; i < IPXSOCKETS; i++)
		ipxsocket[i] = 0;

	// determine my name & address
	if (gethostname(buff, MAXHOSTNAMELEN) == 0)
	{
		// if the Vavoom hostname isn't set, set it to the machine name
		if (strcmp(hostname, "UNNAMED") == 0)
		{
			// see if it's a text IP address (well, close enough)
			for (p = buff; *p; p++)
				if ((*p < '0' || *p > '9') && *p != '.')
					break;

			// if it is a real name, strip off the domain; we only want the host
			if (*p)
			{
				for (i = 0; i < 15; i++)
					if (buff[i] == '.')
						break;
				buff[i] = 0;
			}
			hostname = buff;
		}
	}

    net_controlsocket = WIPX_OpenSocket(0);
	if (net_controlsocket == -1)
	{
		GCon->Log(NAME_Init, "WIPX_Init: Unable to open control socket");
		if (--winsock_initialized == 0)
			WSACleanup();
		return -1;
	}

	((sockaddr_ipx *)&broadcastaddr)->sa_family = AF_IPX;
	memset(((sockaddr_ipx *)&broadcastaddr)->sa_netnum, 0, 4);
	memset(((sockaddr_ipx *)&broadcastaddr)->sa_nodenum, 0xff, 6);
	((sockaddr_ipx *)&broadcastaddr)->sa_socket = htons((word)net_hostport);

	WIPX_GetSocketAddr(net_controlsocket, &addr);
	strcpy(my_ipx_address, WIPX_AddrToString(&addr));
	p = strrchr(my_ipx_address, ':');
	if (p)
		*p = 0;

	GCon->Log(NAME_Init, "Winsock IPX Initialized");
	ipxAvailable = true;

	return net_controlsocket;
	unguard;
}

//==========================================================================
//
//  WIPX_Shutdown
//
//==========================================================================

void WIPX_Shutdown()
{
	guard(WIPX_Shutdown);
	WIPX_Listen(false);
	WIPX_CloseSocket(net_controlsocket);
	if (--winsock_initialized == 0)
		WSACleanup();
	unguard;
}

//==========================================================================
//
//  WIPX_Listen
//
//==========================================================================

void WIPX_Listen(bool state)
{
	guard(WIPX_Listen);
	// enable listening
	if (state)
	{
		if (net_acceptsocket == -1)
		{
	        net_acceptsocket = WIPX_OpenSocket(net_hostport);
			if (net_acceptsocket == -1)
				Sys_Error("WIPX_Listen: Unable to open accept socket\n");
		}
	}
	else
    {
		// disable listening
		if (net_acceptsocket != -1)
		{
			WIPX_CloseSocket (net_acceptsocket);
			net_acceptsocket = -1;
		}
	}
	unguard;
}

//==========================================================================
//
//  WIPX_OpenSocket
//
//==========================================================================

int WIPX_OpenSocket(int port)
{
	guard(WIPX_OpenSocket);
	int				handle;
	int				newsocket;
	sockaddr_ipx	address;
	dword			trueval = 1;

	for (handle = 0; handle < IPXSOCKETS; handle++)
		if (ipxsocket[handle] == 0)
			break;
	if (handle == IPXSOCKETS)
		return -1;

    newsocket = socket(AF_IPX, SOCK_DGRAM, NSPROTO_IPX);
	if (newsocket == INVALID_SOCKET)
		return -1;

	if (ioctlsocket(newsocket, FIONBIO, &trueval) == -1)
		goto ErrorReturn;

	if (setsockopt(newsocket, SOL_SOCKET, SO_BROADCAST, (char *)&trueval, sizeof(trueval)) < 0)
		goto ErrorReturn;

	address.sa_family = AF_IPX;
	memset(address.sa_netnum, 0, 4);
	memset(address.sa_nodenum, 0, 6);;
	address.sa_socket = htons((word)port);
	if (bind(newsocket, (sockaddr *)&address, sizeof(address)) == 0)
	{
		ipxsocket[handle] = newsocket;
		sequence[handle] = 0;
		return handle;
	}

	Sys_Error("Winsock IPX bind failed\n");
ErrorReturn:
	closesocket(newsocket);
	return -1;
	unguard;
}

//==========================================================================
//
//  WIPX_CloseSocket
//
//==========================================================================

int WIPX_CloseSocket(int handle)
{
	guard(WIPX_CloseSocket);
	int socket = ipxsocket[handle];
	int ret;

	ret =  closesocket(socket);
	ipxsocket[handle] = 0;
	return ret;
	unguard;
}

//==========================================================================
//
//  WIPX_Connect
//
//==========================================================================

int WIPX_Connect(int, sockaddr_t*)
{
	return 0;
}

//==========================================================================
//
//  WIPX_CheckNewConnections
//
//==========================================================================

int WIPX_CheckNewConnections()
{
	guard(WIPX_CheckNewConnections);
	dword		available;

	if (net_acceptsocket == -1)
		return -1;

	if (ioctlsocket(ipxsocket[net_acceptsocket], FIONREAD, &available) == -1)
		Sys_Error("WIPX: ioctlsocket (FIONREAD) failed\n");
	if (available)
		return net_acceptsocket;
	return -1;
	unguard;
}

//==========================================================================
//
//  WIPX_Read
//
//==========================================================================

int WIPX_Read(int handle, byte *buf, int len, sockaddr_t *addr)
{
	guard(WIPX_Read);
	int 	addrlen = sizeof(sockaddr_t);
	int 	socket = ipxsocket[handle];
	int 	ret;

	ret = recvfrom(socket, packetBuffer, len + 4, 0, (struct sockaddr *)addr, &addrlen);
	if (ret == -1)
	{
		int e = WSAGetLastError();

		if (e == WSAEWOULDBLOCK || e == WSAECONNREFUSED)
			return 0;
	}

	if (ret < 4)
		return 0;
	
	// remove sequence number, it's only needed for DOS IPX
	ret -= 4;
	memcpy(buf, packetBuffer + 4, ret);

	return ret;
	unguard;
}

//==========================================================================
//
//  WIPX_Write
//
//==========================================================================

int WIPX_Write(int handle, byte *buf, int len, sockaddr_t *addr)
{
	guard(WIPX_Write);
	int 	socket = ipxsocket[handle];
	int 	ret;

	// build packet with sequence number
	*(int *)(&packetBuffer[0]) = sequence[handle];
	sequence[handle]++;
	memcpy(&packetBuffer[4], buf, len);
	len += 4;

	ret = sendto(socket, packetBuffer, len, 0, (sockaddr *)addr, sizeof(sockaddr_t));
	if (ret == -1)
		if (WSAGetLastError() == WSAEWOULDBLOCK)
			return 0;

	return ret;
	unguard;
}

//==========================================================================
//
//  WIPX_Broadcast
//
//==========================================================================

int WIPX_Broadcast(int handle, byte *buf, int len)
{
	guard(WIPX_Broadcast);
	return WIPX_Write(handle, buf, len, &broadcastaddr);
	unguard;
}

//==========================================================================
//
//  WIPX_AddrToString
//
//==========================================================================

char *WIPX_AddrToString(sockaddr_t *addr)
{
	guard(WIPX_AddrToString);
	static char buf[28];

	sprintf(buf, "%02x%02x%02x%02x:%02x%02x%02x%02x%02x%02x:%u",
		((sockaddr_ipx *)addr)->sa_netnum[0] & 0xff,
		((sockaddr_ipx *)addr)->sa_netnum[1] & 0xff,
		((sockaddr_ipx *)addr)->sa_netnum[2] & 0xff,
		((sockaddr_ipx *)addr)->sa_netnum[3] & 0xff,
		((sockaddr_ipx *)addr)->sa_nodenum[0] & 0xff,
		((sockaddr_ipx *)addr)->sa_nodenum[1] & 0xff,
		((sockaddr_ipx *)addr)->sa_nodenum[2] & 0xff,
		((sockaddr_ipx *)addr)->sa_nodenum[3] & 0xff,
		((sockaddr_ipx *)addr)->sa_nodenum[4] & 0xff,
		((sockaddr_ipx *)addr)->sa_nodenum[5] & 0xff,
		ntohs(((sockaddr_ipx *)addr)->sa_socket)
		);
	return buf;
	unguard;
}

//==========================================================================
//
//  WIPX_StringToAddr
//
//==========================================================================

int WIPX_StringToAddr(const char* string, sockaddr_t* addr)
{
	guard(WIPX_StringToAddr);
	int  val;
	char buf[3];

	buf[2] = 0;
	memset(addr, 0, sizeof(sockaddr_t));
	addr->sa_family = AF_IPX;

#define DO(src,dest)	\
	buf[0] = string[src];	\
	buf[1] = string[src + 1];	\
	if (sscanf (buf, "%x", &val) != 1)	\
		return -1;	\
	((struct sockaddr_ipx *)addr)->dest = val

	DO(0, sa_netnum[0]);
	DO(2, sa_netnum[1]);
	DO(4, sa_netnum[2]);
	DO(6, sa_netnum[3]);
	DO(9, sa_nodenum[0]);
	DO(11, sa_nodenum[1]);
	DO(13, sa_nodenum[2]);
	DO(15, sa_nodenum[3]);
	DO(17, sa_nodenum[4]);
	DO(19, sa_nodenum[5]);
#undef DO

	sscanf (&string[22], "%u", &val);
	((sockaddr_ipx *)addr)->sa_socket = htons((word)val);

	return 0;
	unguard;
}

//==========================================================================
//
//  WIPX_GetSocketAddr
//
//==========================================================================

int WIPX_GetSocketAddr(int handle, sockaddr_t* addr)
{
	guard(WIPX_GetSocketAddr);
	int 	socket = ipxsocket[handle];
	int 	addrlen = sizeof(sockaddr_t);

	memset(addr, 0, sizeof(sockaddr_t));
	if (getsockname(socket, (sockaddr *)addr, &addrlen) != 0)
	{
		WSAGetLastError();
	}

	return 0;
	unguard;
}

//==========================================================================
//
//  WIPX_GetNameFromAddr
//
//==========================================================================

int WIPX_GetNameFromAddr(sockaddr_t* addr, char* name)
{
	guard(WIPX_GetNameFromAddr);
	strcpy(name, WIPX_AddrToString(addr));
	return 0;
	unguard;
}

//==========================================================================
//
//  WIPX_GetAddrFromName
//
//==========================================================================

int WIPX_GetAddrFromName(const char* name, sockaddr_t* addr)
{
	guard(WIPX_GetAddrFromName);
	int		n;
	char	buf[32];

	n = strlen(name);

	if (n == 12)
	{
		sprintf(buf, "00000000:%s:%u", name, net_hostport);
		return WIPX_StringToAddr(buf, addr);
	}
	if (n == 21)
	{
		sprintf(buf, "%s:%u", name, net_hostport);
		return WIPX_StringToAddr(buf, addr);
	}
	if (n > 21 && n <= 27)
		return WIPX_StringToAddr(name, addr);

	return -1;
	unguard;
}

//==========================================================================
//
//  WIPX_AddrCompare
//
//==========================================================================

int WIPX_AddrCompare(sockaddr_t* addr1, sockaddr_t* addr2)
{
	guard(WIPX_AddrCompare);
	if (addr1->sa_family != addr2->sa_family)
		return -1;

	if (*((sockaddr_ipx *)addr1)->sa_netnum && *((sockaddr_ipx *)addr2)->sa_netnum)
		if (memcmp(((sockaddr_ipx *)addr1)->sa_netnum, ((sockaddr_ipx *)addr2)->sa_netnum, 4) != 0)
			return -1;
	if (memcmp(((sockaddr_ipx *)addr1)->sa_nodenum, ((sockaddr_ipx *)addr2)->sa_nodenum, 6) != 0)
		return -1;

	if (((sockaddr_ipx *)addr1)->sa_socket != ((sockaddr_ipx *)addr2)->sa_socket)
		return 1;

	return 0;
	unguard;
}

//==========================================================================
//
//  WIPX_GetSocketPort
//
//==========================================================================

int WIPX_GetSocketPort(sockaddr_t* addr)
{
	guard(WIPX_GetSocketPort);
	return ntohs(((sockaddr_ipx *)addr)->sa_socket);
	unguard;
}

//==========================================================================
//
//  WIPX_SetSocketPort
//
//==========================================================================

int WIPX_SetSocketPort(sockaddr_t* addr, int port)
{
	guard(WIPX_SetSocketPort);
	((sockaddr_ipx *)addr)->sa_socket = htons((word)port);
	return 0;
	unguard;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.10  2006/04/05 17:20:37  dj_jl
//	Merged size buffer with message class.
//
//	Revision 1.9  2002/08/05 17:20:00  dj_jl
//	Added guarding.
//	
//	Revision 1.8  2002/05/18 16:56:35  dj_jl
//	Added FArchive and FOutputDevice classes.
//	
//	Revision 1.7  2002/01/11 08:12:49  dj_jl
//	Changes for MinGW
//	
//	Revision 1.6  2002/01/07 12:16:43  dj_jl
//	Changed copyright year
//	
//	Revision 1.5  2001/12/18 19:05:03  dj_jl
//	Made TCvar a pure C++ class
//	
//	Revision 1.4  2001/10/09 17:28:12  dj_jl
//	no message
//	
//	Revision 1.3  2001/07/31 17:16:31  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
