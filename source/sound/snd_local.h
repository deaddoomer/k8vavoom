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

#ifndef _S_LOCAL_H
#define _S_LOCAL_H

//  Sound device types.
//??? Should Default be replaced with all default drivers?
enum
{
  SNDDRV_Default,
  SNDDRV_OpenAL,

  SNDDRV_MAX
};

//  Midi device types.
enum
{
  MIDIDRV_Default,

  MIDIDRV_MAX
};

//  CD audio device types.
enum
{
  CDDRV_Default,

  CDDRV_MAX
};

enum ESSCmds
{
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

//
// SoundFX struct.
//
struct sfxinfo_t
{
  VName TagName;    // Name, by whitch sound is recognised in script
  int   LumpNum;        // lump number of sfx

  int   Priority;   // Higher priority takes precendence
  int   NumChannels;  // total number of channels a sound type may occupy
  float ChangePitch;
  int   UseCount;
  int   Link;
  int *Sounds;     // For random sounds, Link is count.

  bool  bRandomHeader;
  bool  bPlayerReserve;
  bool  bSingular;

  vuint32 SampleRate;
  int   SampleBits;
  vuint32 DataSize;
  void *Data;
};

struct seq_info_t
{
  VName   Name;
  VName   Slot;
  vint32 *Data;
  vint32    StopSound;
};

enum
{
  REVERBF_DecayTimeScale      = 0x01,
  REVERBF_ReflectionsScale    = 0x02,
  REVERBF_ReflectionsDelayScale = 0x04,
  REVERBF_ReverbScale       = 0x08,
  REVERBF_ReverbDelayScale    = 0x10,
  REVERBF_DecayHFLimit      = 0x20,
  REVERBF_EchoTimeScale     = 0x40,
  REVERBF_ModulationTimeScale   = 0x80,
};

struct VReverbProperties
{
  int         Environment;
  float       EnvironmentSize;
  float       EnvironmentDiffusion;
  int         Room;
  int         RoomHF;
  int         RoomLF;
  float       DecayTime;
  float       DecayHFRatio;
  float       DecayLFRatio;
  int         Reflections;
  float       ReflectionsDelay;
  float       ReflectionsPanX;
  float       ReflectionsPanY;
  float       ReflectionsPanZ;
  int         Reverb;
  float       ReverbDelay;
  float       ReverbPanX;
  float       ReverbPanY;
  float       ReverbPanZ;
  float       EchoTime;
  float       EchoDepth;
  float       ModulationTime;
  float       ModulationDepth;
  float       AirAbsorptionHF;
  float       HFReference;
  float       LFReference;
  float       RoomRolloffFactor;
  float       Diffusion;
  float       Density;
  int         Flags;
};

struct VReverbInfo
{
  VReverbInfo *Next;
  const char *Name;
  int         Id;
  bool        Builtin;
  VReverbProperties Props;
};

//
//  VSoundDevice
//
//  Sound device interface. This class implements dummy driver.
//
class VSoundDevice : public VInterface
{
public:
  VSoundDevice() {}

  //  VSoundDevice interface.
  virtual bool Init() = 0;
  virtual int SetChannels(int) = 0;
  virtual void Shutdown() = 0;
  virtual void Tick(float) = 0;
  virtual int PlaySound(int, float, float, float, bool) = 0;
  virtual int PlaySound3D(int, const TVec&, const TVec&, float, float, bool) = 0;
  virtual void UpdateChannel3D(int, const TVec&, const TVec&) = 0;
  virtual bool IsChannelPlaying(int) = 0;
  virtual void StopChannel(int) = 0;
  virtual void UpdateListener(const TVec&, const TVec&, const TVec&,
    const TVec&, const TVec&, VReverbInfo*) = 0;

