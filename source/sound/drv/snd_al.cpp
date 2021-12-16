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
// OpenAL-soft extension list: https://openal-soft.org/openal-extensions/
//**************************************************************************
#include "../../gamedefs.h"
#include "../sound.h"
#include "../snd_local.h"


#ifdef VV_SND_ALLOW_VELOCITY
static VCvarF snd_doppler_factor("snd_doppler_factor", "1", "OpenAL doppler factor.", CVAR_NoShadow/*|CVAR_Archive*/);
static VCvarF snd_doppler_velocity("snd_doppler_velocity", "10000", "OpenAL doppler velocity.", CVAR_NoShadow/*|CVAR_Archive*/);
#endif
static VCvarF snd_rolloff_factor("snd_rolloff_factor", "1", "OpenAL rolloff factor.", CVAR_NoShadow/*|CVAR_Archive*/);
static VCvarF snd_reference_distance("snd_reference_distance", "192", "OpenAL reference distance.", CVAR_NoShadow/*|CVAR_Archive*/); // was 384, and 64, and 192
// it is 4096 in our main sound code; anything futher than this will be dropped
static VCvarF snd_max_distance("snd_max_distance", "8192", "OpenAL max distance.", CVAR_NoShadow/*|CVAR_Archive*/); // was 4096, and 2042, and 8192

static VCvarI snd_resampler("snd_resampler", "-1", "OpenAL sound resampler (-1 means \"default\").", CVAR_Archive|CVAR_NoShadow);

