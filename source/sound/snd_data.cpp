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
#include "gamedefs.h"
#include "snd_local.h"

#define VV_ALLOW_SFX_TRUNCATION

static VCvarB snd_verbose_truncate("snd_verbose_truncate", false, "Show silence-truncated sounds?", CVAR_Archive);

#ifdef _WIN32
# define VV_SND_BACKLOAD  false
#else
//k8: it seems to be weirdly unstable. sigh.
//# define VV_SND_BACKLOAD  true
# define VV_SND_BACKLOAD  false
#endif
VCvarB snd_background_loading("snd_background_loading", VV_SND_BACKLOAD, "Load sounds in background thread?", /*CVAR_Archive|*/CVAR_PreInit);
VCvarB snd_music_background_load("snd_music_background_load", VV_SND_BACKLOAD, "Load music in the background thread?", /*CVAR_Archive|*/CVAR_PreInit);


#ifdef CLIENT
class VRawSampleLoader : public VSampleLoader {
public:
  virtual void Load (sfxinfo_t &, VStream &) override;
};
#endif


VSampleLoader *VSampleLoader::List;
VSoundManager *GSoundManager;

#ifdef CLIENT
static VRawSampleLoader RawSampleLoader;
#endif


static volatile bool sndThreadDebug = false;


static int cli_DebugSoundMT = 0;

/*static*/ bool cliRegister_snddata_args =
  VParsedArgs::RegisterFlagSet("-debug-sound-threading", "!show debug messages from MT sound loader", &cli_DebugSoundMT);


//==========================================================================
//
//  IsNoneSoundName
//
//==========================================================================
static inline bool IsNoneSoundName (const char *name) {
  return
    !name || !name[0] ||
    VStr::strEquCI(name, "None") ||
    VStr::strEquCI(name, "Null") ||
    VStr::strEquCI(name, "nil");
};


//==========================================================================
//
//  VSampleLoader::LoadFromAudioCodec
//
//  codec must be initialized, and it will not be owned
//
//==========================================================================
void VSampleLoader::LoadFromAudioCodec (sfxinfo_t &Sfx, VAudioCodec *Codec) {
  if (!Codec) return;
  //fprintf(stderr, "loading from audio codec; chans=%d; rate=%d; bits=%d\n", Codec->NumChannels, Codec->SampleRate, Codec->SampleBits);
  const int MAX_FRAMES = 65536;

  TArray<short> Data;
  short *buf = (short *)Z_Malloc(MAX_FRAMES*2*2);
  if (!buf) return; // oops
  do {
    int SamplesDecoded = Codec->Decode(buf, MAX_FRAMES);
    if (SamplesDecoded > 0) {
      int oldlen = Data.length();
      Data.SetNumWithReserve(oldlen+SamplesDecoded);
      // downmix stereo to mono
      const short *src = buf;
      short *dst = ((short *)Data.Ptr())+oldlen;
      for (int i = 0; i < SamplesDecoded; ++i, src += 2) {
        int v = (src[0]+src[1])/2;
        if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
        *dst++ = (short)v;
      }
    } else {
      break;
    }
  } while (!Codec->Finished());
  Z_Free(buf);

  if (Data.length() < 1) return;

  // we don't care about timing, so trim trailing silence
#ifdef VV_ALLOW_SFX_TRUNCATION
  int realLen = Data.length()-1;
  while (realLen >= 0 && Data[realLen] > -64 && Data[realLen] < 64) --realLen;
  ++realLen;
  if (realLen == 0) realLen = 1;

  if (realLen < Data.length() && Sfx.LumpNum >= 0) {
    if (snd_verbose_truncate) {
      GCon->Logf("SOUND: '%s' is truncated by %d silent frames (%d frames left)", *W_FullLumpName(Sfx.LumpNum), Data.length()-realLen, realLen);
    }
  }
#else
  int realLen = Data.length();
#endif


  // copy parameters
  Sfx.SampleRate = Codec->SampleRate;
  Sfx.SampleBits = Codec->SampleBits;

  // copy data
  Sfx.DataSize = realLen*2;
  Sfx.Data = (vuint8 *)Z_Malloc(realLen*2);
  memcpy(Sfx.Data, Data.Ptr(), realLen*2);
}


//==========================================================================
//
//  VSoundManager::VSoundManager
//
//==========================================================================
VSoundManager::VSoundManager ()
  : NumPlayerReserves(0)
  , CurrentChangePitch(0) //7.0f / 255.0f)
{
  CurrentDefaultRolloff.RolloffType = ROLLOFF_Doom;
  CurrentDefaultRolloff.MinDistance = 0;
  CurrentDefaultRolloff.MaxDistance = 0;
  memset(AmbientSounds, 0, sizeof(AmbientSounds));
  loaderThreadStarted = false;
}


//==========================================================================
//
//  VSoundManager::~VSoundManager
//
//==========================================================================
VSoundManager::~VSoundManager () {
  // wait for thread deinit
  if (loaderThreadStarted) {
    GCon->Log("shutting down sound loader thread...");
    {
      if (developer) GCon->Log(NAME_Dev, "   ...getting lock...");
      MyThreadLocker lock(&loaderLock);
      if (developer) GCon->Log(NAME_Dev, "   ...setting variable...");
      loaderDoQuit = 1;
      if (developer) GCon->Log(NAME_Dev, "   ...relasing lock...");
    }
    // the signal will be delivered when the mutex is released
    if (developer) GCon->Log(NAME_Dev, "   ...sending signal lock...");
    mythread_cond_signal(&loaderCond);
    if (developer) GCon->Log(NAME_Dev, "   ...joining threads...");
    /*
    for (;;) {
      {
        MyThreadLocker lock(&loaderLock);
        if (loaderDoQuit == 2) break;
      }
      Sys_Yield();
    }
    */
    mythread_join(loaderThread);
    if (developer) GCon->Log(NAME_Dev, "   ...resetting threading flag...");
    loaderThreadStarted = false;
    GCon->Log("sound loader thread shut down.");
  }

  for (int i = 0; i < S_sfx.length(); ++i) {
    if (S_sfx[i].Data) {
      Z_Free(S_sfx[i].Data);
      S_sfx[i].Data = nullptr;
    }
    if (S_sfx[i].Sounds) {
      delete[] S_sfx[i].Sounds;
      S_sfx[i].Sounds = nullptr;
    }
  }
  if (developer) GCon->Logf(NAME_Dev, "all sounds freed.");

  for (int i = 0; i < NUM_AMBIENT_SOUNDS; ++i) {
    if (AmbientSounds[i]) {
      delete AmbientSounds[i];
      AmbientSounds[i] = nullptr;
    }
  }
  if (developer) GCon->Logf(NAME_Dev, "all ambient sounds freed.");

  for (int i = 0; i < SeqInfo.length(); ++i) {
    if (SeqInfo[i].Data) {
      Z_Free(SeqInfo[i].Data);
      SeqInfo[i].Data = nullptr;
    }
  }
  if (developer) GCon->Logf(NAME_Dev, "all sound sequences freed.");

#if defined(VAVOOM_REVERB)
  for (VReverbInfo *R = Environments; R; ) {
    VReverbInfo *Next = R->Next;
    if (!R->Builtin) {
      delete[] const_cast<char*>(R->Name);
      R->Name = nullptr;
      delete R;
      R = nullptr;
    }
    R = Next;
  }
#endif
}


