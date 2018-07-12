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

#ifdef _WIN32
# include "winshit/winlocal.h"
#else
# define INITGUID
#endif
#include <AL/al.h>
#include <AL/alc.h>
// linux headers doesn't define this
#ifndef OPENAL
# define OPENAL
#endif

#include "sound.h"


// ////////////////////////////////////////////////////////////////////////// //
class VOpenALDevice : public VSoundDevice {
private:
  enum { MAX_VOICES = 256 };

  enum { NUM_STRM_BUFFERS = 8 };
  enum { STRM_BUFFER_SIZE = 1024 };

  ALCdevice *Device;
  ALCcontext *Context;
  ALuint *Buffers;
  vint32 BufferCount;

  ALuint StrmSampleRate;
  ALuint StrmFormat;
  ALuint StrmBuffers[NUM_STRM_BUFFERS];
  ALuint StrmAvailableBuffers[NUM_STRM_BUFFERS];
  int StrmNumAvailableBuffers;
  ALuint StrmSource;
  short StrmDataBuffer[STRM_BUFFER_SIZE*2];

  // convars
  static float doppler_factor;
  static float doppler_velocity;
  static float rolloff_factor;
  static float reference_distance;
  static float max_distance;

public:
  // VSoundDevice interface
  virtual bool Init () override;
  virtual int SetChannels (int InNumChannels) override;
  virtual void Shutdown ();
  virtual int PlaySound (int sound_id, float volume, float sep, float pitch, bool Loop) override;
  virtual int PlaySound3D (int sound_id, const TVec &origin, const TVec &velocity, float volume, float pitch, bool Loop) override;
  virtual void UpdateChannel (int Handle, float vol, float sep) override;
  virtual void UpdateChannel3D (int Handle, const TVec &Org, const TVec &Vel) override;
  virtual void UpdateChannelPitch (int Handle, float pitch) override;
  virtual bool IsChannelPlaying (int Handle) override;
  virtual void StopChannel (int Handle) override;
  virtual void UpdateListener (const TVec &org, const TVec &vel, const TVec &fwd, const TVec&, const TVec &up) override;

  virtual bool OpenStream (int Rate, int Bits, int Channels) override;
  virtual void CloseStream () override;
  virtual int GetStreamAvailable () override;
  virtual short *GetStreamBuffer () override;
  virtual void SetStreamData (short *data, int len) override;
  virtual void SetStreamVolume (float vol) override;
  virtual void PauseStream () override;
  virtual void ResumeStream () override;

  bool PrepareSound (int sound_id);
};

IMPLEMENT_SOUND_DEVICE(VOpenALDevice, SNDDRV_OpenAL, "OpenAL", "OpenAL sound device", "-openal");

float VOpenALDevice::doppler_factor = 1.0f;
float VOpenALDevice::doppler_velocity = 10000.0f;
float VOpenALDevice::rolloff_factor = 1.0f;
float VOpenALDevice::reference_distance = 32.0f; // The distance under which the volume for the source would normally drop by half (before being influenced by rolloff factor or AL_MAX_DISTANCE)
float VOpenALDevice::max_distance = 800.0f; // Used with the Inverse Clamped Distance Model to set the distance where there will no longer be any attenuation of the source


//==========================================================================
//
//  VOpenALDevice::Init
//
//  inits sound
//
//==========================================================================
bool VOpenALDevice::Init () {
  ALenum E;

  Device = nullptr;
  Context = nullptr;
  Buffers = nullptr;
  BufferCount = 0;
  StrmSource = 0;
  StrmNumAvailableBuffers = 0;

  // connect to a device
  Device = alcOpenDevice(nullptr);
  if (!Device) {
    fprintf(stderr, "WARNING: couldn't open OpenAL device.\n");
    return false;
  }

  // create a context and make it current
  Context = alcCreateContext(Device, nullptr);
  if (!Context) Sys_Error("Failed to create OpenAL context");
  alcMakeContextCurrent(Context);
  E = alGetError();
  if (E != AL_NO_ERROR) Sys_Error("OpenAL error: %s", alGetString(E));

  // allocate array for buffers
  //Buffers = new ALuint[GSoundManager->S_sfx.length()];
  //memset(Buffers, 0, sizeof(ALuint)*GSoundManager->S_sfx.length());

  return true;
}


