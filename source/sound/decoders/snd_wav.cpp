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
#include "../snd_local.h"

struct __attribute__((packed)) FRiffChunkHeader {
  char ID[4];
  vuint32 Size;
};
static_assert(sizeof(FRiffChunkHeader) == 8, "invalid size of FRiffChunkHeader");

struct __attribute__((packed)) FWavFormatDesc {
  vuint16 Format;
  vuint16 Channels;
  vuint32 Rate;
  vuint32 BytesPerSec;
  vuint16 BlockAlign;
  vuint16 Bits;
};
static_assert(sizeof(FWavFormatDesc) == 2+2+4+4+2+2, "invalid size of FWavFormatDesc");


class VWaveSampleLoader : public VSampleLoader {
public:
  VWaveSampleLoader () : VSampleLoader(AUDIO_DEFAULT_PRIO) {}
  virtual void Load (sfxinfo_t &, VStream &) override;
  virtual const char *GetName () const noexcept override;
};


class VWavAudioCodec : public VAudioCodec {
public:
  VStream *Strm;
  int SamplesLeft;
  int WavChannels;
  int WavBits;
  int BlockAlign;
  vuint8 Buf[1024];

public:
  VWavAudioCodec (VStream *InStrm);
  virtual ~VWavAudioCodec () override;

  virtual int Decode (vint16 *Data, int NumFrames) override;
  virtual bool Finished () override;
  virtual void Restart () override;

  static VAudioCodec *Create (VStream *InStrm, const vuint8 sign[], int signsize);
};


static VWaveSampleLoader WaveSampleLoader;

IMPLEMENT_AUDIO_CODEC_EX(VWavAudioCodec, "Wav", 663); // before XMP


#define WAVE_FORMAT_PCM        (0x1u)


