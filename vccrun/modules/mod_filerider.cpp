/**************************************************************************
 *
 * Coded by Ketmar Dark, 2018
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 **************************************************************************/
#include "mod_filerider.h"

// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_CLASS(V, FileReader);
IMPLEMENT_CLASS(V, FileWriter);


// ////////////////////////////////////////////////////////////////////////// //
void VFileReader::Destroy () {
  if (fstream && !fstream->IsError()) fstream->Close();
  delete fstream;
  fstream = nullptr;
  Super::Destroy();
}


// ////////////////////////////////////////////////////////////////////////// //
// returns `none` on error
//native final static FileReader Open (string filename);
IMPLEMENT_FUNCTION(VFileReader, Open) {
  VStr fname;
  vobjGetParam(fname);
  if (fname.isEmpty()) { RET_REF(nullptr); return; }
  VStream *fs = fsysOpenFile(fname);
  if (!fs) { RET_REF(nullptr); return; }
  VFileReader *res = (VFileReader *)StaticSpawnWithReplace(StaticClass()); // disable replacements?
  res->fstream = fs;
  RET_REF(res);
}

// closes the file, but won't destroy the object
//native void close ();
IMPLEMENT_FUNCTION(VFileReader, close) {
  vobjGetParamSelf();
  if (Self && Self->fstream) {
    if (!Self->fstream->IsError()) Self->fstream->Close();
    delete Self->fstream;
    Self->fstream = nullptr;
  }
}

// returns success flag (false means "error")
// whence is one of SeekXXX; default is SeekStart
//native bool seek (int ofs, optional int whence);
IMPLEMENT_FUNCTION(VFileReader, seek) {
  int ofs;
  VOptParamInt whence(SeekStart);
  vobjGetParamSelf(ofs, whence);
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_BOOL(false); return; }
  switch (whence) {
    case SeekCur: ofs += Self->fstream->Tell(); break;
    case SeekEnd: ofs = Self->fstream->TotalSize()+ofs; break;
  }
  if (ofs < 0) ofs = 0;
  Self->fstream->Seek(ofs);
  RET_BOOL(!Self->fstream->IsError());
}

// returns 8-bit char or -1 on EOF/error
//native int getch ();
IMPLEMENT_FUNCTION(VFileReader, getch) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(-1); return; }
  if (Self->fstream->AtEnd()) { RET_INT(-1); return; }
  vuint8 b;
  Self->fstream->Serialize(&b, 1);
  if (Self->fstream->IsError()) { RET_INT(-1); return; }
  RET_INT(b);
}

// returns empty string on eof or on error
// otherwise, string ends with "\n" (if not truncated due to being very long)
//native final string readln ();
IMPLEMENT_FUNCTION(VFileReader, readln) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_STR(VStr::EmptyString); return; }
  VStr res;
  for (;;) {
    if (Self->fstream->AtEnd()) break;
    if (Self->fstream->IsError()) { res.clear(); break; }
    vuint8 b;
    Self->fstream->Serialize(&b, 1);
    if (Self->fstream->IsError()) { res.clear(); break; }
    res += (char)b;
    if (b == '\n') break;
    if (b == '\r') {
      if (Self->fstream->AtEnd()) break;
      Self->fstream->Serialize(&b, 1);
      if (Self->fstream->IsError()) { res.clear(); break; }
      if (b != '\n') {
        int pos = Self->fstream->Tell();
        if (pos <= 0 || Self->fstream->IsError()) { res.clear(); break; }
        Self->fstream->Seek(pos-1);
      } else {
        res += '\n';
      }
      break;
    }
  }
  RET_STR(res);
}

// reads `size` bytes from file
// always tries to read as max as possible
// returns empty string on EOF/error
// if `exact` is `true`, out of data means "error"
// default is "not exact"
//native string readBuf (int size, optional bool exact/*=false*/);
IMPLEMENT_FUNCTION(VFileReader, readBuf) {
  int size;
  VOptParamBool exact(false);
  vobjGetParamSelf(size, exact);
  if (!Self || !Self->fstream || Self->fstream->IsError() || size < 1) { RET_STR(VStr::EmptyString); return; }
  VStr res;
  if (!exact) {
    int left = Self->fstream->TotalSize()-Self->fstream->Tell();
    if (left < 1) { RET_STR(VStr::EmptyString); return; }
    if (size > left) size = left;
  }
  res.setLength(size);
  Self->fstream->Serialize(res.GetMutableCharPointer(0), size);
  if (Self->fstream->IsError()) {
    //fprintf(stderr, "ERRORED!\n");
    RET_STR(VStr::EmptyString);
    return;
  }
  RET_STR(res);
}

