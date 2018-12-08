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
//**
//**  Archiving: SaveGame I/O.
//**
//**************************************************************************

#include "gamedefs.h"
#include "net/network.h"
#include "sv_local.h"
#include "filesys/zipstream.h"

#include <time.h>
#include <sys/time.h>


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_dbg_save_on_level_exit("r_dbg_save_on_level_exit", false, "Save before exiting a level.\nNote that after loading this save you prolly won't be able to exit again.", CVAR_Archive);
static VCvarI save_compression_level("save_compression_level", "1", "Save file compression level [0..9]", CVAR_Archive);

static VCvarB dbg_save_ignore_wadlist("dbg_save_ignore_wadlist", false, "Ignore list of loaded wads in savegame when hash mated?", 0/*CVAR_Archive*/);

static VCvarB sv_new_map_autosave("sv_new_map_autosave", true, "Autosave when entering new map (except first one)?", 0/*CVAR_Archive*/);

static VCvarB sv_save_messages("sv_save_messages", true, "Show messages on save/load?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarI Skill;


// ////////////////////////////////////////////////////////////////////////// //
#define QUICKSAVE_SLOT  (-666)

#define EMPTYSTRING  "empty slot"
#define MOBJ_NULL  (-1)
/*
#define SAVE_NAME(_slot)      (VStr("saves/save")+(_slot)+".vsg")
#define SAVE_NAME_ABS(_slot)  (SV_GetSavesDir()+"/save"+(_slot)+".vsg")
*/

#define SAVE_DESCRIPTION_LENGTH    (24)
//#define SAVE_VERSION_TEXT_NO_DATE  "Version 1.34.4"
#define SAVE_VERSION_TEXT          "Version 1.34.9"
#define SAVE_VERSION_TEXT_LENGTH   (16)

static_assert(strlen(SAVE_VERSION_TEXT) <= SAVE_VERSION_TEXT_LENGTH, "oops");

#define SAVE_EXTDATA_ID_END      (0)
#define SAVE_EXTDATA_ID_DATEVAL  (1)
#define SAVE_EXTDATA_ID_DATESTR  (2)


// ////////////////////////////////////////////////////////////////////////// //
// flags for SV_MapTeleport
enum {
  CHANGELEVEL_KEEPFACING      = 0x00000001,
  CHANGELEVEL_RESETINVENTORY  = 0x00000002,
  CHANGELEVEL_NOMONSTERS      = 0x00000004,
  CHANGELEVEL_CHANGESKILL     = 0x00000008,
  CHANGELEVEL_NOINTERMISSION  = 0x00000010,
  CHANGELEVEL_RESETHEALTH     = 0x00000020,
  CHANGELEVEL_PRERAISEWEAPON  = 0x00000040,
};


// ////////////////////////////////////////////////////////////////////////// //
enum gameArchiveSegment_t {
  ASEG_MAP_HEADER = 101,
  ASEG_WORLD,
  ASEG_SCRIPTS,
  ASEG_SOUNDS,
  ASEG_END
};


class VSavedMap {
public:
  TArray<vuint8> Data;
  VName Name;
  vint32 DecompressedSize;
};


class VSaveSlot {
public:
  VStr Description;
  VName CurrentMap;
  TArray<VSavedMap *> Maps;

  ~VSaveSlot () { Clear(); }

  void Clear ();
  bool LoadSlot (int Slot);
  void SaveToSlot (int Slot);
  VSavedMap *FindMap (VName Name);
};


// ////////////////////////////////////////////////////////////////////////// //
static VSaveSlot BaseSlot;


// ////////////////////////////////////////////////////////////////////////// //
class VSaveLoaderStream : public VStream {
private:
  VStream *Stream;

public:
  TArray<VName> NameRemap;
  TArray<VObject *> Exports;

  VSaveLoaderStream (VStream *InStream) : Stream(InStream) { bLoading = true; }
  virtual ~VSaveLoaderStream () override { delete Stream; Stream = nullptr; }

  // stream interface
  virtual void Serialise (void *Data, int Len) override { Stream->Serialise(Data, Len); }
  virtual void Seek (int Pos) override { Stream->Seek(Pos); }
  virtual int Tell () override { return Stream->Tell(); }
  virtual int TotalSize () override { return Stream->TotalSize(); }
  virtual bool AtEnd () override { return Stream->AtEnd(); }
  virtual void Flush () override { Stream->Flush(); }
  virtual bool Close () override { return Stream->Close(); }

  virtual VStream &operator << (VName &Name) override {
    int NameIndex;
    *this << STRM_INDEX(NameIndex);
    Name = NameRemap[NameIndex];
    return *this;
  }

  virtual VStream &operator << (VObject *&Ref) override {
    guard(Loader::operator<<VObject*&);
    int TmpIdx;
    *this << STRM_INDEX(TmpIdx);
    if (TmpIdx == 0) {
      Ref = nullptr;
    } else if (TmpIdx > 0) {
      if (TmpIdx > Exports.Num()) Sys_Error("Bad index %d", TmpIdx);
      Ref = Exports[TmpIdx-1];
    } else {
      Ref = GPlayersBase[-TmpIdx-1];
    }
    return *this;
    unguard;
  }

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override {
    int TmpIdx;
    *this << STRM_INDEX(TmpIdx);
    if (Struct->Name == "sector_t") {
      Ptr = (TmpIdx >= 0 ? &GLevel->Sectors[TmpIdx] : nullptr);
    } else if (Struct->Name == "line_t") {
      Ptr = (TmpIdx >= 0 ? &GLevel->Lines[TmpIdx] : nullptr);
    } else {
      dprintf("Don't know how to handle pointer to %s\n", *Struct->Name);
      Ptr = nullptr;
    }
  }
};


class VSaveWriterStream : public VStream {
private:
  VStream *Stream;

public:
  TArray<VName> Names;
  TArray<VObject *> Exports;
  TArray<vint32> NamesMap;
  TArray<vint32> ObjectsMap;

  VSaveWriterStream (VStream *InStream) : Stream(InStream) {
    bLoading = false;
    NamesMap.SetNum(VName::GetNumNames());
    for (int i = 0; i < VName::GetNumNames(); ++i) NamesMap[i] = -1;
  }

  virtual ~VSaveWriterStream () override { delete Stream; Stream = nullptr; }

  // stream interface
  virtual void Serialise (void *Data, int Len) override { Stream->Serialise(Data, Len); }
  virtual void Seek (int Pos) override { Stream->Seek(Pos); }
  virtual int Tell () override { return Stream->Tell(); }
  virtual int TotalSize () override { return Stream->TotalSize(); }
  virtual bool AtEnd () override { return Stream->AtEnd(); }
  virtual void Flush () override { Stream->Flush(); }
  virtual bool Close () override { return Stream->Close(); }

  virtual VStream &operator << (VName &Name) override {
    if (NamesMap[Name.GetIndex()] == -1) NamesMap[Name.GetIndex()] = Names.Append(Name);
    *this << STRM_INDEX(NamesMap[Name.GetIndex()]);
    return *this;
  }

  virtual VStream &operator << (VObject *&Ref) override {
    guard(Saver::operator<<VObject*&);
    int TmpIdx;
    if (!Ref) {
      TmpIdx = 0;
    } else {
      TmpIdx = ObjectsMap[Ref->GetObjectIndex()];
    }
    return *this << STRM_INDEX(TmpIdx);
    unguard;
  }

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override {
    int TmpIdx;
    if (Struct->Name == "sector_t") {
      TmpIdx = (Ptr ? (int)((sector_t *)Ptr-GLevel->Sectors) : -1);
    } else if (Struct->Name == "line_t") {
      TmpIdx = (Ptr ? (int)((line_t *)Ptr-GLevel->Lines) : -1);
    } else {
      dprintf("Don't know how to handle pointer to %s\n", *Struct->Name);
      TmpIdx = -1;
    }
    *this << STRM_INDEX(TmpIdx);
  }
};


//==========================================================================
//
//  SV_GetSavesDir
//
//==========================================================================
static VStr SV_GetSavesDir () {
  VStr res;
#if !defined(_WIN32)
  const char *HomeDir = getenv("HOME");
  if (HomeDir && HomeDir[0]) {
    res = VStr(HomeDir)+"/.vavoom";
  } else {
    res = (fl_savedir.IsNotEmpty() ? fl_savedir : fl_basedir);
  }
#else
  res = (fl_savedir.IsNotEmpty() ? fl_savedir : fl_basedir);
#endif
  res += "/saves";
  return res;
}


//==========================================================================
//
//  GetSaveSlotBaseFileName
//
//  if slot is < 0, this is autosave slot
//  QUICKSAVE_SLOT is quicksave slot
//  returns empty string for invalid slot
//
//==========================================================================
static VStr GetSaveSlotBaseFileName (int slot) {
  if (slot != QUICKSAVE_SLOT && (slot < -64 || slot > 63)) return VStr();
  VStr modlist;
  // get list of loaded modules
  auto wadlist = GetWadPk3List();
  //GCon->Logf("====================="); for (int f = 0; f < wadlist.length(); ++f) GCon->Logf("  %d: %s", f, *wadlist[f]);
  for (int f = 0; f < wadlist.length(); ++f) {
    modlist += wadlist[f];
    modlist += "\n";
  }
#if 0
  // get list hash
  vuint8 sha512[SHA512_DIGEST_SIZE];
  sha512_buf(sha512, *modlist, (size_t)modlist.length());
  // convert to hex
  VStr shahex = VStr::buf2hex(sha512, SHA512_DIGEST_SIZE);
#else
  vuint32 xxhashval = XXHash32::hash(*modlist, (vint32)modlist.length(), (vuint32)wadlist.length());
  VStr shahex = VStr::buf2hex(&xxhashval, 4);
#endif
  if (slot == QUICKSAVE_SLOT) return shahex+VStr("_quicksave");
  if (slot < 0) return VStr(va("%s_autosave_%02d", *shahex, -slot));
  return VStr(va("%s_normsave_%02d", *shahex, slot+1));
}


//==========================================================================
//
//  GetSaveSlotFileName
//
//  see above about slot values meaning
//  returns empty string for invalid slot
//
//==========================================================================
/*
static VStr GetSaveSlotFileName (int slot) {
  VStr bfn = GetSaveSlotBaseFileName(slot);
  if (bfn.isEmpty()) return bfn;
  return SV_GetSavesDir()+"/"+bfn;
}
*/


//==========================================================================
//
//  SV_OpenSlotFileRead
//
//  open savegame slot file if it exists
//
//==========================================================================
static VStream *SV_OpenSlotFileRead (int slot) {
  if (slot != QUICKSAVE_SLOT && (slot < -64 || slot > 63)) return nullptr;
  auto svdir = SV_GetSavesDir();
  auto dir = Sys_OpenDir(svdir);
  if (!dir) return nullptr;
  auto svpfx = GetSaveSlotBaseFileName(slot);
  for (;;) {
    VStr fname = Sys_ReadDir(dir);
    if (fname.isEmpty()) break;
    VStr flow = fname.toLowerCase();
    if (flow.startsWith(svpfx) && flow.endsWith(".vsg")) {
      Sys_CloseDir(dir);
      return FL_OpenSysFileRead(svdir+"/"+fname);
    }
  }
  Sys_CloseDir(dir);
  return nullptr;
}


//==========================================================================
//
//  SV_CreateSlotFileWrite
//
//  create new savegame slot file
//  also, removes any existing savegame file for the same slot
//
//==========================================================================
static VStream *SV_CreateSlotFileWrite (int slot, VStr descr) {
  if (slot != QUICKSAVE_SLOT && (slot < -64 || slot > 63)) return nullptr;
  if (slot == QUICKSAVE_SLOT) descr = VStr();
  auto svdir = SV_GetSavesDir();
  FL_CreatePath(svdir); // just in case
  auto dir = Sys_OpenDir(svdir);
  if (!dir) return nullptr;
  // scan
  auto svpfx = GetSaveSlotBaseFileName(slot);
  TArray<VStr> tokill;
  for (;;) {
    VStr fname = Sys_ReadDir(dir);
    if (fname.isEmpty()) break;
    VStr flow = fname.toLowerCase();
    if (flow.startsWith(svpfx) && flow.endsWith(".vsg")) tokill.append(svdir+"/"+fname);
  }
  Sys_CloseDir(dir);
  // remove old saves
  for (int f = 0; f < tokill.length(); ++f) Sys_FileDelete(tokill[f]);
  // normalize description
  VStr newdesc;
  for (int f = 0; f < descr.length(); ++f) {
    char ch = descr[f];
    if (!ch) continue;
    if (ch >= '0' && ch <= '9') { newdesc += ch; continue; }
    if (ch >= 'A' && ch <= 'Z') { newdesc += ch-'A'+'a'; continue; } // poor man's tolower()
    if (ch >= 'a' && ch <= 'z') { newdesc += ch; continue; }
    // replace with underscore
    if (newdesc.length() == 0) continue;
    if (newdesc[newdesc.length()-1] == '_') continue;
    newdesc += "_";
  }
  while (newdesc.length() && newdesc[0] == '_') newdesc.chopLeft(1);
  while (newdesc.length() && newdesc[newdesc.length()-1] == '_') newdesc.chopRight(1);
  // finalize file name
  svpfx = svdir+"/"+svpfx;
  if (newdesc.length()) { svpfx += "_"; svpfx += newdesc; }
  svpfx += ".vsg";
  return FL_OpenSysFileWrite(svpfx);
}


#ifdef CLIENT
//==========================================================================
//
//  SV_DeleteSlotFile
//
//==========================================================================
static bool SV_DeleteSlotFile (int slot) {
  if (slot != QUICKSAVE_SLOT && (slot < -64 || slot > 63)) return false;
  auto svdir = SV_GetSavesDir();
  auto dir = Sys_OpenDir(svdir);
  if (!dir) return false;
  // scan
  auto svpfx = GetSaveSlotBaseFileName(slot);
  TArray<VStr> tokill;
  for (;;) {
    VStr fname = Sys_ReadDir(dir);
    if (fname.isEmpty()) break;
    VStr flow = fname.toLowerCase();
    if (flow.startsWith(svpfx) && flow.endsWith(".vsg")) tokill.append(svdir+"/"+fname);
  }
  Sys_CloseDir(dir);
  if (tokill.length() == 0) return false;
  // remove old saves
  for (int f = 0; f < tokill.length(); ++f) Sys_FileDelete(tokill[f]);
  return true;
}
#endif


// ////////////////////////////////////////////////////////////////////////// //
struct TTimeVal {
  int secs; // actually, unsigned
  int usecs;
  // for 2030+
  int secshi;

  inline bool operator < (const TTimeVal &tv) const {
    if (secshi < tv.secshi) return true;
    if (secshi > tv.secshi) return false;
    if (secs < tv.secs) return true;
    if (secs > tv.secs) return false;
    return false;
  }
};


//==========================================================================
//
//  GetTimeOfDay
//
//==========================================================================
static void GetTimeOfDay (TTimeVal *tvres) {
  if (!tvres) return;
  tvres->secshi = 0;
  timeval tv;
  if (gettimeofday(&tv, nullptr)) {
    tvres->secs = 0;
    tvres->usecs = 0;
  } else {
    tvres->secs = (int)(tv.tv_sec&0xffffffff);
    tvres->usecs = (int)tv.tv_usec;
    tvres->secshi = (int)(((uint64_t)tv.tv_sec)>>32);
  }
}


//==========================================================================
//
//  TimeVal2Str
//
//==========================================================================
static VStr TimeVal2Str (const TTimeVal *tvin, bool forAutosave=false) {
  timeval tv;
  tv.tv_sec = (((uint64_t)tvin->secs)&0xffffffff)|(((uint64_t)tvin->secshi)<<32);
  //tv.tv_usec = tvin->usecs;
  tm ctm;
  if (localtime_r(&tv.tv_sec, &ctm)) {
    if (forAutosave) {
      // for autosave
      return VStr(va("%02d:%02d", (int)ctm.tm_hour, (int)ctm.tm_min));
    } else {
      // full
      return VStr(va("%04d/%02d/%02d %02d:%02d:%02d",
        (int)(ctm.tm_year+1900),
        (int)ctm.tm_mon,
        (int)ctm.tm_mday,
        (int)ctm.tm_hour,
        (int)ctm.tm_min,
        (int)ctm.tm_sec));
    }
  } else {
    return VStr("unknown");
  }
}


//==========================================================================
//
//  VSaveSlot::Clear
//
//==========================================================================
void VSaveSlot::Clear () {
  guard(VSaveSlot::Clear);
  Description.Clean();
  CurrentMap = NAME_None;
  for (int i = 0; i < Maps.Num(); ++i) { delete Maps[i]; Maps[i] = nullptr; }
  Maps.Clear();
  unguard;
}


//==========================================================================
//
//  SkipExtData
//
//  skip extended data
//
//==========================================================================
static bool SkipExtData (VStream *Strm) {
  for (;;) {
    vint32 id, size;
    *Strm << STRM_INDEX(id);
    if (id == SAVE_EXTDATA_ID_END) break;
    *Strm << STRM_INDEX(size);
    if (size < 0 || size > 65536) return false;
    // skip data
    Strm->Seek(Strm->Tell()+size);
  }
  return true;
}


//==========================================================================
//
//  LoadDateStrExtData
//
//  get date string, or use timeval to build it
//  empty string means i/o error
//
//==========================================================================
static VStr LoadDateStrExtData (VStream *Strm) {
  bool tvvalid = false;
  TTimeVal tv;
  memset((void *)&tv, 0, sizeof(tv));
  VStr res;
  for (;;) {
    vint32 id, size;
    *Strm << STRM_INDEX(id);
    if (id == SAVE_EXTDATA_ID_END) break;
    *Strm << STRM_INDEX(size);
    if (size < 0 || size > 65536) return VStr();

    if (id == SAVE_EXTDATA_ID_DATEVAL && size == (vint32)sizeof(tv)) {
      tvvalid = true;
      Strm->Serialize(&tv, sizeof(tv));
      continue;
    }

    if (id == SAVE_EXTDATA_ID_DATESTR && size > 0 && size < 64) {
      char buf[65];
      memset(buf, 0, sizeof(buf));
      Strm->Serialize(buf, size);
      if (buf[0]) res = VStr(buf);
      continue;
    }

    // skip unknown data
    Strm->Seek(Strm->Tell()+size);
  }
  if (res.length() == 0) {
    if (tvvalid) {
      res = TimeVal2Str(&tv);
    } else {
      res = VStr("UNKNOWN");
    }
  }
  return res;
}


//==========================================================================
//
//  LoadDateTValExtData
//
//==========================================================================
static bool LoadDateTValExtData (VStream *Strm, TTimeVal *tv) {
  memset((void *)tv, 0, sizeof(*tv));
  for (;;) {
    vint32 id, size;
    *Strm << STRM_INDEX(id);
    if (id == SAVE_EXTDATA_ID_END) break;
    //fprintf(stderr, "   id=%d\n", id);
    *Strm << STRM_INDEX(size);
    if (size < 0 || size > 65536) break;

    if (id == SAVE_EXTDATA_ID_DATEVAL && size == (vint32)sizeof(*tv)) {
      Strm->Serialize(&tv, sizeof(tv));
      //fprintf(stderr, "  found TV[%s] (%s)\n", *TimeVal2Str(tv), (Strm->IsError() ? "ERROR" : "OK"));
      return !Strm->IsError();
    }

    // skip unknown data
    Strm->Seek(Strm->Tell()+size);
  }
  return false;
}


//==========================================================================
//
//  VSaveSlot::LoadSlot
//
//==========================================================================
bool VSaveSlot::LoadSlot (int Slot) {
  guard(VSaveSlot::LoadSlot);
  Clear();
  VStream *Strm = SV_OpenSlotFileRead(Slot);
  if (!Strm) {
    GCon->Log("Savegame file doesn't exist");
    return false;
  }

  // check the version text
  char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
  memset(VersionText, 0, sizeof(VersionText));
  Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);
  if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) /*&& VStr::Cmp(VersionText, SAVE_VERSION_TEXT_NO_DATE)*/) {
    // bad version
    Strm->Close();
    delete Strm;
    Strm = nullptr;
    GCon->Log("Savegame is from incompatible version");
    return false;
  }

  *Strm << Description;

  // skip extended data
  if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) == 0) {
    if (!SkipExtData(Strm) || Strm->IsError()) {
      // bad file
      Strm->Close();
      delete Strm;
      Strm = nullptr;
      GCon->Log("Savegame is corrupted");
      return false;
    }
  }

  // check list of loaded modules
  auto wadlist = GetWadPk3List();
  vint32 wcount = wadlist.length();
  *Strm << wcount;

  if (wcount < 1 || wcount > 8192) {
    Strm->Close();
    delete Strm;
    Strm = nullptr;
    GCon->Log("Invalid savegame (bad number of mods)");
    return false;
  }

  if (!dbg_save_ignore_wadlist) {
    if (wcount != wadlist.length()) {
      Strm->Close();
      delete Strm;
      Strm = nullptr;
      GCon->Log("Invalid savegame (bad number of mods)");
      return false;
    }
  }

  for (int f = 0; f < wcount; ++f) {
    VStr s;
    *Strm << s;
    if (!dbg_save_ignore_wadlist) {
      if (s != wadlist[f]) {
        Strm->Close();
        delete Strm;
        Strm = nullptr;
        GCon->Log("Invalid savegame (bad mod)");
        return false;
      }
    }
  }

  VStr TmpName;
  *Strm << TmpName;
  CurrentMap = *TmpName;

  int NumMaps;
  *Strm << STRM_INDEX(NumMaps);
  for (int i = 0; i < NumMaps; ++i) {
    VSavedMap *Map = new VSavedMap();
    Maps.Append(Map);
    vint32 DataLen;
    *Strm << TmpName << Map->DecompressedSize << STRM_INDEX(DataLen);
    Map->Name = *TmpName;
    Map->Data.SetNum(DataLen);
    Strm->Serialise(Map->Data.Ptr(), Map->Data.Num());
  }

  Strm->Close();
  delete Strm;
  Strm = nullptr;
  return true;
  unguard;
}


