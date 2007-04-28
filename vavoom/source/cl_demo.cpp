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
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
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
//**
//**	DEMO CODE
//**
//**	When a demo is playing back, all NET_SendMessages are skipped, and
//**  NET_GetMessages are read from the demo file.
//**
//**	Whenever cl->time gets past the last received message, another message
//**  is read from the demo file.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "network.h"
#include "cl_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class VDemoPlaybackNetConnection : public VNetConnection
{
public:
	VDemoPlaybackNetConnection(VNetContext* AContext, VBasePlayer* AOwner)
	: VNetConnection(NULL, AContext, AOwner)
	, NextPacketTime(0)
	{
		AutoAck = true;
		*cls.demofile << NextPacketTime;
	}

	float			NextPacketTime;

	//	VNetConnection interface
	int GetRawPacket(TArray<vuint8>&);
	void SendRawMessage(VMessageOut&);
};

class VDemoRecordingNetConnection : public VNetConnection
{
public:
	VDemoRecordingNetConnection(VSocketPublic* Sock, VNetContext* AContext,
		VBasePlayer* AOwner)
	: VNetConnection(Sock, AContext, AOwner)
	{}

	//	VNetConnection interface
	int GetRawPacket(TArray<vuint8>&);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void CL_Disconnect();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern VClientNetContext*	ClientNetContext;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	CL_SetUpLocalPlayer
//
//==========================================================================

void CL_SetUpLocalPlayer(VSocketPublic* Sock)
{
	guard(CL_SetUpLocalPlayer);
	VMemberBase::SetUpNetClasses();

	//	Create player structure.
	cl = (VBasePlayer*)VObject::StaticSpawnObject(
		VClass::FindClass("Player"));
	cl->PlayerFlags |= VBasePlayer::PF_IsClient;
	cl->ViewEnt = Spawn<VEntity>();
	cl->ClGame = GClGame;
	GClGame->cl = cl;

	if (cls.demoplayback)
	{
		cl->Net = new VDemoPlaybackNetConnection(ClientNetContext, cl);
	}
	else if (cls.demorecording)
	{
		cl->Net = new VDemoRecordingNetConnection(Sock, ClientNetContext, cl);
	}
	else
	{
		cl->Net = new VNetConnection(Sock, ClientNetContext, cl);
	}
	((VPlayerChannel*)cl->Net->Channels[CHANIDX_Player])->SetPlayer(cl);
	unguard;
}

//==========================================================================
//
//	CL_FinishTimeDemo
//
//==========================================================================

void CL_FinishTimeDemo()
{
	guard(CL_FinishTimeDemo);
	int		frames;
	float	time;
	
	cls.timedemo = false;
	
	// the first frame didn't count
	frames = (host_framecount - cls.td_startframe) - 1;
	time = realtime - cls.td_starttime;
	if (!time)
		time = 1;
	GCon->Logf("%d frames %f seconds %f fps", frames, time, frames / time);
	unguard;
}

//==========================================================================
//
//	CL_StopPlayback
//
//	Called when a demo file runs out, or the user starts a game
//
//==========================================================================

void CL_StopPlayback()
{
	guard(CL_StopPlayback);
	if (!cls.demoplayback)
	{
		return;
	}

	delete cls.demofile;
	cls.demoplayback = false;
	cls.demofile = NULL;
	cls.state = ca_disconnected;

	if (cls.timedemo)
	{
		CL_FinishTimeDemo();
	}
	GClGame->eventDemoPlaybackStopped();
	unguard;
}

//==========================================================================
//
//	CL_WriteDemoMessage
//
//	Dumps the current net message, prefixed by the length and view angles
//
//==========================================================================

void CL_WriteDemoMessage(TArray<vuint8>& msg)
{
	guard(CL_WriteDemoMessage);
	*cls.demofile << GClLevel->Time;
	vint32 MsgSize = msg.Num();
	*cls.demofile << MsgSize;
	*cls.demofile << cl->ViewAngles;
	cls.demofile->Serialise(msg.Ptr(), msg.Num());
	cls.demofile->Flush();
	unguard;
}

//==========================================================================
//
//	VDemoPlaybackNetConnection::GetRawPacket
//
//	Handles recording and playback of demos, on top of NET_ code
//
//==========================================================================

int VDemoPlaybackNetConnection::GetRawPacket(TArray<vuint8>& Data)
{
	guard(VDemoPlaybackNetConnection::GetRawPacket);
	// decide if it is time to grab the next message
	if (cls.signon)	// allways grab until fully connected
	{
		if (cls.timedemo)
		{
			if (host_framecount == cls.td_lastframe)
			{
				return 0;		// allready read this frame's message
			}
			cls.td_lastframe = host_framecount;
			// if this is the second frame, grab the real  cls.td_starttime
			// so the bogus time on the first frame doesn't count
			if (host_framecount == cls.td_startframe + 1)
			{
				cls.td_starttime = realtime;
			}
		}
		else if (GClLevel->Time < NextPacketTime)
		{
			return 0;		// don't need another message yet
		}
	}

	if (cls.demofile->AtEnd())
	{
		CL_StopPlayback();
		return 0;
	}

	// get the next message
	vint32 MsgSize;
	*cls.demofile << MsgSize;
	*cls.demofile << cl->ViewAngles;

	if (MsgSize > OUT_MESSAGE_SIZE)
		Sys_Error("Demo message > MAX_MSGLEN");
	Data.SetNum(MsgSize);
	cls.demofile->Serialise(Data.Ptr(), MsgSize);
	if (cls.demofile->IsError())
	{
		CL_StopPlayback();
		return 0;
	}

	if (!cls.demofile->AtEnd())
	{
		*cls.demofile << NextPacketTime;
	}

	return 1;
	unguard;
}

//==========================================================================
//
//	VDemoPlaybackNetConnection::SendRawMessage
//
//==========================================================================

void VDemoPlaybackNetConnection::SendRawMessage(VMessageOut&)
{
}

//==========================================================================
//
//	VDemoRecordingNetConnection::GetRawPacket
//
//	Handles recording and playback of demos, on top of NET_ code
//
//==========================================================================

int VDemoRecordingNetConnection::GetRawPacket(TArray<vuint8>& Data)
{
	guard(VDemoRecordingNetConnection::GetRawPacket);
	int r = VNetConnection::GetRawPacket(Data);

	if (r == 1 && cls.demorecording)
	{
		CL_WriteDemoMessage(Data);
	}
	
	return r;
	unguard;
}

//==========================================================================
//
//	CL_StopRecording
//
//==========================================================================

void CL_StopRecording()
{
	guard(CL_StopRecording);
	// finish up
	delete cls.demofile;
	cls.demofile = NULL;
	cls.demorecording = false;
	GCon->Log("Completed demo");
	unguard;
}

//==========================================================================
//
//	COMMAND StopDemo
//
//	stop recording a demo
//
//==========================================================================

COMMAND(StopDemo)
{
	guard(COMMAND StopDemo);
	if (Source != SRC_Command)
	{
		return;
	}

	if (!cls.demorecording)
	{
		GCon->Log("Not recording a demo.");
		return;
	}
	CL_StopRecording();
	unguard;
}

//==========================================================================
//
//	COMMAND Record
//
//	record <demoname> <map>
//
//==========================================================================

COMMAND(Record)
{
	guard(COMMAND Record);
	if (Source != SRC_Command)
	{
		return;
	}

	int c = Args.Num();
	if (c != 2 && c != 3)
	{
		GCon->Log("record <demoname> [<map>]");
		return;
	}

	if (strstr(*Args[1], ".."))
	{
		GCon->Log("Relative pathnames are not allowed.");
		return;
	}

	if (c == 2 && cls.state == ca_connected)
	{
		GCon->Log("Can not record - already connected to server");
		GCon->Log("Client demo recording must be started before connecting");
		return;
	}

	VStr name = VStr("demos/") + Args[1].DefaultExtension(".dem");

	//
	// start the map up
	//
	if (c > 2)
	{
		VCommand::ExecuteString(VStr("map ") + Args[2], SRC_Command, NULL);
	}
	
	//
	// open the demo file
	//

	GCon->Logf("recording to %s.", *name);
	cls.demofile = FL_OpenFileWrite(name);
	if (!cls.demofile)
	{
		GCon->Log("ERROR: couldn't open.");
		return;
	}

	cls.demofile->Serialise(const_cast<char*>("VDEM"), 4);

	cls.demorecording = true;
	unguard;
}

//==========================================================================
//
//	COMMAND PlayDemo
//
//	play [demoname]
//
//==========================================================================

COMMAND(PlayDemo)
{
	guard(COMMAND PlayDemo);
	char	magic[8];

	if (Source != SRC_Command)
	{
		return;
	}

	if (Args.Num() != 2)
	{
		GCon->Log("play <demoname> : plays a demo");
		return;
	}

	//
	// open the demo file
	//
	VStr name = VStr("demos/") + Args[1].DefaultExtension(".dem");

	GCon->Logf("Playing demo from %s.", *name);
	cls.demofile = FL_OpenFileRead(name);
	if (!cls.demofile)
	{
		GCon->Log("ERROR: couldn't open.");
		return;
	}

	cls.demofile->Serialise(magic, 4);
	magic[4] = 0;
	if (VStr::Cmp(magic, "VDEM"))
	{
		delete cls.demofile;
		cls.demofile = NULL;
		GCon->Log("ERROR: not a Vavoom demo.");
		return;
	}

	//
	// disconnect from server
	//
	CL_Disconnect();

	cls.demoplayback = true;
	cls.state = ca_connected;
	CL_SetUpLocalPlayer(NULL);
	GClGame->eventDemoPlaybackStarted();
	unguard;
}

//==========================================================================
//
//	COMMAND TimeDemo
//
//	timedemo [demoname]
//
//==========================================================================

COMMAND(TimeDemo)
{
	guard(COMMAND TimeDemo);
	if (Source != SRC_Command)
	{
		return;
	}

	if (Args.Num() != 2)
	{
		GCon->Log("timedemo <demoname> : gets demo speeds");
		return;
	}

	PlayDemo_f.Run();

	// cls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted
	
	cls.timedemo = true;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;		// get a new message this frame
	unguard;
}