// returns name of the opened file (it may be empty)
//native string fileName { get; }
IMPLEMENT_FUNCTION(VFileReader, get_fileName) {
  vobjGetParamSelf();
  RET_STR(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->GetName() : VStr::EmptyString);
}

// is this object open (if object is error'd, it is not open)
//native bool isOpen { get; }
IMPLEMENT_FUNCTION(VFileReader, get_isOpen) {
  vobjGetParamSelf();
  RET_BOOL(Self && Self->fstream && !Self->fstream->IsError());
}

// `true` if something was error'd
// there is no reason to continue after an error (and this is UB)
//native bool error { get; }
IMPLEMENT_FUNCTION(VFileReader, get_error) {
  vobjGetParamSelf();
  RET_BOOL(Self && Self->fstream ? Self->fstream->IsError() : true);
}

// get file size
//native int size { get; }
IMPLEMENT_FUNCTION(VFileReader, get_size) {
  vobjGetParamSelf();
  RET_INT(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->TotalSize() : 0);
}

// get current position
//native int position { get; }
IMPLEMENT_FUNCTION(VFileReader, get_position) {
  vobjGetParamSelf();
  RET_INT(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->Tell() : 0);
}

// set current position
//native void position { set; }
IMPLEMENT_FUNCTION(VFileReader, set_position) {
  int ofs;
  vobjGetParamSelf(ofs);
  if (Self && Self->fstream && !Self->fstream->IsError()) {
    if (ofs < 0) ofs = 0;
    Self->fstream->Seek(ofs);
  }
}


// convenient functions
IMPLEMENT_FUNCTION(VFileReader, readU8) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b;
  Self->fstream->Serialize(&b, 1);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b);
}

IMPLEMENT_FUNCTION(VFileReader, readU16) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8));
}

IMPLEMENT_FUNCTION(VFileReader, readU32) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24));
}

IMPLEMENT_FUNCTION(VFileReader, readI8) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vint8 b;
  Self->fstream->Serialize(&b, 1);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)b);
}

IMPLEMENT_FUNCTION(VFileReader, readI16) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)(vint16)(b[0]|(b[1]<<8)));
}

IMPLEMENT_FUNCTION(VFileReader, readI32) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24));
}

IMPLEMENT_FUNCTION(VFileReader, readU16BE) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[1]|(b[0]<<8));
}

IMPLEMENT_FUNCTION(VFileReader, readU32BE) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[3]|(b[2]<<8)|(b[1]<<16)|(b[0]<<24));
}

IMPLEMENT_FUNCTION(VFileReader, readI16BE) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[2];
  Self->fstream->Serialize(&b[0], 2);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT((vint32)(vint16)(b[1]|(b[0]<<8)));
}

IMPLEMENT_FUNCTION(VFileReader, readI32BE) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_INT(0); return; }
  vuint8 b[4];
  Self->fstream->Serialize(&b[0], 4);
  if (Self->fstream->IsError()) { RET_INT(0); return; }
  RET_INT(b[3]|(b[2]<<8)|(b[1]<<16)|(b[0]<<24));
}

IMPLEMENT_FUNCTION(VFileReader, readF32) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_FLOAT(0.0f); return; }
  float f;
  #ifdef VAVOOM_LITTLE_ENDIAN
  Self->fstream->Serialize(&f, sizeof(f));
  if (Self->fstream->IsError()) { RET_FLOAT(0.0f); return; }
  #else
  vuint8 *b = (vuint8 *)&f;
  Self->fstream->Serialize(&b[3], 1);
  Self->fstream->Serialize(&b[2], 1);
  Self->fstream->Serialize(&b[1], 1);
  Self->fstream->Serialize(&b[0], 1);
  if (Self->fstream->IsError()) { RET_FLOAT(0.0f); return; }
  #endif
  RET_FLOAT(f);
}


IMPLEMENT_FUNCTION(VFileReader, readF32BE) {
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_FLOAT(0.0f); return; }
  float f;
  #ifdef VAVOOM_LITTLE_ENDIAN
  vuint8 *b = (vuint8 *)&f;
  Self->fstream->Serialize(&b[3], 1);
  Self->fstream->Serialize(&b[2], 1);
  Self->fstream->Serialize(&b[1], 1);
  Self->fstream->Serialize(&b[0], 1);
  if (Self->fstream->IsError()) { RET_FLOAT(0.0f); return; }
  #else
  Self->fstream->Serialize(&f, sizeof(f));
  if (Self->fstream->IsError()) { RET_FLOAT(0.0f); return; }
  #endif
  RET_FLOAT(f);
}


