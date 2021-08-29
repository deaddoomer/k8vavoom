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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "../../gamedefs.h"
#include "../sound.h"
#include "../snd_local.h"


#ifdef VV_SND_ALLOW_VELOCITY
static VCvarF snd_doppler_factor("snd_doppler_factor", "1", "OpenAL doppler factor.", 0/*CVAR_Archive*/);
static VCvarF snd_doppler_velocity("snd_doppler_velocity", "10000", "OpenAL doppler velocity.", 0/*CVAR_Archive*/);
#endif
static VCvarF snd_rolloff_factor("snd_rolloff_factor", "1", "OpenAL rolloff factor.", 0/*CVAR_Archive*/);
static VCvarF snd_reference_distance("snd_reference_distance", "192", "OpenAL reference distance.", 0/*CVAR_Archive*/); // was 384, and 64, and 192
// it is 4096 in our main sound code; anything futher than this will be dropped
static VCvarF snd_max_distance("snd_max_distance", "8192", "OpenAL max distance.", 0/*CVAR_Archive*/); // was 4096, and 2042, and 8192

static VCvarB openal_show_extensions("openal_show_extensions", false, "Show available OpenAL extensions?", CVAR_Archive);

// don't update if nothing was changed
#ifdef VV_SND_ALLOW_VELOCITY
static float prevDopplerFactor = -INFINITY;
static float prevDopplerVelocity = -INFINITY;
static TVec prevVelocity = TVec(-INFINITY, -INFINITY, -INFINITY);
#endif
static TVec prevPosition = TVec(-INFINITY, -INFINITY, -INFINITY);
static TVec prevUp = TVec(-INFINITY, -INFINITY, -INFINITY);
static TVec prevFwd = TVec(-INFINITY, -INFINITY, -INFINITY);

const char *cli_AudioDeviceName = nullptr;

/*static*/ bool cliRegister_openal_args =
  VParsedArgs::RegisterStringOption("-audio-device", "set audio device", &cli_AudioDeviceName) ;


//==========================================================================
//
//  alGetErrorString
//
//==========================================================================
static const char *alGetErrorString (ALenum E) {
  switch (E) {
    case AL_NO_ERROR: return "<no error>";
    case AL_INVALID_NAME: return "invalid name";
    case AL_INVALID_ENUM: return "invalid enum";
    case AL_INVALID_VALUE: return "invalid value";
    case AL_INVALID_OPERATION: return "invalid operation";
    case AL_OUT_OF_MEMORY: return "out of memory";
    default: break;
  }
  static char buf[256];
  snprintf(buf, sizeof(buf), "0x%04x", (unsigned)E);
  return buf;
}


//==========================================================================
//
//  VOpenALDevice::VOpenALDevice
//
//==========================================================================
VOpenALDevice::VOpenALDevice ()
  : Device(nullptr)
  , Context(nullptr)
  , Buffers(nullptr)
  , BufferCount(0)
  , StrmSampleRate(0)
  , StrmFormat(0)
  , StrmNumAvailableBuffers(0)
  , StrmSource(0)
{
  memset(StrmBuffers, 0, sizeof(StrmBuffers));
  memset(StrmAvailableBuffers, 0, sizeof(StrmAvailableBuffers));
  memset(StrmDataBuffer, 0, sizeof(StrmDataBuffer));
}


//==========================================================================
//
//  VOpenALDevice::~VOpenALDevice
//
//==========================================================================
VOpenALDevice::~VOpenALDevice () {
  Shutdown(); // just in case
}


//==========================================================================
//
//  VOpenALDevice::IsError
//
//==========================================================================
bool VOpenALDevice::IsError (const char *errmsg, bool errabort) {
  ALenum E = alGetError();
  if (E == AL_NO_ERROR) return false;
  if (errabort) Sys_Error("OpenAL: %s (%s)", errmsg, alGetErrorString(E));
  GCon->Logf(NAME_Warning, "OpenAL: %s (%s)", errmsg, alGetErrorString(E));
  return true;
}