//==========================================================================
//
//  GetWavFormatName
//
//==========================================================================
static const char *GetWavFormatName (const vuint16 fmt) {
  switch (fmt) {
    case 0x0000u: return "unknown";
    case 0x0002u: return "adpcm";
    case 0x0003u: return "ieee-float";
    case 0x0004u: return "vselp";
    case 0x0005u: return "ibm-cvsd";
    case 0x0006u: return "alaw";
    case 0x0007u: return "mulaw";
    case 0x0008u: return "dts";
    case 0x0009u: return "drm";
    case 0x000au: return "wmavoice9";
    case 0x000bu: return "wmavoice10";
    case 0x0010u: return "oki-adpcm";
    case 0x0011u: return "ima-adpcm";
    case 0x0012u: return "mediaspace-adpcm";
    case 0x0013u: return "sierra-adpcm";
    case 0x0014u: return "g723-adpcm";
    case 0x0015u: return "digistd";
    case 0x0016u: return "digifix";
    case 0x0017u: return "dialogic-oki-adpcm";
    case 0x0018u: return "mediavision-adpcm";
    case 0x0019u: return "cu-codec";
    case 0x001au: return "hp-dyn-voice";
    case 0x0020u: return "yamaha-adpcm";
    case 0x0021u: return "sonarc";
    case 0x0022u: return "dspgroup-truespeech";
    case 0x0023u: return "echosc1";
    case 0x0024u: return "audiofile-af36";
    case 0x0025u: return "aptx";
    case 0x0026u: return "audiofile-af10";
    case 0x0027u: return "prosody-1612";
    case 0x0028u: return "lrc";
    case 0x0030u: return "dolby-ac2";
    case 0x0031u: return "gsm610";
    case 0x0032u: return "msnaudio";
    case 0x0033u: return "antex-adpcme";
    case 0x0034u: return "control-res-vqlpc";
    case 0x0035u: return "digireal";
    case 0x0036u: return "digiadpcm";
    case 0x0037u: return "control-res-cr10";
    case 0x0038u: return "nms-vbxadpcm";
    case 0x0039u: return "cs-imaadpcm";
    case 0x003au: return "echosc3";
    case 0x003bu: return "rockwell-adpcm";
    case 0x003cu: return "rockwell-digitalk";
    case 0x003du: return "xebec";
    case 0x0040u: return "g721-adpcm";
    case 0x0041u: return "g728-celp";
    case 0x0042u: return "msg723";
    case 0x0043u: return "intel-g723-1";
    case 0x0044u: return "intel-g729";
    case 0x0045u: return "sharp-g726";
    case 0x0050u: return "mpeg";
    case 0x0052u: return "rt24";
    case 0x0053u: return "pac";
    case 0x0055u: return "mpeglayer3";
    case 0x0059u: return "lucent-g723";
    case 0x0060u: return "cirrus";
    case 0x0061u: return "espcm";
    case 0x0062u: return "voxware";
    case 0x0063u: return "canopus-atrac";
    case 0x0064u: return "g726-adpcm";
    case 0x0065u: return "g722-adpcm";
    case 0x0066u: return "dsat";
    case 0x0067u: return "dsat-display";
    case 0x0069u: return "voxware-byte-aligned";
    case 0x0070u: return "voxware-ac8";
    case 0x0071u: return "voxware-ac10";
    case 0x0072u: return "voxware-ac16";
    case 0x0073u: return "voxware-ac20";
    case 0x0074u: return "voxware-rt24";
    case 0x0075u: return "voxware-rt29";
    case 0x0076u: return "voxware-rt29hw";
    case 0x0077u: return "voxware-vr12";
    case 0x0078u: return "voxware-vr18";
    case 0x0079u: return "voxware-tq40";
    case 0x007au: return "voxware-sc3";
    case 0x007bu: return "voxware-sc3-1";
    case 0x0080u: return "softsound";
    case 0x0081u: return "voxware-tq60";
    case 0x0082u: return "msrt24";
    case 0x0083u: return "g729a";
    case 0x0084u: return "mvi-mvi2";
    case 0x0085u: return "df-g726";
    case 0x0086u: return "df-gsm610";
    case 0x0088u: return "isiaudio";
    case 0x0089u: return "onlive";
    case 0x008au: return "multitude-ft-sx20";
    case 0x008bu: return "infocom-its-g721-adpcm";
    case 0x008cu: return "convedia-g729";
    case 0x008du: return "congruency";
    case 0x0091u: return "sbc24";
    case 0x0092u: return "dolby-ac3-spdif";
    case 0x0093u: return "mediasonic-g723";
    case 0x0094u: return "prosody-8kbps";
    case 0x0097u: return "zyxel-adpcm";
    case 0x0098u: return "philips-lpcbb";
    case 0x0099u: return "packed";
    case 0x00a0u: return "malden-phonytalk";
    case 0x00a1u: return "racal-recorder-gsm";
    case 0x00a2u: return "racal-recorder-g720-a";
    case 0x00a3u: return "racal-recorder-g723-1";
    case 0x00a4u: return "racal-recorder-tetra-acelp";
    case 0x00b0u: return "nec-aac";
    case 0x00ffu: return "raw-aac1";
    case 0x0100u: return "rhetorex-adpcm";
    case 0x0101u: return "irat";
    case 0x0111u: return "vivo-g723";
    case 0x0112u: return "vivo-siren";
    case 0x0120u: return "philips-celp";
    case 0x0121u: return "philips-grundig";
    case 0x0123u: return "digital-g723";
    case 0x0125u: return "sanyo-ld-adpcm";
    case 0x0130u: return "siprolab-aceplnet";
    case 0x0131u: return "siprolab-acelp4800";
    case 0x0132u: return "siprolab-acelp8v3";
    case 0x0133u: return "siprolab-g729";
    case 0x0134u: return "siprolab-g729a";
    case 0x0135u: return "siprolab-kelvin";
    case 0x0136u: return "voiceage-amr";
    case 0x0140u: return "g726adpcm";
    case 0x0141u: return "dictaphone-celp68";
    case 0x0142u: return "dictaphone-celp54";
    case 0x0150u: return "qualcomm-purevoice";
    case 0x0151u: return "qualcomm-halfrate";
    case 0x0155u: return "tubgsm";
    case 0x0160u: return "msaudio1";
    case 0x0161u: return "wmaudio2";
    case 0x0162u: return "wmaudio3";
    case 0x0163u: return "wmaudio-lossless";
    case 0x0164u: return "wmaspdif";
    case 0x0170u: return "unisys-nap-adpcm";
    case 0x0171u: return "unisys-nap-ulaw";
    case 0x0172u: return "unisys-nap-alaw";
    case 0x0173u: return "unisys-nap-16k";
    case 0x0174u: return "sycom-acm-syc008";
    case 0x0175u: return "sycom-acm-syc701-g726l";
    case 0x0176u: return "sycom-acm-syc701-celp54";
    case 0x0177u: return "sycom-acm-syc701-celp68";
    case 0x0178u: return "knowledge-adventure-adpcm";
    case 0x0180u: return "fraunhofer-iis-mpeg2-aac";
    case 0x0190u: return "dts-ds";
    case 0x0200u: return "creative-adpcm";
    case 0x0202u: return "creative-fastspeech8";
    case 0x0203u: return "creative-fastspeech10";
    case 0x0210u: return "uher-adpcm";
    case 0x0215u: return "ulead-dv-audio";
    case 0x0216u: return "ulead-dv-audio-1";
    case 0x0220u: return "quarterdeck";
    case 0x0230u: return "ilink-vc";
    case 0x0240u: return "raw-sport";
    case 0x0241u: return "esst-ac3";
    case 0x0249u: return "generic-passthru";
    case 0x0250u: return "ipi-hsx";
    case 0x0251u: return "ipi-rpelp";
    case 0x0260u: return "cs2";
    case 0x0270u: return "sony-scx";
    case 0x0271u: return "sony-scy";
    case 0x0272u: return "sony-atrac3";
    case 0x0273u: return "sony-spc";
    case 0x0280u: return "telum-audio";
    case 0x0281u: return "telum-ia-audio";
    case 0x0285u: return "norcom-voice-systems-adpcm";
    case 0x0300u: return "fm-towns-snd";
    case 0x0350u: return "micronas";
    case 0x0351u: return "micronas-celp833";
    case 0x0400u: return "btv-digital";
    case 0x0401u: return "intel-music-coder";
    case 0x0402u: return "indeo-audio";
    case 0x0450u: return "qdesign-music";
    case 0x0500u: return "on2-vp7-audio";
    case 0x0501u: return "on2-vp6-audio";
    case 0x0680u: return "vme-vmpcm";
    case 0x0681u: return "tpc";
    case 0x08aeu: return "lightwave-lossless";
    case 0x1000u: return "oligsm";
    case 0x1001u: return "oliadpcm";
    case 0x1002u: return "olicelp";
    case 0x1003u: return "olisbc";
    case 0x1004u: return "oliopr";
    case 0x1100u: return "lh-codec";
    case 0x1101u: return "lh-codec-celp";
    case 0x1102u: return "lh-codec-sbc8";
    case 0x1103u: return "lh-codec-sbc12";
    case 0x1104u: return "lh-codec-sbc16";
    case 0x1400u: return "norris";
    case 0x1401u: return "isiaudio-2";
    case 0x1500u: return "soundspace-musicompress";
    case 0x1600u: return "mpeg-adts-aac";
    case 0x1601u: return "mpeg-raw-aac";
    case 0x1602u: return "mpeg-loas";
    case 0x1608u: return "nokia-mpeg-adts-aac";
    case 0x1609u: return "nokia-mpeg-raw-aac";
    case 0x160au: return "vodafone-mpeg-adts-aac";
    case 0x160bu: return "vodafone-mpeg-raw-aac";
    case 0x1610u: return "mpeg-heaac";
    case 0x181cu: return "voxware-rt24-speech";
    case 0x1971u: return "sonicfoundry-lossless";
    case 0x1979u: return "innings-telecom-adpcm";
    case 0x1c07u: return "lucent-sx8300p";
    case 0x1c0cu: return "lucent-sx5363s";
    case 0x1f03u: return "cuseeme";
    case 0x1fc4u: return "ntcsoft-alf2cm-acm";
    case 0x2000u: return "dvm";
    case 0x2001u: return "dts2";
    case 0x3313u: return "makeavis";
    case 0x4143u: return "divio-mpeg4-aac";
    case 0x4201u: return "nokia-adaptive-multirate";
    case 0x4243u: return "divio-g726";
    case 0x434cu: return "lead-speech";
    case 0x564cu: return "lead-vorbis";
    case 0x5756u: return "wavpack-audio";
    case 0x674fu: return "ogg-vorbis-mode-1";
    case 0x6750u: return "ogg-vorbis-mode-2";
    case 0x6751u: return "ogg-vorbis-mode-3";
    case 0x676fu: return "ogg-vorbis-mode-1-plus";
    case 0x6770u: return "ogg-vorbis-mode-2-plus";
    case 0x6771u: return "ogg-vorbis-mode-3-plus";
    case 0x7000u: return "3com-nbx";
    case 0x706du: return "faad-aac";
    case 0x7a21u: return "gsm-amr-cbr";
    case 0x7a22u: return "gsm-amr-vbr-sid";
    case 0xa100u: return "comverse-infosys-g723-1";
    case 0xa101u: return "comverse-infosys-avqsbc";
    case 0xa102u: return "comverse-infosys-sbc";
    case 0xa103u: return "symbol-g729-a";
    case 0xa104u: return "voiceage-amr-wb";
    case 0xa105u: return "ingenient-g726";
    case 0xa106u: return "mpeg4-aac";
    case 0xa107u: return "encore-g726";
    case 0xa108u: return "zoll-asao";
    case 0xa109u: return "speex-voice";
    case 0xa10au: return "vianix-masc";
    case 0xa10bu: return "wm9-spectrum-analyzer";
    case 0xa10cu: return "wmf-spectrum-anayzer";
    case 0xa10du: return "gsm-610";
    case 0xa10eu: return "gsm-620";
    case 0xa10fu: return "gsm-660";
    case 0xa110u: return "gsm-690";
    case 0xa111u: return "gsm-adaptive-multirate-wb";
    case 0xa112u: return "polycom-g722";
    case 0xa113u: return "polycom-g728";
    case 0xa114u: return "polycom-g729-a";
    case 0xa115u: return "polycom-siren";
    case 0xa116u: return "global-ip-ilbc";
    case 0xa117u: return "radiotime-time-shift-radio";
    case 0xa118u: return "nice-aca";
    case 0xa119u: return "nice-adpcm";
    case 0xa11au: return "vocord-g721";
    case 0xa11bu: return "vocord-g726";
    case 0xa11cu: return "vocord-g722-1";
    case 0xa11du: return "vocord-g728";
    case 0xa11eu: return "vocord-g729";
    case 0xa11fu: return "vocord-g729-a";
    case 0xa120u: return "vocord-g723-1";
    case 0xa121u: return "vocord-lbc";
    case 0xa122u: return "nice-g728";
    case 0xa123u: return "frace-telecom-g729";
    case 0xa124u: return "codian";
    case 0xf1acu: return "flac";
  }
  return va("unk0x%04x", fmt);
}