//==========================================================================
//
//  VOpenALDevice::SetChannels
//
//==========================================================================
int VOpenALDevice::SetChannels (int InNumChannels) {
  if (InNumChannels < 1) InNumChannels = 1;
  int NumChannels = MAX_VOICES;
  if (NumChannels > InNumChannels) NumChannels = InNumChannels;
  return NumChannels;
}


//==========================================================================
//
//  VOpenALDevice::Shutdown
//
//==========================================================================
void VOpenALDevice::Shutdown () {
  // delete buffers
  if (Buffers) {
    alDeleteBuffers(GSoundManager->S_sfx.length(), Buffers);
    delete[] Buffers;
    Buffers = nullptr;
  }
  BufferCount = 0;

  // destroy context
  if (Context) {
#ifndef __linux__
    // this causes a freeze in Linux
    alcMakeContextCurrent(nullptr);
#endif
    alcDestroyContext(Context);
    Context = nullptr;
  }
  // disconnect from a device
  if (Device) {
    alcCloseDevice(Device);
    Device = nullptr;
  }
}


//==========================================================================
//
//  VOpenALDevice::PrepareSound
//
//==========================================================================
bool VOpenALDevice::PrepareSound (int sound_id) {
  if (sound_id < 1 || sound_id >= GSoundManager->S_sfx.length()) return false;

  if (BufferCount < sound_id+1) {
    int newsz = sound_id+4096;
    ALuint *newbuf = new ALuint[newsz];
    for (int f = BufferCount; f < newsz; ++f) newbuf[f] = 0;
    delete[] Buffers;
    Buffers = newbuf;
    BufferCount = newsz;
  }

  if (Buffers[sound_id]) return true;

  // check, that sound lump is loaded
  if (!GSoundManager->S_sfx[sound_id].data) return false;
  //if (!GSoundManager->LoadSound(sound_id, filename)) return false; // missing sound

  // clear error code
  alGetError();

  // create buffer
  alGenBuffers(1, &Buffers[sound_id]);
  if (alGetError() != AL_NO_ERROR) {
    fprintf(stderr, "OpenAL: Failed to gen buffer.\n");
    return false;
  }

  // load buffer data
  alBufferData(Buffers[sound_id],
    (GSoundManager->S_sfx[sound_id].sampleBits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16),
    GSoundManager->S_sfx[sound_id].data,
    GSoundManager->S_sfx[sound_id].dataSize,
    GSoundManager->S_sfx[sound_id].sampleRate);

  if (alGetError() != AL_NO_ERROR) {
    fprintf(stderr, "WARNING: failed to load buffer data.\n");
    return false;
  }

  return true;
}


//==========================================================================
//
//  VOpenALDevice::PlaySound
//
//  This function adds a sound to the list of currently active sounds, which
//  is maintained as a given number of internal channels.
//
//==========================================================================
int VOpenALDevice::PlaySound (int sound_id, float volume, float sep, float pitch, bool Loop) {
  if (!PrepareSound(sound_id)) return -1;

  ALuint src;
  alGetError(); // clear error code
  alGenSources(1, &src);
  if (alGetError() != AL_NO_ERROR) {
    fprintf(stderr, "WARNING: failed to gen sound source.\n");
    return -1;
  }

  alSourcei(src, AL_BUFFER, Buffers[sound_id]);

  alSourcef(src, AL_GAIN, volume);
  alSourcef(src, AL_ROLLOFF_FACTOR, rolloff_factor);
  alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
  alSource3f(src, AL_POSITION, 0.0, 0.0, 0.0 /*-16.0*/);
  alSourcef(src, AL_REFERENCE_DISTANCE, reference_distance);
  alSourcef(src, AL_MAX_DISTANCE, max_distance);
  alSourcef(src, AL_PITCH, pitch);
  if (Loop) alSourcei(src, AL_LOOPING, AL_TRUE);
  alSourcePlay(src);

  /*
  ALint stt;
  alGetSourcei(src, AL_SOURCE_STATE, &stt);
  fprintf(stderr, "2d: stt=%d 0x%04x (%d)\n", stt, (unsigned)stt, (int)(stt == AL_PLAYING));
  fprintf(stderr, "  volume=%f\n", (double)volume);
  fprintf(stderr, "  rolloff_factor=%f\n", (double)rolloff_factor);
  fprintf(stderr, "  reference_distance=%f\n", (double)reference_distance);
  fprintf(stderr, "  max_distance=%f\n", (double)max_distance);
  fprintf(stderr, "  pitch=%f\n", (double)pitch);
  */

  return src;
}