//==========================================================================
//
//  VSaveSlot::SaveToSlot
//
//==========================================================================
void VSaveSlot::SaveToSlot (int Slot) {
  guard(VSaveSlot::SaveToSlot);

  VStream *Strm = SV_CreateSlotFileWrite(Slot, Description);
  if (!Strm) {
    GCon->Logf("ERROR: cannot save to slot %d!", Slot);
    return;
  }

  // write version info
  char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
  memset(VersionText, 0, SAVE_VERSION_TEXT_LENGTH);
  VStr::Cpy(VersionText, SAVE_VERSION_TEXT);
  Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);

  // write game save description
  *Strm << Description;

  // extended data: date value and date string
  {
    // date value
    TTimeVal tv;
    GetTimeOfDay(&tv);
    vint32 id = SAVE_EXTDATA_ID_DATEVAL;
    *Strm << STRM_INDEX(id);
    vint32 size = (vint32)sizeof(tv);
    *Strm << STRM_INDEX(size);
    Strm->Serialize(&tv, sizeof(tv));

    // date string
    VStr dstr = TimeVal2Str(&tv);
    id = SAVE_EXTDATA_ID_DATESTR;
    *Strm << STRM_INDEX(id);
    size = dstr.length();
    *Strm << STRM_INDEX(size);
    Strm->Serialize(*dstr, size);

    // end of data marker
    id = SAVE_EXTDATA_ID_END;
    *Strm << STRM_INDEX(id);
  }

  // write list of loaded modules
  auto wadlist = GetWadPk3List();
  //GCon->Logf("====================="); for (int f = 0; f < wadlist.length(); ++f) GCon->Logf("  %d: %s", f, *wadlist[f]);
  vint32 wcount = wadlist.length();
  *Strm << wcount;
  for (int f = 0; f < wadlist.length(); ++f) *Strm << wadlist[f];

  // write current map
  VStr TmpName(CurrentMap);
  *Strm << TmpName;

  int NumMaps = Maps.Num();
  *Strm << STRM_INDEX(NumMaps);
  for (int i = 0; i < Maps.Num(); ++i) {
    TmpName = VStr(Maps[i]->Name);
    vint32 DataLen = Maps[i]->Data.Num();
    *Strm << TmpName << Maps[i]->DecompressedSize << STRM_INDEX(DataLen);
    Strm->Serialise(Maps[i]->Data.Ptr(), Maps[i]->Data.Num());
  }

  bool err = Strm->IsError();
  Strm->Close();
  err = err || Strm->IsError();
  delete Strm;
  Strm = nullptr;
  if (err) {
    GCon->Logf("ERROR: error saving to slot %d, savegame is corrupted!", Slot);
    return;
  }

  unguard;
}


