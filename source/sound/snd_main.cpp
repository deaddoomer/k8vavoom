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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include "gamedefs.h"
#include "cl_local.h"
#include "snd_local.h"


static int cli_NoSound = 0;
static int cli_NoMusic = 0;
static int cli_DebugSound = 0;

static bool cliRegister_sndmain_args =
  VParsedArgs::RegisterFlagSet("-nosound", "disable all sound (including music)", &cli_NoSound) &&
  VParsedArgs::RegisterAlias("-no-sound", "-nosound") &&
  VParsedArgs::RegisterFlagSet("-nomusic", "disable music", &cli_NoMusic) &&
  VParsedArgs::RegisterAlias("-no-music", "-nomusic") &&
  VParsedArgs::RegisterFlagSet("-debug-sound", "!show some debug messages from sound system", &cli_DebugSound);


// ////////////////////////////////////////////////////////////////////////// //
class VSoundSeqNode {
public:
  vint32 Sequence;
  vint32 *SequencePtr;
  vint32 OriginId;
  TVec Origin;
  vint32 CurrentSoundID;
  float DelayTime;
  float Volume;
  float Attenuation;
  vint32 StopSound;
  vuint32 DidDelayOnce;
  TArray<vint32> SeqChoices;
  vint32 ModeNum;
  VSoundSeqNode *Prev;
  VSoundSeqNode *Next;
  VSoundSeqNode *ParentSeq;
  VSoundSeqNode *ChildSeq;

  VSoundSeqNode (int, const TVec &, int, int);
  ~VSoundSeqNode ();
  void Update (float);
  void Serialise (VStream &);
};


// ////////////////////////////////////////////////////////////////////////// //
// main audio management class
class VAudio : public VAudioPublic {
public:
  // sound sequence list
  int ActiveSequences;
  VSoundSeqNode *SequenceListHead;

  VAudio ();
  virtual ~VAudio () override;

  // top level methods
  virtual void Init () override;
  virtual void Shutdown () override;

  // playback of sound effects
  virtual void PlaySound (int InSoundId, const TVec &origin, const TVec &velocity,
                        int origin_id, int channel, float volume, float Attenuation, bool Loop) override;
  virtual void StopSound (int origin_id, int channel) override;
  virtual void StopAllSound () override;
  virtual bool IsSoundPlaying (int origin_id, int InSoundId) override;

  // music and general sound control
  virtual void StartSong (VName song, bool loop) override;
  virtual void PauseSound () override;
  virtual void ResumeSound () override;
  virtual void Start () override;
  virtual void MusicChanged () override;
  virtual void UpdateSounds () override;

  // sound sequences
  virtual void StartSequence (int OriginId, const TVec &Origin, VName Name, int ModeNum) override;
  virtual void AddSeqChoice (int OriginId, VName Name) override;
  virtual void StopSequence (int origin_id) override;
  virtual void UpdateActiveSequences (float DeltaTime) override;
  virtual void StopAllSequences () override;
  virtual void SerialiseSounds (VStream &Strm) override;

  // returns codec or nullptr
  virtual VAudioCodec *LoadSongInternal (const char *Song, bool wasPlaying) override;

private:
  enum { MAX_CHANNELS = 256 };

  enum { PRIORITY_MAX_ADJUST = 10 };

  // info about sounds currently playing
  struct FChannel {
    int origin_id;
    int channel;
    TVec origin;
    TVec velocity;
    int sound_id;
    int priority;
    float volume;
    float Attenuation;
    int handle;
    bool is3D;
    bool LocalPlayerSound;
    bool Loop;
  };

  // sound curve
  int MaxSoundDist;

  // map's music lump and CD track
  VName MapSong;
  float MusicVolumeFactor;

  // stream music player
  bool MusicEnabled;
  VStreamMusicPlayer *StreamMusicPlayer;

  // list of currently playing sounds
  FChannel Channel[MAX_CHANNELS];
  int NumChannels;
  int ChanUsed;
  vuint32 ChanBitmap[(MAX_CHANNELS+31)/32]; // used bitmap

  // maximum volume for sound
  float MaxVolume;

  // listener orientation
  TVec ListenerForward;
  TVec ListenerRight;
  TVec ListenerUp;

  // hardware devices
  VOpenALDevice *SoundDevice;

  // console variables
  static VCvarF snd_sfx_volume;
  static VCvarF snd_music_volume;
  //static VCvarB snd_swap_stereo;
  static VCvarI snd_channels;
  static VCvarB snd_external_music;

  // friends
  friend class TCmdMusic;

  // sound effect helpers
  int GetChannel (int, int, int, int);
  void StopChannel (int cidx); // won't deallocate it
  void UpdateSfx ();

  // music playback
  void StartMusic ();
  void PlaySong (const char *, bool);

  // execution of console commands
  void CmdMusic (const TArray<VStr>&);

  // don't shutdown, just reset
  void ResetAllChannels ();
  int AllocChannel (); // -1: no more
  void DeallocChannel (int cidx);

  inline int ChanFirstUsed () const { return ChanNextUsed(-1, true); }
  int ChanNextUsed (int cidx, bool wantFirst=false) const;

  // WARNING! this must be called from the main thread, i.e.
  //          from the thread that calls `PlaySound*()` API!
  virtual void NotifySoundLoaded (int sound_id, bool success) override;
};


#define FOR_EACH_CHANNEL(varname) \
  for (int varname = ChanFirstUsed(); varname >= 0; varname = ChanNextUsed(i))


// ////////////////////////////////////////////////////////////////////////// //
FAudioCodecDesc *FAudioCodecDesc::List = nullptr;
VAudioPublic *GAudio = nullptr;

