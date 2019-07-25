/*

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   instrum.h

   */

#include <stdlib.h>
#include <string.h>

#include "timidity.h"

#define __Sound_SetError(x)

namespace LibTimidity
{

/*-------------------------------------------------------------------------*/
/* * * * * * * * * * * * * * * * * load_riff.h * * * * * * * * * * * * * * */
/*-------------------------------------------------------------------------*/
struct RIFF_Chunk
{
	uint32 magic;
	uint32 length;
	uint32 subtype;
	uint8  *data;
	RIFF_Chunk *child;
	RIFF_Chunk *next;
};

extern RIFF_Chunk* LoadRIFF(FILE *src);
extern void FreeRIFF(RIFF_Chunk *chunk);
extern void PrintRIFF(RIFF_Chunk *chunk, int level);
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*-------------------------------------------------------------------------*/
/* * * * * * * * * * * * * * * * * load_riff.c * * * * * * * * * * * * * * */
/*-------------------------------------------------------------------------*/
#define RIFF        0x46464952        /* "RIFF" */
#define LIST        0x5453494c        /* "LIST" */

static RIFF_Chunk *AllocRIFFChunk()
{
	RIFF_Chunk *chunk = (RIFF_Chunk *)safe_malloc(sizeof(*chunk));

	if (!chunk)
	{
		__Sound_SetError(ERR_OUT_OF_MEMORY);
		return NULL;
	}
	memset(chunk, 0, sizeof(*chunk));

	return chunk;
}

static void FreeRIFFChunk(RIFF_Chunk *chunk)
{
	if (chunk->child)
	{
		FreeRIFFChunk(chunk->child);
	}

	if (chunk->next)
	{
		FreeRIFFChunk(chunk->next);
	}
	free(chunk);
	chunk = NULL;
}

static int ChunkHasSubType(uint32 magic)
{
	static uint32 chunk_list[] =
	{
		RIFF, LIST
	};
	int i;

	for (i = 0; i < sizeof(chunk_list) / sizeof(chunk_list[0]); ++i)
	{
		if (magic == chunk_list[i])
		{
			return 1;
		}
	}

	return 0;
}

static int ChunkHasSubChunks(uint32 magic)
{
	static uint32 chunk_list[] =
	{
		RIFF, LIST
	};
	int i;

	for (i = 0; i < sizeof(chunk_list) / sizeof(chunk_list[0]); ++i)
	{
		if (magic == chunk_list[i])
		{
			return 1;
		}
	}

	return 0;
}

static void LoadSubChunks(RIFF_Chunk *chunk, uint8 *data, uint32 left)
{
	uint8 *subchunkData;
	uint32 subchunkDataLen;

	while (left > 8)
	{
		RIFF_Chunk *child = AllocRIFFChunk();
		RIFF_Chunk *next, *prev = NULL;

		for (next = chunk->child; next; next = next->next)
		{
			prev = next;
		}

		if (prev)
		{
			prev->next = child;
		}
		else
		{
			chunk->child = child;
		}
		child->magic = (data[0] <<  0) | (data[1] <<  8) |
						(data[2] << 16) | (data[3] << 24);
		data += 4;
		left -= 4;
		child->length = (data[0] <<  0) | (data[1] <<  8) |
						(data[2] << 16) | (data[3] << 24);
		data += 4;
		left -= 4;
		child->data = data;

		if (child->length > left)
		{
			child->length = left;
		}
		subchunkData = child->data;
		subchunkDataLen = child->length;

		if (ChunkHasSubType(child->magic) && subchunkDataLen >= 4)
		{
			child->subtype = (subchunkData[0] <<  0) | (subchunkData[1] <<  8) |
						(subchunkData[2] << 16) | (subchunkData[3] << 24);
			subchunkData += 4;
			subchunkDataLen -= 4;
		}

		if (ChunkHasSubChunks(child->magic))
		{
			LoadSubChunks(child, subchunkData, subchunkDataLen);
		}
		data += (child->length + 1) & ~1;
		left -= (child->length + 1) & ~1;
	}
}

RIFF_Chunk *LoadRIFF(FILE* src)
{
	RIFF_Chunk *chunk;
	uint8 *subchunkData;
	uint32 subchunkDataLen;

	/* Allocate the chunk structure */
	chunk = AllocRIFFChunk();

	/* Make sure the file is in RIFF format */
	fread(&chunk->magic, 4, 1, src);
	fread(&chunk->length, 4, 1, src);
	chunk->magic = LE_LONG(chunk->magic);
	chunk->length = LE_LONG(chunk->length);

	if (chunk->magic != RIFF)
	{
		__Sound_SetError("Not a RIFF file");
		FreeRIFFChunk(chunk);

		return NULL;
	}
	chunk->data = (uint8 *)safe_malloc(chunk->length);

	if (chunk->data == NULL)
	{
		__Sound_SetError(ERR_OUT_OF_MEMORY);
		FreeRIFFChunk(chunk);

		return NULL;
	}

	if (fread(chunk->data, chunk->length, 1, src) != 1)
	{
		__Sound_SetError(ERR_IO_ERROR);
		FreeRIFF(chunk);

		return NULL;
	}
	subchunkData = chunk->data;
	subchunkDataLen = chunk->length;

	if (ChunkHasSubType(chunk->magic) && subchunkDataLen >= 4)
	{
		chunk->subtype = (subchunkData[0] <<  0) | (subchunkData[1] <<  8) |
					(subchunkData[2] << 16) | (subchunkData[3] << 24);
		subchunkData += 4;
		subchunkDataLen -= 4;
	}

	if (ChunkHasSubChunks(chunk->magic))
	{
		LoadSubChunks(chunk, subchunkData, subchunkDataLen);
	}

	return chunk;
}

void FreeRIFF(RIFF_Chunk *chunk)
{
	free(chunk->data);
	chunk->data = NULL;
	FreeRIFFChunk(chunk);
}

/*-------------------------------------------------------------------------*/
/* * * * * * * * * * * * * * * * * load_dls.h  * * * * * * * * * * * * * * */
/*-------------------------------------------------------------------------*/
/* This code is based on the DLS spec version 1.1, available at:
    http://www.midi.org/about-midi/dls/dlsspec.shtml
*/

/* Some typedefs so the public dls headers don't need to be modified */
#define FAR
typedef uint8   BYTE;
typedef int16   SHORT;
typedef uint16  USHORT;
typedef uint16  WORD;
typedef int32   LONG;
typedef uint32  ULONG;
typedef uint32  DWORD;
#define mmioFOURCC(A, B, C, D)    \
    (((A) <<  0) | ((B) <<  8) | ((C) << 16) | ((D) << 24))
#define DEFINE_GUID(A, B, C, E, F, G, H, I, J, K, L, M)

#include "dls1.h"
#include "dls2.h"

struct WaveFMT
{
	WORD wFormatTag;
	WORD wChannels;
	DWORD dwSamplesPerSec;
	DWORD dwAvgBytesPerSec;
	WORD wBlockAlign;
	WORD wBitsPerSample;
};

struct DLS_Wave
{
	WaveFMT *format;
	uint8 *data;
	uint32 length;
	WSMPL *wsmp;
	WLOOP *wsmp_loop;
};

struct DLS_Region
{
	RGNHEADER *header;
	WAVELINK *wlnk;
	WSMPL *wsmp;
	WLOOP *wsmp_loop;
	CONNECTIONLIST *art;
	CONNECTION *artList;
};

struct DLS_Instrument
{
	const char *name;
	INSTHEADER *header;
	DLS_Region *regions;
	CONNECTIONLIST *art;
	CONNECTION *artList;
};

struct DLS_Data
{
	RIFF_Chunk *chunk;