//==========================================================================
//
//  VSaveSlot::FindMap
//
//==========================================================================
VSavedMap *VSaveSlot::FindMap (VName Name) {
  guard(VSaveSlot::FindMap);
  for (int i = 0; i < Maps.Num(); ++i) if (Maps[i]->Name == Name) return Maps[i];
  return nullptr;
  unguard;
}


//==========================================================================
//
//  SV_GetSaveString
//
//==========================================================================
bool SV_GetSaveString (int Slot, VStr &Desc) {
  guard(SV_GetSaveString);
  VStream *Strm = SV_OpenSlotFileRead(Slot);
  if (Strm) {
    char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
    memset(VersionText, 0, sizeof(VersionText));
    Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);
    bool goodSave = true;
    Desc = "???";
    if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) /*&& VStr::Cmp(VersionText, SAVE_VERSION_TEXT_NO_DATE)*/) {
      // bad version, put an asterisk in front of the description
      goodSave = false;
    } else {
      *Strm << Desc;
      // skip extended data
      if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) == 0) {
        if (!SkipExtData(Strm) || Strm->IsError()) goodSave = false;
      }
      if (goodSave) {
        // check list of loaded modules
        auto wadlist = GetWadPk3List();
        vint32 wcount = wadlist.length();
        *Strm << wcount;
        if (wcount < 1 || wcount > 8192 || wcount != wadlist.length()) {
          goodSave = false;
        } else {
          for (int f = 0; f < wcount; ++f) {
            VStr s;
            *Strm << s;
            if (s != wadlist[f]) { goodSave = false; break; }
          }
        }
      }
    }
    if (!goodSave) Desc = "*"+Desc;
    delete Strm;
    return true;
  } else {
    Desc = EMPTYSTRING;
    return false;
  }
  unguard;
}