//==========================================================================
//
//  ReportInvalidWaveFormat
//
//==========================================================================
static void ReportInvalidWaveFormat (VStr strmname, const vuint16 Format) {
  GCon->Logf(NAME_Error, "cannot decode WAV file '%s' with format \"%s\"", *strmname, GetWavFormatName(Format));
}


//==========================================================================
//
//  FindRiffChunk
//
//  returns chunk size, or -1 if not found
//  if found, seeks on chunk data
//
//==========================================================================
static int FindRiffChunk (VStream &Strm, const char ID[4]) {
  Strm.Seek(12);
  if (Strm.IsError()) return -1;
  int EndPos = Strm.TotalSize();
  while (Strm.Tell()+8 <= EndPos) {
    if (Strm.IsError()) return -1;
    FRiffChunkHeader ChunkHdr;
    Strm.Serialise(&ChunkHdr, 8);
    if (Strm.IsError()) return -1;
    int ChunkSize = LittleLong(ChunkHdr.Size);
    if (memcmp(ChunkHdr.ID, ID, 4) == 0) return ChunkSize; // found chunk
    if (Strm.Tell()+ChunkSize > EndPos) break; // chunk goes beyound end of file
    Strm.Seek(Strm.Tell()+ChunkSize);
    if (Strm.IsError()) return -1;
  }
  return -1;
}