VCvarF VAudio::snd_sfx_volume("snd_sfx_volume", "0.5", "Sound effects volume.", CVAR_Archive);
VCvarF VAudio::snd_music_volume("snd_music_volume", "0.5", "Music volume", CVAR_Archive);
//VCvarB VAudio::snd_swap_stereo("snd_swap_stereo", false, "Swap stereo channels?", CVAR_Archive);
VCvarI VAudio::snd_channels("snd_channels", "64", "Number of sound channels.", CVAR_Archive);
VCvarB VAudio::snd_external_music("snd_external_music", false, "Allow external music remapping?", CVAR_Archive);

static VCvarF snd_random_pitch("snd_random_pitch", "0.27", "Random pitch all sounds (0: none, otherwise max change).", CVAR_Archive);
static VCvarF snd_random_pitch_boost("snd_random_pitch_boost", "1", "Random pitch will be multiplied by this value.", CVAR_Archive);

VCvarI snd_mid_player("snd_mid_player", "1", "MIDI player type (0:Timidity; 1:FluidSynth; -1:none)", CVAR_Archive|CVAR_PreInit);
VCvarI snd_mod_player("snd_mod_player", "2", "Module player type", CVAR_Archive);

extern VCvarB snd_music_background_load;


//==========================================================================
//
//  VAudioPublic::Create
//
//==========================================================================
VAudioPublic *VAudioPublic::Create () {
  return new VAudio();
}


//==========================================================================
//
//  VAudio::VAudio
//
//==========================================================================
VAudio::VAudio ()
  : MaxSoundDist(4096)
  , MapSong(NAME_None)
  , MusicEnabled(true)
  , StreamMusicPlayer(nullptr)
  , NumChannels(0)
  , ChanUsed(0)
  , MaxVolume(0)
  , SoundDevice(nullptr)
{
  ActiveSequences = 0;
  SequenceListHead = nullptr;
  ResetAllChannels();
}


//==========================================================================
//
//  VAudio::~VAudio
//
//==========================================================================
VAudio::~VAudio () {
  Shutdown();
}


//==========================================================================
//
//  VAudio::ResetAllChannels
//
//==========================================================================
void VAudio::ResetAllChannels () {
  memset(Channel, 0, sizeof(Channel));
  ChanUsed = 0;
  for (int f = 0; f < MAX_CHANNELS; ++f) Channel[f].handle = -1;
  memset(ChanBitmap, 0, sizeof(ChanBitmap));
}


//==========================================================================
//
//  VAudio::ChanNextUsed
//
//==========================================================================
int VAudio::ChanNextUsed (int cidx, bool wantFirst) const {
  if (ChanUsed < 1) return -1; // anyway
  if (!wantFirst && cidx < 0) return -1;
  if (wantFirst) cidx = 0; else ++cidx;
  while (cidx < NumChannels) {
    const int bidx = cidx/32;
    const vuint32 mask = 0xffffffffu>>(cidx%32);
    const vuint32 cbv = ChanBitmap[bidx];
    if (cbv&mask) {
      // has some used channels
      for (;;) {
        if (cbv&(0x80000000u>>(cidx%32))) return cidx;
        ++cidx;
      }
    }
    cidx = (cidx|0x1f)+1;
  }
  return -1;
}


//==========================================================================
//
//  VAudio::AllocChannel
//
//  -1: no more
//
//==========================================================================
int VAudio::AllocChannel () {
  if (ChanUsed >= NumChannels) return -1;
  for (int bidx = 0; bidx < (NumChannels+31)/32; ++bidx) {
    vuint32 cbv = ChanBitmap[bidx];
    // has some free channels?
    if (cbv == 0xffffffffu) continue; // nope
    int cidx = bidx*32;
    for (vuint32 mask = 0x80000000u; mask; mask >>= 1, ++cidx) {
      if ((cbv&mask) == 0) {
        ChanBitmap[bidx] |= mask;
        ++ChanUsed;
        return cidx;
      }
    }
    abort(); // we should never come here
  }
  // we should never come here
  vassert(ChanUsed >= NumChannels);
  //memset((void *)&Channel[cidx], 0, sizeof(FChannel));
  //Channel[cidx].handle = -1;
  return -1;
}


//==========================================================================
//
//  VAudio::DeallocChannel
//
//==========================================================================
void VAudio::DeallocChannel (int cidx) {
  if (ChanUsed == 0) return; // wtf?!
  if (cidx < 0 || cidx >= NumChannels) return; // oops
  const int bidx = cidx/32;
  const vuint32 mask = 0x80000000u>>(cidx%32);
  const vuint32 cbv = ChanBitmap[bidx];
  if (cbv&mask) {
    // allocated channel, free it
    ChanBitmap[bidx] ^= mask;
    --ChanUsed;
    vassert(Channel[cidx].handle == -1);
    // just in case
    Channel[cidx].handle = -1;
    Channel[cidx].origin_id = 0;
    Channel[cidx].sound_id = 0;
  }
}


//==========================================================================
//
//  VAudio::Init
//
//  initialises sound stuff, including volume
//  sets channels, SFX and music volume, allocates channel buffer
//
//==========================================================================
void VAudio::Init () {
  // initialise sound driver
  if (!cli_NoSound) {
    SoundDevice = new VOpenALDevice();
    if (!SoundDevice->Init()) {
      delete SoundDevice;
      SoundDevice = nullptr;
    }
  }

  // initialise stream music player
  if (SoundDevice && !cli_NoMusic) {
    StreamMusicPlayer = new VStreamMusicPlayer(SoundDevice);
    StreamMusicPlayer->Init();
  }

  MaxSoundDist = 4096;
  MaxVolume = -1;

  // free all channels for use
  ResetAllChannels();
  NumChannels = (SoundDevice ? SoundDevice->SetChannels(snd_channels) : 0);
}


