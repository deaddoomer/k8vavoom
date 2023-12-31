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
#ifndef VAVOOM_CORELIB_VWADSTREAMS_HEADER
#define VAVOOM_CORELIB_VWADSTREAMS_HEADER


class VVWadStreamReader;

class VVWadArchive {
  friend class VVWadStreamReader;
private:
  VStr archname;  // archive name
  vwad_handle *vw_handle;
  vwad_iostream vw_strm;
  VStream *srcStream;
  bool srcStreamOwn;
  VVWadStreamReader *openlist;

public:
  VV_DISABLE_COPY(VVWadArchive)

  // open from disk
  //VVWadArchive (VStr aArchName);
  // open from stream; will close owned stream even on open error
  // stream will be seeked to 0
  VVWadArchive (VStr aArchName, VStream *strm, bool owned);

  virtual ~VVWadArchive ();

  void Close ();

  inline bool IsOpen () const noexcept { return !!vw_handle; }
  inline VStr GetName () const noexcept { return archname; }

  bool FileExists (VStr name);

  inline int GetFilesCount () const noexcept {
    return (vw_handle ? vwad_get_archive_file_count(vw_handle) : 0);
  }

  inline VStr GetFileName (int fidx) const noexcept {
    if (vw_handle) return VStr(vwad_get_file_name(vw_handle, fidx));
    return VStr::EmptyString;
  }

  inline VStr GetTitle () const noexcept {
    if (vw_handle) return VStr(vwad_get_archive_title(vw_handle));
    return VStr::EmptyString;
  }

  inline VStr GetAuthor () const noexcept {
    if (vw_handle) return VStr(vwad_get_archive_author(vw_handle));
    return VStr::EmptyString;
  }

  inline VStr GetComment () const noexcept {
    if (vw_handle) {
      char *cmt = new char[VWAD_MAX_COMMENT_SIZE + 1];
      vwad_get_archive_comment(vw_handle, cmt, VWAD_MAX_COMMENT_SIZE + 1);
      VStr res = VStr(cmt);
      delete cmt;
      return res;
    }
    return VStr::EmptyString;
  }

  // return `nullptr` on error
  VStream *OpenFile (VStr name);
};


class VVWadStreamReader : public VStream {
  friend class VVWadArchive;
private:
  VVWadArchive *arc;
  vwad_handle *vw_handle;
  vwad_iostream *vw_strm;
  vwad_fd fd;
  VVWadStreamReader *next;

private:
  VVWadStreamReader (VVWadArchive *aarc, vwad_fd afd);

  void DoClose ();

public:
  VV_DISABLE_COPY(VVWadStreamReader)

  virtual ~VVWadStreamReader () override;

  virtual VStr GetName () const override;
  virtual void SetError () override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;

  virtual VStr GetGroupName () const;
};



// ////////////////////////////////////////////////////////////////////////// //
class VVWadStreamWriter;

// currently cannot be used to create signed archives
class VVWadNewArchive {
  friend class VVWadStreamWriter;
private:
  VStr archname;  // archive name
  vwadwr_archive *vw_handle;
  vwadwr_iostream vw_strm;
  VStream *destStream;
  bool destStreamOwn;
  VVWadStreamWriter *openlist;
  bool error;

private:
  // this also closes the archive handle, and possibly destroys destination stream
  void SetError ();

  // return `nullptr` on error
  // otherwise, return unseekable stream suitable for writing
  // close the stream to finish file
  VStream *CreateFile (VStr name, int level/* VADWR_COMP_xxx */,
                       VStr groupname, vwadwr_ftime ftime, bool buffit);

public:
  VV_DISABLE_COPY(VVWadNewArchive)

  // create to stream; will close owned stream even on open error
  // stream will be seeked to 0
  // can set error flag; on error, destroys owned stream
  VVWadNewArchive (VStr aArchName, VStr author, VStr title, VStream *strm, bool owned);

  virtual ~VVWadNewArchive ();

  inline bool IsError () const noexcept { return error; }

  // finish creating, write final dir, and so on
  // return success flag
  bool Close ();

  inline bool IsOpen () const noexcept { return !!vw_handle; }
  inline VStr GetName () const noexcept { return archname; }

  // return `nullptr` on error
  // otherwise, return unseekable stream suitable for writing
  // close the stream to finish file
  inline VStream *CreateFileBuffered (VStr name, int level/* VADWR_COMP_xxx */,
                                      VStr groupname=VStr::EmptyString,
                                      vwadwr_ftime ftime=0)
  {
    return CreateFile(name, level, groupname, ftime, true);
  }

  // return `nullptr` on error
  // otherwise, return unseekable stream suitable for writing
  // close the stream to finish file
  inline VStream *CreateFileDirect (VStr name, int level/* VADWR_COMP_xxx */,
                                    VStr groupname=VStr::EmptyString,
                                    vwadwr_ftime ftime=0)
  {
    return CreateFile(name, level, groupname, ftime, false);
  }
};


class VVWadStreamWriter : public VStream {
  friend class VVWadNewArchive;
private:
  VVWadNewArchive *arc;
  VMemoryStream *stbuf; // can be `NULL` for direct writes
  vwadwr_fhandle fd;
  VStr fname;
  int currpos; // for `Tell()`
  int seekpos;
  VVWadStreamWriter *next;

private:
  VVWadStreamWriter (VVWadNewArchive *aarc, VStr afname, vwadwr_fhandle afd, bool buffit);

  void DoClose ();

public:
  VV_DISABLE_COPY(VVWadStreamWriter)

  virtual ~VVWadStreamWriter () override;

  virtual VStr GetName () const override;
  virtual void SetError () override;
  virtual void Serialise (void *, int) override;
  virtual void Seek (int) override;
  virtual int Tell () override;
  virtual int TotalSize () override;
  virtual bool AtEnd () override;
  virtual bool Close () override;
};


#endif
