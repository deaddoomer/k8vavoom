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
#ifndef VAVOOM_S_LOCAL_HEADER
#define VAVOOM_S_LOCAL_HEADER

#ifdef CLIENT
/*# define AL_ALEXT_PROTOTYPES*/
# ifdef VAVOOM_USE_MOJOAL
#  include "mojoal/AL/al.h"
#  include "mojoal/AL/alc.h"
# else
#  include <AL/al.h>
#  include <AL/alc.h>
#  include <AL/alext.h>
# endif
// linux headers doesn't define this
# ifndef OPENAL
#  define OPENAL
# endif
#endif

#include "sound.h"

// our velocities are almost always zero, so why?
//#define VV_SND_ALLOW_VELOCITY


enum ESSCmds {
  SSCMD_None,
  SSCMD_Play,
  SSCMD_WaitUntilDone, // used by PLAYUNTILDONE
  SSCMD_PlayTime,
  SSCMD_PlayRepeat,
  SSCMD_PlayLoop,
  SSCMD_Delay,
  SSCMD_DelayOnce,
  SSCMD_DelayRand,
  SSCMD_Volume,
  SSCMD_VolumeRel,
  SSCMD_VolumeRand,
  SSCMD_StopSound,
  SSCMD_Attenuation,
  SSCMD_RandomSequence,
  SSCMD_Branch,
  SSCMD_Select,
  SSCMD_End
};

class VSoundSeqNode;


// rolloff types
enum {
  ROLLOFF_Doom, // linear rolloff with a logarithmic volume scale
  ROLLOFF_Linear, // linear rolloff with a linear volume scale
  ROLLOFF_Log, // logarithmic rolloff (standard hardware type)
  ROLLOFF_Custom, // lookup volume from SNDCURVE
};


// SoundFX struct
struct sfxinfo_t {
  enum { ST_Invalid = -1, ST_NotLoaded = 0, ST_Loading = 1, ST_Loaded = 2 };

  atomic_int loadedStateVar; // ST_XXX

  VName TagName; // name, by whitch sound is recognised in script
  int LumpNum; // lump number of sfx

  int Priority; // higher priority takes precendence
  int NumChannels; // total number of channels a sound type may occupy
  float ChangePitch;
  float VolumeAmp; // from sndinfo, cannot exceed 1.0
  FRolloffInfo Rolloff;
  float Attenuation; // multiplies the attenuation passed to S_Sound
  // for "$alias", link is index of "real" sound
  int Link; // usually `-1`
  int *Sounds; // for random sounds, Link is count (and bRandomHeader is set)

  // if set, this is a list of random sounds, `Link` is count, `Sounds` is index array
  bool bRandomHeader;
  // set for "$playersounddup", `Link` is some number of something reserved (TODO: figure this out)
  bool bPlayerReserve;
  bool bSingular;

  vuint32 SampleRate;
  int SampleBits;
  vuint32 DataSize;
  void *Data;

  VVA_FORCEINLINE int GetLoadedState () noexcept { return atomic_get(&loadedStateVar); }
  VVA_FORCEINLINE void SetLoadedState (int value) noexcept { atomic_set(&loadedStateVar, value); }
};

struct seq_info_t {
  VName Name;
  VName Slot;
  vint32 *Data;
  vint32 StopSound;
};

enum {
  REVERBF_DecayTimeScale        = 0x01,
  REVERBF_ReflectionsScale      = 0x02,
  REVERBF_ReflectionsDelayScale = 0x04,
  REVERBF_ReverbScale           = 0x08,
  REVERBF_ReverbDelayScale      = 0x10,
  REVERBF_DecayHFLimit          = 0x20,
  REVERBF_EchoTimeScale         = 0x40,
  REVERBF_ModulationTimeScale   = 0x80,
};


struct VReverbProperties {
  int Environment;
  float EnvironmentSize;
  float EnvironmentDiffusion;
  int Room;
  int RoomHF;
  int RoomLF;
  float DecayTime;
  float DecayHFRatio;
  float DecayLFRatio;
  int Reflections;
  float ReflectionsDelay;
  float ReflectionsPanX;
  float ReflectionsPanY;
  float ReflectionsPanZ;
  int Reverb;
  float ReverbDelay;
  float ReverbPanX;
  float ReverbPanY;
  float ReverbPanZ;
  float EchoTime;
  float EchoDepth;
  float ModulationTime;
  float ModulationDepth;
  float AirAbsorptionHF;
  float HFReference;
  float LFReference;
  float RoomRolloffFactor;
  float Diffusion;
  float Density;
  int Flags;
};