//==========================================================================
//
//  VAudio::Shutdown
//
//  Shuts down all sound stuff
//
//==========================================================================
void VAudio::Shutdown () {
  // stop playback of all sounds
  if (StreamMusicPlayer) {
    //if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): shutting down music player...");
    //StreamMusicPlayer->Shutdown();
    if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): deleting music player...");
    StreamMusicPlayer->Shutdown();
    delete StreamMusicPlayer;
    StreamMusicPlayer = nullptr;
  }
  if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): stopping sequences...");
  StopAllSequences();
  if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): stopping sounds...");
  StopAllSound();
  if (SoundDevice) {
    //if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): shutting down sound device...");
    //SoundDevice->Shutdown();
    if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): deleting sound device...");
    delete SoundDevice;
    SoundDevice = nullptr;
  }
  if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): resetting all channels...");
  ResetAllChannels();
  if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): shutdown complete!");
}


//==========================================================================
//
//  VAudio::PlaySound
//
//  this function adds a sound to the list of currently active sounds, which
//  is maintained as a given number of internal channels
//
//  channel 0 is "CHAN_AUTO"
//
//==========================================================================
void VAudio::PlaySound (int InSoundId, const TVec &origin, const TVec &velocity,
                        int origin_id, int channel, float volume, float Attenuation, bool Loop)
{
  if (!SoundDevice || !InSoundId || !MaxVolume || !volume || NumChannels < 1) return;

  // find actual sound ID to use
  int sound_id = GSoundManager->ResolveSound(InSoundId);

  if (sound_id < 1 || sound_id >= GSoundManager->S_sfx.length()) return; // k8: just in case
  if (GSoundManager->S_sfx[sound_id].VolumeAmp <= 0) return; // nothing to see here, come along

  // if it's a looping sound and it's still playing, then continue playing the existing one
  FOR_EACH_CHANNEL(i) {
    if (Channel[i].origin_id == origin_id && Channel[i].channel == channel &&
        Channel[i].sound_id == sound_id && Channel[i].Loop)
    {
      return;
    }
  }

  // apply sound volume
  volume *= MaxVolume;
  // apply $volume
  volume *= GSoundManager->S_sfx[sound_id].VolumeAmp;
  if (volume <= 0) return; // nothing to see here, come along

  // check if this sound is emited by the local player
  bool LocalPlayerSound = (origin_id == -666 || origin_id == 0 || (cl && cl->MO && cl->MO->SoundOriginID == origin_id));

  // calculate the distance before other stuff so that we can throw out sounds that are beyond the hearing range
  int dist = 0;
  if (origin_id && !LocalPlayerSound && Attenuation > 0 && cl) dist = (int)(Length(origin-cl->ViewOrg)*Attenuation);
  //GCon->Logf("DISTANCE=%d", dist);
  if (dist >= MaxSoundDist) {
    //GCon->Logf("  too far away (%d)", MaxSoundDist);
    return; // sound is beyond the hearing range
  }

  int priority = GSoundManager->S_sfx[sound_id].Priority*(PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist);

  int chan = GetChannel(sound_id, origin_id, channel, priority);
  if (chan == -1) return; // no free channels

  if (developer) {
    if (cli_DebugSound > 0) GCon->Logf(NAME_Dev, "PlaySound: sound(%d)='%s'; origin_id=%d; channel=%d; chan=%d", sound_id, *GSoundManager->S_sfx[sound_id].TagName, origin_id, channel, chan);
  }

  float pitch = 1.0f;
  if (GSoundManager->S_sfx[sound_id].ChangePitch) {
    pitch = 1.0f+(RandomFull()-RandomFull())*(GSoundManager->S_sfx[sound_id].ChangePitch*snd_random_pitch_boost);
    //fprintf(stderr, "SND0: randompitched to %f\n", pitch);
  } else if (!LocalPlayerSound) {
    float rpt = snd_random_pitch;
    if (rpt > 0) {
      if (rpt > 1) rpt = 1;
      pitch = 1.0f+(RandomFull()-RandomFull())*(rpt*snd_random_pitch_boost);
      //fprintf(stderr, "SND1: randompitched to %f\n", pitch);
    }
  }

  int handle;
  bool is3D;
  if (!origin_id || LocalPlayerSound || Attenuation <= 0) {
    // local sound
    handle = SoundDevice->PlaySound(sound_id, volume, pitch, Loop);
    is3D = false;
  } else {
    handle = SoundDevice->PlaySound3D(sound_id, origin, velocity, volume, pitch, Loop);
    is3D = true;
  }
  Channel[chan].origin_id = origin_id;
  Channel[chan].channel = channel;
  Channel[chan].origin = origin;
  Channel[chan].velocity = velocity;
  Channel[chan].sound_id = sound_id;
  Channel[chan].priority = priority;
  Channel[chan].volume = volume;
  Channel[chan].Attenuation = Attenuation;
  Channel[chan].handle = handle;
  Channel[chan].is3D = is3D;
  Channel[chan].LocalPlayerSound = LocalPlayerSound;
  Channel[chan].Loop = Loop;
}