static VCvarB openal_show_extensions("openal_show_extensions", false, "Show available OpenAL extensions?", /*CVAR_Archive|*/CVAR_PreInit|CVAR_NoShadow);

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
  , RealMaxVoices(0)
  , HasTreadContext(false)
  , HasBatchUpdate(false)
  , HasForceSpatialize(false)
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

  HasTreadContext = false;
  p_alcSetThreadContext = nullptr;
  p_alcGetThreadContext = nullptr;

  HasBatchUpdate = false;
  p_alDeferUpdatesSOFT = nullptr;
  p_alProcessUpdatesSOFT = nullptr;

  HasForceSpatialize = false;
  alSrcSpatSoftEnum = 0;
  //alSrcSpatSoftValue = 0;

  p_alGetStringiSOFT = nullptr;
  alNumResSoftValue = 0;
  alDefResSoftValue = 0;
  alResNameSoftValue = 0;
  alSrcResamplerSoftValue = 0;

  ResamplerNames.clear();
  defaultResampler = 0;

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

  // can be done before creating context
  // MojoAL doesn't have this
  if (alcIsExtensionPresent(Device, "ALC_EXT_thread_local_context")) {
    p_alcSetThreadContext = (alcSetThreadContextFn)alcGetProcAddress(Device, "alcSetThreadContext");
    p_alcGetThreadContext = (alcGetThreadContextFn)alcGetProcAddress(Device, "alcGetThreadContext");
    if (p_alcSetThreadContext && p_alcGetThreadContext) {
      HasTreadContext = true;
      GCon->Logf(NAME_Init, "OpenAL: found 'ALC_EXT_thread_local_context'");
    }
  }

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
      #if defined(ALC_SOFT_HRTF) && ALC_SOFT_HRTF
      ALC_HRTF_SOFT, ALC_FALSE, // disable HRTF, we cannot properly support or configure it
      #endif
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

  if (HasTreadContext) {
    p_alcSetThreadContext(Context);
  } else {
    alcMakeContextCurrent(Context);
    ALenum E = alGetError();
    if (E != AL_NO_ERROR) Sys_Error("OpenAL error (setting thread context): %s", alGetErrorString(E));
  }

  GCon->Logf(NAME_Init, "OpenAL: AL_VENDOR: %s", alGetString(AL_VENDOR));
  GCon->Logf(NAME_Init, "OpenAL: AL_RENDERER: %s", alGetString(AL_RENDERER));
  GCon->Logf(NAME_Init, "OpenAL: AL_VERSION: %s", alGetString(AL_VERSION));
  {
    ALCint freq = 0;
    alcGetIntegerv(Device, ALC_FREQUENCY, 1, &freq);
    if (freq) GCon->Logf(NAME_Init, "OpenAL: sample rate is %d", freq);
  }
  #if 0
  { // refresh rate, in Hz
    ALCint refresh = 0;
    alcGetIntegerv(Device, ALC_REFRESH, 1, &refresh);
    if (refresh) GCon->Logf(NAME_Init, "OpenAL: refresh rate: %d", refresh);
  }
  #endif

  // this is default, but hey...
  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

  //k8: this is for using source's `AL_DISTANCE_MODEL` property
  //    we don't need it at all, our distance model is global
  /*
  // MojoAL doesn't have this
  // there is `alEnable()` in MojoAL, but it does nothing, and returns enum error (it is permitted)
  #if defined(AL_EXT_source_distance_model) && AL_EXT_source_distance_model
  alEnable(AL_SOURCE_DISTANCE_MODEL);
  #endif
  */

  // AL extensions should be checked with the active context
  if (alIsExtensionPresent("AL_SOFT_deferred_updates")) {
    p_alDeferUpdatesSOFT = (alDeferUpdatesSOFTFn)alGetProcAddress("alDeferUpdatesSOFT");
    p_alProcessUpdatesSOFT = (alProcessUpdatesSOFTFn)alGetProcAddress("alProcessUpdatesSOFT");
    if (p_alDeferUpdatesSOFT && p_alProcessUpdatesSOFT) {
      HasBatchUpdate = true;
      GCon->Logf(NAME_Init, "OpenAL: found 'AL_SOFT_deferred_updates'");
    }
  }

  if (alIsExtensionPresent("AL_SOFT_source_spatialize")) {
    ClearError();
    alSrcSpatSoftEnum = alGetEnumValue("AL_SOURCE_SPATIALIZE_SOFT");
    //alSrcSpatSoftValue = alGetEnumValue("AL_AUTO_SOFT");
    if (alGetError() == 0 && alSrcSpatSoftEnum) {
      HasForceSpatialize = true;
      GCon->Logf(NAME_Init, "OpenAL: found 'AL_SOFT_source_spatialize'");
    }
  }

  // clear error code
  ClearError();

  if (alIsExtensionPresent("AL_SOFT_source_resampler")) {
    p_alGetStringiSOFT = (alGetStringiSOFTFn)alGetProcAddress("alGetStringiSOFT");
    alNumResSoftValue = alGetEnumValue("AL_NUM_RESAMPLERS_SOFT");
    alDefResSoftValue = alGetEnumValue("AL_DEFAULT_RESAMPLER_SOFT");
    alResNameSoftValue = alGetEnumValue("AL_RESAMPLER_NAME_SOFT");
    alSrcResamplerSoftValue = alGetEnumValue("AL_SOURCE_RESAMPLER_SOFT");
    if (p_alGetStringiSOFT && alNumResSoftValue && alDefResSoftValue && alResNameSoftValue && alSrcResamplerSoftValue) {
      ALint resCount = alGetInteger(alNumResSoftValue);
      defaultResampler = alGetInteger(alDefResSoftValue);
      if (resCount > 0) {
        for (ALint f = 0; f < resCount; ++f) {
          const ALchar *name = p_alGetStringiSOFT(alResNameSoftValue, f);
          if (!name || !name[0]) name = "<unknown>";
          ResamplerNames.append(VStr(name));
        }
        if (defaultResampler < 0 || defaultResampler >= resCount) defaultResampler = 0; // just in case
      } else {
        defaultResampler = 0; // just in case
      }
    }
    // if we have less than two resamplers, there is no need to set anything
    if (ResamplerNames.length() < 2) {
      p_alGetStringiSOFT = nullptr;
      alNumResSoftValue = 0;
      alDefResSoftValue = 0;
      alResNameSoftValue = 0;
      alSrcResamplerSoftValue = 0;
    }
  }

  if (ResamplerNames.length() == 0) {
    ResamplerNames.append(VStr("default"));
  }

  // print some information
  if (openal_show_extensions) {
    GCon->Log(NAME_Init, "AL_EXTENSIONS:");
    TArray<VStr> Exts;
    VStr((char *)alGetString(AL_EXTENSIONS)).Split(' ', Exts);
    for (VStr s : Exts) GCon->Logf(NAME_Init, "- %s", *s);
    GCon->Log(NAME_Init, "ALC_EXTENSIONS:");
    VStr((char *)alcGetString(Device, ALC_EXTENSIONS)).Split(' ', Exts);
    for (VStr s : Exts) GCon->Logf(NAME_Init, "- %s", *s);
    // dump available resamplers
    if (ResamplerNames.length()) {
      GCon->Logf(NAME_Init, "OpenAL resamplers:");
      for (int f = 0; f < ResamplerNames.length(); ++f) {
        GCon->Logf(NAME_Init, "  %d: %s%s", f, *ResamplerNames[f], (defaultResampler == f ? " [default]" : ""));
      }
    }
    // just in case
    ClearError();
  }

  // we will allocate array for buffers later, because we may don't know the proper size yet

  GCon->Log(NAME_Init, "OpenAL initialized.");
  return true;
}