#if defined(VAVOOM_REVERB)
struct VReverbInfo {
  VReverbInfo *Next;
  const char *Name;
  int Id;
  bool Builtin;
  VReverbProperties Props;
};
#endif


#ifdef CLIENT
typedef ALCboolean (ALC_APIENTRY *alcSetThreadContextFn) (ALCcontext *context);
typedef ALCcontext *(ALC_APIENTRY *alcGetThreadContextFn) (void);

typedef void (ALC_APIENTRY *alDeferUpdatesSOFTFn) (void);
typedef void (ALC_APIENTRY *alProcessUpdatesSOFTFn) (void);

typedef const ALchar *(ALC_APIENTRY *alGetStringiSOFTFn) (ALenum pname, ALsizei index);

// sound device interface
// this class implements the only supported OpenAL Soft driver
class VOpenALDevice {
private:
  //enum { MAX_VOICES = 256-4 };
  enum { MAX_VOICES = 64+2 };

  enum { NUM_STRM_BUFFERS = 8*2 };
  enum { STRM_BUFFER_SIZE = 1024*8 };

  ALCdevice *Device;
  ALCcontext *Context;
  ALuint *Buffers;
  vint32 BufferCount;
  vint32 RealMaxVoices;

  bool HasTreadContext;
  alcSetThreadContextFn p_alcSetThreadContext;
  alcGetThreadContextFn p_alcGetThreadContext;

  bool HasBatchUpdate;
  alDeferUpdatesSOFTFn p_alDeferUpdatesSOFT;
  alProcessUpdatesSOFTFn p_alProcessUpdatesSOFT;

  bool HasForceSpatialize;
  ALenum alSrcSpatSoftEnum;
  //ALenum alSrcSpatSoftValue; // AL_AUTO_SOFT

  ALuint StrmSampleRate;
  ALuint StrmFormat;
  ALuint StrmBuffers[NUM_STRM_BUFFERS];
  ALuint StrmAvailableBuffers[NUM_STRM_BUFFERS];
  int StrmNumAvailableBuffers;
  ALuint StrmSource;
  vint16 StrmDataBuffer[STRM_BUFFER_SIZE*2]; // temp buffer we can use to decode data

  alGetStringiSOFTFn p_alGetStringiSOFT;
  ALenum alNumResSoftValue;
  ALenum alDefResSoftValue;
  ALenum alResNameSoftValue;
  ALenum alSrcResamplerSoftValue;

  TArray<VStr> ResamplerNames;
  ALint defaultResampler;

  // if sound is queued to be loaded, we'll remember sound source here
  struct PendingSrc {
    ALuint src;
    int sound_id;
    PendingSrc *next;
  };
  TMapNC<int, PendingSrc *> sourcesPending; // key is sound id
  TMapNC<ALuint, int> srcPendingSet; // key is source id, value is sound id
  TMapNC<ALuint, bool> srcErrorSet; // key is source id
  TMapNC<ALuint, bool> activeSourceSet; // key is source id

private:
  static bool IsError (const char *errmsg, bool errabort=false);
  static void ClearError (); // reset error flag

  bool AllocSource (ALuint *src);

  // records one new pending sound
  // allocates sound source, records,
  // returns `true` on success, `false` on error
  // `src` must not be NULL! returns source id on success
  // returns `VSoundManager::LS_Error` or `VSoundManager::LS_Pending`
  int RecordPendingSound (int sound_id, ALuint *src);

  // returns VSoundManager::LS_XXX
  // if not errored, sets `src` to new sound source
  int LoadSound (int sound_id, ALuint *src);

private:
  int CommonPlaySound (bool is3d, int sound_id, const TVec &origin, const TVec &velocity,
                       float volume, float pitch, bool Loop);

public:
  VOpenALDevice ();
  ~VOpenALDevice ();

  bool Init();
  int SetChannels (int);
  void Shutdown ();
  int PlaySound (int sound_id, float volume, float pitch, bool Loop);
  int PlaySound3D (int sound_id, const TVec &origin, const TVec &velocity,
                   float volume, float pitch, bool Loop);
  void UpdateChannel3D (int Handle, const TVec &Org, const TVec &Vel);
  bool IsChannelPlaying (int Handle);
  void StopChannel (int Handle);
  void UpdateListener (const TVec &org, const TVec &vel, const TVec &fwd, const TVec &/*right*/, const TVec &up
#if defined(VAVOOM_REVERB)
                      , VReverbInfo *Env
#endif
                      );