//==========================================================================
//
//  VOpenALDevice::PlaySound3D
//
//==========================================================================
int VOpenALDevice::PlaySound3D (int sound_id, const TVec &origin, const TVec &velocity, float volume, float pitch, bool Loop) {
  if (!PrepareSound(sound_id)) return -1;

  ALuint src;
  alGetError(); // clear error code

  alGenSources(1, &src);
  if (alGetError() != AL_NO_ERROR) {
    fprintf(stderr, "WARNING: failed to gen sound source.\n");
    return -1;
  }

  alSourcei(src, AL_BUFFER, Buffers[sound_id]);
  if (alGetError() != AL_NO_ERROR) { alDeleteSources(1, &src); fprintf(stderr, "WARNING: failed to set sound source buffer.\n"); return -1; }

  alSourcef(src, AL_GAIN, volume);
  alSourcef(src, AL_ROLLOFF_FACTOR, rolloff_factor);
  alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
  alSource3f(src, AL_POSITION, origin.x, origin.y, origin.z);
  alSource3f(src, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
  alSourcef(src, AL_REFERENCE_DISTANCE, reference_distance);
  alSourcef(src, AL_MAX_DISTANCE, max_distance);
  alSourcef(src, AL_PITCH, pitch);
  if (Loop) alSourcei(src, AL_LOOPING, AL_TRUE);
  if (alGetError() != AL_NO_ERROR) { alDeleteSources(1, &src); fprintf(stderr, "WARNING: failed to set sound source options.\n"); return -1; }
  alSourcePlay(src);
  if (alGetError() != AL_NO_ERROR) { alDeleteSources(1, &src); fprintf(stderr, "WARNING: failed to start sound source.\n"); return -1; }

  /*
  ALint stt;
  alGetSourcei(src, AL_SOURCE_STATE, &stt);
  fprintf(stderr, "3d: stt=%d 0x%04x (%d)\n", stt, (unsigned)stt, (int)(stt == AL_PLAYING));
  fprintf(stderr, "  volume=%f\n", (double)volume);
  fprintf(stderr, "  rolloff_factor=%f\n", (double)rolloff_factor);
  fprintf(stderr, "  origin=(%f,%f,%f)\n", (double)origin.x, origin.y, origin.z);
  fprintf(stderr, "  reference_distance=%f\n", (double)reference_distance);
  fprintf(stderr, "  max_distance=%f\n", (double)max_distance);
  fprintf(stderr, "  pitch=%f\n", (double)pitch);
  */

  return src;
}


//==========================================================================
//
//  VOpenALDevice::UpdateChannel
//
//==========================================================================
void VOpenALDevice::UpdateChannel (int, float, float) {
}


//==========================================================================
//
//  VOpenALDevice::UpdateChannel3D
//
//==========================================================================
void VOpenALDevice::UpdateChannel3D (int Handle, const TVec &Org, const TVec &Vel) {
  if (Handle == -1) return;
  alSource3f(Handle, AL_POSITION, Org.x, Org.y, Org.z);
  alSource3f(Handle, AL_VELOCITY, Vel.x, Vel.y, Vel.z);
}


//==========================================================================
//
//  VOpenALDevice::UpdateChannelPitch
//
//==========================================================================
void VOpenALDevice::UpdateChannelPitch (int Handle, float pitch) {
  if (Handle == -1) return;
  alSourcef(Handle, AL_PITCH, pitch);
}


//==========================================================================
//
//  VOpenALDevice::IsChannelPlaying
//
//==========================================================================
bool VOpenALDevice::IsChannelPlaying (int Handle) {
  if (Handle == -1) return false;
  ALint State;
  alGetSourcei(Handle, AL_SOURCE_STATE, &State);
  return (State == AL_PLAYING);
}


//==========================================================================
//
//  VOpenALDevice::StopChannel
//
//  Stop the sound. Necessary to prevent runaway chainsaw, and to stop
// rocket launches when an explosion occurs.
//  All sounds MUST be stopped;
//
//==========================================================================
void VOpenALDevice::StopChannel (int Handle) {
  if (Handle == -1) return;
  // stop buffer
  alSourceStop(Handle);
  alDeleteSources(1, (ALuint *)&Handle);
}


//==========================================================================
//
//  VOpenALDevice::UpdateListener
//
//==========================================================================
void VOpenALDevice::UpdateListener (const TVec &org, const TVec &vel, const TVec &fwd, const TVec&, const TVec &up) {
  alListener3f(AL_POSITION, org.x, org.y, org.z);
  alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);

  ALfloat orient[6] = { fwd.x, fwd.y, fwd.z, up.x, up.y, up.z};
  alListenerfv(AL_ORIENTATION, orient);

  alDopplerFactor(doppler_factor);
  alDopplerVelocity(doppler_velocity);
}