	uint32 cInstruments;
	DLS_Instrument *instruments;

	POOLTABLE *ptbl;
	POOLCUE *ptblList;
	DLS_Wave *waveList;

	const char *name;
	const char *artist;
	const char *copyright;
	const char *comments;
};

extern DLS_Data* LoadDLS(FILE *src);
extern void FreeDLS(DLS_Data *chunk);
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*-------------------------------------------------------------------------*/
/* * * * * * * * * * * * * * * * * load_dls.c  * * * * * * * * * * * * * * */
/*-------------------------------------------------------------------------*/

#define FOURCC_LIST    0x5453494c   /* "LIST" */
#define FOURCC_FMT     0x20746D66   /* "fmt " */
#define FOURCC_DATA    0x61746164   /* "data" */
#define FOURCC_INFO    mmioFOURCC('I','N','F','O')
#define FOURCC_IARL    mmioFOURCC('I','A','R','L')
#define FOURCC_IART    mmioFOURCC('I','A','R','T')
#define FOURCC_ICMS    mmioFOURCC('I','C','M','S')
#define FOURCC_ICMT    mmioFOURCC('I','C','M','T')
#define FOURCC_ICOP    mmioFOURCC('I','C','O','P')
#define FOURCC_ICRD    mmioFOURCC('I','C','R','D')
#define FOURCC_IENG    mmioFOURCC('I','E','N','G')
#define FOURCC_IGNR    mmioFOURCC('I','G','N','R')
#define FOURCC_IKEY    mmioFOURCC('I','K','E','Y')
#define FOURCC_IMED    mmioFOURCC('I','M','E','D')
#define FOURCC_INAM    mmioFOURCC('I','N','A','M')
#define FOURCC_IPRD    mmioFOURCC('I','P','R','D')
#define FOURCC_ISBJ    mmioFOURCC('I','S','B','J')
#define FOURCC_ISFT    mmioFOURCC('I','S','F','T')
#define FOURCC_ISRC    mmioFOURCC('I','S','R','C')
#define FOURCC_ISRF    mmioFOURCC('I','S','R','F')
#define FOURCC_ITCH    mmioFOURCC('I','T','C','H')


static void FreeRegions(DLS_Instrument *instrument)
{
	if (instrument->regions)
	{
		free(instrument->regions);
		instrument->regions = NULL;
	}
}

static void AllocRegions(DLS_Instrument *instrument)
{
	int datalen = (instrument->header->cRegions * sizeof(DLS_Region));

	FreeRegions(instrument);
	instrument->regions = (DLS_Region *)safe_malloc(datalen);

	if (instrument->regions)
	{
		memset(instrument->regions, 0, datalen);
	}
}

static void FreeInstruments(DLS_Data *data)
{
	if (data->instruments)
	{
		uint32 i;

		for (i = 0; i < data->cInstruments; ++i)
		{
			FreeRegions(&data->instruments[i]);
		}
		free(data->instruments);
		data->instruments = NULL;
	}
}

static void AllocInstruments(DLS_Data *data)
{
	int datalen = (data->cInstruments * sizeof(DLS_Instrument));

	FreeInstruments(data);
	data->instruments = (DLS_Instrument *)safe_malloc(datalen);
	if (data->instruments)
	{
		memset(data->instruments, 0, datalen);
	}
}

static void FreeWaveList(DLS_Data *data)
{
	if (data->waveList)
	{
		free(data->waveList);
		data->waveList = NULL;
	}
}

static void AllocWaveList(DLS_Data *data)
{
	int datalen = (data->ptbl->cCues * sizeof(DLS_Wave));

	FreeWaveList(data);
	data->waveList = (DLS_Wave *)safe_malloc(datalen);
	if (data->waveList)
	{
		memset(data->waveList, 0, datalen);
	}
}

static void Parse_colh(DLS_Data *data, RIFF_Chunk *chunk)
{
	data->cInstruments = LE_LONG(*(uint32 *)chunk->data);
	AllocInstruments(data);
}

static void Parse_insh(DLS_Data *data, RIFF_Chunk *chunk, DLS_Instrument *instrument)
{
	INSTHEADER *header = (INSTHEADER *)chunk->data;
	header->cRegions = LE_LONG(header->cRegions);
	header->Locale.ulBank = LE_LONG(header->Locale.ulBank);
	header->Locale.ulInstrument = LE_LONG(header->Locale.ulInstrument);
	instrument->header = header;
	AllocRegions(instrument);
}

static void Parse_rgnh(DLS_Data *data, RIFF_Chunk *chunk, DLS_Region *region)
{
	RGNHEADER *header = (RGNHEADER *)chunk->data;
	header->RangeKey.usLow = LE_SHORT(header->RangeKey.usLow);
	header->RangeKey.usHigh = LE_SHORT(header->RangeKey.usHigh);
	header->RangeVelocity.usLow = LE_SHORT(header->RangeVelocity.usLow);
	header->RangeVelocity.usHigh = LE_SHORT(header->RangeVelocity.usHigh);
	header->fusOptions = LE_SHORT(header->fusOptions);
	header->usKeyGroup = LE_SHORT(header->usKeyGroup);
	region->header = header;
}

static void Parse_wlnk(DLS_Data *data, RIFF_Chunk *chunk, DLS_Region *region)
{
	WAVELINK *wlnk = (WAVELINK *)chunk->data;
	wlnk->fusOptions = LE_SHORT(wlnk->fusOptions);
	wlnk->usPhaseGroup = LE_SHORT(wlnk->usPhaseGroup);
	wlnk->ulChannel = LE_LONG(wlnk->ulChannel);
	wlnk->ulTableIndex = LE_LONG(wlnk->ulTableIndex);
	region->wlnk = wlnk;
}

static void Parse_wsmp(DLS_Data *data, RIFF_Chunk *chunk, WSMPL **wsmp_ptr, WLOOP **wsmp_loop_ptr)
{
	uint32 i;
	WSMPL *wsmp = (WSMPL *)chunk->data;
	WLOOP *loop;
	wsmp->cbSize = LE_LONG(wsmp->cbSize);
	wsmp->usUnityNote = LE_SHORT(wsmp->usUnityNote);
	wsmp->sFineTune = LE_SHORT(wsmp->sFineTune);
	wsmp->lAttenuation = LE_LONG(wsmp->lAttenuation);
	wsmp->fulOptions = LE_LONG(wsmp->fulOptions);
	wsmp->cSampleLoops = LE_LONG(wsmp->cSampleLoops);
	loop = (WLOOP *)((uint8 *)chunk->data + wsmp->cbSize);
	*wsmp_ptr = wsmp;
	*wsmp_loop_ptr = loop;

	for (i = 0; i < wsmp->cSampleLoops; ++i)
	{
		loop->cbSize = LE_LONG(loop->cbSize);
		loop->ulType = LE_LONG(loop->ulType);
		loop->ulStart = LE_LONG(loop->ulStart);
		loop->ulLength = LE_LONG(loop->ulLength);
		++loop;
	}
}

static void Parse_art(DLS_Data *data, RIFF_Chunk *chunk, CONNECTIONLIST **art_ptr, CONNECTION **artList_ptr)
{
	uint32 i;
	CONNECTIONLIST *art = (CONNECTIONLIST *)chunk->data;
	CONNECTION *artList;
	art->cbSize = LE_LONG(art->cbSize);
	art->cConnections = LE_LONG(art->cConnections);
	artList = (CONNECTION *)((uint8 *)chunk->data + art->cbSize);
	*art_ptr = art;
	*artList_ptr = artList;

	for (i = 0; i < art->cConnections; ++i)
	{
		artList->usSource = LE_SHORT(artList->usSource);
		artList->usControl = LE_SHORT(artList->usControl);
		artList->usDestination = LE_SHORT(artList->usDestination);
		artList->usTransform = LE_SHORT(artList->usTransform);
		artList->lScale = LE_LONG(artList->lScale);
		++artList;
	}
}

static void Parse_lart(DLS_Data *data, RIFF_Chunk *chunk, CONNECTIONLIST **conn_ptr, CONNECTION **connList_ptr)
{
	/* FIXME: This only supports one set of connections */
	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_ART1:
			case FOURCC_ART2:
			{
				Parse_art(data, chunk, conn_ptr, connList_ptr);
				return;
			}
		}
	}
}

