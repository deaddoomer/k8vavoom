//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	Copyright (C) 1999-2001 J�nis Legzdi��
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

//int			Serial_Init (void);
//void		Serial_Listen (qboolean state);
//void		Serial_SearchForHosts (qboolean xmit);
qsocket_t	*Serial_Connect(char *host);
//qsocket_t 	*Serial_CheckNewConnections (void);
int			Serial_GetMessage(qsocket_t *sock);
int			Serial_SendMessage(qsocket_t *sock, TSizeBuf *data);
int			Serial_SendUnreliableMessage(qsocket_t *sock, TSizeBuf *data);
boolean		Serial_CanSendMessage(qsocket_t *sock);
boolean		Serial_CanSendUnreliableMessage(qsocket_t *sock);
//void		Serial_Close (qsocket_t *sock);
//void		Serial_Shutdown (void);
