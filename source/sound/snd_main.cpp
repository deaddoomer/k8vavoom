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
#include "../gamedefs.h"
#include "../host.h"  /* host_frametime */
#include "../psim/p_entity.h"
#include "../psim/p_levelinfo.h"
#include "../psim/p_player.h"
#include "../client/client.h"
#include "sound.h"
#include "snd_local.h"


static int cli_NoSound = 0;
static int cli_NoSfx = 0;
static int cli_NoMusic = 0;
int cli_DebugSound = 0;


/*static*/ bool cliRegister_sndmain_args =
  VParsedArgs::RegisterFlagSet("-nosound", "disable all sound (including music)", &cli_NoSound) &&
  VParsedArgs::RegisterAlias("-no-sound", "-nosound") &&
  VParsedArgs::RegisterFlagSet("-nomusic", "disable music", &cli_NoMusic) &&
  VParsedArgs::RegisterAlias("-no-music", "-nomusic") &&
  VParsedArgs::RegisterFlagSet("-nosfx", "disable sound effectx", &cli_NoSfx) &&
  VParsedArgs::RegisterAlias("-no-sfx", "-nosfx") &&
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
  virtual void StopAllSound (bool keepLastSwitch=false) override;
  virtual bool IsSoundPlaying (int origin_id, int InSoundId) override;
  virtual void MoveSounds (int origin_id, const TVec &neworigin) override;

  // music and general sound control
  virtual void StartSong (VName song, bool loop, bool allowRandom) override;
  virtual void PauseSound () override;
  virtual void ResumeSound () override;
  virtual void Start () override;
  virtual void MusicChanged (bool allowRandom) override;
  virtual void UpdateSounds () override;

  // sound sequences
  virtual void StartSequence (int OriginId, const TVec &Origin, VName Name, int ModeNum) override;
  virtual void AddSeqChoice (int OriginId, VName Name) override;
  virtual void StopSequence (int origin_id) override;
  virtual void UpdateActiveSequences (float DeltaTime) override;
  virtual void StopAllSequences () override;
  virtual void SerialiseSounds (VStream &Strm) override;

  // returns codec or nullptr
  virtual VAudioCodec *LoadSongInternal (const char *Song, bool wasPlaying, bool fromStreamThread) override;

  virtual bool IsMusicAvailable () override;
  virtual bool IsMusicPlaying () override;
  virtual void ResetMusicLoopCounter () override;
  virtual int GetMusicLoopCounter () override;
  virtual void IncMusicLoopCounter () override;

  virtual int GetResamplerCount () const noexcept override;
  virtual VStr GetResamplerName (int idx) const noexcept override;
  virtual int GetDefaultResampler () const noexcept override;

public:
  // console variables
  static VCvarF snd_sfx_volume;
  static VCvarF snd_music_volume;
  //static VCvarB snd_swap_stereo;
  static VCvarI snd_channels;
  static VCvarB snd_external_music;

private:
  enum { MAX_CHANNELS = 256 };

  // info about sounds currently playing
  // sounds with the highest `priority` will be considered for replacing
  struct FChannel {
    int origin_id; // <=0: full-volume local sound
    int channel;
    TVec origin;
    TVec prevOrigin; // do not update source when it isn't moving
#ifdef VV_SND_ALLOW_VELOCITY
    TVec velocity; // unused
    TVec prevVelocity; // do not update source when it isn't moving
#endif
    int sound_id;
    float priority; // the higher priority is worser; this is dynamically adjusted using base sound priority and distance from the listener
    float volume;
    float Attenuation;
    int handle;
    bool is3D;
    bool LocalPlayerSound;
    bool Loop;
    double SysStartTime; // start time
  };

  // sound curve
  float MaxSoundDist;

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

  // friends
  friend class TCmdMusic;

  inline bool AreSfxEnabled () const noexcept {
    return (SoundDevice && !cli_NoSfx);
  }

  // sound effect helpers
  int FindChannelToReplaceInternal (int sound_id, int origin_id, const float priority, int *sndcountp);
  int GetChannel (int sound_id, int origin_id, int channel, const float priority);
  void StopChannel (int cidx); // won't deallocate it
  void UpdateSfx ();

  // music playback
  void StartMusic (bool allowRandom);
  void PlaySong (const char *Song, bool Loop, bool allowRandom);

  // execution of console commands
  void CmdMusic (const TArray<VStr>&);

  // don't shutdown, just reset
  void ResetAllChannels ();
  int AllocChannel (); // -1: no more
  void DeallocChannel (int cidx);

  int ChanNextUsed (int cidx, bool wantFirst=false) const;
  inline int ChanFirstUsed () const { return ChanNextUsed(-1, true); }

  // WARNING! this must be called from the main thread, i.e.
  //          from the thread that calls `PlaySound*()` API!
  virtual void NotifySoundLoaded (int sound_id, bool success) override;

  float CalcSoundPriority (int sound_id, float dist) noexcept;
};


// it is safe to call `AllocChannel()` and `DeallocChannel()` in this loop
#define FOR_EACH_CHANNEL(varname) \
  for (int varname = ChanFirstUsed(); varname >= 0; varname = ChanNextUsed(i))


// ////////////////////////////////////////////////////////////////////////// //
FAudioCodecDesc *FAudioCodecDesc::List = nullptr;
VAudioPublic *GAudio = nullptr;

VCvarF snd_master_volume("snd_master_volume", "1", "Master volume", CVAR_Archive);

VCvarF VAudio::snd_sfx_volume("snd_sfx_volume", "0.5", "Sound effects volume.", CVAR_Archive);
VCvarF VAudio::snd_music_volume("snd_music_volume", "0.5", "Music volume", CVAR_Archive);
//VCvarB VAudio::snd_swap_stereo("snd_swap_stereo", false, "Swap stereo channels?", CVAR_Archive);
VCvarI VAudio::snd_channels("snd_channels", "64", "Number of sound channels.", CVAR_Archive);
VCvarB VAudio::snd_external_music("snd_external_music", false, "Allow external music remapping?", CVAR_Archive);