//==========================================================================
//
//  VWaveSampleLoader::GetName
//
//==========================================================================
const char *VWaveSampleLoader::GetName () const noexcept {
  return "wav";
}


//==========================================================================
//
//  VWaveSampleLoader::Load
//
//==========================================================================
void VWaveSampleLoader::Load (sfxinfo_t &Sfx, VStream &Strm) {
  // check header to see if it's a wave file
  char Header[12];
  Strm.Seek(0);
  Strm.Serialise(Header, 12);
  if (Strm.IsError()) return;
  if (memcmp(Header, "RIFF", 4) != 0 || memcmp(Header+8, "WAVE", 4) != 0) {
    // not a WAVE
    //GCon->Log(NAME_Debug, "WAV: not a RIFF/WAVE");
    return;
  }

  // get format settings
  int FmtSize = FindRiffChunk(Strm, "fmt ");
  if (FmtSize < 16) return; // format not found or too small

  FWavFormatDesc Fmt;
  Strm.Serialise(&Fmt, 16);
  if (Strm.IsError()) return;
  //GCon->Logf(NAME_Debug, "WAV: fmt=%d", (int)Fmt.Format);
  if (LittleShort(Fmt.Format) != WAVE_FORMAT_PCM) {
    ReportInvalidWaveFormat(Strm.GetName(), Fmt.Format);
    return; // not a PCM format
  }

  int SampleRate = LittleLong(Fmt.Rate);
  int WavChannels = LittleShort(Fmt.Channels);
  int WavBits = LittleShort(Fmt.Bits);
  int BlockAlign = LittleShort(Fmt.BlockAlign);
  //if (WavChannels != 1) GCon->Logf("A stereo sample, taking left channel");

  //GCon->Logf(NAME_Debug, "WAV: srate=%d; chans=%d; bits=%d; align=%d", (int)SampleRate, (int)WavChannels, (int)WavBits, (int)BlockAlign);

  if (WavChannels < 1 || WavChannels > 2) return;
  if (SampleRate < 128 || SampleRate > 96000) return;
  if (WavBits != 8 && WavBits != 16) return;
  if (BlockAlign < WavChannels*(WavBits/8) || BlockAlign > 1024) return;

  // find data chunk
  int DataSize = FindRiffChunk(Strm, "data");
  if (DataSize == -1) return; // data not found

  // read wave data
  void *WavData = Z_Malloc(DataSize);
  Strm.Serialise(WavData, DataSize);
  if (Strm.IsError()) { Z_Free(WavData); return; }

  // fill in sample info and allocate data.
  Sfx.SampleRate = SampleRate;
  Sfx.SampleBits = WavBits;
  Sfx.DataSize = (DataSize/BlockAlign)*(WavBits/8);
  Sfx.Data = Z_Malloc(Sfx.DataSize);

  // copy sample data
  DataSize /= BlockAlign;
  if (WavBits == 8) {
    vuint8 *pSrc = (vuint8 *)WavData;
    vuint8 *pDst = (vuint8 *)Sfx.Data;
    for (int i = 0; i < DataSize; ++i, pSrc += BlockAlign) {
      int v = 0;
      for (int f = 0; f < WavChannels; ++f) v += (int)pSrc[f];
      v /= WavChannels;
      if (v < -128) v = -128; else if (v > 127) v = 127;
      *pDst++ = (vuint8)v;
    }
  } else {
    vuint8 *pSrc = (vuint8 *)WavData;
    vint16 *pDst = (vint16 *)Sfx.Data;
    for (int i = 0; i < DataSize; ++i, pSrc += BlockAlign) {
      int v = 0;
      for (int f = 0; f < WavChannels; ++f) v += (int)(LittleShort(*(((vint16 *)pSrc)+f)));
      v /= WavChannels;
      if (v < -32768) v = -32768; else if (v > 32767) v = 32767;
      *pDst++ = (vint16)v;
    }
  }
  Z_Free(WavData);
}


