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

int  WINS_Init();
void WINS_Shutdown();
void WINS_Listen(bool state);
int  WINS_OpenSocket(int port);
int  WINS_CloseSocket(int socket);
int  WINS_Connect(int socket, sockaddr_t* addr);
int  WINS_CheckNewConnections();
int  WINS_Read(int socket, byte* buf, int len, sockaddr_t* addr);
int  WINS_Write(int socket, byte* buf, int len, sockaddr_t* addr);
int  WINS_Broadcast(int socket, byte* buf, int len);
char *WINS_AddrToString(sockaddr_t* addr);
int  WINS_StringToAddr(const char* string, sockaddr_t* addr);
int  WINS_GetSocketAddr(int socket, sockaddr_t* addr);
int  WINS_GetNameFromAddr(sockaddr_t* addr, char* name);
int  WINS_GetAddrFromName(const char *name, sockaddr_t *addr);
int  WINS_AddrCompare(sockaddr_t* addr1, sockaddr_t* addr2);
int  WINS_GetSocketPort(sockaddr_t* addr);
int  WINS_SetSocketPort(sockaddr_t* addr, int port);

//**************************************************************************
//
//	$Log$
//	Revision 1.5  2006/04/05 17:20:37  dj_jl
//	Merged size buffer with message class.
//
//	Revision 1.4  2002/01/07 12:16:42  dj_jl
//	Changed copyright year
//	
//	Revision 1.3  2001/07/31 17:16:31  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