  // all stream functions should be thread-safe
  bool OpenStream (int Rate, int Bits, int Channels);
  void CloseStream ();
  int GetStreamAvailable ();
  vint16 *GetStreamBuffer ();
  void SetStreamData (vint16 *data, int len);
  void SetStreamVolume (float vol);
  void PauseStream ();
  void ResumeStream ();
  void SetStreamPitch (float pitch);

  void AddCurrentThread ();
  void RemoveCurrentThread ();

  // call this to start batch update
  void StartBatchUpdate ();
  // and this to commit it
  void FinishBatchUpdate ();

  int GetResamplerCount () const noexcept { return ResamplerNames.length(); }
  VStr GetResamplerName (int idx) const noexcept { return (idx >= 0 && idx < ResamplerNames.length() ? ResamplerNames[idx] : VStr()); }
  int GetDefaultResampler () const noexcept { return defaultResampler; }

  // WARNING! this must be called from the main thread, i.e.
  //          from the thread that calls `PlaySound*()` API!
  // returns `true` if that sound was pending
  bool NotifySoundLoaded (int sound_id, bool success);
};
#else
class VOpenALDevice {};
#endif


class VAudioCodec;


// loader of sound samples
class VSampleLoader : public VInterface {
public:
  VSampleLoader *Next;
  int Priority;

  static VSampleLoader *List;

private:
  static void InsertIntoList (VSampleLoader *&list, VSampleLoader *codec) noexcept;

public:
  VSampleLoader () = delete;
  VV_DISABLE_COPY(VSampleLoader)
  VSampleLoader (int prio) : Next(nullptr), Priority(prio) { InsertIntoList(List, this); }
  virtual void Load (sfxinfo_t &Sfx, VStream &Strm) = 0;
  virtual const char *GetName () const noexcept = 0;

  // codec must be initialized, and it will not be owned
  void LoadFromAudioCodec (sfxinfo_t &Sfx, VAudioCodec *Codec);
};


// streamed audio decoder interface
class VAudioCodec : public VInterface {
public:
  int SampleRate;
  int SampleBits;
  int NumChannels;

public:
  VAudioCodec () : SampleRate(44100), SampleBits(16), NumChannels(2) {}
  // always decodes interleaved stereo, returns number of frames
  // `NumFrames` is number of stereo samples (frames), and it will never be zero
  // it should always decode `NumFrames`, unless end-of-stream happens
  virtual int Decode (vint16 *Data, int NumFrames) = 0;
  virtual bool Finished () = 0;
  virtual void Restart () = 0;
};


// audio codec creator
// `sign` will always contain at least 4 bytes
typedef VAudioCodec *(*CreatorFn) (VStream *InStrm, const vuint8 sign[], int signsize);

// description of an audio codec
struct FAudioCodecDesc {
  const char *Description;
  CreatorFn Creator;
  int Priority;
  FAudioCodecDesc *Next;

  static FAudioCodecDesc *List;

private:
  static void InsertIntoList (FAudioCodecDesc *&list, FAudioCodecDesc *codec) noexcept;

public:
  FAudioCodecDesc () = delete;
  VV_DISABLE_COPY(FAudioCodecDesc)
  // codecs with `hasGoodSignature` will be checked last
  FAudioCodecDesc (const char *InDescription, CreatorFn InCreator, int prio)
    : Description(InDescription)
    , Creator(InCreator)
    , Priority(prio)
  {
    InsertIntoList(List, this);
  }
};

// priority for decoders without a signature
#define AUDIO_DEFAULT_PRIO  (100)

// priority for decoders without a signature
#define AUDIO_NO_SIGNATURE  (1000)

// audio codec registration helper
#define IMPLEMENT_AUDIO_CODEC(TClass,Description) \
  FAudioCodecDesc TClass##Desc(Description, TClass::Create, AUDIO_DEFAULT_PRIO);

#define IMPLEMENT_AUDIO_CODEC_EX(TClass,Description,Prio) \
  FAudioCodecDesc TClass##Desc(Description, TClass::Create, Prio);