//==========================================================================
//
//  SV_GetSaveDateString
//
//==========================================================================
void SV_GetSaveDateString (int Slot, VStr &datestr) {
  guard(SV_GetSaveDate);
  VStream *Strm = SV_OpenSlotFileRead(Slot);
  if (Strm) {
    char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
    memset(VersionText, 0, sizeof(VersionText));
    Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);
    datestr = "UNKNOWN";
    if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) == 0) {
      VStr Desc;
      *Strm << Desc;
      datestr = LoadDateStrExtData(Strm);
      if (datestr.length() == 0) datestr = "UNKNOWN";
    }
    delete Strm;
  } else {
    datestr = "UNKNOWN";
  }
  unguard;
}


//==========================================================================
//
//  SV_GetSaveDateTVal
//
//  false: slot is empty or invalid
//
//==========================================================================
static bool SV_GetSaveDateTVal (int Slot, TTimeVal *tv) {
  guard(SV_GetSaveDate);
  memset((void *)tv, 0, sizeof(*tv));
  VStream *Strm = SV_OpenSlotFileRead(Slot);
  if (Strm) {
    char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
    memset(VersionText, 0, sizeof(VersionText));
    Strm->Serialise(VersionText, SAVE_VERSION_TEXT_LENGTH);
    //fprintf(stderr, "OPENED slot #%d\n", Slot);
    if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) != 0) { delete Strm; return false; }
    //fprintf(stderr, "  slot #%d has valid version\n", Slot);
    VStr Desc;
    *Strm << Desc;
    //fprintf(stderr, "  slot #%d description: [%s]\n", Slot, *Desc);
    if (!LoadDateTValExtData(Strm, tv)) { delete Strm; return false; }
    delete Strm;
    return true;
  } else {
    return false;
  }
  unguard;
}


