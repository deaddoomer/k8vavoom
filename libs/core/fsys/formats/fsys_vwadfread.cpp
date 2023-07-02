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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
// directly included from "fsys_vwad.cpp"


// ////////////////////////////////////////////////////////////////////////// //
class VVWadFileReader : public VStream {
private:
  mythread_mutex *rdlock;
  VStr fname;
  const VPakFileInfo &Info; // info about the file we are reading
  vwad_handle *vw_handle;
  vwad_iostream *vw_strm;
  vwad_fd fd;
  int total_size;

public:
  VV_DISABLE_COPY(VVWadFileReader)

  VVWadFileReader (VStr afname, const VPakFileInfo &, vwad_handle *, vwad_iostream *, mythread_mutex *ardlock);
  virtual ~VVWadFileReader () override;

  virtual VStr GetName () const override;
  virtual void SetError () override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


//==========================================================================
//
//  VVWadFileReader::VVWadFileReader
//
//==========================================================================
VVWadFileReader::VVWadFileReader (VStr afname, const VPakFileInfo &aInfo,
                                  vwad_handle *a_vw_handle, vwad_iostream *a_vw_strm,
                                  mythread_mutex *ardlock)
  : rdlock(ardlock)
  , fname(afname)
  , Info(aInfo)
  , vw_handle(a_vw_handle)
  , vw_strm(a_vw_strm)
{
  // `rdlock` is not locked here

  if (!rdlock) Sys_Error("VVWadFileReader::VVWadFileReader: empty lock!");

  {
    MyThreadLocker locker(rdlock);
    fd = vwad_fopen(vw_handle, aInfo.pakdataofs);
  }

  if (fd < 0) {
    SetError();
    GLog.Logf(NAME_Error, "Cannot open file '%s'", *afname);
    return;
  }

  //total_size = vwad_get_file_size(vw_handle, vwad_fdfidx(vw_handle, fd));
  total_size = aInfo.filesize;
  bLoading = true;

  #if 0
  GLog.Logf(NAME_Debug, "VWAD: opened file '%s'", *fname);
  #endif
}


//==========================================================================
//
//  VVWadFileReader::~VVWadFileReader
//
//==========================================================================
VVWadFileReader::~VVWadFileReader() {
  vwad_fclose(vw_handle, fd); fd = -1;
  Close();
}


//==========================================================================
//
//  VVWadFileReader::GetName
//
//==========================================================================
VStr VVWadFileReader::GetName () const {
  return fname.cloneUniqueMT();
}


//==========================================================================
//
//  VVWadFileReader::SetError
//
//==========================================================================
void VVWadFileReader::SetError () {
  vwad_fclose(vw_handle, fd); fd = -1;
  VStream::SetError();
}


//==========================================================================
//
//  VVWadFileReader::Close
//
//==========================================================================
bool VVWadFileReader::Close () {
  #if 0
  GLog.Logf(NAME_Debug, "VWAD: closed file '%s'", *fname);
  #endif
  vwad_fclose(vw_handle, fd); fd = -1;
  return !bError;
}


//==========================================================================
//
//  VVWadFileReader::Serialise
//
//==========================================================================
void VVWadFileReader::Serialise (void *V, int length) {
  if (bError) return;
  if (length == 0) return;
  if (length < 0) { SetError(); return; }

  if (!V) {
    SetError();
    GLog.Logf(NAME_Error, "Cannot read into nullptr buffer for file '%s'", *fname);
    return;
  }

  if (fd < 0) {
    SetError();
    GLog.Logf(NAME_Error, "Failed to read from VWAD for file '%s'", *fname);
    return;
  }

  MyThreadLocker locker(rdlock);
  #if 0
  GLog.Logf(NAME_Debug, "VWAD: reading file '%s' (pos=%d; len=%d)", *fname,
            vwad_tell(vw_handle, fd), length);
  #endif
  while (length != 0) {
    int rd = vwad_read(vw_handle, fd, V, length);
    if (rd <= 0) {
      SetError();
      GLog.Logf(NAME_Error, "Failed to read from VWAD for file '%s'", *fname);
      return;
    }
    length -= rd;
    V = ((char *)V) + (unsigned)rd;
  }
  #if 0
  GLog.Logf(NAME_Debug, "VWAD: done reading file '%s' (newpos=%d)", *fname,
            vwad_tell(vw_handle, fd));
  #endif
}


//==========================================================================
//
//  VVWadFileReader::Seek
//
//==========================================================================
void VVWadFileReader::Seek (int InPos) {
  if (bError) return;
  MyThreadLocker locker(rdlock);
  if (vwad_seek(vw_handle, fd, InPos) != 0) {
    SetError();
    GLog.Logf(NAME_Error, "Failed to seek in VWAD for file '%s'", *fname);
  }
}


//==========================================================================
//
//  VVWadFileReader::Tell
//
//==========================================================================
int VVWadFileReader::Tell () {
  MyThreadLocker locker(rdlock);
  return vwad_tell(vw_handle, fd);
}


//==========================================================================
//
//  VVWadFileReader::TotalSize
//
//==========================================================================
int VVWadFileReader::TotalSize () {
  //MyThreadLocker locker(rdlock);
  return total_size;
}


//==========================================================================
//
//  VVWadFileReader::AtEnd
//
//==========================================================================
bool VVWadFileReader::AtEnd () {
  MyThreadLocker locker(rdlock);
  return (vwad_tell(vw_handle, fd) >= total_size);
}