// ////////////////////////////////////////////////////////////////////////// //
void VFileWriter::Destroy () {
  if (fstream && !fstream->IsError()) fstream->Close();
  delete fstream;
  fstream = nullptr;
  Super::Destroy();
}


// ////////////////////////////////////////////////////////////////////////// //
IMPLEMENT_FUNCTION(VFileWriter, GetBaseDir) {
  RET_STR(fsysBaseDir);
}

IMPLEMENT_FUNCTION(VFileWriter, CreateDir) {
  VStr dirname;
  vobjGetParam(dirname);
  if (dirname.isEmpty()) { RET_BOOL(false); return; }
  RET_BOOL(fsysCreateDirSafe(dirname));
}

// returns `none` on error
//native final static FileReader Open (string filename, optional bool notruncate/*=false*/);
IMPLEMENT_FUNCTION(VFileWriter, Open) {
  VStr fname;
  VOptParamBool notrunc(false);
  vobjGetParam(fname, notrunc);
  if (fname.isEmpty()) { RET_REF(nullptr); return; }
  VStream *fs = fsysCreateFileSafe(fname, notrunc.value);
  if (!fs) { RET_REF(nullptr); return; }
  VFileWriter *res = (VFileWriter *)StaticSpawnWithReplace(StaticClass()); // disable replacements?
  res->fstream = fs;
  RET_REF(res);
}


// closes the file, but won't destroy the object
//native void close ();
IMPLEMENT_FUNCTION(VFileWriter, close) {
  vobjGetParamSelf();
  if (Self && Self->fstream) {
    if (!Self->fstream->IsError()) Self->fstream->Close();
    delete Self->fstream;
    Self->fstream = nullptr;
  }
}

// returns success flag (false means "error")
// whence is one of SeekXXX; default is SeekStart
//native bool seek (int ofs, optional int whence);
IMPLEMENT_FUNCTION(VFileWriter, seek) {
  int ofs;
  VOptParamInt whence(SeekStart);
  vobjGetParamSelf(ofs, whence);
  if (!Self || !Self->fstream || Self->fstream->IsError()) { RET_BOOL(false); return; }
  switch (whence) {
    case SeekCur: ofs += Self->fstream->Tell(); break;
    case SeekEnd: ofs = Self->fstream->TotalSize()+ofs; break;
  }
  if (ofs < 0) ofs = 0;
  Self->fstream->Seek(ofs);
  RET_BOOL(!Self->fstream->IsError());
}

//native void putch (int ch);
IMPLEMENT_FUNCTION(VFileWriter, putch) {
  int ch;
  vobjGetParamSelf(ch);
  if (!Self || !Self->fstream || Self->fstream->IsError()) { return; }
  vuint8 b = ch&0xff;
  Self->fstream->Serialize(&b, 1);
}

//native string writeBuf (string s);
IMPLEMENT_FUNCTION(VFileWriter, writeBuf) {
  VStr buf;
  vobjGetParamSelf(buf);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  if (!buf.isEmpty()) Self->fstream->Serialize(*buf, buf.length());
}

//native final void printf (string fmt, ...) [printf,1];
IMPLEMENT_FUNCTION(VFileWriter, printf) {
  VStr s = VObject::PF_FormatString();
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  if (s.isEmpty()) return;
  Self->fstream->Serialize(*s, s.length());
}

//native final void printfln (string fmt, ...) [printf,1];
IMPLEMENT_FUNCTION(VFileWriter, printfln) {
  VStr s = VObject::PF_FormatString();
  vobjGetParamSelf();
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  #ifdef WIN32
  s += "\r\n";
  #else
  s += "\n";
  #endif
  Self->fstream->Serialize(*s, s.length());
}

// returns name of the opened file (it may be empty)
//native string fileName { get; }
IMPLEMENT_FUNCTION(VFileWriter, get_fileName) {
  vobjGetParamSelf();
  RET_STR(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->GetName() : VStr::EmptyString);
}

// is this object open (if object is error'd, it is not open)
//native bool isOpen { get; }
IMPLEMENT_FUNCTION(VFileWriter, get_isOpen) {
  vobjGetParamSelf();
  RET_BOOL(Self && Self->fstream && !Self->fstream->IsError());
}

// `true` if something was error'd
// there is no reason to continue after an error (and this is UB)
//native bool error { get; }
IMPLEMENT_FUNCTION(VFileWriter, get_error) {
  vobjGetParamSelf();
  RET_BOOL(Self && Self->fstream ? Self->fstream->IsError() : true);
}