//==========================================================================
//
//  SV_FindAutosaveSlot
//
//  returns 0 on error
//
//==========================================================================
static int SV_FindAutosaveSlot () {
  TTimeVal tv, besttv;
  int bestslot = 0;
  memset((void *)&tv, 0, sizeof(tv));
  memset((void *)&besttv, 0, sizeof(besttv));
  for (int slot = 1; slot <= 8; ++slot) {
    if (!SV_GetSaveDateTVal(-slot, &tv)) {
      //fprintf(stderr, "AUTOSAVE: free slot #%d found!\n", slot);
      bestslot = -slot;
      break;
    }
    if (!bestslot || tv < besttv) {
      //fprintf(stderr, "AUTOSAVE: better slot #%d found [%s] : [%s]!\n", slot, *TimeVal2Str(&tv), (bestslot ? *TimeVal2Str(&besttv) : ""));
      bestslot = -slot;
      besttv = tv;
    }
  }
  return bestslot;
}


//==========================================================================
//
//  AssertSegment
//
//==========================================================================
static void AssertSegment (VStream &Strm, gameArchiveSegment_t segType) {
  guard(AssertSegment);
  if (Streamer<int>(Strm) != (int)segType) Host_Error("Corrupt save game: Segment [%d] failed alignment check", segType);
  unguard;
}


//==========================================================================
//
//  ArchiveNames
//
//==========================================================================
static void ArchiveNames (VSaveWriterStream *Saver) {
  // write offset to the names in the beginning of the file
  vint32 NamesOffset = Saver->Tell();
  Saver->Seek(0);
  *Saver << NamesOffset;
  Saver->Seek(NamesOffset);

  // serialise names
  vint32 Count = Saver->Names.Num();
  *Saver << STRM_INDEX(Count);
  for (int i = 0; i < Count; ++i) *Saver << *VName::GetEntry(Saver->Names[i].GetIndex());
}


//==========================================================================
//
//  UnarchiveNames
//
//==========================================================================
static void UnarchiveNames (VSaveLoaderStream *Loader) {
  vint32 NamesOffset;
  *Loader << NamesOffset;

  vint32 TmpOffset = Loader->Tell();
  Loader->Seek(NamesOffset);
  vint32 Count;
  *Loader << STRM_INDEX(Count);
  Loader->NameRemap.SetNum(Count);
  for (int i = 0; i < Count; ++i) {
    VNameEntry E;
    *Loader << E;
    Loader->NameRemap[i] = VName(E.Name);
  }
  Loader->Seek(TmpOffset);
}


//==========================================================================
//
//  ArchiveThinkers
//
//==========================================================================
static void ArchiveThinkers (VSaveWriterStream *Saver, bool SavingPlayers) {
  guard(ArchiveThinkers);
  vint32 Seg = ASEG_WORLD;
  *Saver << Seg;

  Saver->ObjectsMap.SetNum(VObject::GetObjectsCount());
  for (int i = 0; i < VObject::GetObjectsCount(); ++i) Saver->ObjectsMap[i] = 0;

  // add level
  Saver->Exports.Append(GLevel);
  Saver->ObjectsMap[GLevel->GetObjectIndex()] = Saver->Exports.Num();

  // add world info
  vuint8 WorldInfoSaved = (byte)SavingPlayers;
  *Saver << WorldInfoSaved;
  if (WorldInfoSaved) {
    Saver->Exports.Append(GGameInfo->WorldInfo);
    Saver->ObjectsMap[GGameInfo->WorldInfo->GetObjectIndex()] = Saver->Exports.Num();
  }

  // add players
  for (int i = 0; i < MAXPLAYERS; ++i) {
    byte Active = (byte)(SavingPlayers && GGameInfo->Players[i]);
    *Saver << Active;
    if (!Active) continue;
    Saver->Exports.Append(GGameInfo->Players[i]);
    Saver->ObjectsMap[GGameInfo->Players[i]->GetObjectIndex()] = Saver->Exports.Num();
  }

  // add thinkers
  int ThinkersStart = Saver->Exports.Num();
  for (TThinkerIterator<VThinker> Th(GLevel); Th; ++Th) {
    VEntity *mobj = Cast<VEntity>(*Th);
    if (mobj != nullptr && mobj->EntityFlags & VEntity::EF_IsPlayer && !SavingPlayers) continue; // skipping player mobjs
    Saver->Exports.Append(*Th);
    Saver->ObjectsMap[Th->GetObjectIndex()] = Saver->Exports.Num();
  }

  vint32 NumObjects = Saver->Exports.Num()-ThinkersStart;
  *Saver << STRM_INDEX(NumObjects);
  for (int i = ThinkersStart; i < Saver->Exports.Num(); ++i) {
    VName CName = Saver->Exports[i]->GetClass()->GetVName();
    *Saver << CName;
  }

  // serialise objects
  for (int i = 0; i < Saver->Exports.Num(); ++i) Saver->Exports[i]->Serialise(*Saver);
  unguard;
}