  virtual bool OpenStream(int, int, int) = 0;
  virtual void CloseStream() = 0;
  virtual int GetStreamAvailable() = 0;
  virtual short *GetStreamBuffer() = 0;
  virtual void SetStreamData(short*, int) = 0;
  virtual void SetStreamVolume(float) = 0;
  virtual void PauseStream() = 0;
  virtual void ResumeStream() = 0;
};

//  Describtion of a sound driver.
struct FSoundDeviceDesc
{
  const char *Name;
  const char *Description;
  const char *CmdLineArg;
  VSoundDevice *(*Creator)();

  FSoundDeviceDesc(int Type, const char *AName, const char *ADescription,
    const char *ACmdLineArg, VSoundDevice *(*ACreator)());
};

//  Sound device registration helper.
#define IMPLEMENT_SOUND_DEVICE(TClass, Type, Name, Description, CmdLineArg) \
VSoundDevice *Create##TClass() \
{ \
  return new TClass(); \
} \
FSoundDeviceDesc TClass##Desc(Type, Name, Description, CmdLineArg, Create##TClass);

//
//  VMidiDevice
//
//  Midi player device interface. This class implements dummy driver.
//
class VMidiDevice : public VInterface
{
public:
  bool    Initialised;
  //  Current playing song info.
  bool    CurrLoop;
  VName   CurrSong;

  VMidiDevice()
  : Initialised(false)
  , CurrLoop(false)
  {}

  //  VMidiDevice interface.
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void SetVolume(float) = 0;
  virtual void Tick(float) = 0;
  virtual void Play(void*, int, const char*, bool) = 0;
  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void Stop() = 0;
  virtual bool IsPlaying() = 0;
};

//  Describtion of a midi driver.
struct FMidiDeviceDesc
{
  const char *Name;
  const char *Description;
  const char *CmdLineArg;
  VMidiDevice *(*Creator)();

  FMidiDeviceDesc(int Type, const char *AName, const char *ADescription,
    const char *ACmdLineArg, VMidiDevice *(*ACreator)());
};

//  Midi device registration helper.
#define IMPLEMENT_MIDI_DEVICE(TClass, Type, Name, Description, CmdLineArg) \
VMidiDevice *Create##TClass() \
{ \
  return new TClass(); \
} \
FMidiDeviceDesc TClass##Desc(Type, Name, Description, CmdLineArg, Create##TClass);

//
//  VCDAudioDevice
//
//  CD player device interface. This class implements dummy driver.
//
class VCDAudioDevice : public VInterface
{
public:
  bool    Initialised;
  bool    Enabled;
  bool    CDValid;
  bool    Playing;
  bool    WasPlaying;
  bool    PlayLooping;
  int     PlayTrack;
  int     MaxTrack;
  int     Remap[100];

  VCDAudioDevice()
  : Initialised(false)
  , Enabled(false)
  , CDValid(false)
  , Playing(false)
  , WasPlaying(false)
  , PlayLooping(false)
  , PlayTrack(0)
  , MaxTrack(0)
  {}

  //  VCDAudioDevice interface.
  virtual void Init() = 0;
  virtual void Update() = 0;
  virtual void Shutdown() = 0;
  virtual void GetInfo() = 0;
  virtual void Play(int, bool) = 0;
  virtual void Pause() = 0;
  virtual void Resume() = 0;
  virtual void Stop() = 0;
  virtual void OpenDoor() = 0;
  virtual void CloseDoor() = 0;
};

//  Describtion of a CD driver.
struct FCDAudioDeviceDesc
{
  const char *Name;
  const char *Description;
  const char *CmdLineArg;
  VCDAudioDevice *(*Creator)();

  FCDAudioDeviceDesc(int Type, const char *AName, const char *ADescription,
    const char *ACmdLineArg, VCDAudioDevice *(*ACreator)());
};

//  CD audio device registration helper.
#define IMPLEMENT_CD_AUDIO_DEVICE(TClass, Type, Name, Description, CmdLineArg) \
VCDAudioDevice *Create##TClass() \
{ \
  return new TClass(); \
} \
FCDAudioDeviceDesc TClass##Desc(Type, Name, Description, CmdLineArg, Create##TClass);

//  Loader of sound samples.
class VSampleLoader : public VInterface
{
public:
  VSampleLoader *Next;