// get file size
//native int size { get; }
IMPLEMENT_FUNCTION(VFileWriter, get_size) {
  vobjGetParamSelf();
  RET_INT(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->TotalSize() : 0);
}

// get current position
//native int position { get; }
IMPLEMENT_FUNCTION(VFileWriter, get_position) {
  vobjGetParamSelf();
  RET_INT(Self && Self->fstream && !Self->fstream->IsError() ? Self->fstream->Tell() : 0);
}

// set current position
//native void position { set; }
IMPLEMENT_FUNCTION(VFileWriter, set_position) {
  int ofs;
  vobjGetParamSelf(ofs);
  if (Self && Self->fstream && !Self->fstream->IsError()) {
    if (ofs < 0) ofs = 0;
    Self->fstream->Seek(ofs);
  }
}


// convenient functions
IMPLEMENT_FUNCTION(VFileWriter, writeU8) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b = v&0xff;
  Self->fstream->Serialize(&b, 1);
}

IMPLEMENT_FUNCTION(VFileWriter, writeU16) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b[2];
  b[0] = v&0xff;
  b[1] = (v>>8)&0xff;
  Self->fstream->Serialize(&b[0], 2);
}

IMPLEMENT_FUNCTION(VFileWriter, writeU32) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b[4];
  b[0] = v&0xff;
  b[1] = (v>>8)&0xff;
  b[2] = (v>>16)&0xff;
  b[3] = (v>>24)&0xff;
  Self->fstream->Serialize(&b[0], 4);
}

IMPLEMENT_FUNCTION(VFileWriter, writeI8) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vint8 b = v&0xff;
  Self->fstream->Serialize(&b, 1);
}

IMPLEMENT_FUNCTION(VFileWriter, writeI16) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b[2];
  b[0] = v&0xff;
  b[1] = (v>>8)&0xff;
  Self->fstream->Serialize(&b[0], 2);
}

IMPLEMENT_FUNCTION(VFileWriter, writeI32) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b[4];
  b[0] = v&0xff;
  b[1] = (v>>8)&0xff;
  b[2] = (v>>16)&0xff;
  b[3] = (v>>24)&0xff;
  Self->fstream->Serialize(&b[0], 4);
}

IMPLEMENT_FUNCTION(VFileWriter, writeU16BE) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b[2];
  b[1] = v&0xff;
  b[0] = (v>>8)&0xff;
  Self->fstream->Serialize(&b[0], 2);
}

IMPLEMENT_FUNCTION(VFileWriter, writeU32BE) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b[4];
  b[3] = v&0xff;
  b[2] = (v>>8)&0xff;
  b[1] = (v>>16)&0xff;
  b[0] = (v>>24)&0xff;
  Self->fstream->Serialize(&b[0], 4);
}

IMPLEMENT_FUNCTION(VFileWriter, writeI16BE) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b[2];
  b[1] = v&0xff;
  b[0] = (v>>8)&0xff;
  Self->fstream->Serialize(&b[0], 2);
}

IMPLEMENT_FUNCTION(VFileWriter, writeI32BE) {
  int v;
  vobjGetParamSelf(v);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  vuint8 b[4];
  b[3] = v&0xff;
  b[2] = (v>>8)&0xff;
  b[1] = (v>>16)&0xff;
  b[0] = (v>>24)&0xff;
  Self->fstream->Serialize(&b[0], 4);
}

IMPLEMENT_FUNCTION(VFileWriter, writeF32) {
  float f;
  vobjGetParamSelf(f);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  #ifdef VAVOOM_LITTLE_ENDIAN
  Self->fstream->Serialize(&f, sizeof(f));
  #else
  const vuint8 *b = (const vuint8 *)&f;
  Self->fstream->Serialize(&b[3], 1);
  Self->fstream->Serialize(&b[2], 1);
  Self->fstream->Serialize(&b[1], 1);
  Self->fstream->Serialize(&b[0], 1);
  #endif
}


IMPLEMENT_FUNCTION(VFileWriter, writeF32BE) {
  float f;
  vobjGetParamSelf(f);
  if (!Self || !Self->fstream || Self->fstream->IsError()) return;
  #ifdef VAVOOM_LITTLE_ENDIAN
  const vuint8 *b = (const vuint8 *)&f;
  Self->fstream->Serialize(&b[3], 1);
  Self->fstream->Serialize(&b[2], 1);
  Self->fstream->Serialize(&b[1], 1);
  Self->fstream->Serialize(&b[0], 1);
  #else
  Self->fstream->Serialize(&f, sizeof(f));
  #endif
}