//==========================================================================
//
//  VOpenALDevice::AddCurrentThread
//
//==========================================================================
void VOpenALDevice::AddCurrentThread () {
  // MojoAL doesn't have this
  // it is usually safe, because it is called only by the streaming player
  // and there is no need to do anything here anyway
  if (HasTreadContext) {
    p_alcSetThreadContext(Context);
  } else {
    // streaming thread should use the same context anwyay
    //alcMakeContextCurrent(Context);
  }
}


//==========================================================================
//
//  VOpenALDevice::RemoveCurrentThread
//
//==========================================================================
void VOpenALDevice::RemoveCurrentThread () {
  // MojoAL doesn't have this
  // do nothing here, this is used only by streaming player
  if (HasTreadContext) p_alcSetThreadContext(nullptr);
}


//==========================================================================
//
//  VOpenALDevice::StartBatchUpdate
//
//==========================================================================
void VOpenALDevice::StartBatchUpdate () {
  if (HasBatchUpdate) p_alDeferUpdatesSOFT();
}


//==========================================================================
//
//  VOpenALDevice::FinishBatchUpdate
//
//==========================================================================
void VOpenALDevice::FinishBatchUpdate () {
  if (HasBatchUpdate) p_alProcessUpdatesSOFT();
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
        Buffers[bidx] = 0; // just in case
      }
    }
    Z_Free(Buffers);
    Buffers = nullptr;
  }
  BufferCount = 0;

  // destroy context
  if (Context) {
    if (developer) GLog.Log(NAME_Dev, "VOpenALDevice::Shutdown(): destroying context");
    if (HasTreadContext) p_alcSetThreadContext(nullptr);
    alcMakeContextCurrent(nullptr);
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
    *src = (ALuint)-1;
    return false;
  }
  activeSourceSet.put(*src, true);
  return true;
}