//==========================================================================
//
//  soundStreamThread
//
//==========================================================================
static MYTHREAD_RET_TYPE soundStreamThread (void *adevobj) {
  VSoundManager *sman = (VSoundManager *)adevobj;

  mythread_mutex_lock(&sman->loaderLock);
  sman->loaderIsIdle = true;
  if (sndThreadDebug) fprintf(stderr, "STRD: started...\n");

  for (;;) {
    mythread_cond_wait(&sman->loaderCond, &sman->loaderLock);
    // mutex is held again
    if (sman->loaderDoQuit) break;
    // check if we have something to do
    while (sman->queuedSounds.length() > 0) {
      if (sman->loaderDoQuit) break;
      // k8: dunno, maybe take the last appended sound, as it will prolly
      //     the one that the game needs right now.
      //     drawback: older sounds will be delayed even more.
      int sndid = sman->queuedSounds[0];
      if (sndThreadDebug) fprintf(stderr, "STRD: trying to load sound #%d (uc=%d) (%s : %s)\n", sndid, sman->S_sfx[sndid].UseCount, *sman->S_sfx[sndid].TagName, *W_FullLumpName(sman->S_sfx[sndid].LumpNum));
      mythread_mutex_unlock(&sman->loaderLock);
      bool ok = sman->LoadSoundInternal(sndid);
      mythread_mutex_lock(&sman->loaderLock);
      sman->queuedSoundsMap.del(sndid);
      sman->queuedSounds.removeAt(0);
      if (ok) {
        if (sndThreadDebug) fprintf(stderr, "STRD: loaded sound #%d (uc=%d) (%s : %s)\n", sndid, sman->S_sfx[sndid].UseCount, *sman->S_sfx[sndid].TagName, *W_FullLumpName(sman->S_sfx[sndid].LumpNum));
        // it loaded, check if it is still needed
        if (sman->S_sfx[sndid].UseCount == 0) {
          // not needed, forget it
          if (sndThreadDebug) fprintf(stderr, "STRD: ...and freed sound #%d (uc=%d) (%s : %s)\n", sndid, sman->S_sfx[sndid].UseCount, *sman->S_sfx[sndid].TagName, *W_FullLumpName(sman->S_sfx[sndid].LumpNum));
          Z_Free(sman->S_sfx[sndid].Data);
          sman->S_sfx[sndid].Data = nullptr;
          sman->S_sfx[sndid].loadedState = sfxinfo_t::ST_NotLoaded;
        } else {
          sman->S_sfx[sndid].loadedState = sfxinfo_t::ST_Loaded;
        }
      } else {
        if (sndThreadDebug) fprintf(stderr, "STRD: failed to load sound #%d (uc=%d) (%s : %s)\n", sndid, sman->S_sfx[sndid].UseCount, *sman->S_sfx[sndid].TagName, *W_FullLumpName(sman->S_sfx[sndid].LumpNum));
        // just in case
        if (sman->S_sfx[sndid].Data) {
          Z_Free(sman->S_sfx[sndid].Data);
          sman->S_sfx[sndid].Data = nullptr;
        }
        sman->S_sfx[sndid].loadedState = sfxinfo_t::ST_Invalid;
      }
      // put into ready list
      sman->readySounds.append(sndid);
    }
    if (sman->loaderDoQuit) break;
  }

  // mutex is held
  sman->loaderDoQuit = 2;
  mythread_mutex_unlock(&sman->loaderLock);
  Z_ThreadDone();
  return MYTHREAD_RET_VALUE;
}


//==========================================================================
//
//  VSoundManager::Init
//
//  Loads sound script lump
//
//==========================================================================
void VSoundManager::Init () {
  int Lump;

  // sound 0 is empty sound
  AddSoundLump(NAME_None, -1);

  // add Strife voices
  for (Lump = W_IterateNS(-1, WADNS_Voices); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Voices)) {
    char SndName[256];
    snprintf(SndName, sizeof(SndName), "svox/%s", *W_LumpName(Lump));
    int id = AddSoundLump(SndName, Lump);
    S_sfx[id].ChangePitch = 0;
    S_sfx[id].VolumeAmp = 1.0f;
  }

  // load SNDINFO script
  for (Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_sndinfo) {
      GCon->Logf(NAME_Init, "loading SNDINFO from '%s'...", *W_FullLumpName(Lump));
      ParseSndinfo(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), W_LumpFile(Lump));
    }
  }

  S_sfx.Condense();

  // load SNDSEQ script
  memset(SeqTrans, -1, sizeof(SeqTrans));
  for (Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_sndseq) {
      ParseSequenceScript(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
    }
  }

#if defined(VAVOOM_REVERB)
  // load REVERBS script
  Environments = nullptr;
  for (Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_reverbs) {
      ParseReverbs(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)));
    }
  }
#endif

  sndThreadDebug = !!cli_DebugSoundMT;
  loaderDoQuit = 0;
  loaderIsIdle = false;
  loaderThreadStarted = false;
  mythread_mutex_init(&loaderLock);
  mythread_cond_init(&loaderCond);
  queuedSounds.clear();
  queuedSoundsMap.clear();
  readySounds.clear();
  // create stream player thread
  if (snd_background_loading) {
    if (mythread_create(&loaderThread, &soundStreamThread, this)) {
      GCon->Logf(NAME_Warning, "cannot create sound streaming thread (this is harmless)");
      sndThreadDebug = false;
    } else {
      GCon->Log(NAME_Init, "starting sound loader thread...");
      loaderThreadStarted = true;
    }
  }
  // wait for thread initialisation
  if (loaderThreadStarted) {
    for (;;) {
      {
        MyThreadLocker lock(&loaderLock);
        if (loaderIsIdle) break;
      }
      Sys_Yield();
    }
    GCon->Log(NAME_Init, "sound loader thread initialised.");
  }
}