static VCvarB snd_random_pitch_enabled("snd_random_pitch_enabled", true, "Global random pitch control.", CVAR_Archive);
static VCvarF snd_random_pitch_default("snd_random_pitch_default", "0.27", "Random pitch sounds without specified pitch (0: none, otherwise max change; 0.27 is ok).", CVAR_Archive);
static VCvarF snd_random_pitch_boost("snd_random_pitch_boost", "2", "Random pitch will be multiplied by this value.", CVAR_Archive);
static VCvarI snd_max_same_sounds("snd_max_same_sounds", "4", "Maximum number of simultaneously playing same sounds?", CVAR_Archive);

VCvarI snd_midi_player("snd_midi_player", "3", "MIDI player type (0:none; 1:FluidSynth; 2:Timidity; 3:NukedOPL)", CVAR_Archive|CVAR_PreInit);
VCvarI snd_module_player("snd_module_player", "1", "Module player type (0:none; 1:XMPLite)", CVAR_Archive);

//k8: it was weirdly unstable under windoze. seems to work ok now.
static VCvarB snd_bgloading_music("snd_bgloading_music", true, "Load music in the background thread?", CVAR_Archive|CVAR_PreInit);

static VCvarS snd_random_midi_dir("snd_random_midi_dir", "", "Directory to load random midis from.", CVAR_Archive);
static VCvarB snd_random_midi_rescan("snd_random_midi_rescan", false, "Force random midi dir rescan.", 0);
static VCvarB snd_random_midi_enabled("snd_random_midi_enabled", true, "Enable random midi replacements?", CVAR_Archive);


static VStr sndLastRandomMidiDir;
static TArray<VStr> sndMusList;
// store replacement here
static TMap<VStrCI, VStr> sndRandomMusReplace;

// need to keep it global for background loading
static VStr selectedSongName;


//==========================================================================
//
//  scanMidiDir
//
//==========================================================================
static void scanMidiDir (VStr path, int level) {
  if (level > 8) return; // just in case
  void *dir = Sys_OpenDir(path, true/*wantdirs*/);
  if (!dir) return;
  GCon->Logf(NAME_Debug, "scanning midi dir '%s' (%d music file%s so far)", *path, sndMusList.length(), (sndMusList.length() == 1 ? "" : "s"));
  TArray<VStr> pathlist;
  for (;;) {
    VStr fn = Sys_ReadDir(dir);
    if (fn.isEmpty()) break;
    const bool isdir = fn.endsWith("/");
    fn = path.appendPath(fn);
    if (isdir) {
      pathlist.append(fn);
    } else {
      VStream *fi = FL_OpenSysFileRead(fn);
      if (!fi) continue;
      char sign[4];
      fi->Serialise(sign, 4);
      const bool err = fi->IsError();
      VStream::Destroy(fi);
      if (err) continue;
      if (memcmp(sign, "MThd", 4) == 0 || memcmp(sign, "MUS\x1a", 4) == 0) {
        fn = VStr("\x01")+fn;
        sndMusList.append(fn);
      }
    }
  }
  Sys_CloseDir(dir);
  for (auto &&dn : pathlist) scanMidiDir(dn, level+1);
}



//==========================================================================
//
//  SelectRandomSong
//
//==========================================================================
static VStr SelectRandomSong (VStr song) {
  VStr rdir = snd_random_midi_dir.asStr();
  if (rdir.isEmpty()) return song;
  if (!snd_random_midi_enabled.asBool()) {
    snd_random_midi_rescan = false;
    sndLastRandomMidiDir.clear();
    sndMusList.clear();
    sndRandomMusReplace.clear();
    return song;
  }
  // need to rescan?
  if (snd_random_midi_rescan.asBool() || sndLastRandomMidiDir != rdir) {
    snd_random_midi_rescan = false;
    sndLastRandomMidiDir = rdir;
    sndMusList.clear();
    sndRandomMusReplace.clear();
    scanMidiDir(rdir, 0);
    GCon->Logf(NAME_Debug, "%d music file%s found", sndMusList.length(), (sndMusList.length() == 1 ? "" : "s"));
  }
  auto pp = sndRandomMusReplace.get(song);
  if (pp) return *pp;
  if (sndMusList.length() == 0) return song;
  int n = GenRandomU31()%sndMusList.length();
  VStr sname = sndMusList[n];
  sndMusList.removeAt(n);
  sndRandomMusReplace.put(song, sname);
  return sname;
}


//==========================================================================
//
//  FAudioCodecDesc::InsertIntoList
//
//==========================================================================
void FAudioCodecDesc::InsertIntoList (FAudioCodecDesc *&list, FAudioCodecDesc *codec) noexcept {
  if (!codec) return;
  FAudioCodecDesc *prev = nullptr;
  FAudioCodecDesc *curr;
  for (curr = list; curr && curr->Priority <= codec->Priority; prev = curr, curr = curr->Next) {}
  if (prev) {
    prev->Next = codec;
    codec->Next = curr;
  } else {
    vassert(curr == list);
    codec->Next = list;
    list = codec;
  }
  //fprintf(stderr, "=== codecs ===\n"); for (const FAudioCodecDesc *c = list; c; c = c->Next) fprintf(stderr,"  %5d: %s\n", c->Priority, c->Description);
}



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
  : MaxSoundDist(4096.0f)
  , MapSong(NAME_None)
  , MusicVolumeFactor(1.0f)
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
//  VAudio::IsMusicAvailable
//
//==========================================================================
bool VAudio::IsMusicAvailable () {
  return !!StreamMusicPlayer;
}