//==========================================================================
//
//  VOpenALDevice::RecordPendingSound
//
//  records one new pending sound
//  allocates sound source, records,
//  returns `true` on success, `false` on error
//  `src` must not be NULL! returns source id on success
//  returns `VSoundManager::LS_Error` or `VSoundManager::LS_Pending`
//
//==========================================================================
int VOpenALDevice::RecordPendingSound (int sound_id, ALuint *src) {
  // just in case
  if (src) *src = (ALuint)-1;
  if (sound_id < 0 || sound_id >= GSoundManager->S_sfx.length()) return VSoundManager::LS_Error;
  // allocate sound source
  if (!AllocSource(src)) return VSoundManager::LS_Error;
  // record new pending sound
  auto pss = sourcesPending.get(sound_id); // previous list head
  PendingSrc *psrc = new PendingSrc;
  psrc->src = *src;
  psrc->sound_id = sound_id;
  psrc->next = (pss ? *pss : nullptr);
  sourcesPending.put(sound_id, psrc);
  srcPendingSet.put(*src, sound_id);
  return VSoundManager::LS_Pending;
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
  if (src) *src = (ALuint)-1;

  if (sound_id < 0 || sound_id >= GSoundManager->S_sfx.length()) {
    GSoundManager->DoneWithLump(sound_id);
    return VSoundManager::LS_Error;
  }

  if (BufferCount < GSoundManager->S_sfx.length()) {
    const int newsz = ((GSoundManager->S_sfx.length()+1)|0xff)+1; // yes, overallocate a little, who cares
    Buffers = (ALuint *)Z_Realloc(Buffers, (size_t)newsz*sizeof(ALuint));
    memset(Buffers+BufferCount, 0, (size_t)(newsz-BufferCount)*sizeof(ALuint));
    BufferCount = newsz;
  }

  if (Buffers[sound_id]) {
    GSoundManager->DoneWithLump(sound_id);
    if (AllocSource(src)) return VSoundManager::LS_Ready;
    if (src) *src = (ALuint)-1;
    return VSoundManager::LS_Error;
  }

  // check that sound lump is already queued
  auto pss = sourcesPending.get(sound_id);
  // do not call `GSoundManager->DoneWithLump()`, we aren't done yet
  if (pss) return RecordPendingSound(sound_id, src);

  // check that sound lump is loaded
  int res = GSoundManager->LoadSound(sound_id);
  // do not call `GSoundManager->DoneWithLump()`, main loaders knows that it failed
  if (res == VSoundManager::LS_Error) return VSoundManager::LS_Error; // missing sound

  if (res == VSoundManager::LS_Pending) {
    res = RecordPendingSound(sound_id, src);
    if (res == VSoundManager::LS_Error) GSoundManager->DoneWithLump(sound_id);
    return res;
  }

  // generate new source
  if (!AllocSource(src)) {
    GSoundManager->DoneWithLump(sound_id);
    return VSoundManager::LS_Error;
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

  // we don't need to keep the lump data anymore
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

  const int res = LoadSound(sound_id, &src);
  if (res == VSoundManager::LS_Error) return -1;

  // force-spatialize it?
  // non-3d sounds need not be spatialized (and they can be stereo, why not?)
  if (HasForceSpatialize) {
    alSourcei(src, alSrcSpatSoftEnum, (is3d ? AL_TRUE : AL_FALSE));
    ClearError();
  }

  // set resampler
  if (ResamplerNames.length() > 2) {
    int smp = snd_resampler.asInt();
    if (smp < 0 || smp >= ResamplerNames.length()) smp = defaultResampler;
    alSourcei(src, alSrcResamplerSoftValue, smp);
  }
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
  // remove pending sounds for this channel
  auto sidp = srcPendingSet.get(hh);
  if (sidp) {
    bool releaseLump = true;
    const int sndid = *sidp;
    // remove from pending list
    srcPendingSet.del(hh);
    // get list head (list of all sources with this sound id)
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
          releaseLump = false;
        } else if (cur->next) {
          // first, not last
          sourcesPending.put(*sidp, cur->next);
          releaseLump = false;
        } else {
          // only one
          sourcesPending.del(*sidp);
        }
        delete cur;
      }
    }
    if (releaseLump) {
      // signal manager that we don't want it anymore (it was pending)
      GSoundManager->DoneWithLump(sndid);
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
//  WARNING! this must be called from the main thread, i.e.
//           from the thread that calls `PlaySound*()` API!
//
//==========================================================================
void VOpenALDevice::NotifySoundLoaded (int sound_id, bool success) {
  // get list head (list of all sources with this sound id)
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
          sfxinfo_t *sfx = &GSoundManager->S_sfx[sound_id];
          vassert(sfx->Data && sfx->DataSize);
          alBufferData(Buffers[sound_id], (sfx->SampleBits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16), sfx->Data, sfx->DataSize, sfx->SampleRate);
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
    delete cur;
    cur = next;
  }
  sourcesPending.del(sound_id);
  GSoundManager->DoneWithLump(sound_id);
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
  // don't remap stereo channels
  bool didStereoSet = false;
  if (alIsExtensionPresent("AL_SOFT_direct_channels_remix")) {
    const ALenum dcc = alGetEnumValue("AL_DIRECT_CHANNELS_SOFT");
    const ALenum rmx = alGetEnumValue("AL_REMIX_UNMATCHED_SOFT");
    if (dcc && rmx) {
      didStereoSet = true;
      alSourcei(StrmSource, dcc, rmx);
      if (alGetError() == 0) GCon->Log("OpenAL: relaxed stereo channel remapping for better stereo quality.");
    }
  }
  if (!didStereoSet && alIsExtensionPresent("AL_SOFT_direct_channels")) {
    const ALenum dcc = alGetEnumValue("AL_DIRECT_CHANNELS_SOFT");
    //GCon->Logf(NAME_Debug, "DCC: 0x%04x", (unsigned)dcc);
    if (dcc) {
      alSourcei(StrmSource, dcc, AL_TRUE);
      if (alGetError() == 0) GCon->Log("OpenAL: disabled stereo channel remapping for better stereo quality.");
    }
  }
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