//==========================================================================
//
//  VSoundManager::ParseSndinfo
//
//==========================================================================
void VSoundManager::ParseSndinfo (VScriptParser *sc, int fileid) {
  TArray<int> list;
  bool insideIf = false;

  while (!sc->AtEnd()) {
    auto loc = sc->GetLoc();
    if (sc->Check("$archivepath")) {
      // $archivepath <directory>
      // ignored
      sc->ExpectString();
    } else if (sc->Check("$map")) {
      // $map <map number> <song name>
      sc->ExpectNumber();
      sc->ExpectName();
      if (sc->Number) P_PutMapSongLump(sc->Number, sc->Name);
    } else if (sc->Check("$registered")) {
      // $registered
      // unused
    } else if (sc->Check("$limit")) {
      // $limit <logical name> <max channels>
      sc->ExpectString();
      int sfx = FindOrAddSound(*sc->String);
      sc->ExpectNumber();
      S_sfx[sfx].NumChannels = midval(0, sc->Number, 255);
    } else if (sc->Check("$pitchshift")) {
      // $pitchshift <logical name> <pitch shift amount>
      sc->ExpectString();
      int sfx = FindOrAddSound(*sc->String);
      sc->ExpectNumber();
      S_sfx[sfx].ChangePitch = ((1<<midval(0, sc->Number, 7))-1)/255.0f;
    } else if (sc->Check("$pitchshiftrange")) {
      // $pitchshiftrange <pitch shift amount>
      sc->ExpectNumber();
      CurrentChangePitch = ((1<<midval(0, sc->Number, 7))-1)/255.0f;
    } else if (sc->Check("$alias")) {
      // $alias <name of alias> <name of real sound>
      sc->ExpectString();
      int sfxfrom = AddSound(*sc->String, -1);
      sc->ExpectString();
      //if (S_sfx[sfxfrom].bPlayerCompat) sfxfrom = S_sfx[sfxfrom].link;
      S_sfx[sfxfrom].Link = FindOrAddSound(*sc->String);
    } else if (sc->Check("$attenuation")) {
      // $attenuation <name of alias> value
      sc->ExpectString();
      int sfx = FindOrAddSound(*sc->String);
      sc->ExpectFloat();
      if (sc->Float < 0) sc->Float = 0;
      S_sfx[sfx].Attenuation = sc->Float;
    } else if (sc->Check("$random")) {
      // $random <logical name> { <logical name> ... }
      list.Clear();
      sc->ExpectString();
      int id = AddSound(*sc->String, -1);
      sc->Expect("{");
      while (!sc->Check("}")) {
        sc->ExpectString();
        int sfxto = FindOrAddSound(*sc->String);
        list.Append(sfxto);
      }
      if (list.length() == 1) {
        // only one sound: treat as $alias
        S_sfx[id].Link = list[0];
      } else if (list.length() > 1) {
        // only add non-empty random lists
        S_sfx[id].Link = list.length();
        S_sfx[id].Sounds = new int[list.length()];
        memcpy(S_sfx[id].Sounds, &list[0], sizeof(int)*list.length());
        S_sfx[id].bRandomHeader = true;
      }
    } else if (sc->Check("$playersound")) {
      // $playersound <player class> <gender> <logical name> <lump name>
      int PClass, Gender, RefId;
      VStr FakeName;
      int id;

      ParsePlayerSoundCommon(sc, PClass, Gender, RefId);
      FakeName = VStr(PlayerClasses[PClass]);
      FakeName += '|';
      FakeName += (char)(Gender+'0');
      FakeName += S_sfx[RefId].TagName;

      id = AddSoundLump(*FakeName, W_CheckNumForName(VName(*sc->String, VName::AddLower), WADNS_Sounds));

      FPlayerSound &PlrSnd = GetOrAddPlayerSound(PClass, Gender, RefId);
      PlrSnd.SoundId = id;
    } else if (sc->Check("$playersounddup")) {
      // $playersounddup <player class> <gender> <logical name> <target sound name>
      int PClass, Gender, RefId, TargId;

      ParsePlayerSoundCommon(sc, PClass, Gender, RefId);
      TargId = FindSound(*sc->String);
      if (!TargId) {
        TargId = AddSound(*sc->String, -1);
        S_sfx[TargId].Link = NumPlayerReserves++;
        S_sfx[TargId].bPlayerReserve = true;
      } else if (!S_sfx[TargId].bPlayerReserve) {
        sc->Error(va("%s is not a player sound", *sc->String));
      }
      int AliasTo = FindPlayerSound(PClass, Gender, TargId);

      FPlayerSound &PlrSnd = GetOrAddPlayerSound(PClass, Gender, RefId);
      PlrSnd.SoundId = AliasTo;
    } else if (sc->Check("$playeralias")) {
      // $playeralias <player class> <gender> <logical name> <logical name of existing sound>
      int PClass, Gender, RefId;

      ParsePlayerSoundCommon(sc, PClass, Gender, RefId);
      int AliasTo = FindOrAddSound(*sc->String);

      FPlayerSound &PlrSnd = GetOrAddPlayerSound(PClass, Gender, RefId);
      PlrSnd.SoundId = AliasTo;
    } else if (sc->Check("$playercompat")) {
      // $playercompat <playerclass> <gender> <logical name> <compatibility name>
      //FIXME: this totally doesn't work (i think); if it works, then i did something wrong
      int PClass, Gender, RefId;

      ParsePlayerSoundCommon(sc, PClass, Gender, RefId);
      FPlayerSound *psndp = GetPlayerSound(PClass, Gender, RefId);
      if (psndp) {
        //sc->ExpectString(); // already done in `ParsePlayerSoundCommon()`
        int linkId = FindOrAddSound(*sc->String);
        S_sfx[linkId].Link = psndp->SoundId;
        //GCon->Logf(NAME_Debug, "PLAYERCOMPAT: <%s> (%d) linked to <%s> (%d)", *S_sfx[linkId].TagName, linkId, *S_sfx[psndp->SoundId].TagName, psndp->SoundId);
      } else {
        sc->Message("trying to define '$playercompat' for unknown player sound");
      }
    } else if (sc->Check("$singular")) {
      // $singular <logical name>
      sc->ExpectString();
      int sfx = FindOrAddSound(*sc->String);
      S_sfx[sfx].bSingular = true;
    } else if (sc->Check("$ambient")) {
      // $ambient <num> <logical name> [point [atten] | surround | [world]]
      //      <continuous | random <minsecs> <maxsecs> | periodic <secs>>
      //      <volume>
      FAmbientSound *ambient, dummy;

      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number >= NUM_AMBIENT_SOUNDS) {
        sc->Message(va("Bad ambient index (%d)", sc->Number));
        ambient = &dummy;
      } else if (AmbientSounds[sc->Number]) {
        ambient = AmbientSounds[sc->Number];
      } else {
        ambient = new FAmbientSound;
        AmbientSounds[sc->Number] = ambient;
      }
      memset((void *)ambient, 0, sizeof(FAmbientSound));

      sc->ExpectString();
      ambient->Sound = *sc->String;
      ambient->Attenuation = 0;

      if (sc->Check("point")) {
        ambient->Type = SNDTYPE_Point;
        if (sc->CheckFloat()) {
          ambient->Attenuation = (sc->Float > 0 ? sc->Float : 1);
        } else {
          ambient->Attenuation = 1;
        }
      } else if (sc->Check("surround")) {
        ambient->Type = SNDTYPE_Surround;
        ambient->Attenuation = -1;
      } else if (sc->Check("world")) {
        // world is an optional keyword
      }

      if (sc->Check("continuous")) {
        ambient->Type |= SNDTYPE_Continuous;
      } else if (sc->Check("random")) {
        ambient->Type |= SNDTYPE_Random;
        sc->ExpectFloat();
        ambient->PeriodMin = sc->Float;
        sc->ExpectFloat();
        ambient->PeriodMax = sc->Float;
      } else if (sc->Check("periodic")) {
        ambient->Type |= SNDTYPE_Periodic;
        sc->ExpectFloat();
        ambient->PeriodMin = sc->Float;
      } else {
        sc->ExpectString();
        sc->Message(va("Unknown ambient type (%s)", *sc->String));
      }

      sc->ExpectFloat();
      ambient->Volume = sc->Float;
      if (ambient->Volume > 1) ambient->Volume = 1; else if (ambient->Volume < 0) ambient->Volume = 0;
    } else if (sc->Check("$musicvolume")) {
      sc->ExpectName();
      VName SongName = sc->Name;
      sc->ExpectFloat();
      int i;
      for (i = 0; i < MusicVolumes.length(); ++i) {
        if (MusicVolumes[i].SongName == SongName) {
          MusicVolumes[i].Volume = sc->Float;
          break;
        }
      }
      if (i == MusicVolumes.length()) {
        VMusicVolume &V = MusicVolumes.Alloc();
        V.SongName = SongName;
        V.Volume = sc->Float;
      }
    } else if (sc->Check("$musicalias")) {
      sc->ExpectName();
      VName SongName = sc->Name;
      sc->ExpectName();
      if (SongName != NAME_None && VStr::ICmp(*SongName, "none") != 0) {
        VMusicAlias &als = MusicAliases.alloc();
        als.origName = SongName;
        als.newName = (sc->Name == NAME_None || VStr::ICmp(*sc->Name, "none") == 0 ? NAME_None : sc->Name);
        als.fileid = fileid;
      }
    } else if (sc->Check("$endif")) {
      if (insideIf) {
        insideIf = false;
      } else {
        sc->Message("stray `$endif` in sound script");
      }
    } else if (sc->Check("$ifdoom") || sc->Check("$ifheretic") ||
               sc->Check("$ifhexen") || sc->Check("$ifstrife"))
    {
      //GCon->Log("Conditional SNDINFO commands are not supported");
      if (insideIf) {
        sc->Error(va("nested conditionals are not allowed (%s)", *sc->String));
      }
      //GCon->Logf(NAME_Warning, "%s: k8vavoom doesn't support conditional game commands in terrain script", *loc.toStringNoCol());
      VStr gmname = VStr(*sc->String+3);
      if (game_name.asStr().startsWithCI(gmname)) {
        insideIf = true;
        GCon->Logf(NAME_Init, "%s: processing conditional section '%s' in sound script", *loc.toStringNoCol(), *gmname);
      } else {
        // skip lines until we hit `endif`
        GCon->Logf(NAME_Init, "%s: skipping conditional section '%s' in sound script", *loc.toStringNoCol(), *gmname);
        while (sc->GetString()) {
          if (sc->Crossed) {
            if (sc->String.strEqu("endif")) {
              //GCon->Logf(NAME_Init, "******************** FOUND ENDIF!");
              break;
            }
          }
        }
      }
    } else if (sc->Check("$volume")) {
      // $volume soundname <volume>
      sc->ExpectString();
      int sfx = FindOrAddSound(*sc->String);
      sc->ExpectFloatWithSign();
      if (sc->Float < 0) sc->Float = 0; else if (sc->Float > 1) sc->Float = 1;
      S_sfx[sfx].VolumeAmp = sc->Float;
    } else if (sc->Check("$rolloff")) {
      // $rolloff *|<logical name> [linear|log|custom] <min dist> <max dist/rolloff factor>
      // Using * for the name makes it the default for sounds that don't specify otherwise.
      FRolloffInfo *rlf;
      sc->ExpectString();
      if (sc->String == "*") {
        rlf = &CurrentDefaultRolloff;
      } else {
        int sfx = FindOrAddSound(*sc->String);
        rlf = &S_sfx[sfx].Rolloff;
      }
      if (!sc->CheckFloat()) {
             if (sc->Check("linear")) rlf->RolloffType = ROLLOFF_Linear;
        else if (sc->Check("log")) rlf->RolloffType = ROLLOFF_Log;
        else if (sc->Check("custom")) rlf->RolloffType = ROLLOFF_Custom;
        else {
          sc->ExpectString();
          sc->Error(va("Unknown rolloff type '%s'", *sc->String));
        }
        sc->ExpectFloat();
      }
      rlf->MinDistance = sc->Float;
      sc->ExpectFloat();
      rlf->MaxDistance = sc->Float;
    } else if (sc->Check("$mididevice")) {
      sc->ExpectString();
      sc->ExpectString();
    } else {
      sc->ExpectString();
      if (**sc->String == '$') sc->Error(va("Unknown command (%s)", *sc->String));
      VName TagName = *sc->String;
      sc->ExpectName();
      int lump = W_CheckNumForName(sc->Name, WADNS_Sounds);
      if (lump < 0) lump = W_CheckNumForName(sc->Name, WADNS_Global);
      if (lump < 0) lump = W_CheckNumForFileName(VStr(sc->Name));
      #if 1
      AddSound(TagName, lump);
      #else
      int aid = AddSound(TagName, lump);
      GCon->Logf(NAME_Debug, "SND: aid=%d; tag=<%s>; name=<%s>; lump=%d", aid, *TagName, *sc->Name, lump);
      #endif
    }
  }
  delete sc;
}