//==========================================================================
//
//  VOpenALDevice::ClearError
//
//==========================================================================
void VOpenALDevice::ClearError () {
  (void)alGetError(); // this does it
}


//==========================================================================
//
//  VOpenALDevice::Init
//
//  Inits sound
//
//==========================================================================
bool VOpenALDevice::Init () {
  Device = nullptr;
  Context = nullptr;
  Buffers = nullptr;
  BufferCount = 0;
  StrmSource = 0;
  StrmNumAvailableBuffers = 0;

#ifdef VV_SND_ALLOW_VELOCITY
  prevDopplerFactor = -INFINITY;
  prevDopplerVelocity = -INFINITY;
  prevVelocity = TVec(-INFINITY, -INFINITY, -INFINITY);
#endif
  prevPosition = TVec(-INFINITY, -INFINITY, -INFINITY);
  prevUp = TVec(-INFINITY, -INFINITY, -INFINITY);
  prevFwd = TVec(-INFINITY, -INFINITY, -INFINITY);

  #if 0
  {
    const char *lst = alcGetString(NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
    if (lst) {
      while (*lst) {
        GCon->Logf(NAME_Init, "available OpenAL device: '%s'", lst);
        lst += strlen(lst)+1;
      }
    }
  }
  #endif

  // connect to a device
  Device = alcOpenDevice(cli_AudioDeviceName);
  if (!Device) {
    if (cli_AudioDeviceName) {
      GCon->Logf(NAME_Warning, "Couldn't open OpenAL device '%s'", cli_AudioDeviceName);
    } else {
      GCon->Log(NAME_Warning, "Couldn't open OpenAL device");
    }
    return false;
  }

  if (cli_AudioDeviceName) GCon->Logf(NAME_Init, "opened OpenAL device '%s'", cli_AudioDeviceName);

  #ifndef VAVOOM_USE_MOJOAL
  // MojoAL doesn't have this
  if (!alcIsExtensionPresent(Device, "ALC_EXT_thread_local_context")) {
    Sys_Error("OpenAL: 'ALC_EXT_thread_local_context' extension is not present.\n"
              "Please, use OpenAL Soft implementation, and make sure that it is recent.");
  }
  #endif

  RealMaxVoices = (vint32)MAX_VOICES;
  Context = nullptr;

  // if you wonder why this code is here, this is because of shitdows.
  // i got bugreports from poor shitdows users about OpenAL failing to
  // create the context sometimes. and OpenAL code says that you cannot
  // call `alGetError()` without an active context, so i don't even know
  // what the fuck is wrong. i decided to remove all error checks, and
  // put this loop in the hope that if first time context creation will
  // fail, the next time it will succeed. ah, and punish the user with
  // reduced total sound channels.
  while (RealMaxVoices >= 64) {
    // create a context and make it current
    ALCint attrs[] = {
      ALC_STEREO_SOURCES, 1, // get at least one stereo source for music
      ALC_MONO_SOURCES, RealMaxVoices, // this should be audio channels in our game engine
      //ALC_FREQUENCY, 48000, // desired frequency; we don't really need this, let OpenAL choose the best
      0,
    };

    // the OpenAL code says that `alGetError()` can only be called with the context set
    Context = alcCreateContext(Device, attrs);
    if (Context) break;
    --RealMaxVoices;
    if (RealMaxVoices < 4) break;
  }

  if (!Context) Sys_Error("Failed to create OpenAL context");
  GCon->Logf(NAME_Init, "OpenAL: created context with %d max voices", RealMaxVoices);

  #ifdef VAVOOM_USE_MOJOAL
  // MojoAL doesn't have this
  alcMakeContextCurrent(Context);
  {
    ALenum E = alGetError();
    if (E != AL_NO_ERROR) Sys_Error("OpenAL error (setting thread context): %s", alGetErrorString(E));
  }
  #else
  alcSetThreadContext(Context);
  #endif

  // this is default, but hey...
  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
  #ifndef VAVOOM_USE_MOJOAL
  // MojoAL doesn't have this, but OpenAL-soft does
  alEnable(AL_SOURCE_DISTANCE_MODEL);
  #endif

  // clear error code
  ClearError();

  // print some information
  if (openal_show_extensions) {
    GCon->Logf(NAME_Init, "AL_VENDOR: %s", alGetString(AL_VENDOR));
    GCon->Logf(NAME_Init, "AL_RENDERER: %s", alGetString(AL_RENDERER));
    GCon->Logf(NAME_Init, "AL_VERSION: %s", alGetString(AL_VERSION));
    GCon->Log(NAME_Init, "AL_EXTENSIONS:");
    TArray<VStr> Exts;
    VStr((char *)alGetString(AL_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.length(); i++) GCon->Log(NAME_Init, VStr("- ")+Exts[i]);
    GCon->Log(NAME_Init, "ALC_EXTENSIONS:");
    VStr((char *)alcGetString(Device, ALC_EXTENSIONS)).Split(' ', Exts);
    for (int i = 0; i < Exts.length(); i++) GCon->Log(NAME_Init, VStr("- ")+Exts[i]);
  }

  // allocate array for buffers
  /*
  Buffers = new ALuint[GSoundManager->S_sfx.length()];
  memset(Buffers, 0, sizeof(ALuint) * GSoundManager->S_sfx.length());
  */

  GCon->Log(NAME_Init, "OpenAL initialized.");
  return true;
}


//==========================================================================
//
//  VOpenALDevice::AddCurrentThread
//
//==========================================================================
void VOpenALDevice::AddCurrentThread () {
  #ifdef VAVOOM_USE_MOJOAL
  // MojoAL doesn't have this
  // it is usually safe, because it is called only by the streaming player
  // and there is no need to do anything here anyway
  //alcMakeContextCurrent(Context);
  #else
  alcSetThreadContext(Context);
  #endif
}


//==========================================================================
//
//  VOpenALDevice::RemoveCurrentThread
//
//==========================================================================
void VOpenALDevice::RemoveCurrentThread () {
  #ifdef VAVOOM_USE_MOJOAL
  // MojoAL doesn't have this
  // do nothing here, this is used only by streaming player
  #else
  alcSetThreadContext(nullptr);
  #endif
}


//==========================================================================
//
//  VOpenALDevice::SetChannels
//
//==========================================================================
int VOpenALDevice::SetChannels (int InNumChannels) {
  return clampval(InNumChannels, 1, (int)RealMaxVoices-2);
}


//==========================================================================
//
//  VOpenALDevice::Shutdown
//
//==========================================================================
void VOpenALDevice::Shutdown () {
  if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): shutting down OpenAL");

  srcPendingSet.clear();
  srcErrorSet.clear();

  if (activeSourceSet.length()) {
    if (developer) GLog.Logf(NAME_Dev, "VOpenALDevice::Shutdown(): aborting %d active sources", activeSourceSet.length());
    while (activeSourceSet.length()) {
      auto it = activeSourceSet.first();
      ALuint src = it.getKey();
      if (developer) GLog.Logf(NAME_Dev, "VOpenALDevice::Shutdown():   aborting source %u", src);
      activeSourceSet.del(src);
      alDeleteSources(1, &src);
    }
  }

  // delete buffers
  if (Buffers) {
    if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): deleting sound buffers");
    //alDeleteBuffers(GSoundManager->S_sfx.length(), Buffers);
    for (int bidx = 0; bidx < BufferCount; ++bidx) {
      if (Buffers[bidx]) {
        alDeleteBuffers(1, Buffers+bidx);
        Buffers[bidx] = 0;
      }
    }
    delete[] Buffers;
    Buffers = nullptr;
  }
  BufferCount = 0;

  // destroy context
  if (Context) {
    if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): destroying context");
    #ifdef VAVOOM_USE_MOJOAL
    // MojoAL doesn't have this
    alcMakeContextCurrent(nullptr);
    #else
    alcSetThreadContext(nullptr);
    #endif
    alcDestroyContext(Context);
    Context = nullptr;
  }

  // disconnect from a device
  if (Device) {
    if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): closing device");
    alcCloseDevice(Device);
    Device = nullptr;
    Context = nullptr;
  }
  if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): shutdown complete!");
}