static void Parse_rgn(DLS_Data *data, RIFF_Chunk *chunk, DLS_Region *region)
{
	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_RGNH:
				Parse_rgnh(data, chunk, region);
				break;
			case FOURCC_WLNK:
				Parse_wlnk(data, chunk, region);
				break;
			case FOURCC_WSMP:
				Parse_wsmp(data, chunk, &region->wsmp, &region->wsmp_loop);
				break;
			case FOURCC_LART:
			case FOURCC_LAR2:
				Parse_lart(data, chunk, &region->art, &region->artList);
				break;
		}
	}
}

static void Parse_lrgn(DLS_Data *data, RIFF_Chunk *chunk, DLS_Instrument *instrument)
{
	uint32 region = 0;

	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_RGN:
			case FOURCC_RGN2:
				if (region < instrument->header->cRegions)
				{
					Parse_rgn(data, chunk, &instrument->regions[region++]);
				}
				break;
		}
	}
}

static void Parse_INFO_INS(DLS_Data *data, RIFF_Chunk *chunk, DLS_Instrument *instrument)
{
	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_INAM: /* Name */
				instrument->name = (const char*)chunk->data;
				break;
		}
	}
}

static void Parse_ins(DLS_Data *data, RIFF_Chunk *chunk, DLS_Instrument *instrument)
{
	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_INSH:
				Parse_insh(data, chunk, instrument);
				break;
			case FOURCC_LRGN:
				Parse_lrgn(data, chunk, instrument);
				break;
			case FOURCC_LART:
			case FOURCC_LAR2:
				Parse_lart(data, chunk, &instrument->art, &instrument->artList);
				break;
			case FOURCC_INFO:
				Parse_INFO_INS(data, chunk, instrument);
				break;
		}
	}
}