//==========================================================================
//
//  VSoundManager::AddSoundLump
//
//==========================================================================
int VSoundManager::AddSoundLump (VName TagName, int Lump) {
  sfxinfo_t S;
  memset((void *)&S, 0, sizeof(S));
  TagName = (TagName != NAME_None ? VName(*TagName, VName::AddLower) : NAME_None);
  S.TagName = TagName;
  S.Data = nullptr;
  S.Priority = 127;
  S.NumChannels = 6; // max instances of this sound; was 2
  S.ChangePitch = CurrentChangePitch;
  S.VolumeAmp = 1.0f;
  S.Attenuation = 1.0f;
  S.Rolloff = CurrentDefaultRolloff;
  S.LumpNum = Lump;
  S.Link = -1;
  int id = S_sfx.Append(S);
  // put into sound map
  if (TagName != NAME_None) sfxMap.put(TagName, id);
  return id;
}


//==========================================================================
//
//  VSoundManager::AddSound
//
//==========================================================================
int VSoundManager::AddSound (VName TagName, int Lump) {
  int id = FindSound(TagName);

  if (id > 0) {
    // if the sound has already been defined, change the old definition
    sfxinfo_t *sfx = &S_sfx[id];
    //if (sfx->bPlayerReserve)
    //{
    //  SC_ScriptError("Sounds that are reserved for players cannot be reassigned");
    //}
    // Redefining a player compatibility sound will redefine the target instead.
    //if (sfx->bPlayerCompat)
    //{
    //  sfx = &S_sfx[sfx->link];
    //}
    if (sfx->bRandomHeader) {
      delete[] sfx->Sounds;
      sfx->Sounds = nullptr;
    }
    sfx->LumpNum = Lump;
    sfx->bRandomHeader = false;
    sfx->Link = -1;
  } else {
    // otherwise, create a new definition
    id = AddSoundLump(TagName, Lump);
  }

  return id;
}


//==========================================================================
//
//  VSoundManager::FindSound
//
//==========================================================================
int VSoundManager::FindSound (VName TagName) {
  if (TagName == NAME_None) return 0;
  TagName = VName(*TagName, VName::FindLower);
  if (TagName == NAME_None) return 0;
  auto ip = sfxMap.find(TagName);
  return (ip ? *ip : 0);
}


//==========================================================================
//
//  VSoundManager::FindOrAddSound
//
//==========================================================================
int VSoundManager::FindOrAddSound (VName TagName) {
  int id = FindSound(TagName);
  return (id ? id : AddSoundLump(TagName, -1));
}


//==========================================================================
//
//  VSoundManager::ParsePlayerSoundCommon
//
//  Parses the common part of playersound commands in SNDINFO
//  (player class, gender, and ref id)
//
//==========================================================================
void VSoundManager::ParsePlayerSoundCommon (VScriptParser *sc, int &PClass, int &Gender, int &RefId) {
  sc->ExpectString();
  PClass = AddPlayerClass(*sc->String);
  sc->ExpectString();
  Gender = AddPlayerGender(*sc->String);
  sc->ExpectString();
  RefId = FindSound(*sc->String);
  if (!RefId) {
    RefId = AddSound(*sc->String, -1);
    S_sfx[RefId].Link = NumPlayerReserves++;
    S_sfx[RefId].bPlayerReserve = true;
  } else if (!S_sfx[RefId].bPlayerReserve) {
    sc->Error(va("'%s' has not been reserved for a player sound", *sc->String));
  }
  sc->ExpectString();
}


//==========================================================================
//
//  VSoundManager::AddPlayerClass
//
//==========================================================================
int VSoundManager::AddPlayerClass (VName CName) {
  int idx = FindPlayerClass(CName);
  return (idx == -1 ? PlayerClasses.Append(CName) : idx);
}