//==========================================================================
//
//  VOpenALDevice::AllocSource
//
//==========================================================================
bool VOpenALDevice::AllocSource (ALuint *src) {
  if (!src) return false;
  ClearError();
  alGenSources(1, src);
  if (IsError("cannot generate source")) {
    if (src) *src = (ALuint)-1;
    return false;
  }
  activeSourceSet.put(*src, true);
  return true;
}


//==========================================================================
//
//  VOpenALDevice::LoadSound
//
//  returns VSoundManager::LS_XXX
//  if not errored, sets `src` to new sound source
//
//==========================================================================
int VOpenALDevice::LoadSound (int sound_id, ALuint *src) {
  if (sound_id < 0 || sound_id >= GSoundManager->S_sfx.length()) {
    if (src) *src = (ALuint)-1;
    return VSoundManager::LS_Error;
  }

  if (BufferCount < GSoundManager->S_sfx.length()) {
    int newsz = ((GSoundManager->S_sfx.length()+1)|0xff)+1;
    ALuint *newbuf = new ALuint[newsz];
    if (BufferCount > 0) {
      for (int f = 0; f < BufferCount; ++f) newbuf[f] = Buffers[f];
    }
    for (int f = BufferCount; f < newsz; ++f) newbuf[f] = 0;
    delete[] Buffers;
    Buffers = newbuf;
    BufferCount = newsz;
  }

  /*
  if (BufferCount < sound_id+1) {
    int newsz = ((sound_id+4)|0xfff)+1;
    ALuint *newbuf = new ALuint[newsz];
    for (int f = BufferCount; f < newsz; ++f) newbuf[f] = 0;
    delete[] Buffers;
    Buffers = newbuf;
    BufferCount = newsz;
  }
  */

  if (Buffers[sound_id]) {
    if (AllocSource(src)) return VSoundManager::LS_Ready;
    if (src) *src = (ALuint)-1;
    return VSoundManager::LS_Error;
  }

  // check that sound lump is queued
  auto pss = sourcesPending.get(sound_id);
  if (pss) {
    // pending sound, generate new source, and add it to pending list
    if (!AllocSource(src)) {
      if (src) *src = (ALuint)-1;
      return VSoundManager::LS_Error;
    }
    PendingSrc *psrc = new PendingSrc;
    psrc->src = *src;
    psrc->sound_id = sound_id;
    psrc->next = *pss;
    sourcesPending.put(sound_id, psrc);
    srcPendingSet.put(*src, sound_id);
    return VSoundManager::LS_Pending;
  }

  // check that sound lump is loaded
  int res = GSoundManager->LoadSound(sound_id);
  if (res == VSoundManager::LS_Error) {
    if (src) *src = (ALuint)-1;
    return VSoundManager::LS_Error; // missing sound
  }

  // generate new source
  if (!AllocSource(src)) {
    GSoundManager->DoneWithLump(sound_id);
    return VSoundManager::LS_Error;
  }

  if (res == VSoundManager::LS_Pending) {
    // pending sound, generate new source, and add it to pending list
    vassert(!sourcesPending.get(sound_id));
    vassert(!srcPendingSet.get(*src));
    PendingSrc *psrc = new PendingSrc;
    psrc->src = *src;
    psrc->sound_id = sound_id;
    psrc->next = nullptr;
    sourcesPending.put(sound_id, psrc);
    srcPendingSet.put(*src, sound_id);
    return VSoundManager::LS_Pending;
  }

  ClearError();

  // create buffer
  alGenBuffers(1, &Buffers[sound_id]);
  if (IsError("cannot generate buffer")) {
    GSoundManager->DoneWithLump(sound_id);
    activeSourceSet.del(*src);
    alDeleteSources(1, src);
    return VSoundManager::LS_Error;
  }

  // load buffer data
  alBufferData(Buffers[sound_id],
    (GSoundManager->S_sfx[sound_id].SampleBits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16),
    GSoundManager->S_sfx[sound_id].Data,
    GSoundManager->S_sfx[sound_id].DataSize,
    GSoundManager->S_sfx[sound_id].SampleRate);

  if (IsError("cannot load buffer data")) {
    GSoundManager->DoneWithLump(sound_id);
    activeSourceSet.del(*src);
    alDeleteSources(1, src);
    return VSoundManager::LS_Error;
  }

  // we don't need to keep lump static
  GSoundManager->DoneWithLump(sound_id);
  return VSoundManager::LS_Ready;
}