//==========================================================================
//
//  UnarchiveThinkers
//
//==========================================================================
static void UnarchiveThinkers (VSaveLoaderStream *Loader) {
  guard(UnarchiveThinkers);
  VObject *Obj;

  AssertSegment(*Loader, ASEG_WORLD);

  // add level
  Loader->Exports.Append(GLevel);

  // add world info
  vuint8 WorldInfoSaved;
  *Loader << WorldInfoSaved;
  if (WorldInfoSaved) Loader->Exports.Append(GGameInfo->WorldInfo);

  // add players
  sv_load_num_players = 0;
  for (int i = 0; i < MAXPLAYERS; ++i) {
    byte Active;
    *Loader << Active;
    if (Active) {
      ++sv_load_num_players;
      Loader->Exports.Append(GPlayersBase[i]);
    }
  }

  vint32 NumObjects;
  *Loader << STRM_INDEX(NumObjects);
  for (int i = 0; i < NumObjects; ++i) {
    // get params
    VName CName;
    *Loader << CName;
    VClass *Class = VClass::FindClass(*CName);
    if (!Class) Sys_Error("No such class '%s'", *CName);

    // allocate object and copy data
    Obj = VObject::StaticSpawnObject(Class, true); // skip replacement

    // handle level info
    if (Obj->IsA(VLevelInfo::StaticClass())) {
      GLevelInfo = (VLevelInfo *)Obj;
      GLevelInfo->Game = GGameInfo;
      GLevelInfo->World = GGameInfo->WorldInfo;
      GLevel->LevelInfo = GLevelInfo;
    }

    Loader->Exports.Append(Obj);
  }

  GLevelInfo->Game = GGameInfo;
  GLevelInfo->World = GGameInfo->WorldInfo;

  for (int i = 0; i < Loader->Exports.Num(); ++i) Loader->Exports[i]->Serialise(*Loader);

  GLevelInfo->eventAfterUnarchiveThinkers();
  GLevel->eventAfterUnarchiveThinkers();
  unguard;
}


//==========================================================================
//
//  ArchiveSounds
//
//==========================================================================
static void ArchiveSounds (VStream &Strm) {
  vint32 Seg = ASEG_SOUNDS;
  Strm << Seg;
#ifdef CLIENT
  GAudio->SerialiseSounds(Strm);
#else
  vint32 Dummy = 0;
  Strm << Dummy;
#endif
}


//==========================================================================
//
//  UnarchiveSounds
//
//==========================================================================
static void UnarchiveSounds (VStream &Strm) {
  AssertSegment(Strm, ASEG_SOUNDS);
#ifdef CLIENT
  GAudio->SerialiseSounds(Strm);
#else
  vint32 count = 0;
  Strm << count;
  //FIXME: keep this in sync with VAudio
  //Strm.Seek(Strm.Tell()+Dummy*36); //FIXME!
  if (count < 0) Sys_Error("invalid sound sequence data");
  while (count-- > 0) {
    vuint8 xver = 0; // current version is 0
    Strm << xver;
    if (xver != 0) Sys_Error("invalid sound sequence data");
    vint32 Sequence;
    vint32 OriginId;
    TVec Origin;
    vint32 CurrentSoundID;
    float DelayTime;
    vuint32 DidDelayOnce;
    float Volume;
    float Attenuation;
    vint32 ModeNum;
    Strm << STRM_INDEX(Sequence)
      << STRM_INDEX(OriginId)
      << Origin
      << STRM_INDEX(CurrentSoundID)
      << DelayTime
      << STRM_INDEX(DidDelayOnce)
      << Volume
      << Attenuation
      << STRM_INDEX(ModeNum);

    vint32 Offset;
    Strm << STRM_INDEX(Offset);

    vint32 Count;
    Strm << STRM_INDEX(Count);
    if (Count < 0) Sys_Error("invalid sound sequence data");
    for (int i = 0; i < Count; ++i) {
      VName SeqName;
      Strm << SeqName;
    }

    vint32 ParentSeqIdx;
    vint32 ChildSeqIdx;
    Strm << STRM_INDEX(ParentSeqIdx) << STRM_INDEX(ChildSeqIdx);
  }
#endif
}


//==========================================================================
//
// SV_SaveMap
//
//==========================================================================
static void SV_SaveMap (bool savePlayers) {
  guard(SV_SaveMap);

  // make sure we don't have any garbage
  VObject::CollectGarbage();

  // open the output file
  VMemoryStream *InStrm = new VMemoryStream();
  VSaveWriterStream *Saver = new VSaveWriterStream(InStrm);

  int NamesOffset = 0;
  *Saver << NamesOffset;

  // place a header marker
  vint32 Seg = ASEG_MAP_HEADER;
  *Saver << Seg;

  // write the level timer
  *Saver << GLevel->Time << GLevel->TicTime;

  ArchiveThinkers(Saver, savePlayers);
  ArchiveSounds(*Saver);

  // place a termination marker
  Seg = ASEG_END;
  *Saver << Seg;

  ArchiveNames(Saver);

  // close the output file
  Saver->Close();

  TArray<vuint8> &Buf = InStrm->GetArray();

  VSavedMap *Map = BaseSlot.FindMap(GLevel->MapName);
  if (!Map) {
    Map = new VSavedMap();
    BaseSlot.Maps.Append(Map);
    Map->Name = GLevel->MapName;
  }

  // compress map data
  Map->DecompressedSize = Buf.Num();
  Map->Data.Clear();
  VArrayStream *ArrStrm = new VArrayStream(Map->Data);
  ArrStrm->BeginWrite();
  VZipStreamWriter *ZipStrm = new VZipStreamWriter(ArrStrm, (int)save_compression_level);
  ZipStrm->Serialise(Buf.Ptr(), Buf.Num());
  delete ZipStrm;
  ZipStrm = nullptr;
  delete ArrStrm;
  ArrStrm = nullptr;

  delete Saver;
  Saver = nullptr;
  unguard;
}


//==========================================================================
//
//  SV_LoadMap
//
//==========================================================================
static void SV_LoadMap (VName MapName) {
  guard(SV_LoadMap);
  // load a base level
  SV_SpawnServer(*MapName, false, false);

  VSavedMap *Map = BaseSlot.FindMap(MapName);
  check(Map);

  // decompress map data
  VArrayStream *ArrStrm = new VArrayStream(Map->Data);
  VZipStreamReader *ZipStrm = new VZipStreamReader(ArrStrm);
  TArray<vuint8> DecompressedData;
  DecompressedData.SetNum(Map->DecompressedSize);
  ZipStrm->Serialise(DecompressedData.Ptr(), DecompressedData.Num());
  delete ZipStrm;
  ZipStrm = nullptr;
  delete ArrStrm;
  ArrStrm = nullptr;

  VSaveLoaderStream *Loader = new VSaveLoaderStream(new VArrayStream(DecompressedData));

  // load names
  UnarchiveNames(Loader);

  AssertSegment(*Loader, ASEG_MAP_HEADER);

  // read the level timer
  *Loader << GLevel->Time << GLevel->TicTime;

  UnarchiveThinkers(Loader);
  UnarchiveSounds(*Loader);

  AssertSegment(*Loader, ASEG_END);

  // free save buffer
  Loader->Close();
  delete Loader;
  Loader = nullptr;

  // do this here so that clients have loaded info, not initial one
  SV_SendServerInfoToClients();
  unguard;
}