//==========================================================================
//
//  VSoundManager::FindPlayerClass
//
//==========================================================================
int VSoundManager::FindPlayerClass (VName CName) {
  for (int i = 0; i < PlayerClasses.length(); ++i) if (PlayerClasses[i] == CName) return i;
  return -1;
}


//==========================================================================
//
//  VSoundManager::AddPlayerGender
//
//==========================================================================
int VSoundManager::AddPlayerGender (VName GName) {
  int idx = FindPlayerGender(GName);
  return (idx == -1 ? PlayerGenders.Append(GName) : idx);
}


//==========================================================================
//
//  VSoundManager::FindPlayerGender
//
//==========================================================================
int VSoundManager::FindPlayerGender (VName GName) {
  for (int i = 0; i < PlayerGenders.length(); ++i) if (PlayerGenders[i] == GName) return i;
  return -1;
}


//==========================================================================
//
//  VSoundManager::FindPlayerSound
//
//==========================================================================
int VSoundManager::FindPlayerSound (int PClass, int Gender, int RefId) {
  for (int i = 0; i < PlayerSounds.length(); ++i) {
    if (PlayerSounds[i].ClassId == PClass &&
        PlayerSounds[i].GenderId == Gender &&
        PlayerSounds[i].RefId == RefId)
    {
      return PlayerSounds[i].SoundId;
    }
  }
  return 0;
}


//==========================================================================
//
//  VSoundManager::GetPlayerSound
//
//==========================================================================
VSoundManager::FPlayerSound *VSoundManager::GetPlayerSound (int PClass, int Gender, int RefId) {
  for (auto &&psnd : PlayerSounds) {
    if (psnd.ClassId == PClass && psnd.GenderId == Gender && psnd.RefId == RefId) return &psnd;
  }
  return nullptr;
}


//==========================================================================
//
//  VSoundManager::GetOrAddPlayerSound
//
//==========================================================================
VSoundManager::FPlayerSound &VSoundManager::GetOrAddPlayerSound (int PClass, int Gender, int RefId) {
  for (auto &&psnd : PlayerSounds) {
    if (psnd.ClassId == PClass && psnd.GenderId == Gender && psnd.RefId == RefId) return psnd;
  }
  // add new one
  FPlayerSound &res = PlayerSounds.Alloc();
  res.ClassId = PClass;
  res.GenderId = Gender;
  res.RefId = RefId;
  res.SoundId = 0;
  return res;
}


//==========================================================================
//
//  VSoundManager::LookupPlayerSound
//
//==========================================================================
int VSoundManager::LookupPlayerSound (int ClassId, int GenderId, int RefId) {
  int Id = FindPlayerSound(ClassId, GenderId, RefId);
  if (Id == 0 || (S_sfx[Id].LumpNum == -1 && S_sfx[Id].Link == -1)) {
    // this sound is unavailable
    if (GenderId) return LookupPlayerSound(ClassId, 0, RefId); // try "male"
    if (ClassId) return LookupPlayerSound(0, GenderId, RefId); // try the default class
  }
  return Id;
}


//==========================================================================
//
//  VSoundManager::GetSoundID
//
//==========================================================================
int VSoundManager::GetSoundID (VName Name) {
  if (Name == NAME_None) return 0;
  if (IsNoneSoundName(*Name)) return 0;
  int res = FindSound(Name);
  if (!res) {
    VStr s = VStr(Name);
    if (!sfxMissingReported.has(s)) {
      sfxMissingReported.put(s, true);
      GCon->Logf("WARNING! Can't find sound named '%s'", *Name);
    }
  }
  return res;
}


//==========================================================================
//
//  VSoundManager::ResolveSound
//
//==========================================================================
int VSoundManager::ResolveSound (int InSoundId) {
  return ResolveSound(0, 0, InSoundId);
}


//==========================================================================
//
//  VSoundManager::ResolveEntitySound
//
//==========================================================================
int VSoundManager::ResolveEntitySound (VName ClassName, VName GenderName, VName SoundName) {
  int ClassId = FindPlayerClass(ClassName);
  if (ClassId == -1) ClassId = 0;
  int GenderId = FindPlayerGender(GenderName);
  if (GenderId == -1) GenderId = 0;
  int SoundId = GetSoundID(SoundName);
  return ResolveSound(ClassId, GenderId, SoundId);
}


//==========================================================================
//
//  VSoundManager::ResolveSound
//
//==========================================================================
int VSoundManager::ResolveSound (int ClassID, int GenderID, int InSoundId) {
  const int slen = S_sfx.length();
  int sound_id = InSoundId;
  while (sound_id > 0 && sound_id < slen) {
    sfxinfo_t &sfx = S_sfx[sound_id];
    if (sfx.Link == -1) break;
         if (sfx.bPlayerReserve) sound_id = LookupPlayerSound(ClassID, GenderID, sound_id);
    else if (sfx.bRandomHeader) sound_id = sfx.Sounds[GenRandomU31()%sfx.Link];
    else sound_id = sfx.Link;
  }
  // just in case
  if (sound_id < 0 || sound_id >= slen) {
    GCon->Logf(NAME_Error, "SOUND: cannot resolve sound %d...", InSoundId);
    return 0;
  }
  return sound_id;
}


//==========================================================================
//
//  VSoundManager::IsSoundPresent
//
//==========================================================================
bool VSoundManager::IsSoundPresent (VName ClassName, VName GenderName, VName SoundName) {
  int SoundId = FindSound(SoundName);
  if (!SoundId) return false;
  int ClassId = FindPlayerClass(ClassName);
  if (ClassId == -1) ClassId = 0;
  int GenderId = FindPlayerGender(GenderName);
  if (GenderId == -1) GenderId = 0;
  return ResolveSound(ClassId, GenderId, SoundId) > 0;
}


//==========================================================================
//
//  VSoundManager::CleanupSounds
//
//  unload all sounds used more than five minutes ago
//
//==========================================================================
void VSoundManager::CleanupSounds () {
}


//==========================================================================
//
//  VSoundManager::Process
//
//==========================================================================
void VSoundManager::Process () {
  MyThreadLocker lock(&loaderLock);
  ProcessLoadedSounds();
}


//==========================================================================
//
//  VSoundManager::ProcessLoadedSounds
//
//  all locks already acquired
//
//==========================================================================
void VSoundManager::ProcessLoadedSounds () {
  while (readySounds.length()) {
    int sound_id = readySounds[0];
    if (S_sfx[sound_id].loadedState != sfxinfo_t::ST_Loaded && S_sfx[sound_id].loadedState != sfxinfo_t::ST_Invalid) Sys_Error(va("STRD: invalid loaded state (%d) for sound #%d (uc=%d) (%s : %s)\n", S_sfx[sound_id].loadedState, sound_id, S_sfx[sound_id].UseCount, *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum)));
    readySounds.removeAt(0);
    if (sndThreadDebug) fprintf(stderr, "STRD: notifying about sound #%d (uc=%d) (%s : %s)\n", sound_id, S_sfx[sound_id].UseCount, *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum));
    // `UseCount` already incremented
    bool hasData = !!S_sfx[sound_id].Data;
    if (hasData && S_sfx[sound_id].UseCount == 0) {
      // not needed, forget it
      if (sndThreadDebug) fprintf(stderr, "STRD: freed sound #%d (uc=%d) (%s : %s)\n", sound_id, S_sfx[sound_id].UseCount, *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum));
      Z_Free(S_sfx[sound_id].Data);
      S_sfx[sound_id].Data = nullptr;
      S_sfx[sound_id].loadedState = sfxinfo_t::ST_NotLoaded;
      continue;
    }
    mythread_mutex_unlock(&loaderLock);