//==========================================================================
//
//  VOpenALDevice::CommonPlaySound
//
//  workhorse for `PlaySound()` and `PlaySound3D()`
//
//==========================================================================
int VOpenALDevice::CommonPlaySound (bool is3d, int sound_id, const TVec &origin, const TVec &velocity,
                                    float volume, float pitch, bool Loop)
{
  ALuint src;
  (void)velocity;

  int res = LoadSound(sound_id, &src);
  if (res == VSoundManager::LS_Error) return -1;

  alSourcef(src, AL_GAIN, volume);
  alSourcei(src, AL_SOURCE_RELATIVE, (is3d ? AL_FALSE : AL_TRUE));
  alSource3f(src, AL_POSITION, origin.x, origin.y, origin.z); // at the listener origin
  #ifdef VV_SND_ALLOW_VELOCITY
  alSource3f(src, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
  #endif
  alSourcef(src, AL_ROLLOFF_FACTOR, snd_rolloff_factor);
  alSourcef(src, AL_REFERENCE_DISTANCE, snd_reference_distance);
  alSourcef(src, AL_MAX_DISTANCE, snd_max_distance);
  alSourcef(src, AL_PITCH, pitch);
  alSourcei(src, AL_LOOPING, (Loop ? AL_TRUE : AL_FALSE));
  if (res == VSoundManager::LS_Ready) {
    alSourcei(src, AL_BUFFER, Buffers[sound_id]);
    alSourcePlay(src);
  }
  ClearError();
  return src;
}


//==========================================================================
//
//  VOpenALDevice::PlaySound
//
//  This function adds a sound to the list of currently active sounds, which
//  is maintained as a given number of internal channels.
//
//==========================================================================
int VOpenALDevice::PlaySound (int sound_id, float volume, float pitch, bool Loop) {
  //k8: it seems that i was trying to put the sound slightly at the back; but hey, direction vectors!
  //alSource3f(src, AL_POSITION, 0.0f, 0.0f, -16.0f); //k8: really? wtf?!
  return CommonPlaySound(false, sound_id, TVec::ZeroVector, TVec::ZeroVector, volume, pitch, Loop);
}


//==========================================================================
//
//  VOpenALDevice::PlaySound3D
//
//==========================================================================
int VOpenALDevice::PlaySound3D (int sound_id, const TVec &origin, const TVec &velocity,
                                float volume, float pitch, bool Loop)
{
  return CommonPlaySound(true, sound_id, origin, velocity, volume, pitch, Loop);
}


//==========================================================================
//
//  VOpenALDevice::UpdateChannel3D
//
//==========================================================================
void VOpenALDevice::UpdateChannel3D (int Handle, const TVec &Org, const TVec &Vel) {
  (void)Vel;
  if (Handle == -1) return;
  alSource3f(Handle, AL_POSITION, Org.x, Org.y, Org.z);
  #ifdef VV_SND_ALLOW_VELOCITY
  alSource3f(Handle, AL_VELOCITY, Vel.x, Vel.y, Vel.z);
  #endif
  ClearError();
}


//==========================================================================
//
//  VOpenALDevice::IsChannelPlaying
//
//==========================================================================
bool VOpenALDevice::IsChannelPlaying (int Handle) {
  if (Handle == -1) return false;
  // pending sounds are "playing"
  if (srcPendingSet.has((ALuint)Handle)) return true;
  if (srcErrorSet.has((ALuint)Handle)) return false;
  ALint State;
  alGetSourcei((ALuint)Handle, AL_SOURCE_STATE, &State);
  ClearError();
  return (State == AL_PLAYING);
}


//==========================================================================
//
//  VOpenALDevice::StopChannel
//
//  Stop the sound. Necessary to prevent runaway chainsaw, and to stop
//  rocket launches when an explosion occurs.
//  All sounds MUST be stopped;
//
//==========================================================================
void VOpenALDevice::StopChannel (int Handle) {
  if (Handle == -1) return;
  ALuint hh = (ALuint)Handle;
  // remove pending sounds
  auto sidp = srcPendingSet.get(hh);
  if (sidp) {
    // remove from pending list
    srcPendingSet.del(hh);
    PendingSrc **pss = sourcesPending.get(*sidp);
    if (pss) {
      PendingSrc *prev = nullptr, *cur = *pss;
      while (cur && cur->src != hh) {
        prev = cur;
        cur = cur->next;
      }
      if (cur) {
        // i found her!
        if (prev) {
          // not first
          prev->next = cur->next;
        } else if (cur->next) {
          // first, not last
          sourcesPending.put(*sidp, cur->next);
        } else {
          // only one
          sourcesPending.del(*sidp);
        }
        // signal manager that we don't want it anymore
        GSoundManager->DoneWithLump(cur->sound_id);
        delete cur;
      }
    }
  } else {
    // stop buffer
    alSourceStop(hh);
  }
  srcErrorSet.del(hh);
  activeSourceSet.del(hh);
  alDeleteSources(1, &hh);
  ClearError();
}


//==========================================================================
//
//  VOpenALDevice::UpdateListener
//
//==========================================================================
void VOpenALDevice::UpdateListener (const TVec &org, const TVec &vel,
                                    const TVec &fwd, const TVec &/*right*/, const TVec &up
#if defined(VAVOOM_REVERB)
                                    , VReverbInfo *Env
#endif
)
{
  (void)vel;

  if (prevPosition != org) {
    prevPosition = org;
    alListener3f(AL_POSITION, org.x, org.y, org.z);
  }

  if (prevUp != up || prevFwd != fwd) {
    prevUp = up;
    prevFwd = fwd;
    ALfloat orient[6] = { fwd.x, fwd.y, fwd.z, up.x, up.y, up.z};
    alListenerfv(AL_ORIENTATION, orient);
  }

#ifdef VV_SND_ALLOW_VELOCITY
  if (prevVelocity != vel) {
    prevVelocity = vel;
    alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);
  }

  if (prevDopplerFactor != snd_doppler_factor.asFloat()) {
    prevDopplerFactor = snd_doppler_factor.asFloat();
    alDopplerFactor(prevDopplerFactor);
  }

  if (prevDopplerVelocity != snd_doppler_velocity.asFloat()) {
    prevDopplerVelocity = snd_doppler_velocity.asFloat();
    alDopplerVelocity(prevDopplerVelocity);
  }
#endif

  ClearError();
}