static void Parse_lins(DLS_Data *data, RIFF_Chunk *chunk)
{
	uint32 instrument = 0;

	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_INS:
				if (instrument < data->cInstruments)
				{
					Parse_ins(data, chunk, &data->instruments[instrument++]);
				}
				break;
		}
	}
}

static void Parse_ptbl(DLS_Data *data, RIFF_Chunk *chunk)
{
	uint32 i;
	POOLTABLE *ptbl = (POOLTABLE *)chunk->data;
	ptbl->cbSize = LE_LONG(ptbl->cbSize);
	ptbl->cCues = LE_LONG(ptbl->cCues);
	data->ptbl = ptbl;
	data->ptblList = (POOLCUE *)((uint8 *)chunk->data + ptbl->cbSize);

	for (i = 0; i < ptbl->cCues; ++i)
	{
		data->ptblList[i].ulOffset = LE_LONG(data->ptblList[i].ulOffset);
	}
	AllocWaveList(data);
}

static void Parse_fmt(DLS_Data *data, RIFF_Chunk *chunk, DLS_Wave *wave)
{
	WaveFMT *fmt = (WaveFMT *)chunk->data;
	fmt->wFormatTag = LE_SHORT(fmt->wFormatTag);
	fmt->wChannels = LE_SHORT(fmt->wChannels);
	fmt->dwSamplesPerSec = LE_LONG(fmt->dwSamplesPerSec);
	fmt->dwAvgBytesPerSec = LE_LONG(fmt->dwAvgBytesPerSec);
	fmt->wBlockAlign = LE_SHORT(fmt->wBlockAlign);
	fmt->wBitsPerSample = LE_SHORT(fmt->wBitsPerSample);
	wave->format = fmt;
}