// quick MUS to MIDI converter
class VQMus2Mid {
private:
  struct VTrack {
    vint32 DeltaTime;
    vuint8 LastEvent;
    vint8 Vel;
    TArrayNC<vuint8> Data; // primary data
  };

  VTrack Tracks[32];
  vuint16 TrackCnt;
  vint32 Mus2MidChannel[16];

  static const vuint8 Mus2MidControl[15];
  static const vuint8 TrackEnd[];
  static const vuint8 MidiKey[];
  static const vuint8 MidiTempo[];

  int FirstChannelAvailable ();
  void TWriteByte (int, vuint8);
  void TWriteBuf (int, const vuint8 *, int);
  void TWriteVarLen (int, vuint32);
  vuint32 ReadTime (VStream &);
  bool Convert (VStream &);
  void WriteMIDIFile (VStream &);
  void FreeTracks ();

public:
  int Run (VStream &, VStream &);
};


// music player (controls streaming thread)
class VStreamMusicPlayer {
  friend class VStreamMusicPlayerWorker;
private:
  // stream player is using a separate thread
  mythread stpThread;
  mythread_mutex stpPingLock;
  mythread_cond stpPingCond;
  mythread_mutex stpLockPong;
  mythread_cond stpCondPong;
  float lastVolume;
  bool threadInited;
  char namebuf[1024];

private:
  //WARNING! never use/access those directly!
  bool StrmOpened;
  VAudioCodec *Codec;
  // current playing song info
  bool CurrLoop; // access with data locked
  VStr CurrSong; // access with data locked
  bool Stopping;
  bool Paused;
  double FinishTime;
  VOpenALDevice *SoundDevice; // `nullptr` means "shutdown called"

private:
  atomic_int loopCounter;
  atomic_int dataSpinlock;

  // streamer thread ping/pong bussiness
  enum STPCommand {
    STP_Quit, // stop playing, and quit immediately
    STP_Start, // start playing current stream
    STP_Restart, // restart playing current stream
    STP_Stop, // stop current stream
    STP_Pause, // pause current stream
    STP_Resume, // resume current stream
    STP_IsPlaying, // check if current stream is playing
    STP_SetPitch, // change stream pitch
    STP_SetVolume,
    // the following two commands will replace current music with the new one
    // music name is in `namebuf`
    STP_PlaySong,
    STP_PlaySongLooped,
  };
  volatile STPCommand stpcmd;
  volatile bool stpIsPlaying; // it will return `STP_IsPlaying` result here
  volatile float stpNewPitch;
  volatile float stpNewVolume;

  bool stpThreadWaitPing (unsigned int msecs);
  void stpThreadSendPong ();

  void stpThreadSendCommand (STPCommand acmd);

  inline void LockData () noexcept {
    // this returns old value; wait unit it will be zero
    while (atomic_cmp_xchg(&dataSpinlock, 0, 1)) {}
  }

  inline void UnlockData () noexcept {
    atomic_set(&dataSpinlock, 0);
  }

  struct DataLocker {
    VStreamMusicPlayer *plr;
    inline DataLocker (VStreamMusicPlayer *aplr) noexcept : plr(aplr) { plr->LockData(); }
    inline ~DataLocker () noexcept { plr->UnlockData(); }
    DataLocker (const DataLocker &) = delete;
    DataLocker &operator = (const DataLocker &) = delete;
  };

public:
  inline VStreamMusicPlayer (VOpenALDevice *InSoundDevice)
    : lastVolume(1.0f)
    , threadInited(false)
    , StrmOpened(false)
    , Codec(nullptr)
    , CurrLoop(false)
    , Stopping(false)
    , Paused(false)
    , SoundDevice(InSoundDevice)
    , loopCounter(0)
    , dataSpinlock(0)
    , stpIsPlaying(false)
    , stpNewPitch(1.0f)
    , stpNewVolume(1.0f)
  {}

  inline ~VStreamMusicPlayer () { Shutdown(); }

  void Init ();
  void Shutdown ();
  void Play (VAudioCodec *InCodec, const char *InName, bool InLoop);
  void Pause ();
  void Resume ();
  void Stop ();
  void Restart ();
  bool IsPlaying ();
  void SetPitch (float pitch);
  void SetVolume (float volume, bool fromStreamThread=false);

  VStr GetCurrentSong ();
  bool IsCurrentSongLooped ();
  float GetNewVolume ();