//==========================================================================
//
//  VAudio::IsMusicPlaying
//
//==========================================================================
bool VAudio::IsMusicPlaying () {
  return (StreamMusicPlayer ? StreamMusicPlayer->IsPlaying() : false);
}


//==========================================================================
//
//  VAudio::ResetMusicLoopCounter
//
//==========================================================================
void VAudio::ResetMusicLoopCounter () {
  if (StreamMusicPlayer) StreamMusicPlayer->ResetLoopCounter();
}


//==========================================================================
//
//  VAudio::GetMusicLoopCounter
//
//==========================================================================
int VAudio::GetMusicLoopCounter () {
  return (StreamMusicPlayer ? StreamMusicPlayer->GetLoopCounter() : 0);
}


//==========================================================================
//
//  VAudio::IncMusicLoopCounter
//
//==========================================================================
void VAudio::IncMusicLoopCounter () {
  if (StreamMusicPlayer) StreamMusicPlayer->IncLoopCounter();
}


//==========================================================================
//
//  VAudio::GetResamplerCount
//
//==========================================================================
int VAudio::GetResamplerCount () const noexcept {
  return (SoundDevice ? SoundDevice->GetResamplerCount() : 1);
}


//==========================================================================
//
//  VAudio::GetResamplerName
//
//==========================================================================
VStr VAudio::GetResamplerName (int idx) const noexcept {
  if (SoundDevice) return SoundDevice->GetResamplerName(idx);
  if (idx != 0) return VStr();
  return VStr("default");
}


//==========================================================================
//
//  VAudio::GetDefaultResampler
//
//==========================================================================
int VAudio::GetDefaultResampler () const noexcept {
  return (SoundDevice ? SoundDevice->GetDefaultResampler() : 0);
}


//==========================================================================
//
//  VAudio::ResetAllChannels
//
//==========================================================================
void VAudio::ResetAllChannels () {
  memset((void *)Channel, 0, sizeof(Channel));
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
  if (ChanUsed < 1 || NumChannels < 1) return -1; // anyway
  if (!wantFirst && cidx < 0) return -1;
  unsigned ucidx = (wantFirst ? 0u : (unsigned)(cidx+1));
  //if (wantFirst) cidx = 0; else ++cidx;
  while (ucidx < (unsigned)NumChannels) {
    const unsigned bidx = ucidx/32;
    const vuint32 mask = 0xffffffffu>>(ucidx%32);
    const vuint32 cbv = ChanBitmap[bidx];
    if (cbv&mask) {
      // has some used channels
      for (;;) {
        if (cbv&(0x80000000u>>(ucidx%32))) return (int)ucidx;
        ++ucidx;
      }
    }
    ucidx = (ucidx|0x1fu)+1u;
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
  for (unsigned bidx = 0u; bidx < (unsigned)(NumChannels+31)/32; ++bidx) {
    vuint32 cbv = ChanBitmap[bidx];
    // has some free channels?
    if (cbv == 0xffffffffu) continue; // nope
    unsigned cidx = bidx*32;
    for (unsigned mask = 0x80000000u; mask; mask >>= 1, ++cidx) {
      if ((cbv&mask) == 0) {
        ChanBitmap[bidx] |= mask;
        ++ChanUsed;
        return (int)cidx;
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
  const unsigned bidx = (unsigned)cidx/32;
  const unsigned mask = 0x80000000u>>((unsigned)cidx%32);
  const unsigned cbv = ChanBitmap[bidx];
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
  } else {
    SoundDevice = nullptr; // just in case ;-)
  }

  // initialise stream music player
  if (SoundDevice && !cli_NoMusic) {
    StreamMusicPlayer = new VStreamMusicPlayer(SoundDevice);
    StreamMusicPlayer->Init();
  } else {
    StreamMusicPlayer = nullptr; // just in case
  }

  MaxSoundDist = 4096.0f;
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
    //if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): shutting down music player");
    //StreamMusicPlayer->Shutdown();
    if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): deleting music player");
    StreamMusicPlayer->Shutdown();
    delete StreamMusicPlayer;
    StreamMusicPlayer = nullptr;
  }
  if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): stopping sequences");
  StopAllSequences();
  if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): stopping sounds");
  StopAllSound();
  if (SoundDevice) {
    //if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): shutting down sound device");
    //SoundDevice->Shutdown();
    if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): deleting sound device");
    delete SoundDevice;
    SoundDevice = nullptr;
  }
  if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): resetting all channels");
  ResetAllChannels();
  if (developer) GLog.Log(NAME_Dev, "VAudio::Shutdown(): shutdown complete!");
}