#ifdef CLIENT
    if (GAudio) {
      // this will call `DoneWithLump()`
      GAudio->NotifySoundLoaded(sound_id, hasData);
    } else
#endif
    {
      if (hasData) DoneWithLump(sound_id);
    }
    // report errors
    if (!hasData) {
      MyThreadLocker lock(&loaderLock);
      // it must be failed one
      if (S_sfx[sound_id].loadedState != sfxinfo_t::ST_Invalid) {
        GCon->Logf(NAME_Error, "*** INVALID STATE (%d) FOR SOUND '%s' (%s)", S_sfx[sound_id].loadedState, *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum));
        S_sfx[sound_id].loadedState = sfxinfo_t::ST_Invalid;
        if (S_sfx[sound_id].Data) {
          Z_Free(S_sfx[sound_id].Data);
          S_sfx[sound_id].Data = nullptr;
        }
      }
      S_sfx[sound_id].UseCount = 0;
      if (S_sfx[sound_id].LumpNum >= 0) {
        GCon->Logf(NAME_Warning, "Failed to load sound '%s' (%s)", *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum));
      } else {
        GCon->Logf(NAME_Warning, "Cannot find sound '%s'", *S_sfx[sound_id].TagName);
      }
    }
    mythread_mutex_lock(&loaderLock);
    if (sndThreadDebug) fprintf(stderr, "STRD: notified about sound #%d (uc=%d) (%s : %s)\n", sound_id, S_sfx[sound_id].UseCount, *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum));
  }
}


//==========================================================================
//
//  VSoundManager::LoadSoundInternal
//
//  lock should not be held
//
//==========================================================================
bool VSoundManager::LoadSoundInternal (int sound_id) {
  /*static*/ const char *Exts[] = { "flac", "opus", "wav", "raw", "ogg", "mp3", nullptr };

  sfxinfo_t *sfx;
  {
    MyThreadLocker lock(&loaderLock);
    sfx = &S_sfx[sound_id];
    if (sfx->loadedState == sfxinfo_t::ST_Invalid) return false;
    if (sfx->loadedState == sfxinfo_t::ST_Loading) Sys_Error("internal error is sound loader");
    if (sfx->loadedState == sfxinfo_t::ST_Loaded) return true;
    if (sfx->loadedState != sfxinfo_t::ST_NotLoaded) Sys_Error("internal error is sound loader");
    sfx->loadedState = sfxinfo_t::ST_Loading;
  }

  int Lump = sfx->LumpNum;
  if (Lump < 0) {
    //soundsWarned.put(*S_sfx[sound_id].TagName);
    //GCon->Logf(NAME_Warning, "Sound '%s' lump not found", *S_sfx[sound_id].TagName);
    MyThreadLocker lock(&loaderLock);
    sfx->loadedState = sfxinfo_t::ST_Invalid;
    return false;
  }

  int FileLump = W_FindLumpByFileNameWithExts(va("sound/%s", *W_LumpName(Lump)), Exts);
  if (Lump < FileLump) Lump = FileLump;

  VStream *Strm = W_CreateLumpReaderNum(Lump);
  if (!Strm) {
    MyThreadLocker lock(&loaderLock);
    sfx->LumpNum = Lump;
    sfx->loadedState = sfxinfo_t::ST_Invalid;
    return false;
  }

  for (VSampleLoader *Ldr = VSampleLoader::List; Ldr && !S_sfx[sound_id].Data; Ldr = Ldr->Next) {
    Strm->Seek(0);
    Ldr->Load(*sfx, *Strm);
    if (sfx->Data) {
      //GCon->Logf("sound '%s' is %s", *W_FullLumpName(Lump), typeid(*Ldr).name());
      //if (sndThreadDebug) fprintf(stderr, "STRD: loaded sound #%d (uc=%d) (%s : %s) format is '%s'\n", sound_id, S_sfx[sound_id].UseCount, *S_sfx[sound_id].TagName, *W_FullLumpName(Lump), typeid(*Ldr).name());
      break;
    }
  }
  delete Strm;

  if (!sfx->Data) {
    //soundsWarned.put(*S_sfx[sound_id].TagName);
    //GCon->Logf(NAME_Warning, "Failed to load sound '%s' (%s)", *S_sfx[sound_id].TagName, *W_FullLumpName(Lump));
    MyThreadLocker lock(&loaderLock);
    sfx->LumpNum = Lump;
    sfx->loadedState = sfxinfo_t::ST_Invalid;
    return false;
  } else {
    // don't set "loaded" here, caller will do it
    MyThreadLocker lock(&loaderLock);
    sfx->LumpNum = Lump;
    //sfx->loadedState = sfxinfo_t::ST_Loaded;
    //GCon->Logf("SND: loaded sound '%s' (rc=%d)", *S_sfx[sound_id].TagName, S_sfx[sound_id].UseCount+1);
    return true;
  }
}


//==========================================================================
//
//  VSoundManager::LoadSound
//
//==========================================================================
int VSoundManager::LoadSound (int sound_id) {
  if (sound_id < 1 || sound_id >= S_sfx.length()) return LS_Error;

  if (!loaderThreadStarted) {
    // no streaming thread
    if (!S_sfx[sound_id].Data) {
      if (S_sfx[sound_id].loadedState == sfxinfo_t::ST_Invalid) return LS_Error;
      if (!LoadSoundInternal(sound_id)) {
        if (S_sfx[sound_id].LumpNum >= 0) {
          GCon->Logf(NAME_Warning, "Failed to load sound '%s' (%s)", *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum));
        } else {
          GCon->Logf(NAME_Warning, "Cannot find sound '%s'", *S_sfx[sound_id].TagName);
        }
        return LS_Error;
      }
      S_sfx[sound_id].loadedState = sfxinfo_t::ST_Loaded;
    }
    ++S_sfx[sound_id].UseCount;
    return LS_Ready;
  } else {
    MyThreadLocker lock(&loaderLock);
    // loaded?
    if (S_sfx[sound_id].loadedState == sfxinfo_t::ST_Loaded) {
      ++S_sfx[sound_id].UseCount;
      return LS_Ready;
    }
    // do not try to load sound that already failed once
    //if (soundsWarned.has(*S_sfx[sound_id].TagName)) return LS_Error;
    if (S_sfx[sound_id].loadedState == sfxinfo_t::ST_Invalid) return LS_Error;
    // mark current sound as used
    ++S_sfx[sound_id].UseCount; // it will be released in audio driver
    if (S_sfx[sound_id].loadedState == sfxinfo_t::ST_Loading) return LS_Pending;
    // process loaded sounds (why not?)
    ProcessLoadedSounds();
    if (!queuedSoundsMap.has(sound_id)) {
      queuedSoundsMap.put(sound_id, true);
      queuedSounds.append(sound_id);
      // the signal will be delivered when the mutex is released
      mythread_cond_signal(&loaderCond);
    }
    return LS_Pending;
  }
}