  static VSampleLoader *List;

  VSampleLoader()
  {
    Next = List;
    List = this;
  }
  virtual void Load(sfxinfo_t&, VStream&) = 0;
};

//  Streamed audio decoder interface.
class VAudioCodec : public VInterface
{
public:
  int     SampleRate;
  int     SampleBits;
  int     NumChannels;

  VAudioCodec()
  : SampleRate(44100)
  , SampleBits(16)
  , NumChannels(2)
  {}
  virtual int Decode(short*, int) = 0;
  virtual bool Finished() = 0;
  virtual void Restart() = 0;
};

//  Description of an audio codec.
struct FAudioCodecDesc
{
  const char *Description;
  VAudioCodec *(*Creator)(VStream*);
  FAudioCodecDesc *Next;

  static FAudioCodecDesc *List;

  FAudioCodecDesc(const char *InDescription, VAudioCodec *(*InCreator)(VStream*))
  : Description(InDescription)
  , Creator(InCreator)
  {
    Next = List;
    List = this;
  }
};

//  Audio codec registration helper.
#define IMPLEMENT_AUDIO_CODEC(TClass, Description) \
FAudioCodecDesc   TClass##Desc(Description, TClass::Create);

//  Quick MUS to MIDI converter.
class VQMus2Mid
{
private:
  struct VTrack
  {
    vint32        DeltaTime;
    vuint8        LastEvent;
    vint8       Vel;
    TArray<vuint8>    Data; //  Primary data
  };

  VTrack          Tracks[32];
  vuint16         TrackCnt;
  vint32          Mus2MidChannel[16];

  static const vuint8   Mus2MidControl[15];
  static const vuint8   TrackEnd[];
  static const vuint8   MidiKey[];
  static const vuint8   MidiTempo[];

  int FirstChannelAvailable();
  void TWriteByte(int, vuint8);
  void TWriteBuf(int, const vuint8*, int);
  void TWriteVarLen(int, vuint32);
  vuint32 ReadTime(VStream&);
  bool Convert(VStream&);
  void WriteMIDIFile(VStream&);
  void FreeTracks();

public:
  int Run(VStream&, VStream&);
};

class VStreamMusicPlayer
{
public:
  bool      StrmOpened;
  VAudioCodec *Codec;
  //  Current playing song info.
  bool      CurrLoop;
  VName     CurrSong;
  bool      Stopping;
  bool      Paused;
  double      FinishTime;
  VSoundDevice *SoundDevice;

  VStreamMusicPlayer(VSoundDevice *InSoundDevice)
  : StrmOpened(false)
  , Codec(nullptr)
  , CurrLoop(false)
  , Stopping(false)
  , Paused(false)
  , SoundDevice(InSoundDevice)
  {}
  ~VStreamMusicPlayer()
  {}

  void Init();
  void Shutdown();
  void Tick(float);
  void Play(VAudioCodec *InCodec, const char *InName, bool InLoop);
  void Pause();
  void Resume();
  void Stop();
  bool IsPlaying();
};

//**************************************************************************
//
//  MIDI and MUS file header structures.
//
//**************************************************************************

#define MUSMAGIC        "MUS\032"
#define MIDIMAGIC       "MThd"

#pragma pack(1)

struct FMusHeader
{
  char    ID[4];      // identifier "MUS" 0x1A
  vuint16   ScoreSize;
  vuint16   ScoreStart;
  vuint16   NumChannels;  // count of primary channels
  vuint16   NumSecChannels; // count of secondary channels (?)
  vuint16   InstrumentCount;
  vuint16   Dummy;
};

struct MIDheader
{
  char    ID[4];
  vuint32   hdr_size;
  vuint16   type;
  vuint16   num_tracks;
  vuint16   divisions;
};

#pragma pack()

#endif