//==========================================================================
//
//  SV_SaveGame
//
//==========================================================================
void SV_SaveGame (int slot, const VStr &Description) {
  guard(SV_SaveGame);

  BaseSlot.Description = Description;
  BaseSlot.CurrentMap = GLevel->MapName;

  // save out the current map
  SV_SaveMap(true); // true = save player info

  // write data to destination slot
  BaseSlot.SaveToSlot(slot);
  unguard;
}


//==========================================================================
//
//  SV_LoadGame
//
//==========================================================================
void SV_LoadGame (int slot) {
  guard(SV_LoadGame);
  SV_ShutdownGame();

  if (!BaseSlot.LoadSlot(slot)) return;

  sv_loading = true;

  // load the current map
  SV_LoadMap(BaseSlot.CurrentMap);

#ifdef CLIENT
  if (GGameInfo->NetMode != NM_DedicatedServer) CL_SetUpLocalPlayer();
#endif

  // launch waiting scripts
  if (!deathmatch) GLevel->Acs->CheckAcsStore();

  unguard;
}


//==========================================================================
//
//  SV_InitBaseSlot
//
//==========================================================================
void SV_InitBaseSlot () {
  BaseSlot.Clear();
}


//==========================================================================
//
// SV_MapTeleport
//
//==========================================================================
/*
  CHANGELEVEL_KEEPFACING      = 0x00000001,
  CHANGELEVEL_RESETINVENTORY  = 0x00000002,
  CHANGELEVEL_NOMONSTERS      = 0x00000004,
  CHANGELEVEL_CHANGESKILL     = 0x00000008,
  CHANGELEVEL_NOINTERMISSION  = 0x00000010,
  CHANGELEVEL_RESETHEALTH     = 0x00000020,
  CHANGELEVEL_PRERAISEWEAPON  = 0x00000040,
*/
void SV_MapTeleport (VName mapname, int flags, int newskill) {
  guard(SV_MapTeleport);
  TArray<VThinker *> TravelObjs;

  if (newskill >= 0 && (flags&CHANGELEVEL_CHANGESKILL) != 0) {
    GCon->Logf("SV_MapTeleport: new skill is %d", newskill);
    Skill = newskill;
    flags &= ~CHANGELEVEL_CHANGESKILL; // clear flag
  }

  // we won't show intermission anyway, so remove this flag
  flags &= ~CHANGELEVEL_NOINTERMISSION;

  if (flags) {
    GCon->Logf("SV_MapTeleport: unimplemented flag set: 0x%04x", flags);
    if (flags&CHANGELEVEL_KEEPFACING) GCon->Logf("SV_MapTeleport:   KEEPFACING");
    if (flags&CHANGELEVEL_RESETINVENTORY) GCon->Logf("SV_MapTeleport:   RESETINVENTORY");
    if (flags&CHANGELEVEL_NOMONSTERS) GCon->Logf("SV_MapTeleport:   NOMONSTERS");
    if (flags&CHANGELEVEL_NOINTERMISSION) GCon->Logf("SV_MapTeleport:   NOINTERMISSION");
    if (flags&CHANGELEVEL_PRERAISEWEAPON) GCon->Logf("SV_MapTeleport:   PRERAISEWEAPON");
  }

  // call PreTravel event
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!GGameInfo->Players[i]) continue;
    GGameInfo->Players[i]->eventPreTravel();
  }

  // collect list of thinkers that will go to the new level
  for (VThinker *Th = GLevel->ThinkerHead; Th; Th = Th->Next) {
    VEntity *vent = Cast<VEntity>(Th);
    if (vent != nullptr && (//(vent->EntityFlags & VEntity::EF_IsPlayer) ||
        (vent->Owner && (vent->Owner->EntityFlags & VEntity::EF_IsPlayer))))
    {
      TravelObjs.Append(vent);
      GLevel->RemoveThinker(vent);
      vent->UnlinkFromWorld();
      GLevel->DelSectorList();
      vent->StopSound(0);
    }
    if (Th->IsA(VPlayerReplicationInfo::StaticClass())) {
      TravelObjs.Append(Th);
      GLevel->RemoveThinker(Th);
    }
  }

  if (!deathmatch) {
    const mapInfo_t &old_info = P_GetMapInfo(GLevel->MapName);
    const mapInfo_t &new_info = P_GetMapInfo(mapname);
    // all maps in cluster 0 are treated as in different clusters
    if (old_info.Cluster && old_info.Cluster == new_info.Cluster &&
        (P_GetClusterDef(old_info.Cluster)->Flags & CLUSTERF_Hub))
    {
      // same cluster: save map without saving player mobjs
      SV_SaveMap(false);
    } else {
      // entering new cluster: clear base slot
      BaseSlot.Clear();
    }
  }

  sv_map_travel = true;
  if (!deathmatch && BaseSlot.FindMap(mapname)) {
    // unarchive map
    SV_LoadMap(mapname);
  } else {
    // new map
    SV_SpawnServer(*mapname, true, false);
  }

  // add traveling thinkers to the new level
  for (int i = 0; i < TravelObjs.Num(); ++i) {
    GLevel->AddThinker(TravelObjs[i]);
    VEntity *Ent = Cast<VEntity>(TravelObjs[i]);
    if (Ent) Ent->LinkToWorld();
  }

#ifdef CLIENT
  bool doSaveGame = false;
  if (GGameInfo->NetMode == NM_TitleMap ||
      GGameInfo->NetMode == NM_Standalone ||
      GGameInfo->NetMode == NM_ListenServer)
  {
    CL_SetUpStandaloneClient();
    doSaveGame = sv_new_map_autosave;
  }
#else
  const bool doSaveGame = false;
#endif

  // launch waiting scripts
  if (!deathmatch) GLevel->Acs->CheckAcsStore();

  if (doSaveGame) GCmdBuf << "AutoSaveEnter\n";

  unguard;
}


#ifdef CLIENT
void Draw_SaveIcon ();
void Draw_LoadIcon ();


static bool CheckIfLoadIsAllowed () {
  if (deathmatch) {
    GCon->Log("Can't load in deathmatch game");
    return false;
  }

  return true;
}
#endif


static bool CheckIfSaveIsAllowed () {
  if (deathmatch) {
    GCon->Log("Can't save in deathmatch game");
    return false;
  }

  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_TitleMap || GGameInfo->NetMode == NM_Client) {
    GCon->Log("You can't save if you aren't playing!");
    return false;
  }

  if (sv.intermission) {
    GCon->Log("You can't save while in intermission!");
    return false;
  }

  return true;
}


