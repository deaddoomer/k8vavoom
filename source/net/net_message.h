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
//**
//**  MESSAGE IO FUNCTIONS
//**
//**    Handles byte ordering and avoids alignment errors
//**
//**************************************************************************

// ////////////////////////////////////////////////////////////////////////// //
class VChannel;


// ////////////////////////////////////////////////////////////////////////// //
class VMessageIn : public VBitStreamReader {
public:
  VMessageIn (vuint8 *Src = nullptr, vint32 Length=0);

  VMessageIn *Next;
  vuint8 ChanType;
  vint32 ChanIndex;
  bool bReliable; // reliable message
  bool bOpen; // open channel message
  bool bClose; // close channel message
  vuint32 Sequence; // reliable message sequence ID
};


// ////////////////////////////////////////////////////////////////////////// //
class VMessageOut : public VBitStreamWriter {
public:
  VMessageOut (VChannel *);

  VMessageOut *Next;
  vuint8 ChanType;
  vint32 ChanIndex;
  bool bReliable; // needs ACK or not
  bool bOpen;
  bool bClose;
  bool bReceivedAck;
  vuint32 Sequence; // reliable message sequence ID
  double Time; // time this message has been sent
  vuint32 PacketId; // packet in which this message was sent
};


inline float ByteToAngle (vuint8 angle) { return (float)angle*360.0/256.0; }
inline vuint8 AngleToByte (float angle) { return (vuint8)(angle*256.0/360.0); }
