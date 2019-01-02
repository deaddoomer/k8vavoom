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
#include "snd_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

#pragma pack(1)
struct FRiffChunkHeader
{
  char    ID[4];
  vuint32   Size;
};

struct FWavFormatDesc
{
  vuint16   Format;
  vuint16   Channels;
  vuint32   Rate;
  vuint32   BytesPerSec;
  vuint16   BlockAlign;
  vuint16   Bits;
};
#pragma pack()

class VWaveSampleLoader : public VSampleLoader
{
public:
  virtual void Load(sfxinfo_t&, VStream&) override;
};

class VWavAudioCodec : public VAudioCodec
{
public:
  VStream *Strm;
  int       SamplesLeft;

  int       WavChannels;
  int       WavBits;
  int       BlockAlign;

  VWavAudioCodec(VStream *InStrm);
  virtual ~VWavAudioCodec() override;
  virtual int Decode(short *Data, int NumSamples) override;
  virtual bool Finished() override;
  virtual void Restart() override;

  static VAudioCodec *Create(VStream *InStrm);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static VWaveSampleLoader    WaveSampleLoader;

IMPLEMENT_AUDIO_CODEC(VWavAudioCodec, "Wav");

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  FindChunk
//
//==========================================================================

static int FindRiffChunk(VStream &Strm, const char *ID)
{
  guard(VWavAudioCodec::FindChunk);
  Strm.Seek(12);
  if (Strm.IsError()) return -1;
  int EndPos = Strm.TotalSize();
  while (Strm.Tell() + 8 <= EndPos)
  {
    if (Strm.IsError()) return -1;
    FRiffChunkHeader ChunkHdr;
    Strm.Serialise(&ChunkHdr, 8);
    int ChunkSize = LittleLong(ChunkHdr.Size);
    if (!memcmp(ChunkHdr.ID, ID, 4))
    {
      //  Found chunk.
      return ChunkSize;
    }
    if (Strm.Tell() + ChunkSize > EndPos)
    {
      //  Chunk goes beyound end of file.
      break;
    }
    Strm.Seek(Strm.Tell() + ChunkSize);
    if (Strm.IsError()) return -1;
  }
  return -1;
  unguard;
}

//==========================================================================
//
//  VWaveSampleLoader::Load
//
//==========================================================================

void VWaveSampleLoader::Load(sfxinfo_t &Sfx, VStream &Strm)
{
  guard(VWaveSampleLoader::Load);
  //  Check header to see if it's a wave file.
  char Header[12];
  Strm.Seek(0);
  Strm.Serialise(Header, 12);
  if (memcmp(Header, "RIFF", 4) || memcmp(Header + 8, "WAVE", 4))
  {
    //  Not a WAVE.
    return;
  }

  //  Get format settings.
  int FmtSize = FindRiffChunk(Strm, "fmt ");
  if (FmtSize < 16)
  {
    //  Format not found or too small.
    return;
  }
  FWavFormatDesc Fmt;
  Strm.Serialise(&Fmt, 16);
  if (LittleShort(Fmt.Format) != 1)
  {
    //  Not a PCM format.
    return;
  }
  int SampleRate = LittleLong(Fmt.Rate);
  int WavChannels = LittleShort(Fmt.Channels);
  int WavBits = LittleShort(Fmt.Bits);
  int BlockAlign = LittleShort(Fmt.BlockAlign);
  //if (WavChannels != 1) GCon->Logf("A stereo sample, taking left channel");
  if (WavChannels < 1 || WavChannels > 2) return;
  if (SampleRate < 128 || SampleRate > 96000) return;
  if (WavBits != 8 && WavBits != 16) return;

  //  Find data chunk.
  int DataSize = FindRiffChunk(Strm, "data");
  if (DataSize == -1)
  {
    //  Data not found
    return;
  }

  //  Fill in sample info and allocate data.
  Sfx.SampleRate = SampleRate;
  Sfx.SampleBits = WavBits;
  Sfx.DataSize = (DataSize/BlockAlign)*(WavBits/8);
  Sfx.Data = Z_Malloc(Sfx.DataSize);

  //  Read wav data.
  void *WavData = Z_Malloc(DataSize);
  Strm.Serialise(WavData, DataSize);

  //  Copy sample data.
  DataSize /= BlockAlign;
  if (WavBits == 8) {
    byte *pSrc = (byte*)WavData;
    byte *pDst = (byte*)Sfx.Data;
    for (int i = 0; i < DataSize; i++, pSrc += BlockAlign) {
      int v = 0;
      for (int f = 0; f < WavChannels; ++f) v += (int)pSrc[f];
      v /= WavChannels;
      if (v < -128) v = -128; else if (v > 127) v = 127;
      *pDst++ = (byte)v;
    }
  } else {
    byte *pSrc = (byte *)WavData;
    short *pDst = (short *)Sfx.Data;
    for (int i = 0; i < DataSize; i++, pSrc += BlockAlign) {
      int v = 0;
      for (int f = 0; f < WavChannels; ++f) v += (int)(LittleShort(*(((short *)pSrc)+f)));
      v /= WavChannels;
      if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
      *pDst++ = (short)v;
    }
  }
  Z_Free(WavData);

  unguard;
}

//==========================================================================
//
//  VWavAudioCodec::VWavAudioCodec
//
//==========================================================================

VWavAudioCodec::VWavAudioCodec(VStream *InStrm)
: Strm(InStrm)
, SamplesLeft(-1)
{
  guard(VWavAudioCodec::VWavAudioCodec);
  int FmtSize = FindRiffChunk(*Strm, "fmt ");
  if (FmtSize < 16)
  {
    //  Format not found or too small.
    return;
  }
  FWavFormatDesc Fmt;
  Strm->Serialise(&Fmt, 16);
  if (LittleShort(Fmt.Format) != 1)
  {
    //  Not a PCM format.
    return;
  }
  SampleRate = LittleLong(Fmt.Rate);
  WavChannels = LittleShort(Fmt.Channels);
  WavBits = LittleShort(Fmt.Bits);
  BlockAlign = LittleShort(Fmt.BlockAlign);
  if (WavBits != 8 && WavBits != 16) return;
  if (WavChannels < 1 || WavChannels > 2) return;
  if (SampleRate < 128 || SampleRate > 96000) return;

  SamplesLeft = FindRiffChunk(*Strm, "data");
  if (SamplesLeft == -1)
  {
    //  Data not found
    return;
  }
  SamplesLeft /= BlockAlign;
  unguard;
}

//==========================================================================
//
//  VWavAudioCodec::~VWavAudioCodec
//
//==========================================================================

VWavAudioCodec::~VWavAudioCodec()
{
  //guard(VWavAudioCodec::~VWavAudioCodec);
  if (SamplesLeft != -1)
  {
    Strm->Close();
    delete Strm;
    Strm = nullptr;
  }
  //unguard;
}

//==========================================================================
//
//  VWavAudioCodec::Decode
//
//==========================================================================

int VWavAudioCodec::Decode(short *Data, int NumSamples)
{
  guard(VWavAudioCodec::Decode);
  int CurSample = 0;
  byte Buf[1024];
  while (SamplesLeft && CurSample < NumSamples)
  {
    int ReadSamples = 1024/BlockAlign;
    if (ReadSamples > NumSamples-CurSample) ReadSamples = NumSamples-CurSample;
    if (ReadSamples > SamplesLeft) ReadSamples = SamplesLeft;
    Strm->Serialise(Buf, ReadSamples*BlockAlign);
    for (int i = 0; i < 2; ++i) {
      byte *pSrc = Buf;
      if (i && WavChannels > 1) pSrc += WavBits/8;
      short *pDst = Data+CurSample*2+i;
      if (WavBits == 8) {
        for (int j = 0; j < ReadSamples; j++, pSrc += BlockAlign, pDst += 2) {
          *pDst = (*pSrc - 127) << 8;
        }
      } else {
        for (int j = 0; j < ReadSamples; j++, pSrc += BlockAlign, pDst += 2) {
          *pDst = LittleShort(*(short*)pSrc);
        }
      }
    }
    SamplesLeft -= ReadSamples;
    CurSample += ReadSamples;
  }
  return CurSample;
  unguard;
}

//==========================================================================
//
//  VWavAudioCodec::Finished
//
//==========================================================================

bool VWavAudioCodec::Finished()
{
  return !SamplesLeft;
}

//==========================================================================
//
//  VWavAudioCodec::Restart
//
//==========================================================================

void VWavAudioCodec::Restart()
{
  guard(VWavAudioCodec::Restart);
  SamplesLeft = FindRiffChunk(*Strm, "data") / BlockAlign;
  unguard;
}

//==========================================================================
//
//  VWavAudioCodec::Create
//
//==========================================================================

VAudioCodec *VWavAudioCodec::Create(VStream *InStrm)
{
  guard(VWavAudioCodec::Create);
  char Header[12];
  InStrm->Seek(0);
  InStrm->Serialise(Header, 12);
  if (!memcmp(Header, "RIFF", 4) && !memcmp(Header + 8, "WAVE", 4))
  {
    //  It's a WAVE file.
    VWavAudioCodec *Codec = new VWavAudioCodec(InStrm);
    if (Codec->SamplesLeft != -1)
    {
      return Codec;
    }
    //  File seams to be broken.
    delete Codec;
    Codec = nullptr;
  }
  return nullptr;
  unguard;
}