//==========================================================================
//
//  VOpenALDevice::NotifySoundLoaded
//
// WARNING! this must be called from the main thread, i.e.
//          from the thread that calls `PlaySound*()` API!
//
//==========================================================================
void VOpenALDevice::NotifySoundLoaded (int sound_id, bool success) {
  PendingSrc **pss = sourcesPending.get(sound_id);
  if (!pss) return; // nothing to do
  PendingSrc *cur = *pss;
  while (cur) {
    PendingSrc *next = cur->next;
    vassert(cur->sound_id == sound_id);
    srcPendingSet.del(cur->src);
    if (success) {
      // play it
      //GCon->Logf("OpenAL: playing source #%u (sound #%d)", cur->src, sound_id);
      if (!Buffers[sound_id]) {
        ClearError();
        // create buffer
        alGenBuffers(1, &Buffers[sound_id]);
        if (IsError("cannot generate buffer")) {
          srcErrorSet.put(cur->src, true);
          Buffers[sound_id] = 0;
        } else {
          // load buffer data
          alBufferData(Buffers[sound_id],
            (GSoundManager->S_sfx[sound_id].SampleBits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16),
            GSoundManager->S_sfx[sound_id].Data,
            GSoundManager->S_sfx[sound_id].DataSize,
            GSoundManager->S_sfx[sound_id].SampleRate);
          if (IsError("cannot load buffer data")) {
            srcErrorSet.put(cur->src, true);
            Buffers[sound_id] = 0;
          } else {
            //GCon->Logf("OpenAL: playing source #%u (sound #%d; sr=%d; bits=%d; size=%d)", cur->src, sound_id, GSoundManager->S_sfx[sound_id].SampleRate, GSoundManager->S_sfx[sound_id].SampleBits, GSoundManager->S_sfx[sound_id].DataSize);
            alSourcei(cur->src, AL_BUFFER, Buffers[sound_id]);
            alSourcePlay(cur->src);
          }
        }
      } else {
        // already buffered, just play it
        //GCon->Logf("OpenAL: playing already buffered source #%u (sound #%d; sr=%d; bits=%d; size=%d)", cur->src, sound_id, GSoundManager->S_sfx[sound_id].SampleRate, GSoundManager->S_sfx[sound_id].SampleBits, GSoundManager->S_sfx[sound_id].DataSize);
        alSourcei(cur->src, AL_BUFFER, Buffers[sound_id]);
        alSourcePlay(cur->src);
      }
      ClearError();
    } else {
      // mark as invalid
      srcErrorSet.put(cur->src, true);
    }
    GSoundManager->DoneWithLump(sound_id);
    delete cur;
    cur = next;
  }
  sourcesPending.del(sound_id);
}


