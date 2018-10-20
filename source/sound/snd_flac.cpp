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
#include <FLAC++/decoder.h>

#include "gamedefs.h"
#include "snd_local.h"


class VFlacSampleLoader : public VSampleLoader {
public:
  class FStream : public FLAC::Decoder::Stream {
  public:
    VStream &Strm;
    size_t BytesLeft;
    int SampleBits;
    int SampleRate;
    void *Data;
    size_t DataSize;

    FStream (VStream &InStream);
    void StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len);

  protected:
    // flac decoder callbacks
    virtual ::FLAC__StreamDecoderReadStatus read_callback (FLAC__byte buffer[], size_t *bytes) override;
    virtual ::FLAC__StreamDecoderWriteStatus write_callback (const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) override;
    virtual void metadata_callback (const ::FLAC__StreamMetadata *metadata) override;
    virtual void error_callback (::FLAC__StreamDecoderErrorStatus status) override;
  };

  virtual void Load (sfxinfo_t &, VStream &) override;
};


class VFlacAudioCodec : public VAudioCodec {
public:
  class FStream : public FLAC::Decoder::Stream {
  public:
    VStream *Strm;
    size_t BytesLeft;
    int NumChannels;
    int SampleBits;
    int SampleRate;

    FLAC__int32 *SamplePool[2];
    size_t PoolSize;
    size_t PoolUsed;
    size_t PoolPos;

    short *StrmBuf;
    size_t StrmSize;

    FStream (VStream *InStream);
    virtual ~FStream () override;
    void StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len);

  protected:
    // flac decoder callbacks
    virtual ::FLAC__StreamDecoderReadStatus read_callback (FLAC__byte buffer[], size_t *bytes) override;
    virtual ::FLAC__StreamDecoderWriteStatus write_callback (const ::FLAC__Frame *frame, const FLAC__int32 * const buffer[]) override;
    virtual void metadata_callback (const ::FLAC__StreamMetadata *metadata) override;
    virtual void error_callback (::FLAC__StreamDecoderErrorStatus status) override;
  };

  FStream *Stream;

  VFlacAudioCodec (FStream *InStream);
  virtual ~VFlacAudioCodec () override;
  virtual int Decode (short *Data, int NumSamples) override;
  virtual bool Finished () override;
  virtual void Restart () override;
  static VAudioCodec *Create (VStream *InStream);
};


VFlacSampleLoader FlacSampleLoader;

IMPLEMENT_AUDIO_CODEC(VFlacAudioCodec, "FLAC");


