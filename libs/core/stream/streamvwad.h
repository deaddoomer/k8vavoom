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
  VVWadArchive (VStr aArchName);
  // open from stream; will close owned stream even on open error
  VVWadArchive (VStr aArchName, VStream *strm, bool owned);

  virtual ~VVWadArchive ();

  void Close ();

  inline bool IsOpen () const noexcept { return !!vw_handle; }
  inline VStr GetName () const noexcept { return archname; }

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
  //VVWadStreamReader (VStr aArchName, vwad_handle *vw_handle, vwad_iostream *vw_strm, vwad_fd afd);
  // if there is no file with this name, the stream set in error state
  //VVWadStreamReader (VStr aArchName, vwad_handle *vw_handle, vwad_iostream *vw_strm, VStr name);

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


// `dstrm` should be alive until archive is closed!
// `dstrm` will be seeked to 0
vwadwr_dir *vwadwr_create_archive_stream (VStream *dstrm, VStr author, VStr title);
// will set `vwad` to `nullptr`
bool vwadwr_finish_archive_stream (vwadwr_dir *&vwad);
// use this to abort writing; will not destroy stream
void vwadwr_destroy_archive_stream (vwadwr_dir *&vwad);
VStream *vwadwr_arc_get_archive_stream (vwadwr_dir *vwad);

// `strm` will be seeked to 0
vwadwr_result vwadwr_write_vstream (vwadwr_dir *wad, VStream *strm,
                                    int level, /* VADWR_COMP_xxx */
                                    const char *pkfname,
                                    const char *groupname = nullptr, /* can be NULL */
                                    unsigned long long ftime = 0, /* can be 0; seconds since Epoch */
                                    int *upksizep = nullptr, int *pksizep = nullptr,
                                    vwadwr_pack_progress progcb = nullptr, void *progudata = nullptr);

#endif