static void Parse_data(DLS_Data *data, RIFF_Chunk *chunk, DLS_Wave *wave)
{
	wave->data = chunk->data;
	wave->length = chunk->length;
}

static void Parse_wave(DLS_Data *data, RIFF_Chunk *chunk, DLS_Wave *wave)
{
	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_FMT:
				Parse_fmt(data, chunk, wave);
				break;
			case FOURCC_DATA:
				Parse_data(data, chunk, wave);
				break;
			case FOURCC_WSMP:
				Parse_wsmp(data, chunk, &wave->wsmp, &wave->wsmp_loop);
				break;
		}
	}
}

static void Parse_wvpl(DLS_Data *data, RIFF_Chunk *chunk)
{
	uint32 wave = 0;

	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_wave:
				if (wave < data->ptbl->cCues)
				{
					Parse_wave(data, chunk, &data->waveList[wave++]);
				}
				break;
		}
	}
}

static void Parse_INFO_DLS(DLS_Data *data, RIFF_Chunk *chunk)
{
	for (chunk = chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_IARL: /* Archival Location */
				break;
			case FOURCC_IART: /* Artist */
				data->artist = (const char*)chunk->data;
				break;
			case FOURCC_ICMS: /* Commisioned */
				break;
			case FOURCC_ICMT: /* Comments */
				data->comments = (const char*)chunk->data;
				break;
			case FOURCC_ICOP: /* Copyright */
				data->copyright = (const char*)chunk->data;
				break;
			case FOURCC_ICRD: /* Creation Date */
				break;
			case FOURCC_IENG: /* Engineer */
				break;
			case FOURCC_IGNR: /* Genre */
				break;
			case FOURCC_IKEY: /* Keywords */
				break;
			case FOURCC_IMED: /* Medium */
				break;
			case FOURCC_INAM: /* Name */
				data->name = (const char*)chunk->data;
				break;
			case FOURCC_IPRD: /* Product */
				break;
			case FOURCC_ISBJ: /* Subject */
				break;
			case FOURCC_ISFT: /* Software */
				break;
			case FOURCC_ISRC: /* Source */
				break;
			case FOURCC_ISRF: /* Source Form */
				break;
			case FOURCC_ITCH: /* Technician */
				break;
		}
	}
}