//==========================================================================
//
//  VAudio::GetChannel
//
//  channel 0 is "CHAN_AUTO"
//
//==========================================================================
int VAudio::GetChannel (int sound_id, int origin_id, int channel, int priority) {
  const int numchannels = GSoundManager->S_sfx[sound_id].NumChannels;

  // first, look if we want to replace sound on some channel
  if (channel != 0) {
    FOR_EACH_CHANNEL(i) {
      if (Channel[i].origin_id == origin_id && Channel[i].channel == channel) {
        StopChannel(i);
        return i;
      }
    }
  }

  if (numchannels > 0) {
    int lp = -1; // least priority
    int found = 0;
    int prior = priority;
    FOR_EACH_CHANNEL(i) {
      if (Channel[i].sound_id == sound_id) {
        if (GSoundManager->S_sfx[sound_id].bSingular) {
          // this sound is already playing, so don't start it again
          return -1;
        }
        ++found; // found one; now, should we replace it?
        if (prior >= Channel[i].priority) {
          // if we're gonna kill one, then this will be it
          lp = i;
          prior = Channel[i].priority;
        }
      }
    }

    if (found >= numchannels) {
      if (lp == -1) {
        // other sounds have greater priority
        return -1; // don't replace any sounds
      }
      StopChannel(lp);
      return lp;
    }
  }

  // get a free channel, if there is any
  if (ChanUsed < NumChannels) return AllocChannel();
  if (NumChannels < 1) return -1;

  // look for a lower priority sound to replace
  int lowestlp = -1;
  int lowestprio = priority;
  FOR_EACH_CHANNEL(i) {
    if (lowestprio > Channel[i].priority) {
      lowestlp = i;
      lowestprio = Channel[i].priority;
    } else if (Channel[i].priority == lowestprio) {
      if (lowestlp < 0 || Channel[lowestlp].origin_id == Channel[i].origin_id) {
        lowestlp = i;
      }
    }
  }
  if (lowestlp < 0) return -1; // no free channels

  // replace the lower priority sound
  StopChannel(lowestlp);
  return lowestlp;
}


//==========================================================================
//
//  VAudio::StopChannel
//
//==========================================================================
void VAudio::StopChannel (int cidx) {
  if (cidx < 0 || cidx >= NumChannels) return;
  if (Channel[cidx].sound_id || Channel[cidx].handle >= 0) {
    SoundDevice->StopChannel(Channel[cidx].handle);
    Channel[cidx].handle = -1;
    Channel[cidx].origin_id = 0;
    Channel[cidx].sound_id = 0;
  }
}


//==========================================================================
//
//  VAudio::StopSound
//
//==========================================================================
void VAudio::StopSound (int origin_id, int channel) {
  FOR_EACH_CHANNEL(i) {
    if (Channel[i].origin_id == origin_id && (!channel || Channel[i].channel == channel)) {
      StopChannel(i);
      DeallocChannel(i);
    }
  }
}


//==========================================================================
//
//  VAudio::StopAllSound
//
//==========================================================================
void VAudio::StopAllSound () {
  // stop all sounds
  FOR_EACH_CHANNEL(i) StopChannel(i);
  ResetAllChannels();
}


