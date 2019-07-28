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

enum seqtype_t {
  SEQ_Door,
  SEQ_Platform,
  SEQ_Environment,
};

struct sfxinfo_t;
struct seq_info_t;
#if defined(VAVOOM_REVERB)
struct VReverbInfo;
#endif

enum ESoundType {
  SNDTYPE_World    = 0,
  SNDTYPE_Point    = 1,
  SNDTYPE_Surround = 2,

  SNDTYPE_Continuous = 4,
  SNDTYPE_Random     = 8,
  SNDTYPE_Periodic   = 12,
};

struct FAmbientSound {
  vuint32 Type; // type of ambient sound
  float PeriodMin; // # of tics between repeats
  float PeriodMax; // max # of tics for random ambients
  float Volume; // relative volume of sound
  float Attenuation;
  VName Sound; // logical name of sound to play
};

// rolloff information.
struct FRolloffInfo {
  int RolloffType;
  float MinDistance;
  union { float MaxDistance; float RolloffFactor; };
};


// handles list of registered sound and sound sequences
class VSoundManager {
public:
  enum { LS_Error = -1, LS_Pending = 0, LS_Ready = 1 };

public:
  // the complete set of sound effects
  TArray<sfxinfo_t> S_sfx; // 0 is reserved
  TArray<seq_info_t> SeqInfo;
  TMap<VStr, int> sfxMap; // name is lowercased

protected:
  TMap<VStr, bool> sfxMissingReported; // name is lowercased

  void SoundJustUsed (int idx);

public:
  VSoundManager ();
  ~VSoundManager ();

  void Init ();
  int GetSoundID (VName Name);
  int ResolveSound (int);
  int ResolveEntitySound (VName, VName, VName);
  bool IsSoundPresent (VName, VName, VName);
  int LoadSound (int sound_id); // returns LS_XXX
  void DoneWithLump (int);
  float GetMusicVolume (const char *SongName);
  FAmbientSound *GetAmbientSound (int);

  void SetSeqTrans (VName, int, int);
  VName GetSeqTrans (int, int);
  VName GetSeqSlot (VName);
  int FindSequence (VName);

  void GetSoundLumpNames (TArray<FReplacedString> &);
  void ReplaceSoundLumpNames (TArray<FReplacedString> &);

  // call this when loading a new map
  void CleanupSounds ();

#if defined(VAVOOM_REVERB)
  VReverbInfo *FindEnvironment (int);
#endif

  // call this *VERY* often, so pending sounds can be sent to player
  void Process ();

public:
  struct VMusicAlias {
    VName origName;
    VName newName;
    int fileid;
  };

  TArray<VMusicAlias> MusicAliases;

private:
  struct FPlayerSound {
    int ClassId;
    int GenderId;
    int RefId;
    int SoundId;
  };

  enum { NUM_AMBIENT_SOUNDS = 256 };

  struct VMusicVolume {
    VName SongName;
    float Volume;
  };

  TArray<VName> PlayerClasses;
  TArray<VName> PlayerGenders;
  TArray<FPlayerSound> PlayerSounds;
  int NumPlayerReserves;
  float CurrentChangePitch;
  FRolloffInfo CurrentDefaultRolloff;
  FAmbientSound *AmbientSounds[NUM_AMBIENT_SOUNDS];
  TArray<VMusicVolume> MusicVolumes;
  int SeqTrans[64*3];
#if defined(VAVOOM_REVERB)
  VReverbInfo *Environments;
#endif

  void ParseSndinfo (VScriptParser *sc, int fileid);
  int AddSoundLump(VName, int);
  int AddSound(VName, int);
  int FindSound(VName);
  int FindOrAddSound(VName);
  void ParsePlayerSoundCommon(VScriptParser*, int&, int&, int&);
  int AddPlayerClass(VName);
  int FindPlayerClass(VName);
  int AddPlayerGender(VName);
  int FindPlayerGender(VName);
  int FindPlayerSound(int, int, int);
  int LookupPlayerSound(int, int, int);
  int ResolveSound(int, int, int);

  void ParseSequenceScript(VScriptParser*);
  void AssignSeqTranslations(VScriptParser*, int, seqtype_t);

  void ParseReverbs(VScriptParser*);
};

//
//  VAudio
//
//  Main audio management class.
//
class VAudioPublic : public VInterface
{
public:
  //  Top level methods.
  virtual void Init() = 0;
  virtual void Shutdown() = 0;

  //  Playback of sound effects
  virtual void PlaySound(int, const TVec&, const TVec&, int, int, float,
    float, bool) = 0;
  virtual void StopSound(int, int) = 0;
  virtual void StopAllSound() = 0;
  virtual bool IsSoundPlaying(int, int) = 0;

  //  Music and general sound control
  virtual void StartSong(VName, bool) = 0;
  virtual void PauseSound() = 0;
  virtual void ResumeSound() = 0;
  virtual void Start() = 0;
  virtual void MusicChanged() = 0;
  virtual void UpdateSounds() = 0;

  //  Sound sequences
  virtual void StartSequence(int, const TVec&, VName, int) = 0;
  virtual void AddSeqChoice(int, VName) = 0;
  virtual void StopSequence(int) = 0;
  virtual void UpdateActiveSequences(float) = 0;
  virtual void StopAllSequences() = 0;
  virtual void SerialiseSounds(VStream&) = 0;

  static VAudioPublic *Create();
};
extern VCvarI snd_mid_player;
extern VCvarI snd_mod_player;

extern VSoundManager *GSoundManager;
extern VAudioPublic *GAudio;