//==========================================================================
//
//  VSoundManager::DoneWithLump
//
//==========================================================================
void VSoundManager::DoneWithLump (int sound_id) {
  if (sound_id > 0 && sound_id < S_sfx.length()) {
    MyThreadLocker lock(&loaderLock);
    sfxinfo_t &sfx = S_sfx[sound_id];
    if (sfx.loadedState == sfxinfo_t::ST_Invalid) return; // oops
    if (sfx.loadedState == sfxinfo_t::ST_NotLoaded) return; // oops
    if (sndThreadDebug) fprintf(stderr, "STRD: releasing sound #%d (uc=%d) (%s : %s)\n", sound_id, S_sfx[sound_id].UseCount, *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum));
    if (sfx.UseCount <= 0) Sys_Error(va("invalid UseCount for sound #%d (uc=%d) (%s : %s)\n", sound_id, S_sfx[sound_id].UseCount, *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum)));
    --sfx.UseCount;
    //GCon->Logf("SND: done with sound '%s' (rc=%d)", *S_sfx[sound_id].TagName, S_sfx[sound_id].UseCount);
    if (sfx.UseCount == 0 && sfx.loadedState == sfxinfo_t::ST_Loaded) {
      //GCon->Logf("SND: unloaded sound '%s'", *S_sfx[sound_id].TagName);
      if (sndThreadDebug) fprintf(stderr, "STRD: unloading sound #%d (uc=%d) (%s : %s)\n", sound_id, S_sfx[sound_id].UseCount, *S_sfx[sound_id].TagName, *W_FullLumpName(S_sfx[sound_id].LumpNum));
      if (sfx.Data) {
        Z_Free(sfx.Data);
        sfx.Data = nullptr;
      }
      sfx.loadedState = sfxinfo_t::ST_NotLoaded;
    }
  }
}


//==========================================================================
//
//  VSoundManager::GetMusicVolume
//
//==========================================================================
float VSoundManager::GetMusicVolume (const char *SongName) {
  if (!SongName || !SongName[0]) return 1.0f;
  for (int i = 0; i < MusicVolumes.length(); ++i) {
    if (VStr::ICmp(*MusicVolumes[i].SongName, SongName) == 0) return MusicVolumes[i].Volume;
  }
  return 1.0f;
}


//==========================================================================
//
//  VSoundManager::GetAmbientSound
//
//==========================================================================
FAmbientSound *VSoundManager::GetAmbientSound (int Idx) {
  if (Idx < 0 || Idx >= NUM_AMBIENT_SOUNDS) return nullptr;
  return AmbientSounds[Idx];
}


//==========================================================================
//
//  VSoundManager::ParseSequenceScript
//
//==========================================================================
void VSoundManager::ParseSequenceScript (VScriptParser *sc) {
  TArray<vint32> TempData;
  bool inSequence = false;
  int SeqId = 0;
  int DelayOnceIndex = 0;
  char SeqType = ':';

  while (!sc->AtEnd()) {
    sc->ExpectString();
    if (**sc->String == ':' || **sc->String == '[') {
      if (inSequence) sc->Error("SNDSEQ: Nested Script Error");
      for (SeqId = 0; SeqId < SeqInfo.length(); ++SeqId) {
        if (SeqInfo[SeqId].Name == *sc->String+1) {
          Z_Free(SeqInfo[SeqId].Data);
          break;
        }
      }
      if (SeqId == SeqInfo.length()) SeqInfo.Alloc();
      TempData.Clear();
      inSequence = true;
      DelayOnceIndex = 0;
      SeqType = sc->String[0];
      SeqInfo[SeqId].Name = *sc->String+1;
      SeqInfo[SeqId].Slot = NAME_None;
      SeqInfo[SeqId].Data = nullptr;
      SeqInfo[SeqId].StopSound = 0;
      if (SeqType == '[') {
        TempData.Append(SSCMD_Select);
        TempData.Append(0);
        sc->SetCMode(true);
      }
      continue; // parse the next command
    }

    if (!inSequence) {
      if (sc->String.strEquCI("end")) {
        // some defective zdoom wads has this
        sc->Message("`end` directive outside of sequence");
        continue;
      }
      sc->Error("String outside sequence");
      continue;
    }
    sc->UnGet();

    if (sc->Check("door")) {
      // door <number>...
      AssignSeqTranslations(sc, SeqId, SEQ_Door);
      continue;
    }

    if (sc->Check("platform")) {
      // platform <number>...
      AssignSeqTranslations(sc, SeqId, SEQ_Platform);
      continue;
    }

    if (sc->Check("environment")) {
      // environment <number>...
      AssignSeqTranslations(sc, SeqId, SEQ_Environment);
      continue;
    }

    if (SeqType == '[') {
      // selection sequence
      if (sc->Check("]")) {
        TempData[1] = (TempData.length()-2)/2;
        TempData.Append(SSCMD_End);
        int tmplen = TempData.length()*4;
        SeqInfo[SeqId].Data = (vint32 *)Z_Malloc(tmplen);
        memset(SeqInfo[SeqId].Data, 0, tmplen);
        if (TempData.length()) memcpy(SeqInfo[SeqId].Data, TempData.Ptr(), TempData.length()*sizeof(vint32));
        inSequence = false;
        sc->SetCMode(false);
      } else {
        sc->ExpectNumber();
        TempData.Append(sc->Number);
        sc->ExpectString();
        TempData.Append(VName(*sc->String).GetIndex());
      }
      continue;
    }

    if (sc->Check("play")) {
      // play <sound>
      sc->ExpectString();
      TempData.Append(SSCMD_Play);
      TempData.Append(GetSoundID(*sc->String));
    } else if (sc->Check("playuntildone")) {
      // playuntildone <sound>
      sc->ExpectString();
      TempData.Append(SSCMD_Play);
      TempData.Append(GetSoundID(*sc->String));
      TempData.Append(SSCMD_WaitUntilDone);
    } else if (sc->Check("playtime")) {
      // playtime <string> <tics>
      sc->ExpectString();
      TempData.Append(SSCMD_Play);
      TempData.Append(GetSoundID(*sc->String));
      sc->ExpectNumber();
      TempData.Append(SSCMD_Delay);
      TempData.Append(sc->Number);
    } else if (sc->Check("playrepeat")) {
      // playrepeat <sound>
      sc->ExpectString();
      TempData.Append(SSCMD_PlayRepeat);
      TempData.Append(GetSoundID(*sc->String));
    } else if (sc->Check("playloop")) {
      // playloop <sound> <count>
      sc->ExpectString();
      TempData.Append(SSCMD_PlayLoop);
      TempData.Append(GetSoundID(*sc->String));
      sc->ExpectNumber();
      TempData.Append(sc->Number);
    } else if (sc->Check("delay")) {
      // delay <tics>
      TempData.Append(SSCMD_Delay);
      sc->ExpectNumber();
      TempData.Append(sc->Number);
    } else if (sc->Check("delayonce")) {
      // delayonce <tics>
      TempData.Append(SSCMD_DelayOnce);
      sc->ExpectNumber();
      TempData.Append(sc->Number);
      TempData.Append(DelayOnceIndex++);
    } else if (sc->Check("delayrand")) {
      // delayrand <tics_from> <tics_to>
      TempData.Append(SSCMD_DelayRand);
      sc->ExpectNumber();
      TempData.Append(sc->Number);
      sc->ExpectNumber();
      TempData.Append(sc->Number);
    } else if (sc->Check("volume")) {
      // volume <volume>
      TempData.Append(SSCMD_Volume);
      sc->ExpectFloat();
      TempData.Append((vint32)(sc->Float*100.0f));
    } else if (sc->Check("volumerel")) {
      // volumerel <volume_delta>
      TempData.Append(SSCMD_VolumeRel);
      sc->ExpectFloat();
      TempData.Append((vint32)(sc->Float*100.0f));
    } else if (sc->Check("volumerand")) {
      // volumerand <volume_from> <volume_to>
      TempData.Append(SSCMD_VolumeRand);
      sc->ExpectFloat();
      TempData.Append((vint32)(sc->Float*100.0f));
      sc->ExpectFloat();
      TempData.Append((vint32)(sc->Float*100.0f));
    } else if (sc->Check("attenuation")) {
      // attenuation none|normal|idle|static|surround
      TempData.Append(SSCMD_Attenuation);
      vint32 Atten = 0;
           if (sc->Check("none")) Atten = 0;
      else if (sc->Check("normal")) Atten = 1;
      else if (sc->Check("idle")) Atten = 2;
      else if (sc->Check("static")) Atten = 3;
      else if (sc->Check("surround")) Atten = -1;
      else sc->Error("Bad attenuation");
      TempData.Append(Atten);
    } else if (sc->Check("randomsequence")) {
      // randomsequence
      TempData.Append(SSCMD_RandomSequence);
    } else if (sc->Check("restart")) {
      // restart
      TempData.Append(SSCMD_Branch);
      TempData.Append(TempData.length()-1);
    } else if (sc->Check("stopsound")) {
      // stopsound <sound>
      sc->ExpectString();
      SeqInfo[SeqId].StopSound = GetSoundID(*sc->String);
      TempData.Append(SSCMD_StopSound);
    } else if (sc->Check("nostopcutoff")) {
      // nostopcutoff
      SeqInfo[SeqId].StopSound = -1;
      TempData.Append(SSCMD_StopSound);
    } else if (sc->Check("slot")) {
      // slot <name>...
      sc->ExpectString();
      SeqInfo[SeqId].Slot = *sc->String;
    } else if (sc->Check("end")) {
      // end
      TempData.Append(SSCMD_End);
      //SeqInfo[SeqId].Data = new vint32[TempData.length()];
      int tmplen = TempData.length()*4;
      SeqInfo[SeqId].Data = (vint32 *)Z_Malloc(tmplen);
      memset(SeqInfo[SeqId].Data, 0, tmplen);
      if (TempData.length()) memcpy(SeqInfo[SeqId].Data, TempData.Ptr(), TempData.length()*sizeof(vint32));
      inSequence = false;
    } else {
      sc->Error(va("Unknown commmand '%s'.", *sc->String));
    }
  }
  delete sc;
}