//==========================================================================
//
//  VOpenALDevice::OpenStream
//
//==========================================================================
bool VOpenALDevice::OpenStream (int Rate, int Bits, int Channels) {
  StrmSampleRate = Rate;
  StrmFormat = (Channels == 2 ?
    (Bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16) :
    (Bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16));

  alGetError(); // clear error code
  alGenSources(1, &StrmSource);
  if (alGetError() != AL_NO_ERROR) {
    fprintf(stderr, "WARNING: failed to gen sound source.\n");
    return false;
  }

  alSourcei(StrmSource, AL_SOURCE_RELATIVE, AL_TRUE);
  alGenBuffers(NUM_STRM_BUFFERS, StrmBuffers);
  alSourceQueueBuffers(StrmSource, NUM_STRM_BUFFERS, StrmBuffers);
  alSourcePlay(StrmSource);
  StrmNumAvailableBuffers = 0;

  return true;
}


//==========================================================================
//
//  VOpenALDevice::CloseStream
//
//==========================================================================
void VOpenALDevice::CloseStream () {
  if (StrmSource) {
    alDeleteBuffers(NUM_STRM_BUFFERS, StrmBuffers);
    alDeleteSources(1, &StrmSource);
    StrmSource = 0;
  }
}


//==========================================================================
//
//  VOpenALDevice::GetStreamAvailable
//
//==========================================================================
int VOpenALDevice::GetStreamAvailable () {
  if (!StrmSource) return 0;

  ALint NumProc;
  alGetSourcei(StrmSource, AL_BUFFERS_PROCESSED, &NumProc);
  if (NumProc > 0) {
    alSourceUnqueueBuffers(StrmSource, NumProc, StrmAvailableBuffers+StrmNumAvailableBuffers);
    StrmNumAvailableBuffers += NumProc;
  }
  return (StrmNumAvailableBuffers > 0 ? STRM_BUFFER_SIZE : 0);
}


//==========================================================================
//
//  VOpenALDevice::GetStreamBuffer
//
//==========================================================================
short *VOpenALDevice::GetStreamBuffer () {
  return StrmDataBuffer;
}


//==========================================================================
//
//  VOpenALDevice::SetStreamData
//
//==========================================================================
void VOpenALDevice::SetStreamData (short *data, int len) {
  ALuint Buf;
  ALint State;

  Buf = StrmAvailableBuffers[StrmNumAvailableBuffers-1];
  --StrmNumAvailableBuffers;
  alBufferData(Buf, StrmFormat, data, len*4, StrmSampleRate);
  alSourceQueueBuffers(StrmSource, 1, &Buf);
  alGetSourcei(StrmSource, AL_SOURCE_STATE, &State);
  if (State != AL_PLAYING) {
    if (StrmSource) alSourcePlay(StrmSource);
  }
}


//==========================================================================
//
//  VOpenALDevice::SetStreamVolume
//
//==========================================================================
void VOpenALDevice::SetStreamVolume (float vol) {
  if (StrmSource) alSourcef(StrmSource, AL_GAIN, vol);
}


//==========================================================================
//
//  VOpenALDevice::PauseStream
//
//==========================================================================
void VOpenALDevice::PauseStream () {
  if (StrmSource) alSourcePause(StrmSource);
}


//==========================================================================
//
//  VOpenALDevice::ResumeStream
//
//==========================================================================
void VOpenALDevice::ResumeStream () {
  if (StrmSource) alSourcePlay(StrmSource);
}
