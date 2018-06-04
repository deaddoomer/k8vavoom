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

#include "gamedefs.h"
#include "r_tex.h"

#ifdef CLIENT
extern "C"
{
#ifdef VAVOOM_USE_LIBJPG
# include <jpeglib.h>
#else
# include "../../libs/jpeg/jpeglib.h"
#endif
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
#ifdef CLIENT
struct VJpegClientData {
  VStream *Strm;
  JOCTET Buffer[4096];
};
#endif

static VCvarI jpeg_quality("jpeg_quality", "80", "Jpeg screenshot quality.", CVAR_Archive);


//==========================================================================
//
//  VJpegTexture::Create
//
//==========================================================================
VTexture *VJpegTexture::Create (VStream &Strm, int LumpNum) {
  guard(VJpegTexture::Create);
  if (Strm.TotalSize() < 11) return nullptr; // file is too small

  vuint8 Buf[8];

  // check header
  Strm.Seek(0);
  Strm.Serialise(Buf, 2);
  if (Buf[0] != 0xff || Buf[1] != 0xd8) return nullptr;

  // find SOFn marker to get the image dimensions
  int Len;
  do {
    if (Strm.TotalSize()-Strm.Tell() < 4) return nullptr;

    // read marker
    Strm.Serialise(Buf, 2);
    if (Buf[0] != 0xff) return nullptr; // missing identifier of a marker

    // skip padded 0xff-s
    while (Buf[1] == 0xff) {
      if (Strm.TotalSize()-Strm.Tell() < 3) return nullptr;
      Strm.Serialise(Buf+1, 1);
    }

    // read length
    Strm.Serialise(Buf+2, 2);
    Len = Buf[3]+(Buf[2]<<8);
    if (Len < 2) return nullptr;
    if (Strm.Tell()+Len-2 >= Strm.TotalSize()) return nullptr;

    // if it's not a SOFn marker, then skip it
    if (Buf[1] != 0xc0 && Buf[1] != 0xc1 && Buf[1] != 0xc2) Strm.Seek(Strm.Tell()+Len-2);
  } while (Buf[1] != 0xc0 && Buf[1] != 0xc1 && Buf[1] != 0xc2);

  if (Len < 7) return nullptr;
  Strm.Serialise(Buf, 5);
  vint32 Width = Buf[4]+(Buf[3]<<8);
  vint32 Height = Buf[2]+(Buf[1]<<8);
  return new VJpegTexture(LumpNum, Width, Height);
  unguard;
}


//==========================================================================
//
//  VJpegTexture::VJpegTexture
//
//==========================================================================
VJpegTexture::VJpegTexture (int ALumpNum, int AWidth, int AHeight) : Pixels(nullptr)
{
  SourceLump = ALumpNum;
  Name = W_LumpName(SourceLump);
  Width = AWidth;
  Height = AHeight;
}


//==========================================================================
//
//  VJpegTexture::~VJpegTexture
//
//==========================================================================
VJpegTexture::~VJpegTexture () {
  //guard(VJpegTexture::~VJpegTexture);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  //unguard;
}


#ifdef CLIENT
//==========================================================================
//
//  my_init_source
//
//==========================================================================
static void my_init_source (j_decompress_ptr cinfo) {
  guard(my_init_source);
  cinfo->src->next_input_byte = nullptr;
  cinfo->src->bytes_in_buffer = 0;
  unguard;
}


//==========================================================================
//
//  my_fill_input_buffer
//
//==========================================================================
static boolean my_fill_input_buffer (j_decompress_ptr cinfo) {
  guard(my_fill_input_buffer);
  VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
  if (cdata->Strm->AtEnd()) {
    // insert a fake EOI marker
    cdata->Buffer[0] = 0xff;
    cdata->Buffer[1] = JPEG_EOI;
    cinfo->src->next_input_byte = cdata->Buffer;
    cinfo->src->bytes_in_buffer = 2;
    return FALSE;
  }

  int Count = 4096;
  if (Count > cdata->Strm->TotalSize()-cdata->Strm->Tell()) {
    Count = cdata->Strm->TotalSize()-cdata->Strm->Tell();
  }
  cdata->Strm->Serialise(cdata->Buffer, Count);
  cinfo->src->next_input_byte = cdata->Buffer;
  cinfo->src->bytes_in_buffer = Count;
  return TRUE;
  unguard;
}


//==========================================================================
//
//  my_skip_input_data
//
//==========================================================================
static void my_skip_input_data (j_decompress_ptr cinfo, long num_bytes) {
  guard(my_skip_input_data);
  if (num_bytes <= 0) return;
  if ((long)cinfo->src->bytes_in_buffer > num_bytes) {
    cinfo->src->bytes_in_buffer -= num_bytes;
    cinfo->src->next_input_byte += num_bytes;
  } else {
    VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
    int Pos = cdata->Strm->Tell()+num_bytes-cinfo->src->bytes_in_buffer;
    if (Pos > cdata->Strm->TotalSize()) Pos = cdata->Strm->TotalSize();
    cdata->Strm->Seek(Pos);
    cinfo->src->bytes_in_buffer = 0;
  }
  unguard;
}


//==========================================================================
//
//  my_term_source
//
//==========================================================================
static void my_term_source (j_decompress_ptr) {
}


//==========================================================================
//
//  my_error_exit
//
//==========================================================================
static void my_error_exit (j_common_ptr cinfo) {
  guard(my_error_exit);
  (*cinfo->err->output_message)(cinfo);
  throw -1;
  unguard;
}


//==========================================================================
//
//  my_output_message
//
//==========================================================================
static void my_output_message (j_common_ptr cinfo) {
  guard(my_output_message);
  char Msg[JMSG_LENGTH_MAX];
  cinfo->err->format_message(cinfo, Msg);
  GCon->Log(Msg);
  unguard;
}
#endif


//==========================================================================
//
//  VJpegTexture::GetPixels
//
//==========================================================================
vuint8 *VJpegTexture::GetPixels () {
  guard(VJpegTexture::GetPixels);
#ifdef CLIENT
  // if we already have loaded pixels, return them
  if (Pixels) return Pixels;

  Format = TEXFMT_RGBA;
  Pixels = new vuint8[Width*Height*4];
  memset(Pixels, 0, Width*Height*4);

  jpeg_decompress_struct cinfo;
  jpeg_source_mgr smgr;
  jpeg_error_mgr jerr;
  VJpegClientData cdata;

  // open stream
  VStream *Strm = W_CreateLumpReaderNum(SourceLump);
  check(Strm);

  try {
    // set up the JPEG error routines
    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = my_error_exit;
    jerr.output_message = my_output_message;

    // set client data pointer
    cinfo.client_data = &cdata;
    cdata.Strm = Strm;

    // initialise the JPEG decompression object
    jpeg_create_decompress(&cinfo);

    // specify data source
    smgr.init_source = my_init_source;
    smgr.fill_input_buffer = my_fill_input_buffer;
    smgr.skip_input_data = my_skip_input_data;
    smgr.resync_to_restart = jpeg_resync_to_restart;
    smgr.term_source = my_term_source;
    cinfo.src = &smgr;

    // read file parameters with jpeg_read_header()
    jpeg_read_header(&cinfo, TRUE);

    if (!((cinfo.out_color_space == JCS_RGB && cinfo.num_components == 3) ||
        (cinfo.out_color_space == JCS_CMYK && cinfo.num_components == 4) ||
        (cinfo.out_color_space == JCS_GRAYSCALE && cinfo.num_components == 1)))
    {
      GCon->Log("Unsupported JPEG file format");
      throw -1;
    }

    // start decompressor
    jpeg_start_decompress(&cinfo);

    // JSAMPLEs per row in output buffer
    int row_stride = cinfo.output_width*cinfo.output_components;
    // make a one-row-high sample array that will go away when done with image
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

    // read image
    vuint8 *pDst = Pixels;
    while (cinfo.output_scanline < cinfo.output_height) {
      jpeg_read_scanlines(&cinfo, buffer, 1);
      JOCTET *pSrc = buffer[0];
      switch (cinfo.out_color_space) {
        case JCS_RGB:
          for (int x = 0; x < Width; ++x) {
            pDst[0] = pSrc[0];
            pDst[1] = pSrc[1];
            pDst[2] = pSrc[2];
            pDst[3] = 0xff;
            pSrc += 3;
            pDst += 4;
          }
          break;
        case JCS_GRAYSCALE:
          for (int x = 0; x < Width; ++x) {
            pDst[0] = pSrc[0];
            pDst[1] = pSrc[0];
            pDst[2] = pSrc[0];
            pDst[3] = 0xff;
            pSrc++;
            pDst += 4;
          }
          break;
        case JCS_CMYK:
          for (int x = 0; x < Width; ++x) {
            pDst[0] = (255-pSrc[0])*(255-pSrc[3])/255;
            pDst[1] = (255-pSrc[1])*(255-pSrc[3])/255;
            pDst[2] = (255-pSrc[2])*(255-pSrc[3])/255;
            pDst[3] = 0xff;
            pSrc += 4;
            pDst += 4;
          }
          break;
        default:
          break;
      }
    }

    // finish decompression
    jpeg_finish_decompress(&cinfo);
  } catch (int) {
  }

  // release JPEG decompression object
  jpeg_destroy_decompress(&cinfo);

  // free memory
  delete Strm;
  return Pixels;

#else
  Sys_Error("ReadPixels on dedicated server");
  return nullptr;
#endif

  unguard;
}


//==========================================================================
//
//  VJpegTexture::Unload
//
//==========================================================================
void VJpegTexture::Unload () {
  guard(VJpegTexture::Unload);
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
  unguard;
}


#ifdef CLIENT

#ifdef VAVOOM_USE_LIBJPG
//==========================================================================
//
//  my_init_destination
//
//==========================================================================
static void my_init_destination (j_compress_ptr cinfo) {
  guard(my_init_destination);
  VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
  cinfo->dest->next_output_byte = cdata->Buffer;
  cinfo->dest->free_in_buffer = 4096;
  unguard;
}


//==========================================================================
//
//  my_empty_output_buffer
//
//==========================================================================
static boolean my_empty_output_buffer (j_compress_ptr cinfo) {
  guard(my_empty_output_buffer);
  VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
  cdata->Strm->Serialise(cdata->Buffer, 4096);
  cinfo->dest->next_output_byte = cdata->Buffer;
  cinfo->dest->free_in_buffer = 4096;
  return TRUE;
  unguard;
}


//==========================================================================
//
//  my_term_destination
//
//==========================================================================
static void my_term_destination (j_compress_ptr cinfo) {
  guard(my_term_destination);
  VJpegClientData *cdata = (VJpegClientData*)cinfo->client_data;
  cdata->Strm->Serialise(cdata->Buffer, 4096-cinfo->dest->free_in_buffer);
  unguard;
}


//==========================================================================
//
//  WriteJPG
//
//==========================================================================
void WriteJPG (const VStr &FileName, const void *Data, int Width, int Height, int Bpp, bool Bot2top) {
  guard(WriteJPG);
  VStream *Strm = FL_OpenFileWrite(FileName, true);
  if (!Strm) {
    GCon->Log("Couldn't write jpg");
    return;
  }

  jpeg_compress_struct cinfo;
  jpeg_destination_mgr dmgr;
  jpeg_error_mgr jerr;
  VJpegClientData cdata;

  // set up the JPEG error routines
  cinfo.err = jpeg_std_error(&jerr);
  jerr.error_exit = my_error_exit;
  jerr.output_message = my_output_message;

  // set client data pointer
  cinfo.client_data = &cdata;
  cdata.Strm = Strm;

  try {
    // initialise the JPEG decompression object
    jpeg_create_compress(&cinfo);

    // specify data source
    dmgr.init_destination = my_init_destination;
    dmgr.empty_output_buffer = my_empty_output_buffer;
    dmgr.term_destination = my_term_destination;
    cinfo.dest = &dmgr;

    // specify image data
    cinfo.image_width = Width;
    cinfo.image_height = Height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    // set up compression
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, jpeg_quality, TRUE);

    // perform compression
    jpeg_start_compress(&cinfo, TRUE);
    TArray<JSAMPROW> RowPointers;
    TArray<vuint8> TmpData;
    RowPointers.SetNum(Height);
    if (Bpp == 8) {
      // convert image to 24 bit
      TmpData.SetNum(Width*Height*3);
      for (int i = 0; i < Width*Height; ++i) {
        int Col = ((byte*)Data)[i];
        TmpData[i*3] = r_palette[Col].r;
        TmpData[i*3+1] = r_palette[Col].g;
        TmpData[i*3+2] = r_palette[Col].b;
      }
      for (int i = 0; i < Height; ++i) {
        RowPointers[i] = &TmpData[(Bot2top ? Height-i-1 : i)*Width*3];
      }
    } else {
      for (int i = 0; i < Height; ++i) {
        RowPointers[i] = ((byte*)Data)+(Bot2top ? Height-i-1 : i)*Width*3;
      }
    }
    jpeg_write_scanlines(&cinfo, RowPointers.Ptr(), Height);
    jpeg_finish_compress(&cinfo);
  } catch (int) {
  }

  // finish with the image
  jpeg_destroy_compress(&cinfo);
  Strm->Close();
  delete Strm;

  unguard;
}
#endif


#endif