//==========================================================================
//
//  VAudio::IsSoundPlaying
//
//==========================================================================
bool VAudio::IsSoundPlaying (int origin_id, int InSoundId) {
  int sound_id = GSoundManager->ResolveSound(InSoundId);
  FOR_EACH_CHANNEL(i) {
    if (Channel[i].sound_id == sound_id &&
        Channel[i].origin_id == origin_id &&
        SoundDevice->IsChannelPlaying(Channel[i].handle))
    {
      return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VAudio::StartSequence
//
//==========================================================================
void VAudio::StartSequence (int OriginId, const TVec &Origin, VName Name, int ModeNum) {
  int Idx = GSoundManager->FindSequence(Name);
  if (Idx != -1) {
    StopSequence(OriginId); // stop any previous sequence
    new VSoundSeqNode(OriginId, Origin, Idx, ModeNum);
  }
}


//==========================================================================
//
//  VAudio::AddSeqChoice
//
//==========================================================================
void VAudio::AddSeqChoice (int OriginId, VName Name) {
  int Idx = GSoundManager->FindSequence(Name);
  if (Idx == -1) return;
  for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next) {
    if (node->OriginId == OriginId) {
      node->SeqChoices.Append(Idx);
      return;
    }
  }
}


//==========================================================================
//
//  VAudio::StopSequence
//
//==========================================================================
void VAudio::StopSequence (int origin_id) {
  VSoundSeqNode *node = SequenceListHead;
  while (node) {
    VSoundSeqNode *next = node->Next;
    if (node->OriginId == origin_id) delete node; // this should exclude node from list
    node = next;
  }
}


//==========================================================================
//
//  VAudio::UpdateActiveSequences
//
//==========================================================================
void VAudio::UpdateActiveSequences (float DeltaTime) {
  if (!ActiveSequences || GGameInfo->IsPaused() || !cl) {
    // no sequences currently playing/game is paused or there's no player in the map
    return;
  }
  //k8: no simple loop, 'cause sequence can delete itself
  VSoundSeqNode *node = SequenceListHead;
  while (node) {
    VSoundSeqNode *next = node->Next;
    node->Update(DeltaTime);
    node = next;
  }
}


//==========================================================================
//
//  VAudio::StopAllSequences
//
//==========================================================================
void VAudio::StopAllSequences () {
  //k8: no simple loop, 'cause sequence can delete itself
  VSoundSeqNode *node = SequenceListHead;
  while (node) {
    VSoundSeqNode *next = node->Next;
    node->StopSound = 0; // don't play any stop sounds
    delete node;
    node = next;
  }
}


//==========================================================================
//
//  VAudio::SerialiseSounds
//
//==========================================================================
void VAudio::SerialiseSounds (VStream &Strm) {
  if (Strm.IsLoading()) {
    // reload and restart all sound sequences
    vint32 numSequences = Streamer<vint32>(Strm);
    for (int i = 0; i < numSequences; ++i) {
      new VSoundSeqNode(0, TVec(0, 0, 0), -1, 0);
    }
    VSoundSeqNode *node = SequenceListHead;
    for (int i = 0; i < numSequences; i++, node = node->Next) {
      node->Serialise(Strm);
    }
  } else {
    // save the sound sequences
    Strm << ActiveSequences;
    for (VSoundSeqNode *node = SequenceListHead; node; node = node->Next) {
      node->Serialise(Strm);
    }
  }
}


//==========================================================================
//
//  VAudio::UpdateSfx
//
//  update the sound parameters
//  used to control volume and pan changes such as when a player turns
//
//==========================================================================
void VAudio::UpdateSfx () {
  if (!SoundDevice || NumChannels <= 0) return;

  if (snd_sfx_volume != MaxVolume) {
    MaxVolume = snd_sfx_volume;
    if (!MaxVolume) StopAllSound();
  }

  if (!MaxVolume) return; // silence

  if (cl) AngleVectors(cl->ViewAngles, ListenerForward, ListenerRight, ListenerUp);

  FOR_EACH_CHANNEL(i) {
    // active channel?
    if (!Channel[i].sound_id) {
      vassert(Channel[i].handle == -1);
      DeallocChannel(i);
      continue;
    }

    // still playing?
    if (!SoundDevice->IsChannelPlaying(Channel[i].handle)) {
      StopChannel(i);
      DeallocChannel(i);
      continue;
    }

    // full volume sound?
    if (!Channel[i].origin_id || Channel[i].Attenuation <= 0) continue;

    // client sound?
    if (Channel[i].LocalPlayerSound) continue;

    // move sound
    Channel[i].origin += Channel[i].velocity*host_frametime;

    if (!cl) continue;

    int dist = (int)(Length(Channel[i].origin-cl->ViewOrg)*Channel[i].Attenuation);
    if (dist >= MaxSoundDist) {
      // too far away
      StopChannel(i);
      DeallocChannel(i);
      continue;
    }

    // update params
    if (Channel[i].is3D) SoundDevice->UpdateChannel3D(Channel[i].handle, Channel[i].origin, Channel[i].velocity);
    Channel[i].priority = GSoundManager->S_sfx[Channel[i].sound_id].Priority*(PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist);
  }

  if (cl) {
    SoundDevice->UpdateListener(cl->ViewOrg, TVec(0, 0, 0),
      ListenerForward, ListenerRight, ListenerUp
#if defined(VAVOOM_REVERB)
      , GSoundManager->FindEnvironment(cl->SoundEnvironment)
#endif
      );
  }

  //SoundDevice->Tick(host_frametime);
}


//==========================================================================
//
//  VAudio::StartSong
//
//==========================================================================
void VAudio::StartSong (VName song, bool loop) {
  if (loop) {
    GCmdBuf << "Music Loop " << *VStr(song).quote() << "\n";
  } else {
    GCmdBuf << "Music Play " << *VStr(song).quote() << "\n";
  }
}


//==========================================================================
//
//  VAudio::PauseSound
//
//==========================================================================
void VAudio::PauseSound () {
  GCmdBuf << "Music Pause\n";
}


//==========================================================================
//
//  VAudio::ResumeSound
//
//==========================================================================
void VAudio::ResumeSound () {
  GCmdBuf << "Music resume\n";
}


//==========================================================================
//
//  VAudio::StartMusic
//
//==========================================================================
void VAudio::StartMusic () {
  StartSong(MapSong, true);
}


//==========================================================================
//
//  VAudio::Start
//
//  per level startup code
//  kills playing sounds at start of level, determines music if any,
//  changes music
//
//==========================================================================
void VAudio::Start () {
  StopAllSequences();
  StopAllSound();
}


//==========================================================================
//
//  VAudio::MusicChanged
//
//==========================================================================
void VAudio::MusicChanged () {
  MapSong = GClLevel->LevelInfo->SongLump;
  StartMusic();
}


//==========================================================================
//
//  VAudio::UpdateSounds
//
//  Updates music & sounds
//
//==========================================================================
void VAudio::UpdateSounds () {
  // check sound volume
  if (snd_sfx_volume < 0.0f) snd_sfx_volume = 0.0f;
  if (snd_sfx_volume > 1.0f) snd_sfx_volume = 1.0f;

  // check music volume
  if (snd_music_volume < 0.0f) snd_music_volume = 0.0f;
  if (snd_music_volume > 1.0f) snd_music_volume = 1.0f;

  // update any Sequences
  UpdateActiveSequences(host_frametime);

  UpdateSfx();
  if (StreamMusicPlayer) {
    SoundDevice->SetStreamVolume(snd_music_volume*MusicVolumeFactor);
    //StreamMusicPlayer->Tick(host_frametime);
  }
}


//==========================================================================
//
//  FindMusicLump
//
//==========================================================================
static int FindMusicLump (const char *songName) {
  static const char *Exts[] = {
    "opus", "ogg", "flac", "mp3", "wav",
    "mid", "mus",
    "mod", "xm", "it", "s3m", "stm",
    //"669", "amf", "dsm", "far", "gdm", "imf", "it", "m15", "med", "mod", "mtm",
    //"okt", "s3m", "stm", "stx", "ult", "uni", "xm", "flac", "ay", "gbs",
    //"gym", "hes", "kss", "nsf", "nsfe", "sap", "sgc", "spc", "vgm",
    nullptr
  };

  if (!songName || !songName[0] || VStr::ICmp(songName, "none") == 0) return -1;
  int Lump = -1;
  VName sn8 = VName(songName, VName::FindLower8);
  if (sn8 != NAME_None) Lump = W_CheckNumForName(sn8, WADNS_Music);
  // if there is no 8-char lump, try harder
  if (Lump < 0) {
    Lump = max2(Lump, W_FindLumpByFileNameWithExts(va("music/%s", songName), Exts));
    Lump = max2(Lump, W_CheckNumForFileName(songName));
  }
  return Lump;
}


//==========================================================================
//
//  VAudio::LoadSongInternal
//
//==========================================================================
VAudioCodec *VAudio::LoadSongInternal (const char *Song, bool wasPlaying) {
  static const char *ExtraExts[] = { "opus", "ogg", "flac", "mp3", nullptr };

  if (!Song || !Song[0]) return nullptr;

  if ((Song[0] == '*' && !Song[1]) ||
      VStr::strEquCI(Song, "none") ||
      VStr::strEquCI(Song, "null"))
  {
    // this looks like common "stop song" action
    if (wasPlaying) GCon->Log("stopped current song");
    return nullptr;
  }

#if 0
  // this is done in mapinfo
  VStr xsong;
  if (Song[0] == '$') {
    VName sn = VName(Song+1, VName::FindLower);
    if (sn != NAME_None) {
      xsong = GLanguage[sn];
      if (!xsong.strEquCI(Song+1)) Song = *xsong;
    }
  }
#endif

  // find the song
  int Lump = -1;
  if (snd_external_music) {
    // check external music definition file
    //TODO: cache this!
    VStream *XmlStrm = FL_OpenFileRead("extras/music/remap.xml");
    if (XmlStrm) {
      VXmlDocument *Doc = new VXmlDocument();
      Doc->Parse(*XmlStrm, "extras/music/remap.xml");
      delete XmlStrm;
      XmlStrm = nullptr;
      for (VXmlNode *N = Doc->Root.FirstChild; N; N = N->NextSibling) {
        if (N->Name != "song") continue;
        if (!N->GetAttribute("name").strEquCI(Song)) continue;
        VStr fname = N->GetAttribute("file");
        if (fname.length() == 0 || fname.strEquCI("none")) {
          delete Doc;
          return nullptr;
        }
        Lump = W_CheckNumForFileName(fname);
        if (Lump >= 0) break;
      }
      delete Doc;
    }
    // also try OGG or MP3 directly
    if (Lump < 0) Lump = W_FindLumpByFileNameWithExts(va("extras/music/%s", Song), ExtraExts);
  }

  if (Lump < 0) {
    // get the lump that comes last
    Lump = FindMusicLump(Song);
    // look for replacement
    VSoundManager::VMusicAlias *mal = nullptr;
    for (int f = GSoundManager->MusicAliases.length()-1; f >= 0; --f) {
      if (VStr::ICmp(*GSoundManager->MusicAliases[f].origName, Song) == 0) {
        mal = &GSoundManager->MusicAliases[f];
        break;
      }
    }
    if (mal && (Lump < 0 || mal->fileid >= W_LumpFile(Lump))) {
      if (mal->newName == NAME_None) return nullptr; // replaced with nothing
      int l2 = FindMusicLump(*mal->newName);
      if (Lump < 0 || (l2 >= 0 && mal->fileid >= W_LumpFile(l2))) {
        Lump = l2;
        Song = *mal->newName;
      }
    }
  }

  if (Lump < 0) {
    GCon->Logf(NAME_Warning, "Can't find song \"%s\"", Song);
    return nullptr;
  }

  // get music volume for this song
  MusicVolumeFactor = GSoundManager->GetMusicVolume(Song);
  if (StreamMusicPlayer) SoundDevice->SetStreamVolume(snd_music_volume*MusicVolumeFactor);

  VStream *Strm = W_CreateLumpReaderNum(Lump);
  if (Strm->TotalSize() < 4) {
    GCon->Logf(NAME_Warning, "Lump '%s' for song \"%s\" is too small (%d)", *W_FullLumpName(Lump), Song, Strm->TotalSize());
    delete Strm;
    return nullptr;
  }

  vuint8 Hdr[4];
  Strm->Serialise(Hdr, 4);
  if (!memcmp(Hdr, MUSMAGIC, 4)) {
    // convert mus to mid with a wonderfull function
    // thanks to S.Bacquet for the source of qmus2mid
    Strm->Seek(0);
    VMemoryStream *MidStrm = new VMemoryStream();
    MidStrm->BeginWrite();
    VQMus2Mid Conv;
    int MidLength = Conv.Run(*Strm, *MidStrm);
    delete Strm;
    if (!MidLength) {
      delete MidStrm;
      return nullptr;
    }
    MidStrm->Seek(0);
    MidStrm->BeginRead();
    Strm = MidStrm;
  }

  // try to create audio codec
  const char *codecName = nullptr;
  VAudioCodec *Codec = nullptr;
  for (FAudioCodecDesc *Desc = FAudioCodecDesc::List; Desc && !Codec; Desc = Desc->Next) {
    //GCon->Logf(va("Using %s to open the stream", Desc->Description));
    Codec = Desc->Creator(Strm);
    if (Codec) codecName = Desc->Description;
  }

  if (Codec) {
    GCon->Logf("starting song '%s' with codec '%s'", *W_FullLumpName(Lump), codecName);
    // start playing streamed music
    //StreamMusicPlayer->Play(Codec, Song, Loop);
    return Codec;
  }

  GCon->Logf(NAME_Warning, "couldn't find codec for song '%s'", *W_FullLumpName(Lump));
  delete Strm;
  return nullptr;
}


//==========================================================================
//
//  VAudio::PlaySong
//
//==========================================================================
void VAudio::PlaySong (const char *Song, bool Loop) {
  if (!Song || !Song[0] || !StreamMusicPlayer) return;

  if (snd_music_background_load) {
    StreamMusicPlayer->LoadAndPlay(Song, Loop);
  } else {
    bool wasPlaying = StreamMusicPlayer->IsPlaying();
    if (wasPlaying) StreamMusicPlayer->Stop();

    VAudioCodec *Codec = LoadSongInternal(Song, wasPlaying);

    if (Codec && StreamMusicPlayer) {
      StreamMusicPlayer->Play(Codec, Song, Loop);
    }
  }
}


//==========================================================================
//
//  VAudio::NotifySoundLoaded
//
//==========================================================================
void VAudio::NotifySoundLoaded (int sound_id, bool success) {
  if (SoundDevice) SoundDevice->NotifySoundLoaded(sound_id, success);
}


//==========================================================================
//
//  VAudio::CmdMusic
//
//==========================================================================
void VAudio::CmdMusic (const TArray<VStr> &Args) {
  if (!StreamMusicPlayer) return;

  if (Args.Num() < 2) return;

  VStr command = Args[1];

  if (command.ICmp("on") == 0) {
    MusicEnabled = true;
    return;
  }

  if (command.ICmp("off") == 0) {
    if (StreamMusicPlayer) StreamMusicPlayer->Stop();
    MusicEnabled = false;
    return;
  }

  if (!MusicEnabled) return;

  if (command.ICmp("play") == 0) {
    if (Args.Num() < 3) {
      GCon->Log(NAME_Warning, "Please enter name of the song (play).");
      return;
    }
    PlaySong(*Args[2].ToLower(), false);
    return;
  }

  if (command.ICmp("loop") == 0) {
    if (Args.Num() < 3) {
      GCon->Log(NAME_Warning, "Please enter name of the song (loop).");
      return;
    }
    PlaySong(*Args[2].ToLower(), true);
    return;
  }

  if (command.ICmp("pause") == 0) {
    StreamMusicPlayer->Pause();
    return;
  }

  if (command.ICmp("resume") == 0) {
    StreamMusicPlayer->Resume();
    return;
  }

  if (command.ICmp("stop") == 0) {
    StreamMusicPlayer->Stop();
    return;
  }

  if (command.ICmp("info") == 0) {
    if (StreamMusicPlayer->IsPlaying()) {
      GCon->Logf("Currently %s %s.", (StreamMusicPlayer->CurrLoop ? "looping" : "playing"), *StreamMusicPlayer->CurrSong);
    } else {
      GCon->Log("No song currently playing");
    }
    return;
  }
}


//==========================================================================
//
//  VSoundSeqNode::VSoundSeqNode
//
//==========================================================================
VSoundSeqNode::VSoundSeqNode (int AOriginId, const TVec &AOrigin, int ASequence, int AModeNum)
  : Sequence(ASequence)
  , SequencePtr(nullptr)
  , OriginId(AOriginId)
  , Origin(AOrigin)
  , CurrentSoundID(0)
  , DelayTime(0.0f)
  , Volume(1.0f) // Start at max volume
  , Attenuation(1.0f)
  , StopSound(0)
  , DidDelayOnce(0)
  , ModeNum(AModeNum)
  , Prev(nullptr)
  , Next(nullptr)
  , ParentSeq(nullptr)
  , ChildSeq(nullptr)
{
  if (Sequence >= 0) {
    SequencePtr = GSoundManager->SeqInfo[Sequence].Data;
    StopSound = GSoundManager->SeqInfo[Sequence].StopSound;
  }
  // add to the list of sound sequences
  if (!((VAudio *)GAudio)->SequenceListHead) {
    ((VAudio *)GAudio)->SequenceListHead = this;
  } else {
    ((VAudio *)GAudio)->SequenceListHead->Prev = this;
    Next = ((VAudio *)GAudio)->SequenceListHead;
    ((VAudio *)GAudio)->SequenceListHead = this;
  }
  ++((VAudio *)GAudio)->ActiveSequences;
}


//==========================================================================
//
//  VSoundSeqNode::~VSoundSeqNode
//
//==========================================================================
VSoundSeqNode::~VSoundSeqNode () {
  if (ParentSeq && ParentSeq->ChildSeq == this) {
    // re-activate parent sequence
    if (ParentSeq->SequencePtr) ++ParentSeq->SequencePtr;
    ParentSeq->ChildSeq = nullptr;
    ParentSeq = nullptr;
  }

  if (ChildSeq) {
    delete ChildSeq;
    ChildSeq = nullptr;
  }

  // play stop sound
  if (GAudio) {
    if (StopSound >= 0) ((VAudio *)GAudio)->StopSound(OriginId, 0);
    if (StopSound >= 1) ((VAudio *)GAudio)->PlaySound(StopSound, Origin, TVec(0, 0, 0), OriginId, 1, Volume, Attenuation, false);

    // remove from the list of active sound sequences
    if (((VAudio*)GAudio)->SequenceListHead == this) ((VAudio *)GAudio)->SequenceListHead = Next;
    if (Prev) Prev->Next = Next;
    if (Next) Next->Prev = Prev;

    --((VAudio *)GAudio)->ActiveSequences;
  }
}


//==========================================================================
//
//  VSoundSeqNode::Update
//
//==========================================================================
void VSoundSeqNode::Update (float DeltaTime) {
  if (DelayTime) {
    DelayTime -= DeltaTime;
    if (DelayTime <= 0.0f) DelayTime = 0.0f;
    return;
  }

  if (!SequencePtr) {
    delete this;
    return;
  }

  bool sndPlaying = GAudio->IsSoundPlaying(OriginId, CurrentSoundID);
  switch (*SequencePtr) {
    case SSCMD_None:
      ++SequencePtr;
      break;
    case SSCMD_Play:
      if (!sndPlaying) {
        CurrentSoundID = SequencePtr[1];
        GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0), OriginId, 1, Volume, Attenuation, false);
      }
      SequencePtr += 2;
      break;
    case SSCMD_WaitUntilDone:
      if (!sndPlaying) {
        ++SequencePtr;
        CurrentSoundID = 0;
      }
      break;
    case SSCMD_PlayRepeat:
      if (!sndPlaying) {
        CurrentSoundID = SequencePtr[1];
        GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0), OriginId, 1, Volume, Attenuation, false);
      }
      break;
    case SSCMD_PlayLoop:
      CurrentSoundID = SequencePtr[1];
      GAudio->PlaySound(CurrentSoundID, Origin, TVec(0, 0, 0), OriginId, 1, Volume, Attenuation, false);
      DelayTime = SequencePtr[2]/35.0f;
      break;
    case SSCMD_Delay:
      DelayTime = SequencePtr[1]/35.0f;
      SequencePtr += 2;
      CurrentSoundID = 0;
      break;
    case SSCMD_DelayOnce:
      if (!(DidDelayOnce&(1<<SequencePtr[2]))) {
        DidDelayOnce |= 1<<SequencePtr[2];
        DelayTime = SequencePtr[1]/35.0f;
        CurrentSoundID = 0;
      }
      SequencePtr += 3;
      break;
    case SSCMD_DelayRand:
      DelayTime = (SequencePtr[1]+GenRandomU31()%(SequencePtr[2]-SequencePtr[1]))/35.0f;
      SequencePtr += 3;
      CurrentSoundID = 0;
      break;
    case SSCMD_Volume:
      Volume = SequencePtr[1]/10000.0f;
      SequencePtr += 2;
      break;
    case SSCMD_VolumeRel:
      Volume += SequencePtr[1]/10000.0f;
      SequencePtr += 2;
      break;
    case SSCMD_VolumeRand:
      Volume = (SequencePtr[1]+GenRandomU31()%(SequencePtr[2]-SequencePtr[1]))/10000.0f;
      SequencePtr += 3;
      break;
    case SSCMD_Attenuation:
      Attenuation = SequencePtr[1];
      SequencePtr += 2;
      break;
    case SSCMD_RandomSequence:
      if (SeqChoices.Num() == 0) {
        ++SequencePtr;
      } else if (!ChildSeq) {
        int Choice = GenRandomU31()%SeqChoices.Num();
        ChildSeq = new VSoundSeqNode(OriginId, Origin, SeqChoices[Choice], ModeNum);
        ChildSeq->ParentSeq = this;
        ChildSeq->Volume = Volume;
        ChildSeq->Attenuation = Attenuation;
        return;
      } else {
        // waiting for child sequence to finish
        return;
      }
      break;
    case SSCMD_Branch:
      SequencePtr -= SequencePtr[1];
      break;
    case SSCMD_Select:
      {
        // transfer sequence to the one matching the ModeNum
        int NumChoices = SequencePtr[1];
        int i;
        for (i = 0; i < NumChoices; ++i) {
          if (SequencePtr[2+i*2] == ModeNum) {
            int Idx = GSoundManager->FindSequence(*(VName *)&SequencePtr[3+i*2]);
            if (Idx != -1) {
              Sequence = Idx;
              SequencePtr = GSoundManager->SeqInfo[Sequence].Data;
              StopSound = GSoundManager->SeqInfo[Sequence].StopSound;
              break;
            }
          }
        }
        if (i == NumChoices) SequencePtr += 2+NumChoices; // not found
      }
      break;
    case SSCMD_StopSound:
      // wait until something else stops the sequence
      break;
    case SSCMD_End:
      delete this;
      break;
    default:
      break;
  }
}