//==========================================================================
//
//  VOpenALDevice::OpenStream
//
//==========================================================================
bool VOpenALDevice::OpenStream (int Rate, int Bits, int Channels) {
  StrmSampleRate = Rate;
  StrmFormat =
    Channels == 2 ?
      (Bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16) :
      (Bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16);

  CloseStream(); // just in case
  ClearError();
  alGenSources(1, &StrmSource);
  if (IsError("cannot generate source")) { StrmSource = 0; return false; }
  activeSourceSet.put(StrmSource, true);
  alSourcei(StrmSource, AL_SOURCE_RELATIVE, AL_TRUE);
  memset((void *)StrmBuffers, 0, sizeof(StrmBuffers));
  alGenBuffers(NUM_STRM_BUFFERS, StrmBuffers);
  #if 0
  // this is required for MojoAL
  for (unsigned f = 0; f < NUM_STRM_BUFFERS; ++f) {
    vint16 data[2];
    data[0] = data[1] = 0;
    alBufferData(StrmBuffers[f], StrmFormat, data, 4, StrmSampleRate);
  }
  alSourceQueueBuffers(StrmSource, NUM_STRM_BUFFERS, StrmBuffers);
  alSourcePlay(StrmSource);
  ClearError();
  StrmNumAvailableBuffers = 0;
  #else
  // don't queue any buffers, because why?
  alSourceStop(StrmSource); // just in case
  ClearError();
  StrmNumAvailableBuffers = NUM_STRM_BUFFERS;
  //ALint NumProc = -1; // just in case
  //alGetSourcei(StrmSource, AL_BUFFERS_PROCESSED, &NumProc);
  //fprintf(stderr, "OpenStream: NumProc=%d (%d); StrmNumAvailableBuffers=%d\n", NumProc, NUM_STRM_BUFFERS, StrmNumAvailableBuffers);
  // all buffers are available
  for (unsigned f = 0; f < NUM_STRM_BUFFERS; ++f) {
    StrmAvailableBuffers[f] = StrmBuffers[f];
    //fprintf(stderr, "  f=%u: %u\n", f, StrmBuffers[f]);
  }
  #endif
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
    activeSourceSet.del(StrmSource);
    alDeleteSources(1, &StrmSource);
    ClearError();
    StrmSource = 0;
  }
}