//==========================================================================
//
//  VWavAudioCodec::VWavAudioCodec
//
//==========================================================================
VWavAudioCodec::VWavAudioCodec (VStream *InStrm)
  : Strm(InStrm)
  , SamplesLeft(-1)
{
  //GCon->Log(NAME_Debug, "WAVCRT: trying...");
  int FmtSize = FindRiffChunk(*Strm, "fmt ");
  if (FmtSize < 16) return; // format not found or too small

  FWavFormatDesc Fmt;
  Strm->Serialise(&Fmt, 16);
  if (Strm->IsError()) return;
  //GCon->Logf(NAME_Debug, "WAV: fmt=%d", (int)Fmt.Format);
  //GCon->Logf(NAME_Debug, "WAV: srate=%d; chans=%d; bits=%d; align=%d", (int)LittleLong(Fmt.Rate), (int)LittleShort(Fmt.Channels), (int)LittleShort(Fmt.Bits), (int)LittleShort(Fmt.BlockAlign));
  if (LittleShort(Fmt.Format) != WAVE_FORMAT_PCM) {
    ReportInvalidWaveFormat(Strm->GetName(), Fmt.Format);
    return; // not a PCM format
  }

  SampleRate = LittleLong(Fmt.Rate);
  WavChannels = LittleShort(Fmt.Channels);
  WavBits = LittleShort(Fmt.Bits);
  BlockAlign = LittleShort(Fmt.BlockAlign);
  //GCon->Logf(NAME_Debug, "WAV: srate=%d; chans=%d; bits=%d; align=%d", (int)SampleRate, (int)WavChannels, (int)WavBits, (int)BlockAlign);
  if (WavBits != 8 && WavBits != 16) return;
  if (WavChannels < 1 || WavChannels > 2) return;
  if (SampleRate < 128 || SampleRate > 96000) return;
  if (BlockAlign < WavChannels*(WavBits/8) || BlockAlign > 1024) return;

  SamplesLeft = FindRiffChunk(*Strm, "data");
  if (SamplesLeft == -1) return; // data not found

  SamplesLeft /= BlockAlign;
}