  // all play functions will reset loop counter
  inline void ResetLoopCounter () noexcept { atomic_set(&loopCounter, 0); }
  inline int GetLoopCounter () noexcept { return atomic_get(&loopCounter); }
  inline void IncLoopCounter () noexcept { atomic_increment(&loopCounter); }

  void LoadAndPlay (const char *InName, bool InLoop);
};

class VStreamMusicPlayerWorker {
  friend class VStreamMusicPlayer;
private:
  static bool doTick (VStreamMusicPlayer *strm);
  static MYTHREAD_RET_TYPE streamPlayerThread (void *adevobj);
};


//**************************************************************************
//
//  MIDI and MUS file header structures.
//
//**************************************************************************

#define MUSMAGIC   "MUS\032"
#define MIDIMAGIC  "MThd"

#pragma pack(1)

struct FMusHeader {
  char ID[4]; // identifier "MUS" 0x1A
  vuint16 ScoreSize;
  vuint16 ScoreStart;
  vuint16 NumChannels;  // count of primary channels
  vuint16 NumSecChannels; // count of secondary channels (?)
  vuint16 InstrumentCount;
  vuint16 Dummy;
};

struct MIDheader {
  char ID[4];
  vuint32 hdr_size;
  vuint16 type;
  vuint16 num_tracks;
  vuint16 divisions;
};

#pragma pack()


// ////////////////////////////////////////////////////////////////////////// //
// SMF parser
class MIDIData {
public:
  enum { MIDI_MAX_CHANNEL = 16, };

  enum /*event type*/ {
    MIDI_NOOP = 0, // special synthesized event type
    // channel messages
    NOTE_OFF = 0x80,
    NOTE_ON = 0x90,
    KEY_PRESSURE = 0xa0,
    CONTROL_CHANGE = 0xb0,
    PROGRAM_CHANGE = 0xc0,
    CHANNEL_PRESSURE = 0xd0,
    PITCH_BEND = 0xe0,
    // system exclusive
    MIDI_SYSEX = 0xf0,
    MIDI_EOX = 0xf7,
    // meta event
    MIDI_META_EVENT = 0xff,
  };

  enum /*metaevent*/ {
    MIDI_SEQ_NUM = 0x00,
    MIDI_TEXT = 0x01,
    MIDI_COPYRIGHT = 0x02,
    MIDI_TRACK_NAME = 0x03,
    MIDI_INST_NAME = 0x04,
    MIDI_LYRIC = 0x05,
    MIDI_MARKER = 0x06,
    MIDI_CUE_POINT = 0x07,
    MIDI_CHANNEL = 0x20, // channel for the following meta
    MIDI_EOT = 0x2f,
    MIDI_SET_TEMPO = 0x51,
    MIDI_SMPTE_OFFSET = 0x54,
    MIDI_TIME_SIGNATURE = 0x58,
    MIDI_KEY_SIGNATURE = 0x59,
    MIDI_SEQUENCER_EVENT = 0x7f,
  };

  struct MidiTrack {
  private:
    vint32 datasize;
    const vuint8 *tkdata;
    const vuint8 *pos;
    MIDIData *song;
    double nextetime; // milliseconds
    // after last track command, wait a little
    double fadeoff;

  public:
    vuint8 runningStatus;
    vuint8 lastMetaChannel; // 0xff: all
    VStr copyright; // track copyright
    VStr tname; // track name
    VStr iname; // instrument name

  public:
    MidiTrack () : datasize(0), tkdata(nullptr), pos(nullptr), song(nullptr), nextetime(0), fadeoff(0), runningStatus(0), lastMetaChannel(0xff) {}

    void setup (MIDIData *asong, const vuint8 *adata, vint32 alen) {
      if (alen <= 0) adata = nullptr;
      if (!adata) alen = 0;
      datasize = alen;
      tkdata = adata;
      song = asong;
      reset();
    }

    inline void abort (bool full) { if (tkdata) pos = tkdata+datasize; if (full) fadeoff = 0; }

    inline void reset () {
      pos = tkdata;
      nextetime = 0;
      fadeoff = (song->type != 2 ? 100 : 0);
      runningStatus = 0;
      lastMetaChannel = 0xff;
      copyright.clear();
      tname.clear();
      iname.clear();
    }

    inline bool isEndOfData () const { return (!tkdata || pos == tkdata+datasize); }
    inline bool isEOT () const { return (isEndOfData() ? (nextEventTime() <= song->currtime) : false); }