DLS_Data *LoadDLS(FILE *src)
{
	RIFF_Chunk *chunk;
	DLS_Data *data = (DLS_Data *)safe_malloc(sizeof(*data));

	if (!data)
	{
		__Sound_SetError(ERR_OUT_OF_MEMORY);
		return NULL;
	}
	memset(data, 0, sizeof(*data));
	data->chunk = LoadRIFF(src);

	if (!data->chunk)
	{
		FreeDLS(data);
		return NULL;
	}

	for (chunk = data->chunk->child; chunk; chunk = chunk->next)
	{
		uint32 magic = (chunk->magic == FOURCC_LIST) ? chunk->subtype : chunk->magic;

		switch(magic)
		{
			case FOURCC_COLH:
				Parse_colh(data, chunk);
				break;
			case FOURCC_LINS:
				Parse_lins(data, chunk);
				break;
			case FOURCC_PTBL:
				Parse_ptbl(data, chunk);
				break;
			case FOURCC_WVPL:
				Parse_wvpl(data, chunk);
				break;
			case FOURCC_INFO:
				Parse_INFO_DLS(data, chunk);
				break;
		}
	}

	return data;
}

void FreeDLS(DLS_Data *data)
{
	if (data->chunk)
	{
		FreeRIFF(data->chunk);
	}
	FreeInstruments(data);
	FreeWaveList(data);
	free(data);
	data = NULL;
}

/*-------------------------------------------------------------------------*/
/* * * * * * * * * * * * * * * * * instrum_dls.c * * * * * * * * * * * * * */
/*-------------------------------------------------------------------------*/

DLS_Data *Timidity_LoadDLS(FILE *src)
{
	DLS_Data *patches = LoadDLS(src);
	//if (!patches)
	//{
	//	SNDDBG(("%s", SDL_GetError()));
	//}

	return patches;
}

void Timidity_FreeDLS(DLS_Data *patches)
{
	FreeDLS(patches);
}

/*
static double RelativeGainToLinear(int centibels)
{
	// v = 10^(cb/(200*65536)) * V
	return 100.0 * pow(10.0, (double)(centibels / 65536) / 200.0);
}
*/

/* convert timecents to sec */
static double to_msec(int timecent)
{
	if (timecent == 0x80000000)
	{
		return 0.0;
	}

	return 1000.0 * pow(2.0, (double)(timecent / 65536) / 1200.0);
}

/* convert decipercent to {0..1} */
static double to_normalized_percent(int decipercent)
{
	return ((double)(decipercent / 65536)) / 1000.0;
}

/* convert from 8bit value to fractional offset (15.15) */
static int32 to_offset(int offset)
{
	return (int32)offset << (7+15);
}

/* calculate ramp rate in fractional unit;
* diff = 8bit, time = msec
*/
static int32 calc_rate(MidiSong* song, int diff, int sample_rate, double msec)
{
	double rate;

	if(msec < 6)
	{
		msec = 6;
	}

	if(diff == 0)
	{
		diff = 255;
	}
	diff <<= (7+15);
	rate = ((double)diff / OUTPUT_RATE) * song->control_ratio * 1000.0 / msec;

	return (int32)rate;
}

static int load_connection(ULONG cConnections, CONNECTION *artList, USHORT destination)
{
	ULONG i;
	int value = 0;

	for (i = 0; i < cConnections; ++i)
	{
		CONNECTION *conn = &artList[i];

		if(conn->usDestination == destination)
		{
			// The formula for the destination is:
			// usDestination = usDestination + usTransform(usSource * (usControl * lScale))
			// Since we are only handling source/control of NONE and identity
			// transform, this simplifies to: usDestination = usDestination + lScale
			if (conn->usSource == CONN_SRC_NONE &&
				conn->usControl == CONN_SRC_NONE &&
				conn->usTransform == CONN_TRN_NONE)
			{
				if (destination == CONN_DST_EG1_ATTACKTIME)
				{
					if (conn->lScale > 78743200)
					{
						conn->lScale -= 78743200; // maximum velocity
					}
				}

				if (destination == CONN_DST_EG1_SUSTAINLEVEL)
				{
					conn->lScale /= (1000*512);
				}

				if (destination == CONN_DST_PAN)
				{
					conn->lScale /= (65536000/128);
				}
				value += conn->lScale;
			}
		}
	}

	return value;
}