//==========================================================================
//
//  VFlacSampleLoader::Load
//
//==========================================================================
void VFlacSampleLoader::Load (sfxinfo_t &Sfx, VStream &Stream) {
  guard(VFlacSampleLoader::Load);
  // check if it's a flac file
  Stream.Seek(0);
  char Hdr[4];
  Stream.Serialise(Hdr, 4);
  if (Hdr[0] != 'f' || Hdr[1] != 'L' || Hdr[2] != 'a' || Hdr[3] != 'C') return;
  // create reader sream
  FStream *Strm = new FStream(Stream);
  Strm->Data = Z_Malloc(1);
  Strm->init();
  Strm->process_until_end_of_metadata();
  if (!Strm->SampleRate) {
    Z_Free(Strm->Data);
    Sfx.Data = nullptr;
    delete Strm;
    return;
  }
  if (!Strm->process_until_end_of_stream()) {
    GCon->Logf("Failed to process FLAC file");
    Z_Free(Strm->Data);
    Sfx.Data = nullptr;
    delete Strm;
    return;
  }
  Sfx.SampleRate = Strm->SampleRate;
  Sfx.SampleBits = Strm->SampleBits;
  Sfx.DataSize = Strm->DataSize;
  Sfx.Data = Strm->Data;
  delete Strm;
  unguard;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::FStream
//
//==========================================================================
VFlacSampleLoader::FStream::FStream (VStream &InStream)
  : Strm(InStream)
  , SampleBits(0)
  , SampleRate(0)
  , Data(0)
  , DataSize(0)
{
  guard(VFlacSampleLoader::FStream::FStream);
  Strm.Seek(0);
  BytesLeft = Strm.TotalSize();
  unguard;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::read_callback
//
//==========================================================================
::FLAC__StreamDecoderReadStatus VFlacSampleLoader::FStream::read_callback (FLAC__byte buffer[], size_t *bytes) {
  if (*bytes > 0) {
    if (!BytesLeft) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    if (*bytes > BytesLeft) *bytes = BytesLeft;
    Strm.Serialise(buffer, *bytes);
    BytesLeft -= *bytes;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  } else {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::write_callback
//
//==========================================================================
::FLAC__StreamDecoderWriteStatus VFlacSampleLoader::FStream::write_callback (const ::FLAC__Frame *frame, const FLAC__int32 *const buffer[]) {
  void *Temp = Data;
  Data = Z_Malloc(DataSize+frame->header.blocksize*SampleBits/8);
  memcpy(Data, Temp, DataSize);
  Z_Free(Temp);

  const FLAC__int32 *pSrc = buffer[0];
  if (SampleBits == 8) {
    vuint8 *pDst = (vuint8 *)Data+DataSize;
    for (size_t j = 0; j < frame->header.blocksize; ++j, ++pSrc, ++pDst) *pDst = vuint8(*pSrc)^0x80;
  } else {
    vint16 *pDst = (vint16 *)((vuint8 *)Data+DataSize);
    for (size_t j = 0; j < frame->header.blocksize; ++j, ++pSrc, ++pDst) *pDst = vint16(*pSrc);
  }
  DataSize += frame->header.blocksize*SampleBits/8;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::metadata_callback
//
//==========================================================================
void VFlacSampleLoader::FStream::metadata_callback (const ::FLAC__StreamMetadata *metadata) {
  guard(VFlacSampleLoader::FStream::metadata_callback);
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    if (metadata->data.stream_info.bits_per_sample != 8 &&
        metadata->data.stream_info.bits_per_sample != 16)
    {
      GCon->Log("Only 8 and 16 bit FLAC files are supported");
      return;
    }
    //if (metadata->data.stream_info.channels != 1) GCon->Log("Stereo FLAC, taking left channel");
    SampleRate = metadata->data.stream_info.sample_rate;
    SampleBits = metadata->data.stream_info.bits_per_sample;
  }
  unguard;
}


//==========================================================================
//
//  VFlacSampleLoader::FStream::error_callback
//
//==========================================================================
void VFlacSampleLoader::FStream::error_callback (::FLAC__StreamDecoderErrorStatus) {
}


//==========================================================================
//
//  VFlacAudioCodec::VFlacAudioCodec
//
//==========================================================================
VFlacAudioCodec::VFlacAudioCodec (FStream *InStream)
  : Stream(InStream)
{
  SampleRate = Stream->SampleRate;
}


//==========================================================================
//
//  VFlacAudioCodec::~VFlacAudioCodec
//
//==========================================================================
VFlacAudioCodec::~VFlacAudioCodec () {
  delete Stream;
  Stream = nullptr;
}


//==========================================================================
//
//  VFlacAudioCodec::Decode
//
//==========================================================================
int VFlacAudioCodec::Decode(short *Data, int NumSamples) {
  guard(VFlacAudioCodec::Decode);
  Stream->StrmBuf = Data;
  Stream->StrmSize = NumSamples;

  if (Stream->PoolUsed > Stream->PoolPos) {
    size_t poolGrab = Stream->PoolUsed-Stream->PoolPos;
    if (poolGrab > (size_t)NumSamples) poolGrab = (size_t)NumSamples;
    Stream->StrmWrite(Stream->SamplePool, Stream->PoolPos, poolGrab);
    Stream->PoolPos += poolGrab;
    if (Stream->PoolPos == Stream->PoolUsed) {
      Stream->PoolPos = 0;
      Stream->PoolUsed = 0;
    }
  }

  while (Stream->StrmSize > 0 && !Finished()) {
    if (!Stream->process_single()) break;
  }
  return NumSamples - Stream->StrmSize;
  unguard;
}


//==========================================================================
//
//  VFlacAudioCodec::Finished
//
//==========================================================================
bool VFlacAudioCodec::Finished () {
  guard(VFlacAudioCodec::Finished);
  return (Stream->get_state() == FLAC__STREAM_DECODER_END_OF_STREAM);
  unguard;
}


//==========================================================================
//
//  VFlacAudioCodec::Restart
//
//==========================================================================
void VFlacAudioCodec::Restart () {
  guard(VFlacAudioCodec::Restart);
  Stream->Strm->Seek(0);
  Stream->BytesLeft = Stream->Strm->TotalSize();
  Stream->reset();
  unguard;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::FStream
//
//==========================================================================
VFlacAudioCodec::FStream::FStream (VStream *InStream)
  : Strm(InStream)
  , NumChannels(0)
  , SampleBits(0)
  , SampleRate(0)
  , PoolSize(0)
  , PoolUsed(0)
  , PoolPos(0)
  , StrmBuf(0)
  , StrmSize(0)
{
  guard(VFlacAudioCodec::FStream::FStream);
  Strm->Seek(0);
  BytesLeft = Strm->TotalSize();
  init();
  process_until_end_of_metadata();
  unguard;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::~FStream
//
//==========================================================================
VFlacAudioCodec::FStream::~FStream () {
  if (PoolSize > 0 && SamplePool[0] != nullptr) {
    Z_Free(SamplePool[0]);
    SamplePool[0] = nullptr;
    Strm->Close();
    delete Strm;
    Strm = nullptr;
  }
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::StrmWrite
//
//==========================================================================
void VFlacAudioCodec::FStream::StrmWrite (const FLAC__int32 *const Buf[], size_t Offs, size_t Len) {
  guard(VFlacAudioCodec::FStream::StrmWrite);
  for (int i = 0; i < 2; ++i) {
    const FLAC__int32 *pSrc = Buf[NumChannels == 1 ? 0 : i]+Offs;
    short *pDst = StrmBuf+i;
    if (SampleBits == 8) {
      for (size_t j = 0; j < Len; j++, pSrc++, pDst += 2) *pDst = char(*pSrc) << 8;
    } else {
      for (size_t j = 0; j < Len; j++, pSrc++, pDst += 2) *pDst = short(*pSrc);
    }
  }
  StrmBuf += Len * 2;
  StrmSize -= Len;
  unguard;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::read_callback
//
//==========================================================================
::FLAC__StreamDecoderReadStatus VFlacAudioCodec::FStream::read_callback (FLAC__byte buffer[], size_t *bytes) {
  guard(VFlacAudioCodec::FStream::read_callback);
  if (*bytes > 0) {
    if (!BytesLeft) return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    if (*bytes > BytesLeft) *bytes = BytesLeft;
    Strm->Serialise(buffer, *bytes);
    BytesLeft -= *bytes;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  } else {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
  unguard;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::write_callback
//
//==========================================================================
::FLAC__StreamDecoderWriteStatus VFlacAudioCodec::FStream::write_callback (const ::FLAC__Frame *frame, const FLAC__int32 *const buffer[]) {
  guard(VFlacAudioCodec::FStream::write_callback);
  size_t blockSize = frame->header.blocksize;
  size_t blockGrab = 0;
  size_t blockOfs;

  blockGrab = MIN(StrmSize, blockSize);
  StrmWrite(buffer, 0, blockGrab);
  blockSize -= blockGrab;
  blockOfs = blockGrab;

  if (blockSize > 0) {
    blockGrab = PoolSize-PoolUsed;
    if (blockGrab > blockSize) blockGrab = blockSize;
    memcpy(SamplePool[0]+PoolUsed, buffer[0]+blockOfs, sizeof(*buffer[0])*blockGrab);
    if (NumChannels > 1) {
      memcpy(SamplePool[1]+PoolUsed, buffer[1]+blockOfs, sizeof(*buffer[1])*blockGrab);
    }
    PoolUsed += blockGrab;
  }

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
  unguard;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::metadata_callback
//
//==========================================================================
void VFlacAudioCodec::FStream::metadata_callback (const ::FLAC__StreamMetadata *metadata) {
  guard(VFlacAudioCodec::FStream::metadata_callback);
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO && PoolSize == 0) {
    if (metadata->data.stream_info.bits_per_sample != 8 &&
        metadata->data.stream_info.bits_per_sample != 16)
    {
      GCon->Log("Only 8 and 16 bit FLAC files are supported");
      return;
    }
    SampleRate = metadata->data.stream_info.sample_rate;
    NumChannels = MIN(2, metadata->data.stream_info.channels);
    SampleBits = metadata->data.stream_info.bits_per_sample;
    PoolSize = metadata->data.stream_info.max_blocksize*2;

    SamplePool[0] = (FLAC__int32 *)Z_Malloc(sizeof(FLAC__int32)*PoolSize*NumChannels);
    SamplePool[1] = SamplePool[0]+PoolSize;
  }
  unguard;
}


//==========================================================================
//
//  VFlacAudioCodec::FStream::error_callback
//
//==========================================================================
void VFlacAudioCodec::FStream::error_callback (::FLAC__StreamDecoderErrorStatus) {
}


//==========================================================================
//
//  VFlacAudioCodec::Create
//
//==========================================================================
VAudioCodec *VFlacAudioCodec::Create (VStream *InStream) {
  guard(VFlacAudioCodec::Create);
  // check if it's a FLAC file
  InStream->Seek(0);
  char Hdr[4];
  InStream->Serialise(Hdr, 4);
  if (Hdr[0] != 'f' || Hdr[1] != 'L' || Hdr[2] != 'a' || Hdr[3] != 'C') return nullptr;

  FStream *Strm = new FStream(InStream);
  if (!Strm->SampleRate) {
    delete Strm;
    Strm = nullptr;
    return nullptr;
  }
  return new VFlacAudioCodec(Strm);
  unguard;
}