//==========================================================================
//
//  VSoundSeqNode::Serialise
//
//==========================================================================
void VSoundSeqNode::Serialise (VStream &Strm) {
  vuint8 xver = 0; // current version is 0
  Strm << xver;
  Strm << STRM_INDEX(Sequence)
    << STRM_INDEX(OriginId)
    << Origin
    << STRM_INDEX(CurrentSoundID)
    << DelayTime
    << STRM_INDEX(DidDelayOnce)
    << Volume
    << Attenuation
    << STRM_INDEX(ModeNum);

  if (Strm.IsLoading()) {
    vint32 Offset;
    Strm << STRM_INDEX(Offset);
    SequencePtr = GSoundManager->SeqInfo[Sequence].Data+Offset;
    StopSound = GSoundManager->SeqInfo[Sequence].StopSound;

    vint32 Count;
    Strm << STRM_INDEX(Count);
    for (int i = 0; i < Count; ++i) {
      VName SeqName;
      Strm << SeqName;
      SeqChoices.Append(GSoundManager->FindSequence(SeqName));
    }

    vint32 ParentSeqIdx;
    vint32 ChildSeqIdx;
    Strm << STRM_INDEX(ParentSeqIdx) << STRM_INDEX(ChildSeqIdx);
    if (ParentSeqIdx != -1 || ChildSeqIdx != -1) {
      int i = 0;
      for (VSoundSeqNode *n = ((VAudio*)GAudio)->SequenceListHead; n; n = n->Next, ++i) {
        if (ParentSeqIdx == i) ParentSeq = n;
        if (ChildSeqIdx == i) ChildSeq = n;
      }
    }
  } else {
    vint32 Offset = SequencePtr - GSoundManager->SeqInfo[Sequence].Data;
    Strm << STRM_INDEX(Offset);

    vint32 Count = SeqChoices.Num();
    Strm << STRM_INDEX(Count);
    for (int i = 0; i < SeqChoices.Num(); ++i) Strm << GSoundManager->SeqInfo[SeqChoices[i]].Name;

    vint32 ParentSeqIdx = -1;
    vint32 ChildSeqIdx = -1;
    if (ParentSeq || ChildSeq) {
      int i = 0;
      for (VSoundSeqNode *n = ((VAudio*)GAudio)->SequenceListHead; n; n = n->Next, ++i) {
        if (ParentSeq == n) ParentSeqIdx = i;
        if (ChildSeq == n) ChildSeqIdx = i;
      }
    }
    Strm << STRM_INDEX(ParentSeqIdx) << STRM_INDEX(ChildSeqIdx);
  }
}


//==========================================================================
//
//  COMMAND Music
//
//==========================================================================
COMMAND_WITH_AC(Music) {
  if (GAudio) ((VAudio *)GAudio)->CmdMusic(Args);
}


//==========================================================================
//
//  COMMAND_AC Music
//
//==========================================================================
COMMAND_AC(Music) {
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    list.append("info");
    list.append("loop");
    list.append("off");
    list.append("on");
    list.append("pause");
    list.append("play");
    list.append("resume");
    list.append("stop");
    return AutoCompleteFromList(prefix, list, true); // return unchanged as empty
  } else {
    return VStr::EmptyString;
  }
}


//==========================================================================
//
//  COMMAND CD
//
//==========================================================================
COMMAND(CD) {
}