static void load_region_dls(MidiSong* song, DLS_Data *patches, Sample *sample, DLS_Instrument *ins, uint32 index)
{
	DLS_Region *rgn = &ins->regions[index];
	DLS_Wave *wave = &patches->waveList[rgn->wlnk->ulTableIndex];

	sample->low_freq = freq_table[rgn->header->RangeKey.usLow];
	sample->high_freq = freq_table[rgn->header->RangeKey.usHigh];
	sample->root_freq = freq_table[rgn->wsmp->usUnityNote];
	sample->low_vel = rgn->header->RangeVelocity.usLow;
	sample->high_vel = rgn->header->RangeVelocity.usHigh;

	sample->modes = MODES_16BIT;
	sample->sample_rate = wave->format->dwSamplesPerSec;
	sample->data_length = wave->length / 2;
	sample->data = (sample_t *)safe_malloc(wave->length);
	memcpy(sample->data, wave->data, wave->length);

	if (rgn->wsmp->cSampleLoops)
	{
		sample->modes |= (MODES_LOOPING|MODES_SUSTAIN);
		sample->loop_start = rgn->wsmp_loop->ulStart / 2;
		sample->loop_end = sample->loop_start + (rgn->wsmp_loop->ulLength / 2);
	}
	sample->volume = 1.0f;

	if (sample->modes & MODES_SUSTAIN)
	{
		int value;
		double attack, hold, decay, release;
		int sustain;
		CONNECTIONLIST *art = NULL;
		CONNECTION *artList = NULL;

		if (ins->art && ins->art->cConnections > 0 && ins->artList)
		{
			art = ins->art;
			artList = ins->artList;
		}
		else
		{
			art = rgn->art;
			artList = rgn->artList;
		}

		value = load_connection(art->cConnections, artList, CONN_DST_EG1_ATTACKTIME);
		attack = to_msec(value);
		if (attack < 0)
		{
			attack = 0;
		}
		if (attack >= 20)
		{
			attack = attack / 20;
		}

		value = load_connection(art->cConnections, artList, CONN_DST_EG1_HOLDTIME);
		hold = to_msec(value);
		if (hold >= 20)
		{
			hold = hold / 20;
		}

		value = load_connection(art->cConnections, artList, CONN_DST_EG1_DECAYTIME);
		decay = to_msec(value);
		if (decay >= 20)
		{
			decay = decay / 20;
		}

		value = load_connection(art->cConnections, artList, CONN_DST_EG1_RELEASETIME);
		release = to_msec(value);
		if (release >= 20)
		{
			release = release / 20;
		}

		value = load_connection(art->cConnections, artList, CONN_DST_EG1_SUSTAINLEVEL) * 2;
		sustain = (int)((1.0 - to_normalized_percent(value)) * 250.0);
		if (sustain < 0)
		{
			sustain = 0;
		}
		if (sustain > 255)
		{
			sustain = 250;
		}

		value = load_connection(art->cConnections, artList, CONN_DST_PAN) / 2;
		int panval = (int)((0.5 + to_normalized_percent(value)) * 127.0);
		if (panval < 0) panval = 0; else if (panval > 127) panval = 127;
		sample->panning = panval;

		//ctl->cmsg(CMSG_INFO, VERB_NORMAL,
		//	"%d, Rate=%d LV=%d HV=%d Low=%d Hi=%d Root=%d Pan=%d Attack=%f Hold=%f Sustain=%d Decay=%f Release=%f\n", index, sample->sample_rate, rgn->header->RangeVelocity.usLow, rgn->header->RangeVelocity.usHigh, sample->low_freq, sample->high_freq, sample->root_freq, sample->panning, attack, hold, sustain, decay, release);
		/*
		printf("%d, Rate=%d LV=%d HV=%d Low=%d Hi=%d Root=%d Pan=%d Attack=%f Hold=%f Sustain=%d Decay=%f Release=%f\n", index, sample->sample_rate, rgn->header->RangeVelocity.usLow, rgn->header->RangeVelocity.usHigh, sample->low_freq, sample->high_freq, sample->root_freq, sample->panning, attack, hold, sustain, decay, release);
		*/

		sample->envelope_offset[ATTACK] = to_offset(255);
		sample->envelope_rate[ATTACK] = calc_rate(song, 255, sample->sample_rate, attack);

		sample->envelope_offset[HOLD] = to_offset(250);
		sample->envelope_rate[HOLD] = calc_rate(song, 5, sample->sample_rate, hold);

		sample->envelope_offset[DECAY] = to_offset(sustain);
		sample->envelope_rate[DECAY] = calc_rate(song, 255 - sustain, sample->sample_rate, decay);

		sample->envelope_offset[RELEASE] = to_offset(0);
		sample->envelope_rate[RELEASE] = calc_rate(song, 5 + sustain, sample->sample_rate, release);

		sample->envelope_offset[RELEASEB] = to_offset(0);
		sample->envelope_rate[RELEASEB] = to_offset(1);

		sample->envelope_offset[RELEASEC] = to_offset(0);
		sample->envelope_rate[RELEASEC] = to_offset(1);

		sample->modes |= MODES_ENVELOPE;
	}

	sample->data_length <<= FRACTION_BITS;
	sample->loop_start <<= FRACTION_BITS;
	sample->loop_end <<= FRACTION_BITS;
}