//==========================================================================
//
//  VAudio::CalcSoundPriority
//
//  note that the sounds with the higher priority will be replaced first
//
//==========================================================================
float VAudio::CalcSoundPriority (int sound_id, float dist) noexcept {
  if (sound_id <= 0 || sound_id >= GSoundManager->S_sfx.length()) return +INFINITY; // just in case
  // get default sound priority
  float prio = (float)GSoundManager->S_sfx[sound_id].Priority;
  // adjust priority according to the distance
  // yes, zero distance will set the priority to zero; this is indended
  dist = clampval(dist, 0.0f, MaxSoundDist);
  constexpr float PRIORITY_MAX_ADJUST = 16.0f;
  //prio *= PRIORITY_MAX_ADJUST-PRIORITY_MAX_ADJUST*dist/MaxSoundDist;
  prio *= PRIORITY_MAX_ADJUST*dist/MaxSoundDist;
  return prio;
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
//  if `origin_id` is `-666`, this is local player sound
//
//==========================================================================
void VAudio::PlaySound (int InSoundId, const TVec &origin, const TVec &velocity,
                        int origin_id, int channel, float volume, float Attenuation, bool Loop)
{
  if (!AreSfxEnabled() || !InSoundId || !MaxVolume || volume <= 0.0f || NumChannels < 1) return;

  // find actual sound ID to use
  int sound_id = GSoundManager->ResolveSound(InSoundId);

  if (sound_id < 1 || sound_id >= GSoundManager->S_sfx.length()) return; // k8: just in case
  if (GSoundManager->S_sfx[sound_id].VolumeAmp <= 0) return; // nothing to see here, come along

  // check if this sound is emited by the local player
  if (origin_id < 0) origin_id = -666; // for local sounds, there is no defined origin, `-666` is a fun fake
  //if (!origin_id && Attenuation <= 0.0f) origin_id = -666; // full-volume sounds are always local
  const bool LocalPlayerSound = (origin_id <= 0 || (cl && cl->MO && cl->MO->SoundOriginID == origin_id) || Attenuation <= 0.0f);
  // sound from unknown origin, but with zero attenuation is "player local sound", there's no other way

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

  // calculate the distance before other stuff so that we can throw out sounds that are beyond the hearing range
  float dist = 0.0f;
  if (!LocalPlayerSound && Attenuation > 0.0f && cl) {
    dist = (origin-cl->ViewOrg).length()*Attenuation;
    if (dist <= 0.0f) dist = 0.001f; // safeguard
  }
  //GCon->Logf("DISTANCE=%d", dist);
  if (dist >= MaxSoundDist) {
    //GCon->Logf("  too far away (%d)", MaxSoundDist);
    return; // sound is beyond the hearing range
  }

  // initial priority
  const float priority = CalcSoundPriority(sound_id, dist);

  int chan = GetChannel(sound_id, origin_id, channel, priority);
  if (chan < 0) return; // no free channels

  if (cli_DebugSound > 0) GCon->Logf(NAME_Debug, "PlaySound: sound(%d)='%s'; origin_id=%d; channel=%d; chan=%d; loop=%d", sound_id, *GSoundManager->S_sfx[sound_id].TagName, origin_id, channel, chan, (int)Loop);

  float pitch = 1.0f;
  if (snd_random_pitch_enabled) {
    float sndcp = GSoundManager->S_sfx[sound_id].ChangePitch;
    // apply default pitch?
    if (sndcp < 0.0f) {
      const char *tagstr = *GSoundManager->S_sfx[sound_id].TagName;
      //hack!
      if (LocalPlayerSound || VStr::startsWithCI(tagstr, "menu/") || VStr::startsWithCI(tagstr, "misc/")) sndcp = 0.0f;
      else sndcp = clampval(snd_random_pitch_default.asFloat(), 0.0f, 1.0f);
    }
    // apply pitch
    if (sndcp) {
      pitch += (RandomFull()-RandomFull())*(sndcp*snd_random_pitch_boost.asFloat());
      //GCon->Logf(NAME_Debug, "applied random pitch to sound '%s' (ccp=%g; sndcp=%g); pitch=%g", *GSoundManager->S_sfx[sound_id].TagName, GSoundManager->S_sfx[sound_id].ChangePitch, sndcp, pitch);
    }
  }

  int handle;
  bool is3D;
  if (LocalPlayerSound) {
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
  Channel[chan].prevOrigin = origin;
#ifdef VV_SND_ALLOW_VELOCITY
  Channel[chan].velocity = velocity;
  Channel[chan].prevVelocity = velocity;
#endif
  Channel[chan].sound_id = sound_id;
  Channel[chan].priority = priority;
  Channel[chan].volume = volume;
  Channel[chan].Attenuation = Attenuation;
  Channel[chan].handle = handle;
  Channel[chan].is3D = is3D;
  Channel[chan].LocalPlayerSound = LocalPlayerSound;
  Channel[chan].Loop = Loop;
  Channel[chan].SysStartTime = Sys_Time();
}


//==========================================================================
//
//  VAudio::MoveSounds
//
//==========================================================================
void VAudio::MoveSounds (int origin_id, const TVec &neworigin) {
  if (origin_id <= 0) return; // no need to move local full-volume sounds
  if (!neworigin.isValid()) return;
  FOR_EACH_CHANNEL(i) {
    if (!Channel[i].sound_id || Channel[i].handle == -1 || Channel[i].origin_id != origin_id) continue;
    //GCon->Logf(NAME_Debug, "moving sound channel #%d (%d) from (%g,%g,%g) to (%g,%g,%g)", i, origin_id, Channel[i].origin.x, Channel[i].origin.y, Channel[i].origin.z, neworigin.x, neworigin.y, neworigin.z);
    Channel[i].origin = neworigin;
  }
}


//==========================================================================
//
//  VAudio::FindChannelToReplaceInternal
//
//  find the internal sound channel to replace
//  meant to be used in `VAudio::GetChannel()`
//
//  this prefers sounds from the same origin, if possible (priority check)
//  also, if `sound_id` is not zero, only check sounds with this id
//
//  returns internal channel number or -1
//  if `sound_id` is not zero, sets `sndcountp` to the count
//
//  WARNING! DO NOT CALL WITH INVALID ARGS!
//
//==========================================================================
int VAudio::FindChannelToReplaceInternal (int sound_id, int origin_id, const float priority, int *sndcountp) {
  // oither origins
  int lp = -1;
  float prior = priority;
  double lowesttime = HUGE_VAL;
  // given origin
  int oidlp = -1;
  float oidprior = priority;
  double oidlowesttime = HUGE_VAL;
  // counter
  int count = 0;

  // loop over all active channels
  FOR_EACH_CHANNEL(i) {
    if (sound_id && Channel[i].sound_id != sound_id) continue; // not interesting
    ++count; // count them
    // check origin
    if (Channel[i].origin_id == origin_id) {
      // same origin
      if (Channel[i].priority > oidprior ||
          (Channel[i].priority == oidprior && (oidlp < 0 || oidlowesttime > Channel[i].SysStartTime)))
      {
        oidlp = i;
        oidlowesttime = Channel[i].SysStartTime;
        oidprior = Channel[i].priority;
      }
    } else {
      // other origin
      if (Channel[i].priority > prior ||
          (Channel[i].priority == prior && (lp < 0 || lowesttime > Channel[i].SysStartTime)))
      {
        // if we're gonna kill one, then this will be it
        lp = i;
        lowesttime = Channel[i].SysStartTime;
        prior = Channel[i].priority;
      }
    }
  }

  // return counter
  if (sndcountp) *sndcountp = count;
  // prefer sounds from the same origin
  return (oidlp >= 0 ? oidlp : lp);
}


//==========================================================================
//
//  VAudio::GetChannel
//
//  channel 0 is "CHAN_AUTO"
//
//  `priority` is adjusted according to the distance
//  sounds with the higher priority will be replaced first
//
//  `origin_id` can be anything (including zero and negative numbers)
//  zero means "unknown origin", negative means "local" (it is -666)
//
//==========================================================================
int VAudio::GetChannel (int sound_id, int origin_id, int channel, const float priority) {
  // just in case
  if (sound_id < 1 || sound_id >= GSoundManager->S_sfx.length()) return -1; // invalid sound id

  const int maxcc = min2(snd_max_same_sounds.asInt(), 16);
  // <0: unlimited; 0: default; >0: hard limit
  const int numchannels = (maxcc >= 0 ? clampval(GSoundManager->S_sfx[sound_id].NumChannels, 0, (maxcc ? maxcc : 16)) : 0);

  // first, look if we want to replace a sound on some channel
  // sounds from unknown origin will never replace each other, though
  if (channel != 0 && origin_id != 0) {
    FOR_EACH_CHANNEL(i) {
      // `sound_id` is zero for unused channel (because sound with zero id is never used)
      // this should not happen here, but...
      if (Channel[i].sound_id && Channel[i].origin_id == origin_id && Channel[i].channel == channel) {
        // this channel already playing some sound; replace it
        StopChannel(i);
        return i;
      }
    }
  }

  // check for "singular" sounds
  if (GSoundManager->S_sfx[sound_id].bSingular) {
    FOR_EACH_CHANNEL(i) {
      if (Channel[i].sound_id == sound_id) {
        // this sound is already playing, so don't start it again
        return -1;
      }
    }
  }

  // if we have a defined maximum for simultaneously played sounds with this id, replace the "worst" one
  // note that "singular" sounds will not end up here, so no need to check
  // prefer sounds from the same origin (sounds from unknown origin will replace other sounds from unknown origin)
  if (numchannels > 0) {
    int found = 0;
    const int lp = FindChannelToReplaceInternal(sound_id, origin_id, priority, &found);
    // too many equal sounds? (prevent ear ripping ;-)
    if (found >= numchannels) {
      if (lp >= 0) StopChannel(lp);
      return lp;
    }
  }

  // get a free channel, if there is any
  if (ChanUsed < NumChannels) return AllocChannel();
  if (NumChannels < 1) return -1;

  // look for a lower priority sound to replace (with higher priority value)
  const int lowestlp = FindChannelToReplaceInternal(0/*any sound id*/, origin_id, priority, nullptr/*counter is not interesting*/);
  if (lowestlp >= 0) StopChannel(lowestlp);
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
//  oid 0 means "all origin ids"
//  channel 0 means "all channels for this origin id"
//
//==========================================================================
void VAudio::StopSound (int origin_id, int channel) {
  FOR_EACH_CHANNEL(i) {
    if ((origin_id <= 0 || Channel[i].origin_id == origin_id) &&
        (!channel || Channel[i].channel == channel))
    {
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
void VAudio::StopAllSound (bool keepLastSwitch) {
  // stop all sounds
  if (keepLastSwitch) {
    // check if we have sector sound, and it was started "right now"
    // this is used to keep the sound of exit switch
    int sectorKeep = -1;
    FOR_EACH_CHANNEL(i) {
      if (Channel[i].origin_id&(SNDORG_Sector<<24)) {
        if (sectorKeep < 0 || Channel[i].SysStartTime > Channel[sectorKeep].SysStartTime) {
          sectorKeep = i;
        }
      }
    }
    if (sectorKeep >= 0 && Sys_Time()-Channel[sectorKeep].SysStartTime < 0.6) {
      // stop all sounds except this one
      //GCon->Logf(NAME_Debug, "keeping sector channel #%d", sectorKeep);
      FOR_EACH_CHANNEL(i) {
        if (i != sectorKeep) StopChannel(i);
      }
    }
    return;
  }
  //GCon->Log(NAME_Debug, "stopping all sounds");
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
  if (!AreSfxEnabled()) return; // don't bother adding sequences if the sound is off
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
  if (!AreSfxEnabled()) return; // don't bother adding sequences if the sound is off
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
  if (!AreSfxEnabled()) return; // just in case
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
  if (!AreSfxEnabled() || NumChannels <= 0) return;

  const float currVolume = clampval(snd_sfx_volume.asFloat()*snd_master_volume.asFloat(), 0.0f, 1.0f);
  if (currVolume != MaxVolume) {
    MaxVolume = currVolume;
    if (MaxVolume <= 0.0f) StopAllSound();
  }

  if (MaxVolume <= 0.0f) return; // silence

  if (cl) AngleVectors(cl->ViewAngles, ListenerForward, ListenerRight, ListenerUp);

  FOR_EACH_CHANNEL(i) {
    // active channel?
    // this should not happen
    if (!Channel[i].sound_id) {
      vassert(Channel[i].handle == -1);
      DeallocChannel(i);
      continue;
    }

    // still playing?
    if (!SoundDevice->IsChannelPlaying(Channel[i].handle)) {
      // nope
      if (cli_DebugSound > 0 && Channel[i].sound_id >= 0) {
        GCon->Logf(NAME_Debug, "UpdateSfx: finished sound(%d)='%s'; origin_id=%d; channel=%d; chan=%d; loop=%d",
          Channel[i].sound_id, *GSoundManager->S_sfx[Channel[i].sound_id].TagName, Channel[i].origin_id, Channel[i].channel, i, (int)Channel[i].Loop);
      }
      StopChannel(i);
      DeallocChannel(i);
      continue;
    }

    // full volume sound?
    if (Channel[i].origin_id <= 0 || Channel[i].Attenuation <= 0.0f) continue;

    if (cl) {
      if (cl->MO && Channel[i].origin_id == cl->MO->SoundOriginID) {
        //GCon->Logf(NAME_Debug, "channel #%d (%d), origin=(%g,%g,%g); new origin=(%g,%g,%g)", i, Channel[i].origin_id, Channel[i].origin.x, Channel[i].origin.y, Channel[i].origin.z, cl->MO->Origin.x, cl->MO->Origin.y, cl->MO->Origin.z);
        Channel[i].origin = cl->MO->Origin;
        #ifdef VV_SND_ALLOW_VELOCITY
        Channel[i].velocity = TVec(0.0f, 0.0f, 0.0f);
        #endif
      } else if (!Channel[i].LocalPlayerSound) {
        VLevel *Level = (GClLevel ? GClLevel : GLevel);
        if (Level) {
          // update positions
          switch ((Channel[i].origin_id>>24)&0xff) {
            case SNDORG_Entity:
              // move sound with the entity
              {
                VEntity *ent = Level->FindEntityBySoundID(Channel[i].origin_id&0x00ffffff);
                if (ent) {
                  Channel[i].origin = ent->Origin;
                  #ifdef VV_SND_ALLOW_VELOCITY
                  Channel[i].velocity = ent->Velocity;
                  // this will be re-added later
                  Channel[i].origin -= Channel[i].velocity*host_frametime;
                  #endif
                }
              }
              break;
            case SNDORG_Sector:
              {
                sector_t *sec = Level->FindSectorBySoundID(Channel[i].origin_id&0x00ffffff);
                if (sec) {
                  if (!sec->isInnerPObj()) {
                    // normal sector
                    Channel[i].origin = sec->soundorg;
                    Channel[i].origin.z = (sec->floor.minz+sec->floor.maxz)*0.5f+8.0f;
                  } else {
                    // 3d pobj
                    Channel[i].origin = sec->ownpobj->startSpot;
                  }
                  #ifdef VV_SND_ALLOW_VELOCITY
                  Channel[i].velocity = TVec(0.0f, 0.0f, 0.0f);
                  #endif
                }
              }
              break;
            case SNDORG_PolyObj:
              {
                polyobj_t *pobj = Level->FindPObjBySoundID(Channel[i].origin_id&0x00ffffff);
                // do not move origin for non-3d polyobject
                // this is because non-3d pobjs are usually used for things like doors and such, and
                // moving their sound origin won't do anything good
                if (pobj && pobj->Is3D()) {
                  Channel[i].origin = pobj->startSpot;
                  #ifdef VV_SND_ALLOW_VELOCITY
                  Channel[i].velocity = TVec(0.0f, 0.0f, 0.0f);
                  #endif
                }
              }
              break;
          }
        }
      }
    }

    // client sound?
    if (Channel[i].LocalPlayerSound) continue;

    // move sound
    #ifdef VV_SND_ALLOW_VELOCITY
    Channel[i].origin += Channel[i].velocity*host_frametime;
    #endif

    if (!cl) continue;

    float dist = (Channel[i].origin-cl->ViewOrg).length()*Channel[i].Attenuation;
    if (dist < 0.0f) dist = 0.0f; // just in case
    if (dist >= MaxSoundDist) {
      // too far away
      StopChannel(i);
      DeallocChannel(i);
      continue;
    }

    //GCon->Logf(NAME_Debug, "channel #%d (%d), origin=(%g,%g,%g); dist=%d", i, Channel[i].origin_id, Channel[i].origin.x, Channel[i].origin.y, Channel[i].origin.z, dist);

    // update params
    if (Channel[i].is3D) {
      if (Channel[i].prevOrigin != Channel[i].origin
          #ifdef VV_SND_ALLOW_VELOCITY
          || Channel[i].prevVelocity != Channel[i].velocity
          #endif
         )
      {
        Channel[i].prevOrigin = Channel[i].origin;
        #ifdef VV_SND_ALLOW_VELOCITY
        Channel[i].prevVelocity = Channel[i].velocity;
        #endif
        SoundDevice->UpdateChannel3D(Channel[i].handle, Channel[i].origin
          #ifdef VV_SND_ALLOW_VELOCITY
          , Channel[i].velocity
          #else
          , TVec(0.0f, 0.0f, 0.0f)
          #endif
        );
      }
    }
    Channel[i].priority = CalcSoundPriority(Channel[i].sound_id, dist);
  }

  if (cl) {
    SoundDevice->UpdateListener(cl->ViewOrg, TVec(0.0f, 0.0f, 0.0f),
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
void VAudio::StartSong (VName song, bool loop, bool allowRandom) {
  VStr cmd("Music");
  if (allowRandom) {
    cmd += (loop ? " LoopRandom " : " PlayRandom ");
  } else {
    cmd += (loop ? " Loop " : " Play ");
  }
  cmd += *VStr(song).quote();
  GCmdBuf << *cmd << "\n";
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
  GCmdBuf << "Music Resume\n";
}


//==========================================================================
//
//  VAudio::StartMusic
//
//==========================================================================
void VAudio::StartMusic (bool allowRandom) {
  StartSong(MapSong, true/*loop*/, allowRandom);
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
//  called on map change, or when music changed via ACS
//
//==========================================================================
void VAudio::MusicChanged (bool allowRandom) {
  MapSong = GClLevel->LevelInfo->SongLump;
  StartMusic(allowRandom);
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

  if (snd_master_volume < 0.0f) snd_master_volume = 0.0f;
  if (snd_master_volume > 1.0f) snd_master_volume = 1.0f;

  // with "-nosound", we have no sound device (and there cannot be any music device too ;-)
  if (!SoundDevice) return;

  SoundDevice->StartBatchUpdate();
  // update any sequences
  UpdateActiveSequences(host_frametime);
  // update sounds
  UpdateSfx();
  // done
  SoundDevice->FinishBatchUpdate();

  if (StreamMusicPlayer) StreamMusicPlayer->SetVolume(snd_music_volume*MusicVolumeFactor);
}


//==========================================================================
//
//  FindMusicLump
//
//==========================================================================
static int FindMusicLump (const char *songName) {
  /*static*/ const char *Exts[] = {
    "opus", "ogg", "flac", "mp3", "wav",
    "mid", "mus",
    "mod", "xm", "it", "s3m", "stm",
    //"669", "amf", "dsm", "far", "gdm", "imf", "it", "m15", "med", "mod", "mtm",
    //"okt", "s3m", "stm", "stx", "ult", "uni", "xm", "flac", "ay", "gbs",
    //"gym", "hes", "kss", "nsf", "nsfe", "sap", "sgc", "spc", "vgm",
    nullptr
  };

  if (!songName || !songName[0] || VStr::strEquCI(songName, "none")) return -1;
  int Lump = -1;
  VName sn8 = VName(songName, VName::FindLower8);
  if (sn8 != NAME_None) {
    Lump = W_CheckNumForName(sn8, WADNS_Music);
    //GCon->Logf(NAME_Debug, "*** 000: looking for 8-mus: <%s>: Lump=%d : %s", *sn8, Lump, *W_FullLumpName(Lump));
    Lump = max2(Lump, W_CheckNumForName(sn8, WADNS_Global));
    //GCon->Logf(NAME_Debug, "*** 001: looking for 8-mus: <%s>: Lump=%d : %s", *sn8, Lump, *W_FullLumpName(Lump));
  }
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
VAudioCodec *VAudio::LoadSongInternal (const char *Song, bool wasPlaying, bool fromStreamThread) {
  /*static*/ const char *ExtraExts[] = { "opus", "ogg", "flac", "mp3", "mid", "mus", nullptr };

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

  VStream *diskStream = nullptr;
  int Lump = -1;
  VStr songName;

  // special: from the disk
  if (Song[0] == '\x01') {
    songName = VStr(Song+1);
    diskStream = FL_OpenSysFileRead(songName);
    if (!diskStream) {
      GCon->Logf(NAME_Warning, "Can't find random song \"%s\"", *songName);
      return nullptr;
    }
  } else {
    // find the song
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
        if (VStr::strEquCI(*GSoundManager->MusicAliases[f].origName, Song)) {
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

    songName = W_FullLumpName(Lump);
  }

  // get music volume for this song
  MusicVolumeFactor = GSoundManager->GetMusicVolume(Song);
  if (StreamMusicPlayer) {
    StreamMusicPlayer->SetVolume(snd_music_volume*MusicVolumeFactor, fromStreamThread);
    //if (fromStreamThread) SoundDevice->SetStreamVolume(snd_music_volume*MusicVolumeFactor*snd_master_volume);
    if (fromStreamThread) SoundDevice->SetStreamVolume(StreamMusicPlayer->GetNewVolume());
  }

  VStream *Strm = (Lump >= 0 ? W_CreateLumpReaderNum(Lump) : diskStream);
  diskStream = nullptr;
  if (Strm->TotalSize() < 4) {
    GCon->Logf(NAME_Warning, "Lump '%s' for song \"%s\" is too small (%d)", *songName, Song, Strm->TotalSize());
    VStream::Destroy(Strm);
    return nullptr;
  }

  // if the song is quite small, load it in memory
  const int strmsize = Strm->TotalSize();
  if (strmsize < 1024*1024*32) {
    VMemoryStream *ms = new VMemoryStream(Strm->GetName());
    TArray<vuint8> &arr = ms->GetArray();
    arr.setLength(strmsize);
    Strm->Serialise(arr.ptr(), strmsize);
    const bool err = Strm->IsError();
    VStream::Destroy(Strm);
    if (err) {
      ms->Close();
      delete ms;
      GCon->Logf(NAME_Warning, "Lump '%s' for song \"%s\" cannot be read", *songName, Song);
      return nullptr;
    }
    ms->BeginRead();
    Strm = ms;
    GCon->Logf(NAME_Debug, "Lump '%s' for song \"%s\" loaded into memory (%d bytes)", *songName, Song, strmsize);
  }

  // load signature, so we can pass it to codecs
  vuint8 sign[4];
  Strm->Serialise(sign, 4);
  if (Strm->IsError()) {
    GCon->Logf(NAME_Error, "error loading song '%s'", *songName);
    VStream::Destroy(Strm);
    return nullptr;
  }

  // do not convert mus to midi if current midi player is NukedOPL (3)
  if (snd_midi_player.asInt() != 3 && memcmp(sign, MUSMAGIC, 4) == 0) {
    // convert mus to mid with a wonderfull function
    // thanks to S.Bacquet for the source of qmus2mid
    Strm->Seek(0);
    VMemoryStream *MidStrm = new VMemoryStream();
    MidStrm->BeginWrite();
    VQMus2Mid Conv;
    int MidLength = Conv.Run(*Strm, *MidStrm);
    VStream::Destroy(Strm);
    if (!MidLength) {
      delete MidStrm;
      return nullptr;
    }
    MidStrm->Seek(0);
    MidStrm->BeginRead();
    Strm = MidStrm;
    Strm->Serialise(sign, 4);
    if (Strm->IsError()) {
      GCon->Logf(NAME_Error, "error loading song '%s'", *songName);
      VStream::Destroy(Strm);
      return nullptr;
    }
    GCon->Logf("converted MUS '%s' to MIDI", *songName);
  }

  // try to create audio codec
  const char *codecName = nullptr;
  VAudioCodec *Codec = nullptr;

  for (FAudioCodecDesc *Desc = FAudioCodecDesc::List; Desc && !Codec; Desc = Desc->Next) {
    //GCon->Logf(NAME_Debug, "Trying codec `%s` (%d) to open the stream", Desc->Description, Desc->Priority);
    Strm->Seek(0);
    if (Strm->IsError()) {
      GCon->Logf(NAME_Error, "error loading song '%s'", *songName);
      VStream::Destroy(Strm);
      return nullptr;
    }
    Codec = Desc->Creator(Strm, sign, 4);
    if (Codec) codecName = Desc->Description;
  }

  if (Codec) {
    GCon->Logf("starting song '%s' with codec '%s'", *songName, codecName);
    // start playing streamed music
    //StreamMusicPlayer->Play(Codec, Song, Loop);
    return Codec;
  }

  GCon->Logf(NAME_Warning, "couldn't find codec for song '%s'", *songName);
  VStream::Destroy(Strm);
  return nullptr;
}


//==========================================================================
//
//  VAudio::PlaySong
//
//==========================================================================
void VAudio::PlaySong (const char *Song, bool Loop, bool allowRandom) {
  if (!Song || !Song[0] || !StreamMusicPlayer) return;

  if (SoundHasBadApple && VStr::strEquCI(Song, "d_runnin")) return;

  VStr ss(Song);
  while (ss.length() && ss[0] == '\x01') ss.chopLeft(1);
  if (allowRandom) ss = SelectRandomSong(ss);
  selectedSongName = ss.cloneUniqueMT();

  if (snd_bgloading_music) {
    StreamMusicPlayer->LoadAndPlay(*selectedSongName, Loop);
  } else {
    bool wasPlaying = StreamMusicPlayer->IsPlaying();
    if (wasPlaying) StreamMusicPlayer->Stop();

    VAudioCodec *Codec = LoadSongInternal(*selectedSongName, wasPlaying, false); // not from a stream thread

    if (Codec && StreamMusicPlayer) {
      StreamMusicPlayer->Play(Codec, *selectedSongName, Loop);
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

  if (Args.length() < 2) return;

  VStr command = Args[1];

  if (command.strEquCI("on")) {
    MusicEnabled = true;
    return;
  }

  if (command.strEquCI("off")) {
    if (StreamMusicPlayer) StreamMusicPlayer->Stop();
    MusicEnabled = false;
    return;
  }

  if (!MusicEnabled) return;

  if (command.strEquCI("play") || command.strEquCI("playrandom")) {
    if (Args.length() < 3) {
      GCon->Log(NAME_Warning, "Please enter name of the song (play).");
      return;
    }
    PlaySong(*Args[2].ToLower(), false, command.strEquCI("playrandom"));
    return;
  }

  if (command.strEquCI("loop") || command.strEquCI("looprandom")) {
    if (Args.length() < 3) {
      GCon->Log(NAME_Warning, "Please enter name of the song (loop).");
      return;
    }
    PlaySong(*Args[2].ToLower(), true, command.strEquCI("looprandom"));
    return;
  }

  if (command.strEquCI("pause")) {
    StreamMusicPlayer->Pause();
    return;
  }

  if (command.strEquCI("resume")) {
    StreamMusicPlayer->Resume();
    return;
  }

  if (command.strEquCI("stop")) {
    StreamMusicPlayer->Stop();
    return;
  }

  if (command.strEquCI("restart")) {
    StreamMusicPlayer->Restart();
    return;
  }

  if (command.strEquCI("info")) {
    if (StreamMusicPlayer->IsPlaying()) {
      GCon->Logf("Currently %s %s.", (StreamMusicPlayer->IsCurrentSongLooped() ? "looping" : "playing"), *StreamMusicPlayer->GetCurrentSong());
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
  , Volume(1.0f) // start at max volume
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
    if (((VAudio *)GAudio)->SequenceListHead == this) ((VAudio *)GAudio)->SequenceListHead = Next;
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
      if (SeqChoices.length() == 0) {
        ++SequencePtr;
      } else if (!ChildSeq) {
        int Choice = GenRandomU31()%SeqChoices.length();
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
      for (VSoundSeqNode *n = ((VAudio *)GAudio)->SequenceListHead; n; n = n->Next, ++i) {
        if (ParentSeqIdx == i) ParentSeq = n;
        if (ChildSeqIdx == i) ChildSeq = n;
      }
    }
  } else {
    vint32 Offset = SequencePtr - GSoundManager->SeqInfo[Sequence].Data;
    Strm << STRM_INDEX(Offset);

    vint32 Count = SeqChoices.length();
    Strm << STRM_INDEX(Count);
    for (int i = 0; i < SeqChoices.length(); ++i) Strm << GSoundManager->SeqInfo[SeqChoices[i]].Name;

    vint32 ParentSeqIdx = -1;
    vint32 ChildSeqIdx = -1;
    if (ParentSeq || ChildSeq) {
      int i = 0;
      for (VSoundSeqNode *n = ((VAudio *)GAudio)->SequenceListHead; n; n = n->Next, ++i) {
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
    list.append("looprandom");
    list.append("off");
    list.append("on");
    list.append("pause");
    list.append("play");
    list.append("playrandom");
    list.append("restart");
    list.append("resume");
    list.append("stop");
    return AutoCompleteFromListCmd(prefix, list);
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