//==========================================================================
//
//  VOpenALDevice::GetStreamAvailable
//
//==========================================================================
int VOpenALDevice::GetStreamAvailable () {
  if (!StrmSource) {
    //fprintf(stderr, "NO STREAM SOURCE!\n");
    return 0;
  }
  // check if we have any free buffers
  if (StrmNumAvailableBuffers < NUM_STRM_BUFFERS) {
    ALint NumProc = 0; // just in case
    ClearError();
    alGetSourcei(StrmSource, AL_BUFFERS_PROCESSED, &NumProc);
    if (IsError("cannot get stream source info")) NumProc = 0;
    //fprintf(stderr, "000: NumProc=%d; StrmNumAvailableBuffers=%d\n", NumProc, StrmNumAvailableBuffers);
    if (NumProc > 0) {
      alSourceUnqueueBuffers(StrmSource, NumProc, StrmAvailableBuffers+StrmNumAvailableBuffers);
      ClearError();
      StrmNumAvailableBuffers += NumProc;
      // just in case; this should not happen, but...
      if (StrmNumAvailableBuffers > NUM_STRM_BUFFERS) StrmNumAvailableBuffers = NUM_STRM_BUFFERS;
      //fprintf(stderr, "001: NumProc=%d; StrmNumAvailableBuffers=%d\n", NumProc, StrmNumAvailableBuffers);
    }
  }
  return (StrmNumAvailableBuffers > 0 ? STRM_BUFFER_SIZE : 0);
}