    inline int getPos () const { return (tkdata ? (int)(ptrdiff_t)(pos-tkdata) : 0); }
    inline int getLeft () const { return (tkdata ? (int)(ptrdiff_t)(tkdata+datasize-pos) : 0); }

    inline const vuint8 *getCurPosPtr () const { return pos; }

    inline int size () const { return datasize; }
    inline vuint8 dataAt (int pos) const { return (tkdata && datasize > 0 && pos >= 0 && pos < datasize ? tkdata[pos] : 0); }
    inline vuint8 operator [] (int ofs) const {
      if (!tkdata || !datasize) return 0;
      if (ofs >= 0) {
        const int left = getLeft();
        if (ofs >= left) return 0;
        return pos[ofs];
      } else {
        if (ofs == MIN_VINT32) return 0;
        ofs = -ofs;
        const int pos = getLeft();
        if (ofs > pos) return 0;
        return tkdata[pos-ofs];
      }
    }

    inline MIDIData *getSong () { return song; }
    inline const MIDIData *getSong () const { return song; }

    inline vuint8 peekNextMidiByte () {
      if (isEndOfData()) return 0;
      return *pos;
    }

    inline vuint8 getNextMidiByte () {
      if (isEndOfData()) return 0;
      return *pos++;
    }

    inline void skipNextMidiBytes (int len) {
      while (len-- > 0) (void)getNextMidiByte();
    }

    // reads a variable-length SMF number
    vuint32 readVarLen () {
      vuint32 res = 0;
      int left = 4;
      for (;;) {
        if (left == 0) { abort(true); return 0; }
        --left;
        vuint8 t = getNextMidiByte();
        res = (res<<7)|(t&0x7fu);
        if ((t&0x80) == 0) break;
      }
      return res;
    }

    inline vuint32 getDeltaTic () { return readVarLen(); }

    inline double nextEventTime () const { return (nextetime+(isEndOfData() ? fadeoff : 0)); }

    inline void advanceTics (vuint32 tics) {
      if (isEndOfData()) return;
      nextetime += song->tic2ms(tics);
    }
  };

public:
  struct MidiEvent {
    vuint8 type = MIDI_NOOP;
    vuint8 channel = 0;
    vuint32 data1 = 0; // payload size for MIDI_SYSEX and MIDI_SEQUENCER_EVENT
    vuint32 data2 = 0;
    // this is for MIDI_SYSEX and MIDI_SEQUENCER_EVENT
    const void *payload = nullptr;
  };

  // timemsecs -- time for the current event (need not to monotonically increase, can jump around)
  // note: time can jump around due to events from different tracks
  typedef void (*EventCBType) (double timemsecs, const MidiEvent &ev, void *udata);

protected:
  // midi song info
  TArray<MidiTrack> tracks;
  vuint16 type;
  vuint16 delta;
  vuint32 tempo;
  double currtime; // milliseconds
  int currtrack; // for midi type 2
  // loaded MIDI data
  vuint8 *midiData;
  int dataSize;

protected:
  inline void setTempo (vint32 atempo) {
    if (atempo <= 0) atempo = 480000;
    tempo = atempo;
  }

  inline double tic2ms (vuint32 tic) const { return (((double)tic*tempo)/(delta*1000.0)); }
  inline double ms2tic (double msec) const { return ((msec*delta*1000.0)/(double)tempo); }

  bool parseMem ();

  bool runTrack (int tidx, EventCBType cb, void *udata);

public:
  VV_DISABLE_COPY(MIDIData)
  MIDIData ();
  ~MIDIData ();

  inline bool isValid () const { return (type != 0xff && delta != 0 && tempo != 0); }
  inline bool isEmpty () const { return (!isValid() || tracks.length() == 0); }

  void clear ();

  static bool isMidiStream (VStream &strm);
  bool parseStream (VStream &strm);

public:
  // <0: error
  //  0: done decoding
  // >0: number of frames one can generate until next event
  int decodeStep (EventCBType cb, int SampleRate, void *udata);

  bool isFinished ();

  void restart ();
  void abort ();
};


extern VCvarB snd_sf2_autoload;
extern VCvarS snd_sf2_file;

extern bool SoundHasBadApple;

extern TArray<VStr> sf2FileList;

bool SF2_NeedDiskScan ();
void SF2_SetDiskScanned (bool v);
void SF2_ScanDiskBanks (); // this fills `sf2FileList`


#endif