//==========================================================================
//
//  VWavAudioCodec::~VWavAudioCodec
//
//==========================================================================
VWavAudioCodec::~VWavAudioCodec () {
  if (SamplesLeft != -1) {
    VStream::Destroy(Strm);
  }
}


//==========================================================================
//
//  VWavAudioCodec::Decode
//
//==========================================================================
int VWavAudioCodec::Decode (vint16 *Data, int NumFrames) {
  vassert(WavChannels == 1 || WavChannels == 2);
  int CurFrame = 0;
  while (SamplesLeft && CurFrame < NumFrames) {
    int ReadSamples = 1024/BlockAlign;
    if (ReadSamples > NumFrames-CurFrame) ReadSamples = NumFrames-CurFrame;
    if (ReadSamples > SamplesLeft) ReadSamples = SamplesLeft;
    Strm->Serialise(Buf, ReadSamples*BlockAlign);
    // decode interleaved sound data
    for (int i = 0; i < 2; ++i) {
      const vuint8 *pSrc = Buf;
      if (i && WavChannels > 1) pSrc += WavBits/8;
      vint16 *pDst = Data+CurFrame*2+i;
      for (int j = ReadSamples; j--; pSrc += BlockAlign, pDst += 2) {
        vint16 v;
        if (WavBits == 8) {
          #if 0
          const vuint8 b = *pSrc;
          v = b<<8;
          // extend last bit
          if (b&0x01) v |= 0xff;
          #else
          // use FP for conversion, why not?
          const float fv = (float)(*pSrc)/255.0f*32767.0f;
          const int v32 = (int)fv;
          v = (v32 <= -32768 ? -32768 : v32 >= 32767 ? 32767 : (vint16)v32);
          #endif
        } else {
          v = LittleShort(*(const int16_t *)pSrc);
        }
        *pDst = v;
      }
    }
    SamplesLeft -= ReadSamples;
    CurFrame += ReadSamples;
  }
  return CurFrame;
}


//==========================================================================
//
//  VWavAudioCodec::Finished
//
//==========================================================================
bool VWavAudioCodec::Finished () {
  return !SamplesLeft;
}


//==========================================================================
//
//  VWavAudioCodec::Restart
//
//==========================================================================
void VWavAudioCodec::Restart () {
  SamplesLeft = FindRiffChunk(*Strm, "data");
  // just in case
  if (SamplesLeft > 0) SamplesLeft /= BlockAlign; else SamplesLeft = 0;
}


//==========================================================================
//
//  VWavAudioCodec::Create
//
//==========================================================================
VAudioCodec *VWavAudioCodec::Create (VStream *InStrm, const vuint8 sign[], int signsize) {
  //GCon->Logf(NAME_Debug, "WAVCRT: signsize=%d; <%.*s>", signsize, signsize, (const char *)sign);
  if (memcmp(sign, "RIFF", 4) != 0) return nullptr;
  char Header[12];
  if (signsize >= 12) {
    memcpy(Header, sign, 12);
  } else {
    InStrm->Seek(0);
    InStrm->Serialise(Header, 12);
  }
  if (memcmp(Header, "RIFF", 4) != 0 || memcmp(Header+8, "WAVE", 4) != 0) {
    //GCon->Log(NAME_Debug, "WAVCRT: not a RIFF/WAVE");
    return nullptr;
  }

  // it's a WAVE file
  VWavAudioCodec *Codec = new VWavAudioCodec(InStrm);
  if (Codec->SamplesLeft != -1) return Codec;
  // file seams to be broken
  delete Codec;
  return nullptr;
}