Instrument* load_instrument_dls(MidiSong *song, int drum, int bank, int instrument)
{
	Instrument *inst;
	uint32 i;
	DLS_Instrument *dls_ins;

	if (!song->patches)
	{
		return(NULL);
	}

	if (!drum)
	{
		for (i = 0; i < song->patches->cInstruments; ++i)
		{
			dls_ins = &song->patches->instruments[i];

			if (!(dls_ins->header->Locale.ulBank & 0x80000000) &&
				((dls_ins->header->Locale.ulBank >> 8) & 0xFF) == bank &&
				dls_ins->header->Locale.ulInstrument == instrument)
			{
				break;
			}
		}

		if (i == song->patches->cInstruments && !bank)
		{
			for (i = 0; i < song->patches->cInstruments; ++i)
			{
				dls_ins = &song->patches->instruments[i];

				if (!(dls_ins->header->Locale.ulBank & 0x80000000) &&
					dls_ins->header->Locale.ulInstrument == instrument)
				{
					break;
				}
			}
		}

		if (i == song->patches->cInstruments)
		{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Couldn't find melodic instrument %d in bank %d\n", instrument, bank);
			return(NULL);
		}
		inst = (Instrument *)safe_malloc(sizeof(*inst));
		inst->type = INST_DLS;
		inst->samples = dls_ins->header->cRegions;
		inst->sample = (Sample *)safe_malloc(inst->samples * sizeof(*inst->sample));
		memset(inst->sample, 0, inst->samples * sizeof(*inst->sample));

		for (i = 0; i < dls_ins->header->cRegions; ++i)
		{
			load_region_dls(song, song->patches, &inst->sample[i], dls_ins, i);
		}
	}
	else
	{
		for (i = 0; i < song->patches->cInstruments; ++i)
		{
			dls_ins = &song->patches->instruments[i];

			if ((dls_ins->header->Locale.ulBank & 0x80000000) &&
				dls_ins->header->Locale.ulInstrument == bank)
			{
				break;
			}
		}

		if (i == song->patches->cInstruments && !bank)
		{
			for (i = 0; i < song->patches->cInstruments; ++i)
			{
				dls_ins = &song->patches->instruments[i];

				if ((dls_ins->header->Locale.ulBank & 0x80000000) &&
					dls_ins->header->Locale.ulInstrument == 0)
				{
					break;
				}
			}
		}

		if (i == song->patches->cInstruments)
		{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Couldn't find drum instrument %d\n", bank);
			return(NULL);
		}
		int drum_reg = -1;

		for (i = 0; i < dls_ins->header->cRegions; i++)
		{
			if (dls_ins->regions[i].header->RangeKey.usLow == instrument)
			{
				drum_reg = i;
				break;
			}
		}

		if (drum_reg == -1)
		{
			ctl->cmsg(CMSG_ERROR, VERB_NORMAL, "Couldn't find drum note %d\n", instrument);
			return(NULL);
		}

		inst = (Instrument *)safe_malloc(sizeof(*inst));
		inst->type = INST_DLS;
		inst->samples = 1;
		inst->sample = (Sample *)safe_malloc(inst->samples * sizeof(*inst->sample));
		memset(inst->sample, 0, inst->samples * sizeof(*inst->sample));
		load_region_dls(song, song->patches, &inst->sample[0], dls_ins, drum_reg);
	}

	return inst;
}

}
