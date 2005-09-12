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

#include <allegro.h>
#include "gamedefs.h"
#include "s_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class VAllegroMidiDevice : public VMidiDevice
{
public:
	bool		DidInitAllegro;
	MIDI		mididata;
	bool		midi_locked;

	void*		Mus_SndPtr;
	bool		MusicPaused;
	float		MusVolume;

	VAllegroMidiDevice();
	void Init();
	void Shutdown();
	void Tick(float);
	void Play(void*, int, const char*, bool);
	void Pause();
	void Resume();
	void Stop();
	bool IsPlaying();

	bool LoadMIDI();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern bool				allegro_sound_initialised;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

IMPLEMENT_MIDI_DEVICE(VAllegroMidiDevice, MIDIDRV_Default, "Default",
	"Allegro midi device", NULL);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	VAllegroMidiDevice::VAllegroMidiDevice
//
//==========================================================================

VAllegroMidiDevice::VAllegroMidiDevice()
: DidInitAllegro(false)
, midi_locked(false)
, Mus_SndPtr(NULL)
, MusicPaused(false)
, MusVolume(-1)
{
	memset(&mididata, 0, sizeof(mididata));
}

//==========================================================================
//
//	VAllegroMidiDevice::Init
//
//==========================================================================

void VAllegroMidiDevice::Init()
{
	guard(VAllegroMidiDevice::Init);
	if (M_CheckParm("-nosound") || M_CheckParm("-nomusic"))
		return;

	if (!allegro_sound_initialised)
	{
		// Init sound device
		if (install_sound(DIGI_NONE, MIDI_AUTODETECT, NULL) == -1)
		{
			GCon->Logf(NAME_Init, "ALLEGRO SOUND INIT ERROR!!!!\n%s\n", allegro_error);
			return;
		}
		DidInitAllegro = true;
	}
	Initialised = true;
	Enabled = true;
	unguard;
}

//==========================================================================
//
//	VAllegroMidiDevice::Tick
//
//==========================================================================

void VAllegroMidiDevice::Tick(float)
{
	guard(VAllegroMidiDevice::Tick);
	if (!Initialised)
		return;

	//	Update volume
	if (music_volume < 0.0)
	{
		music_volume = 0.0;
	}
	if (music_volume > 1.0)
	{
		music_volume = 1.0;
	}

	if (music_volume != MusVolume)
	{
		if (!MusVolume && (float)music_volume && !MusicPaused)
		{
			midi_resume();
		}
		if (MusVolume && !(float)music_volume)
		{
			midi_pause();
		}
		MusVolume = music_volume;
		set_volume(-1, int(MusVolume * 255));
	}
	unguard;
}

//==========================================================================
//
//	VAllegroMidiDevice::Shutdown
//
//==========================================================================

void VAllegroMidiDevice::Shutdown()
{
	guard(VAllegroMidiDevice::Shutdown);
	if (Initialised)
	{
		Stop();
		if (DidInitAllegro)
		{
			remove_sound();
		}
	}
	unguard;
}

//==========================================================================
//
//	VAllegroMidiDevice::Play
//
//==========================================================================

void VAllegroMidiDevice::Play(void* Data, int len, const char* song, bool loop)
{
	guard(VAllegroMidiDevice::Play);
	boolean		res;

	Mus_SndPtr = Data;
	res = LoadMIDI();
	if (!res)
	{
		Z_Free(Mus_SndPtr);
		Mus_SndPtr = NULL;
		return;
	}

	play_midi(&mididata, loop); // 'true' denotes endless looping.
	if (!MusVolume || MusicPaused)
	{
		midi_pause();
	}
	CurrSong = FName(song, FNAME_AddLower8);
	CurrLoop = loop;
	unguard;
}

//==========================================================================
//
//  VAllegroMidiDevice::LoadMIDI
//
//	Convert an in-memory copy of a MIDI format 0 or 1 file to
// an Allegro MIDI structure
//
//==========================================================================

bool VAllegroMidiDevice::LoadMIDI()
{
	guard(VAllegroMidiDevice::LoadMIDI);
	int 		i;
	int 		num_tracks;
	byte		*data;
	MIDheader	*hdr;

	memset(&mididata, 0, sizeof(mididata));

	hdr = (MIDheader*)Mus_SndPtr;

	// MIDI file type
	i = BigShort(hdr->type);
	if ((i != 0) && (i != 1))
	{
		// only type 0 and 1 are suported
		GCon->Log(NAME_Dev, "Unsuported MIDI type");
		return false;
	}

	// number of tracks
	num_tracks = BigShort(hdr->num_tracks);
	if ((num_tracks < 1) || (num_tracks > MIDI_TRACKS))
	{
		GCon->Log(NAME_Dev, "Invalid MIDI track count");
		return false;
	}

	// beat divisions
	mididata.divisions = BigShort(hdr->divisions);

	// read each track
	data = (byte*)hdr + sizeof(*hdr);
	for (i = 0; i < num_tracks; i++)
	{
		if (memcmp(data, "MTrk", 4))
		{
			GCon->Logf(NAME_Dev, "Bad MIDI track %d header", i);
			return false;
		}
		data += 4;

		mididata.track[i].len = BigLong(*(int*)data);
		data += 4;

		mididata.track[i].data = data;
		data += mididata.track[i].len;
	}

	lock_midi(&mididata);
	midi_locked = true;
	return true;
	unguard;
}

//==========================================================================
//
//	VAllegroMidiDevice::Pause
//
//==========================================================================

void VAllegroMidiDevice::Pause()
{
	guard(VAllegroMidiDevice::Pause);
	midi_pause();
	MusicPaused = true;
	unguard;
}

//==========================================================================
//
//	VAllegroMidiDevice::Resume
//
//==========================================================================

void VAllegroMidiDevice::Resume()
{
	guard(VAllegroMidiDevice::Resume);
	if (MusVolume)
		midi_resume();
	MusicPaused = false;
	unguard;
}

//==========================================================================
//
//  VAllegroMidiDevice::Stop
//
//==========================================================================

void VAllegroMidiDevice::Stop()
{
	guard(VAllegroMidiDevice::Stop);
	stop_midi();
	if (midi_locked)
    {
		int		i;

		for (i = 0; i < MIDI_TRACKS; i++)
		{
			if (mididata.track[i].data)
			{
				UNLOCK_DATA(mididata.track[i].data, mididata.track[i].len);
			}
		}
		UNLOCK_DATA(&mididata, sizeof(MIDI));
		midi_locked = false;
	}
	if (Mus_SndPtr)
	{
		Z_Free(Mus_SndPtr);
		Mus_SndPtr = NULL;
	}
	CurrSong = NAME_None;
	unguard;
}

//==========================================================================
//
//	VAllegroMidiDevice::IsPlaying
//
//==========================================================================

bool VAllegroMidiDevice::IsPlaying()
{
	guard(VAllegroMidiDevice::IsPlaying);
	return midi_pos >= 0;
	unguard;
}

//**************************************************************************
//
//	$Log$
//	Revision 1.7  2005/09/12 19:45:16  dj_jl
//	Created midi device class.
//
//	Revision 1.6  2002/07/23 16:29:56  dj_jl
//	Replaced console streams with output device class.
//	
//	Revision 1.5  2002/01/11 08:12:01  dj_jl
//	Added guard macros
//	
//	Revision 1.4  2002/01/07 12:16:43  dj_jl
//	Changed copyright year
//	
//	Revision 1.3  2001/07/31 17:16:31  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