static void BroadcastSaveText (const char *msg) {
  if (!msg || !msg[0]) return;
  if (sv_save_messages) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *plr = GGameInfo->Players[i];
      if (!plr) continue;
      if ((plr->PlayerFlags&VBasePlayer::PF_Spawned) == 0) continue;
      plr->eventClientPrint(msg);
    }
  } else {
    GCon->Log(msg);
  }
}


void SV_AutoSave () {
  if (!CheckIfSaveIsAllowed()) return;

  int aslot = SV_FindAutosaveSlot();
  if (!aslot) {
    BroadcastSaveText("Cannot find autosave slot (this should not happen)!");
    return;
  }

#ifdef CLIENT
  Draw_SaveIcon();
#endif

  TTimeVal tv;
  GetTimeOfDay(&tv);
  VStr svname = TimeVal2Str(&tv, true)+": "+VStr("AUTO: ")+(*GLevel->MapName);

  SV_SaveGame(aslot, svname);

  BroadcastSaveText(va("Game autosaved to slot #%d", -aslot));
}


#ifdef CLIENT
void SV_AutoSaveOnLevelExit () {
  if (!r_dbg_save_on_level_exit) return;

  if (!CheckIfSaveIsAllowed()) return;

  int aslot = SV_FindAutosaveSlot();
  if (!aslot) {
    BroadcastSaveText("Cannot find autosave slot (this should not happen)!");
    return;
  }

  Draw_SaveIcon();

  TTimeVal tv;
  GetTimeOfDay(&tv);
  VStr svname = TimeVal2Str(&tv, true)+": "+VStr("OUT: ")+(*GLevel->MapName);

  SV_SaveGame(aslot, svname);

  BroadcastSaveText(va("Game autosaved to slot #%d", -aslot));
}


//==========================================================================
//
//  COMMAND Save
//
//  Called by the menu task. Description is a 24 byte text string
//
//==========================================================================
COMMAND(Save) {
  guard(COMMAND Save)
  if (Args.Num() != 3) return;

  if (!CheckIfSaveIsAllowed()) return;

  if (Args[2].Length() >= 32) {
    BroadcastSaveText("Description too long!");
    return;
  }

  Draw_SaveIcon();

  SV_SaveGame(atoi(*Args[1]), Args[2]);

  BroadcastSaveText("Game saved.");
  unguard;
}


//==========================================================================
//
//  COMMAND DeleteSavedGame <slotidx|quick>
//
//==========================================================================
COMMAND(DeleteSavedGame) {
  guard(COMMAND DeleteSavedGame);

  //GCon->Logf("DeleteSavedGame: argc=%d", Args.length());

  if (Args.Num() != 2) return;

  if (!CheckIfLoadIsAllowed()) return;

  VStr numstr = Args[1];

  //GCon->Logf("DeleteSavedGame: <%s>", *numstr);

  if (numstr.ICmp("quick") == 0) {
    if (SV_DeleteSlotFile(QUICKSAVE_SLOT)) BroadcastSaveText("Quicksave deleted.");
    return;
  }

  int pos = 0;
  while (pos < numstr.length() && (vuint8)numstr[pos] <= ' ') ++pos;
  if (pos >= numstr.length()) return;

  bool neg = false;
  if (numstr[pos] == '-') {
    neg = true;
    ++pos;
    if (pos >= numstr.length()) return;
  }

  int slot = 0;
  while (pos < numstr.length()) {
    char ch = numstr[pos++];
    if (ch < '0' || ch > '9') return;
    slot = slot*10+ch-'0';
  }
  //GCon->Logf("DeleteSavedGame: slot=%d (neg=%d)", slot, (neg ? 1 : 0));
  if (slot < 0 || slot > 9) return;
  if (neg) slot = -slot;

  if (SV_DeleteSlotFile(slot)) {
    if (slot < 0) BroadcastSaveText(va("Autosave #%d deleted", -slot)); else BroadcastSaveText(va("Savegame #%d deleted", slot));
  }

  unguard;
}


//==========================================================================
//
//  COMMAND Load
//
//==========================================================================
COMMAND(Load) {
  guard(COMMAND Load);
  if (Args.Num() != 2) return;

  if (!CheckIfLoadIsAllowed()) return;

  int slot = atoi(*Args[1]);
  VStr desc;
  if (!SV_GetSaveString(slot, desc)) {
    BroadcastSaveText("Empty slot!");
    return;
  }
  GCon->Logf("Loading \"%s\"...", *desc);

  Draw_LoadIcon();
  SV_LoadGame(slot);
  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
  BroadcastSaveText(va("Loaded save \"%s\".", *desc));
  unguard;
}


//==========================================================================
//
//  COMMAND QuickSave
//
//==========================================================================
COMMAND(QuickSave) {
  guard(COMMAND QuickSave)

  if (!CheckIfSaveIsAllowed()) return;

  Draw_SaveIcon();

  SV_SaveGame(QUICKSAVE_SLOT, "quicksave");

  BroadcastSaveText("Game quicksaved.");
  unguard;
}


//==========================================================================
//
//  COMMAND QuickLoad
//
//==========================================================================
COMMAND(QuickLoad) {
  guard(COMMAND QuickLoad);

  if (!CheckIfLoadIsAllowed()) return;

  VStr desc;
  if (!SV_GetSaveString(QUICKSAVE_SLOT, desc)) {
    BroadcastSaveText("Empty quicksave slot");
    return;
  }
  GCon->Log("Loading quicksave...");

  Draw_LoadIcon();
  SV_LoadGame(QUICKSAVE_SLOT);
  // don't copy to reborn slot -- this is quickload after all!

  BroadcastSaveText("Quicksave loaded.");
  unguard;
}


//==========================================================================
//
//  COMMAND AutoSaveEnter
//
//==========================================================================
COMMAND(AutoSaveEnter) {
  guard(COMMAND AutoSaveEnter)

  if (!CheckIfSaveIsAllowed()) return;

  int aslot = SV_FindAutosaveSlot();
  if (!aslot) {
    BroadcastSaveText("Cannot find autosave slot (this should not happen)!");
    return;
  }

  Draw_SaveIcon();

  TTimeVal tv;
  GetTimeOfDay(&tv);
  VStr svname = TimeVal2Str(&tv, true)+": "+(*GLevel->MapName);

  SV_SaveGame(aslot, svname);

  BroadcastSaveText(va("Game autosaved to slot #%d", -aslot));
  unguard;
}


//==========================================================================
//
//  COMMAND AutoSaveLeave
//
//==========================================================================
COMMAND(AutoSaveLeave) {
  guard(COMMAND AutoSaveLeave)
  SV_AutoSaveOnLevelExit();
  unguard;
}
#endif