//==========================================================================
//
//  VOpenALDevice::GetStreamBuffer
//
//==========================================================================
vint16 *VOpenALDevice::GetStreamBuffer () {
  return StrmDataBuffer;
}


//==========================================================================
//
//  VOpenALDevice::SetStreamData
//
//==========================================================================
void VOpenALDevice::SetStreamData (vint16 *Data, int Len) {
  if (Len <= 0 || !Data) return;
  if (!StrmSource) return;
  if (StrmNumAvailableBuffers <= 0) __builtin_trap(); // the thing that should not be!
  ALint State;
  --StrmNumAvailableBuffers;
  ALuint Buf = StrmAvailableBuffers[StrmNumAvailableBuffers];
  //fprintf(stderr, "SetStreamData: using buffer #%d/%d (%u)\n", StrmNumAvailableBuffers, NUM_STRM_BUFFERS-1, Buf);
  alBufferData(Buf, StrmFormat, Data, Len*4, StrmSampleRate);
  alSourceQueueBuffers(StrmSource, 1, &Buf);
  alGetSourcei(StrmSource, AL_SOURCE_STATE, &State);
  if (State != AL_PLAYING) {
    if (StrmSource) alSourcePlay(StrmSource);
  }
  ClearError();
}


//==========================================================================
//
//  VOpenALDevice::SetStreamVolume
//
//==========================================================================
void VOpenALDevice::SetStreamVolume (float Vol) {
  if (StrmSource) {
    alSourcef(StrmSource, AL_GAIN, Vol);
    ClearError();
  }
}


//==========================================================================
//
//  VOpenALDevice::SetStreamPitch
//
//==========================================================================
void VOpenALDevice::SetStreamPitch (float pitch) {
  if (StrmSource) {
    alSourcef(StrmSource, AL_PITCH, pitch);
    ClearError();
  }
}


//==========================================================================
//
//  VOpenALDevice::PauseStream
//
//==========================================================================
void VOpenALDevice::PauseStream () {
  if (StrmSource) {
    alSourcePause(StrmSource);
    ClearError();
  }
}


//==========================================================================
//
//  VOpenALDevice::ResumeStream
//
//==========================================================================
void VOpenALDevice::ResumeStream () {
  if (StrmSource) {
    alSourcePlay(StrmSource);
    ClearError();
  }
}