//==========================================================================
//
//  VSoundManager::AssignSeqTranslations
//
//==========================================================================
void VSoundManager::AssignSeqTranslations (VScriptParser *sc, int SeqId, seqtype_t SeqType) {
  sc->Crossed = false;
  while (sc->GetString() && !sc->Crossed) {
    char *Stopper;
    int Num = strtol(*sc->String, &Stopper, 0);
    if (*Stopper == 0) SeqTrans[(Num&63)+SeqType*64] = SeqId;
  }
  sc->UnGet();
}


//==========================================================================
//
//  VSoundManager::SetSeqTrans
//
//==========================================================================
void VSoundManager::SetSeqTrans (VName Name, int Num, int SeqType) {
  int Idx = FindSequence(Name);
  if (Idx != -1) SeqTrans[(Num&63)+SeqType*64] = Idx;
}


//==========================================================================
//
//  VSoundManager::GetSeqTrans
//
//==========================================================================
VName VSoundManager::GetSeqTrans (int Num, int SeqType) {
  if (Num < 0) Num = 0; // if not assigned, use 0 as default
  if (SeqTrans[(Num&63)+SeqType*64] < 0) return NAME_None;
  return SeqInfo[SeqTrans[(Num&63)+SeqType*64]].Name;
}


//==========================================================================
//
//  VSoundManager::GetSeqSlot
//
//==========================================================================
VName VSoundManager::GetSeqSlot (VName Name) {
  int Idx = FindSequence(Name);
  if (Idx != -1) return SeqInfo[Idx].Slot;
  return NAME_None;
}


//==========================================================================
//
//  VSoundManager::FindSequence
//
//==========================================================================
int VSoundManager::FindSequence (VName Name) {
  for (int i = 0; i < SeqInfo.length(); ++i) if (SeqInfo[i].Name == Name) return i;
  return -1;
}


//==========================================================================
//
//  VSoundManager::GetSoundLumpNames
//
//==========================================================================
void VSoundManager::GetSoundLumpNames (TArray<FReplacedString> &List) {
  for (int i = 1; i < S_sfx.length(); ++i) {
    if (S_sfx[i].LumpNum < 0) continue;
    const char *LName = *W_LumpName(S_sfx[i].LumpNum);
    if (LName[0] == 'd' && LName[1] == 's') {
      FReplacedString &R = List.Alloc();
      R.Index = i;
      R.Replaced = false;
      R.Old = LName+2;
    }
  }
}


//==========================================================================
//
//  VSoundManager::ReplaceSoundLumpNames
//
//==========================================================================
void VSoundManager::ReplaceSoundLumpNames (TArray<FReplacedString> &List) {
  for (int i = 0; i < List.length(); ++i) {
    if (!List[i].Replaced) continue;
    S_sfx[List[i].Index].LumpNum = W_CheckNumForName(VName(*(VStr("ds")+List[i].New), VName::AddLower));
  }
}


#ifdef CLIENT
//==========================================================================
//
//  VRawSampleLoader::Load
//
//==========================================================================
void VRawSampleLoader::Load (sfxinfo_t &Sfx, VStream &Strm) {
  // read header and see if it's a valid raw sample
  vuint16 Version;
  vuint16 SampleRate;
  vuint32 DataSize;

  Strm.Seek(0);
  Strm << Version << SampleRate << DataSize;
  //if (Version == 3) GCon->Logf("RAW: sr=%d; datasize=%d; totalsize=%d; diff=%d", SampleRate, DataSize, Strm.TotalSize(), Strm.TotalSize()-DataSize);
  // it has 32 padding bytes
  if (Version != 3) return;
  if (SampleRate < 1024 || SampleRate > 48000) {
    GCon->Logf(NAME_Warning, "invalid DMX sound '%s' sample rate: %u", *Strm.GetName(), SampleRate);
    return;
  }
  if ((vint32)DataSize > Strm.TotalSize()-8) {
    GCon->Logf(NAME_Warning, "DMX sound '%s' missing data (%d bytes)", *Strm.GetName(), (vint32)DataSize-(Strm.TotalSize()-8));
    return;
  }
  if ((vint32)DataSize != Strm.TotalSize()-8) {
    GCon->Logf(NAME_Warning, "DMX sound '%s' contains extra data (%d bytes, this is not fatal)", *Strm.GetName(), (Strm.TotalSize()-8)-(vint32)DataSize);
    //return;
  }
  //if (Version != 3 || SampleRate < 1024 || SampleRate > 48000 || DataSize < 32 || (vint32)DataSize != Strm.TotalSize()-8) return;

  Sfx.SampleBits = 8;
  Sfx.SampleRate = SampleRate;

  // check for padding
  if (DataSize > 32) {
    // has some actual sound data
    vuint8 padbuf[16];
    Strm.Serialise(padbuf, 16); // skip prepad
    Sfx.DataSize = DataSize-32; // skip both paddings
    Sfx.Data = Z_Malloc(Sfx.DataSize);
    Strm.Serialise(Sfx.Data, Sfx.DataSize);
    //GCon->Logf("RAW: sr=%d; datasize=%d", SampleRate, DataSize);
  } else {
    GCon->Logf(NAME_Warning, "empty DMX sound '%s' (%u bytes, this is not fatal)", *Strm.GetName(), DataSize);
    // empty sample
    Sfx.DataSize = 1;
    Sfx.Data = Z_Malloc(Sfx.DataSize);
    memset(Sfx.Data, 0x80, Sfx.DataSize);
  }
}
#endif
