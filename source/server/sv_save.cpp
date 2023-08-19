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
//**
//**  Archiving: SaveGame I/O.
//**
//**************************************************************************
//#define USE_OLD_SAVE_CODE

#ifdef USE_OLD_SAVE_CODE
# include "sv_save.old.cpp"
#else

//#define VXX_DEBUG_SECTION_READER
//#define VXX_DEBUG_SECTION_WRITER

#include "../gamedefs.h"
#include "../host.h"
#include "../psim/p_entity.h"
#include "../psim/p_worldinfo.h"
#include "../psim/p_levelinfo.h"
#include "../psim/p_playerreplicationinfo.h"
#include "../psim/p_player.h"
#include "../filesys/files.h"
#ifdef CLIENT
# include "../drawer.h"  /* VRenderLevelPublic */
# include "../client/client.h"
# include "../sound/sound.h"
#endif
#include "../utils/qs_data.h"
#include "../decorate/vc_decorate.h" /* ListLoaderCanSkipClass */
#include "server.h"
#include "sv_local.h"
#include "sv_save.h"

#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef _WIN32
//# if __GNUC__ < 6
static inline struct tm *localtime_r (const time_t *_Time, struct tm *_Tm) {
  return (localtime_s(_Tm, _Time) ? NULL : _Tm);
}
//# endif
#endif


// ////////////////////////////////////////////////////////////////////////// //
#define VAVOOM_LOADER_CAN_SKIP_CLASSES


// ////////////////////////////////////////////////////////////////////////// //
enum { NUM_AUTOSAVES = 9 };


// ////////////////////////////////////////////////////////////////////////// //
#define NEWFMT_VWAD_AUTHOR  "k8vavoom engine (save game)"

#define NEWFMT_FNAME_SAVE_HEADER   "k8vavoom_savegame_header.dat"
#define NEWFMT_FNAME_SAVE_DESCR    "description.dat"
#define NEWFMT_FNAME_SAVE_WADLIST  "wadlist.dat"
#define NEWFMT_FNAME_SAVE_HWADLIST "wadlist.txt"
#define NEWFMT_FNAME_SAVE_CURRMAP  "current_map.dat"
#define NEWFMT_FNAME_SAVE_MAPLIST  "maplist.dat"
#define NEWFMT_FNAME_SAVE_CPOINT   "checkpoint.dat"
#define NEWFMT_FNAME_SAVE_SKILL    "skill.dat"
#define NEWFMT_FNAME_SAVE_DATE     "datetime.dat"

#define NEWFMT_FNAME_MAP_STRTBL    "strings.dat"
#define NEWFMT_FNAME_MAP_NAMES     "names.dat"
#define NEWFMT_FNAME_MAP_ACS_EXPT  "acs_exports.dat"
#define NEWFMT_FNAME_MAP_WORDINFO  "worldinfo.dat"
#define NEWFMT_FNAME_MAP_ACTPLYS   "active_players.dat"
#define NEWFMT_FNAME_MAP_EXPOBJNS  "object_nameidx.dat"
#define NEWFMT_FNAME_MAP_THINKERS  "object_data.dat"
#define NEWFMT_FNAME_MAP_ACS_STATE "acs_state.dat"
//#define NEWFMT_FNAME_MAP_ACS_DATA  "acs_data.dat"
#define NEWFMT_FNAME_MAP_SOUNDS    "sounds.dat"
#define NEWFMT_FNAME_MAP_GINFO     "ginfo.dat"


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB dbg_save_on_level_exit("dbg_save_on_level_exit", false, "Save before exiting a level.\nNote that after loading this save you prolly won't be able to exit again.", CVAR_PreInit|CVAR_NoShadow/*|CVAR_Archive*/);
static VCvarB dbg_load_ignore_wadlist("dbg_load_ignore_wadlist", false, "Ignore list of loaded wads in savegame when hash mated?", CVAR_PreInit|CVAR_NoShadow/*|CVAR_Archive*/);
static VCvarB dbg_save_in_old_format("dbg_save_in_old_format", false, "Save games in old format instead of vwads?", CVAR_PreInit|CVAR_NoShadow/*|CVAR_Archive*/);

static VCvarI save_compression_level("save_compression_level", "1", "Save file compression level [0..3]", CVAR_Archive|CVAR_NoShadow);

static VCvarB sv_new_map_autosave("sv_new_map_autosave", true, "Autosave when entering new map (except first one)?", CVAR_PreInit|CVAR_NoShadow/*|CVAR_Archive*/);

static VCvarB sv_save_messages("sv_save_messages", true, "Show messages on save/load?", CVAR_Archive|CVAR_NoShadow);

//static VCvarB loader_recalc_z("loader_recalc_z", true, "Recalculate Z on load (this should help with some edge cases)?", CVAR_Archive|CVAR_NoShadow);
static VCvarB loader_ignore_kill_on_unarchive("loader_ignore_kill_on_unarchive", false, "Ignore 'Kill On Unarchive' flag when loading a game?", CVAR_PreInit|CVAR_NoShadow/*|CVAR_Archive*/);

static VCvarI dbg_save_verbose("dbg_save_verbose", "0", "Slightly more verbose save. DO NOT USE, THIS IS FOR DEBUGGING!\n  0x01: register skips player\n  0x02: registered object\n  0x04: skipped actual player write\n  0x08: skipped unknown object\n  0x10: dump object data writing\b  0x20: dump checkpoints", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);

static VCvarB dbg_checkpoints("dbg_checkpoints", false, "Checkpoint save/load debug dumps", CVAR_NoShadow);

extern VCvarB r_precalc_static_lights;
extern int r_precalc_static_lights_override; // <0: not set
extern VCvarB loader_cache_data;


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarI Skill;
static VCvarB sv_autoenter_checkpoints("sv_autoenter_checkpoints", true, "Use checkpoints for autosaves when possible?", CVAR_Archive|CVAR_NoShadow);

static VStr saveFileBase;


// ////////////////////////////////////////////////////////////////////////// //
#define QUICKSAVE_SLOT  (-666)

#define EMPTYSTRING  "Empty Slot"
#define MOBJ_NULL    (-1)

#define SAVE_DESCRIPTION_LENGTH    (24)
//#define SAVE_VERSION_TEXT_NO_DATE  "Version 1.34.4"
#define SAVE_VERSION_TEXT          "Version 1.34.12"
#define SAVE_VERSION_TEXT_LENGTH   (16)

static_assert(strlen(SAVE_VERSION_TEXT) <= SAVE_VERSION_TEXT_LENGTH, "oops");

#define SAVE_EXTDATA_ID_END      (0)
#define SAVE_EXTDATA_ID_DATEVAL  (1)
#define SAVE_EXTDATA_ID_DATESTR  (2)


// ////////////////////////////////////////////////////////////////////////// //
enum gameArchiveSegment_t {
  ASEG_MAP_HEADER = 101,
  ASEG_WORLD,
  ASEG_SCRIPTS,
  ASEG_SOUNDS,
  ASEG_END,
};


// extra gameslot data
enum {
  GSLOT_DATA_SKILL = 202,

  GSLOT_DATA_START = 667,
  GSLOT_DATA_END = 666,
};


// ////////////////////////////////////////////////////////////////////////// //
class VSavedCheckpoint {
public:
  struct EntityInfo {
    //vint32 index;
    VEntity *ent;
    VStr ClassName; // used only in loader
  };

public:
  TArray<QSValue> QSList;
  TArray<EntityInfo> EList;
  vint32 ReadyWeapon; // 0: none, otherwise entity index+1
  vint32 Skill; // -1 means "don't change"

  VSavedCheckpoint () : QSList(), EList(), ReadyWeapon(0), Skill(-1) {}
  ~VSavedCheckpoint () { Clear(); }

  void AddEntity (VEntity *ent) {
    vassert(ent);
    const int len = EList.length();
    for (int f = 0; f < len; ++f) {
      if (EList[f].ent == ent) return;
    }
    EntityInfo &ei = EList.alloc();
    //ei.index = EList.length()-1;
    ei.ent = ent;
    ei.ClassName = VStr(ent->GetClass()->GetName());
  }

  int FindEntity (VEntity *ent) const {
    if (!ent) return 0;
    const int len = EList.length();
    for (int f = 0; f < len; ++f) {
      if (EList[f].ent == ent) return f+1;
    }
    // the thing that should not be
    abort();
    return -1;
  }

  void Clear () {
    QSList.Clear();
    EList.Clear();
    ReadyWeapon = 0;
    Skill = -1; // this should be "no skill"
  }

  void Serialise (VStream *strm) {
    vassert(strm);
    // note that we cannot use `VNTValueIOEx` here, because names are already written!
    if (strm->IsLoading()) {
      // load players inventory
      Clear();
      // load ready weapon
      vint32 rw = 0;
      *strm << STRM_INDEX(rw);
      ReadyWeapon = rw;
      // load entity list
      vint32 entCount;
      *strm << STRM_INDEX(entCount);
      if (dbg_save_verbose&0x20) GCon->Logf("*** LOAD: rw=%d; entCount=%d", rw, entCount);
      if (entCount < 0 || entCount > 1024*1024) Host_Error("invalid entity count (%d)", entCount);
      for (int f = 0; f < entCount; ++f) {
        EntityInfo &ei = EList.alloc();
        ei.ent = nullptr;
        *strm << ei.ClassName;
        if (dbg_save_verbose&0x20) GCon->Logf("  ent #%d: '%s'", f+1, *ei.ClassName);
      }
      // load value list
      vint32 valueCount;
      *strm << STRM_INDEX(valueCount);
      if (dbg_save_verbose&0x20) GCon->Logf(" valueCount=%d", valueCount);
      for (int f = 0; f < valueCount; ++f) {
        QSValue &v = QSList.alloc();
        v.Serialise(*strm);
        // check for special values
        if (v.ent == nullptr) {
          if (v.name.strEqu("\x07 skill")) {
            if (v.type != QST_Int) Host_Error("invalid skill variable type in checkpoint save");
            Skill = v.ival;
            QSList.removeAt(QSList.length()-1);
            continue;
          }
        }
        if (dbg_save_verbose&0x20) GCon->Logf("  val #%d(%d): %s", f, v.objidx, *v.toString());
      }
      if (ReadyWeapon < 0 || ReadyWeapon > EList.length()) Host_Error("invalid ready weapon index (%d)", ReadyWeapon);
      if (dbg_checkpoints) GCon->Logf(NAME_Debug, "checkpoint data loaded (rw=%d; skill=%d)", ReadyWeapon, Skill);
    } else {
      // save players inventory
      // ready weapon
      vint32 rw = ReadyWeapon;
      *strm << STRM_INDEX(rw);
      // save entity list
      vint32 entCount = EList.length();
      *strm << STRM_INDEX(entCount);
      for (int f = 0; f < entCount; ++f) {
        EntityInfo &ei = EList[f];
        *strm << ei.ClassName;
      }
      const int addvalsCount = 1;
      // save value list
      vint32 valueCount = QSList.length()+addvalsCount; // one is reserved for skill
      *strm << STRM_INDEX(valueCount);
      // write special variables (only the skill for now)
      {
        QSValue v = QSValue::CreateInt(nullptr, "\x07 skill", Skill);
        v.Serialise(*strm);
      }
      for (auto &&v : QSList) v.Serialise(*strm);
      //GCon->Logf("*** SAVE: rw=%d; entCount=%d", rw, entCount);
    }
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class VSavedMap {
public:
  VName Name;
  int Index;
  // for old format
  // for new format, we are keeping map vwads here
  TArrayNC<vuint8> Data;
  // only for old format
  vuint8 Compressed;
  vint32 DecompressedSize;

public:
  VSavedMap (bool asNewFormat) : Compressed(asNewFormat ? 69 : 0), DecompressedSize(0) {}

  inline void ClearData (bool asNewFormat) {
    Data.clear();
    Compressed = (asNewFormat ? 69 : 0);
    DecompressedSize = 0;
  }

  //inline void SetNewFormat () noexcept { Compressed = 69; }
  inline bool IsNewFormat () const noexcept { return (Compressed == 69); }

  VStr GenVWadName () const {
    /*
    RIPEMD160_Ctx ctx;
    uint8_t hash[RIPEMD160_BYTES];
    ripemd160_init(&ctx);
    VStr n = VStr(Name);
    if (!n.IsEmpty()) {
      ripemd160_put(&ctx, *n, (unsigned)n.length());
    }
    ripemd160_finish(&ctx, hash);
    return "map_"+VStr::buf2hex(&hash, RIPEMD160_BYTES)+".vwad";
    */
    return va("map_%04d.vwad", Index);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
// save slot may contain several maps for hub saves
// also, some maps may be overwritten time and time again (hub)
// for new format, we will use subdir with vwads to store it all
class VSaveSlot {
public:
  VStr Description;
  VName CurrentMap;

  TArray<VSavedMap *> Maps; // if there are no maps, it is a checkpoint

  VSavedCheckpoint CheckPoint;
  vint32 SavedSkill; // -1: don't change/unknown

private:
  bool newFormat;

private:
  // doesn't destroy stream on error
  bool LoadSlotOld (int Slot, VStream *strm);
  // `vwad` should be set
  bool LoadSlotNew (int Slot, VVWadArchive *vwad);

  bool SaveToSlotOld (int Slot, VStr &savefilename);
  bool SaveToSlotNew (int Slot, VStr &savefilename);

public:
  VSaveSlot () : SavedSkill(-1), newFormat(true) {}
  ~VSaveSlot () { Clear(true); }

  inline bool IsNewFormat () const noexcept { return newFormat; }
  // DO NOT USE!
  inline void ForceNewFormat () noexcept { newFormat = true; }

  void Clear (bool asNewFormat);
  bool LoadSlot (int Slot);
  bool SaveToSlot (int Slot);

  VSavedMap *FindMap (VName Name);
};


// ////////////////////////////////////////////////////////////////////////// //
static VSaveSlot BaseSlot;


// ////////////////////////////////////////////////////////////////////////// //
class VStreamIOStrMapperWriter : public VStreamIOStrMapper {
public:
  TArray<VStr> strTable;
  TMap<VStr, int> strMap;

public:
  VV_DISABLE_COPY(VStreamIOStrMapperWriter)

  inline VStreamIOStrMapperWriter () { strTable.append(VStr::EmptyString); }
  virtual ~VStreamIOStrMapperWriter () override {}

  // interface functions for objects and classes streams
  virtual void io (VStream *strm, VStr &s) override {
    vint32 sidx;
    if (s.IsEmpty()) {
      sidx = 0;
    } else {
      auto kp = strMap.get(s);
      if (kp) {
        sidx = *kp;
      } else {
        sidx = strTable.length();
        if (sidx >= 1024 * 1024) {
          strm->SetError();
          return;
        }
        strTable.Append(s);
        strMap.put(s, sidx);
      }
    }
    //GCon->Logf(NAME_Debug, "WRSM: string #%d: <%s>", sidx, *s);
    *strm << STRM_INDEX(sidx);
  }

  void WriteStrings (VStream *st) {
    if (!st || st->IsError()) return;
    vint32 tlen = strTable.length();
    *st << tlen;
    // string #0 is always empty
    for (int f = 1; f < tlen; f += 1) {
      *st << strTable[f];
      if (st->IsError()) return;
    }
  }
};


class VStreamIOStrMapperLoader : public VStreamIOStrMapper {
public:
  TArray<VStr> strTable;

public:
  VV_DISABLE_COPY(VStreamIOStrMapperLoader)

  inline VStreamIOStrMapperLoader () {}
  virtual ~VStreamIOStrMapperLoader () override {}

  // interface functions for objects and classes streams
  virtual void io (VStream *strm, VStr &s) override {
    vint32 sidx = -1;
    *strm << STRM_INDEX(sidx);
    if (strm->IsError() || sidx < 0 || sidx >= strTable.length()) {
      strm->SetError();
    } else {
      s = strTable[sidx];
    }
    //GCon->Logf(NAME_Debug, "RDSM: string #%d: <%s>", sidx, *s);
  }

  void LoadStrings (VStream *strm) {
    strTable.clear();

    vint32 count = -1;
    *strm << count;
    if (strm->IsError() || count < 1 || count > 1024*1024) {
      strm->SetError();
      return;
    }

    // string #0 is always empty
    strTable.setLength(count);
    for (int f = 1; f < count; f += 1) {
      *strm << strTable[f];
      if (strm->IsError()) { strm->SetError(); return; }
    }
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class VSaveLoaderStream : public VStream {
private:
  VStream *Stream;
  VVWadArchive *vwad;
  VStreamIOStrMapperLoader *strMapper;

private:
  // extended sections support
  struct ExtSection {
    VStream *strm;
    VStr name;
  };
  TArray<ExtSection> esections;

  void WipeESections (bool setErr) {
    while (esections.length() != 0) {
      const int eidx = esections.length() - 1;
      VStream *strm = esections[eidx].strm;
      esections[eidx].strm = nullptr; esections[eidx].name.clear();
      esections.drop();
      if (strm) {
        if (setErr) strm->SetError();
        VStream::Destroy(strm);
      }
    }
  }

  inline VStream *GetCurrStream () const {
    return (esections.length() ? esections[esections.length() - 1].strm : Stream);
  }

private:
  void LoadStringTable () {
    if (!vwad->FileExists(NEWFMT_FNAME_MAP_STRTBL)) return;

    VStream *sst = vwad->OpenFile(NEWFMT_FNAME_MAP_STRTBL);
    if (!sst) { SetError(); return; }

    strMapper = new VStreamIOStrMapperLoader();
    strMapper->LoadStrings(sst);
    const bool err = sst->IsError();

    VStream::Destroy(sst);
    AttachStringMapper(strMapper);
    if (err) SetError();
  }

public:
  TArray<VName> NameRemap;
  TArray<VObject *> Exports;
  TArray<VLevelScriptThinker *> AcsExports;

public:
  VV_DISABLE_COPY(VSaveLoaderStream)

  VSaveLoaderStream (VStream *InStream)
    : Stream(InStream)
    , vwad(nullptr)
    , strMapper(nullptr)
  {
    bLoading = true;
  }

  VSaveLoaderStream (VVWadArchive *avwad)
    : Stream(nullptr)
    , vwad(avwad)
    , strMapper(nullptr)
  {
    bLoading = true;
    LoadStringTable();
  }

  virtual ~VSaveLoaderStream () override {
    AttachStringMapper(nullptr);
    Close();
    VStream::Destroy(Stream);
    delete strMapper;
  }

  inline bool IsNewFormat () const noexcept { return !!vwad; }

  // close current stream, open a new one, assign it to `Stream`
  void OpenFile (VStr name) {
    if (vwad) {
      if (Stream) {
        if (Stream->IsError()) SetError();
        VStream::Destroy(Stream);
      }
      Stream = vwad->OpenFile(name);
      if (!Stream) {
        // memleak!
        Host_Error("cannot find save part named '%s'", *name);
      } else {
        Stream->AttachStringMapper(strMapper);
      }
    } else {
      // what a brilliant error message! also, memleak
      //Host_Error("trying to read old save as new save (vfs: %s)", *name);
      // nope, this may be called for old saves
    }
  }

  // stream interface
  virtual bool IsExtendedFormat () const noexcept override {
    return IsNewFormat();
  }

  // opening non-existing section is error, so check if necessary
  // asking for empty name still returns `false`
  virtual bool HasExtendedSection (VStr name) override {
    if (IsNewFormat() && !IsError()) {
      if (name.IsEmpty()) return false;
      return vwad->FileExists(name);
    } else {
      return false;
    }
  }

  virtual bool OpenExtendedSection (VStr name, bool seekable) override {
    if (IsNewFormat() && !IsError()) {
      if (name.isEmpty()) {
        #ifdef VXX_DEBUG_SECTION_READER
        GCon->Log(NAME_Debug, "RDX: trying to open nameless section");
        #endif
        SetError();
      } else {
        // open new section stream
        ExtSection es;
        es.name = name;
        es.strm = vwad->OpenFile(name);
        if (es.strm == nullptr) {
          #ifdef VXX_DEBUG_SECTION_READER
          GCon->Logf(NAME_Debug, "RDX: cannot open section '%s'", *name);
          #endif
          SetError();
        } else {
          es.strm->AttachStringMapper(strMapper);
          #ifdef VXX_DEBUG_SECTION_READER
          GCon->Logf(NAME_Debug, "RDX: opened section '%s'", *name);
          #endif
          esections.Append(es);
        }
      }
    }
    return !IsError();
  }

  bool CloseExtendedSection () override {
    if (IsNewFormat() && !IsError()) {
      const int eslen = esections.length();
      if (eslen == 0) {
        #ifdef VXX_DEBUG_SECTION_READER
        GCon->Log(NAME_Debug, "RDX: trying to close non-opened extended section");
        #endif
        SetError();
      } else {
        VStream *strm = esections[eslen - 1].strm;
        esections.drop();
        if (strm->IsError() || !strm->Close()) {
          delete strm;
          SetError();
        } else {
          delete strm;
        }
      }
    }
    return !IsError();
  }

  virtual void SetError () override {
    WipeESections(true);
    VStream::Destroy(Stream);
    VStream::SetError();
  }

  virtual bool IsError () const noexcept override {
    if (bError) return true;
    VStream *s = GetCurrStream();
    return ((s && s->IsError()) || (Stream && Stream->IsError()));
  }

  virtual void Serialise (void *Data, int Len) override {
    VStream *s = GetCurrStream();
    if (s) {
      s->Serialise(Data, Len);
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
  }

  virtual void Seek (int Pos) override {
    VStream *s = GetCurrStream();
    if (s) {
      s->Seek(Pos);
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
  }

  virtual int Tell () override {
    VStream *s = GetCurrStream();
    int res = 0;
    if (s) {
      res = s->Tell();
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
    return res;
  }

  virtual int TotalSize () override {
    VStream *s = GetCurrStream();
    int res = 0;
    if (s) {
      res = s->TotalSize();
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
    return res;
  }

  virtual bool AtEnd () override {
    VStream *s = GetCurrStream();
    bool res = true;
    if (s) {
      res = s->AtEnd();
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
    return res;
  }

  virtual void Flush () override {
    VStream *s = GetCurrStream();
    if (s) {
      s->Flush();
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
  }

  virtual bool Close () override {
    bool err = IsError();
    WipeESections(err);
    err = IsError();
    if (Stream) {
      if (!err) err = !Stream->Close();
      VStream::Destroy(Stream);
    }
    if (vwad) {
      if (!err) vwad->Close();
      delete vwad; vwad = nullptr;
    }
    if (err) SetError(); // just in case
    return !err;
  }

  virtual void io (VSerialisable *&Ref) override {
    vint32 scpIndex;
    *this << STRM_INDEX(scpIndex);
    if (scpIndex == 0) {
      Ref = nullptr;
    } else {
      Ref = AcsExports[scpIndex-1];
    }
    //GCon->Logf("LOADING: VSerialisable<%s>(%p); idx=%d", (Ref ? *Ref->GetClassName() : "[none]"), (void *)Ref, scpIndex);
  }

  virtual void io (VName &Name) override {
    vint32 NameIndex;
    *this << STRM_INDEX(NameIndex);
    if (NameIndex < 0 || NameIndex >= NameRemap.length()) {
      //GCon->Logf(NAME_Error, "SAVEGAME: invalid name index %d (max is %d)", NameIndex, NameRemap.length()-1);
      Host_Error("SAVEGAME: invalid name index %d (max is %d)", NameIndex, NameRemap.length()-1);
    }
    Name = NameRemap[NameIndex];
  }

  virtual void io (VObject *&Ref) override {
    vint32 TmpIdx;
    *this << STRM_INDEX(TmpIdx);
    if (TmpIdx == 0) {
      Ref = nullptr;
    } else if (TmpIdx > 0) {
      if (TmpIdx > Exports.length()) Sys_Error("Bad index %d", TmpIdx);
      Ref = Exports[TmpIdx-1];
      //vassert(Ref);
      //GCon->Logf(NAME_Debug, "IO OBJECT: '%s'", Ref->GetClass()->GetName());
    } else {
      GCon->Logf(NAME_Warning, "LOAD: playerbase %d", -TmpIdx-1);
      Ref = GPlayersBase[-TmpIdx-1];
    }
  }

  // need to expose others too (all overloaded versions should be explicitly overriden)
  virtual void io (VStr &s) override {
    VStream::io(s);
  }

  virtual void io (VMemberBase *&o) override {
    VStream::io(o);
  }

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override {
    vint32 TmpIdx;
    *this << STRM_INDEX(TmpIdx);
    if (Struct->Name == "sector_t") {
      Ptr = (TmpIdx >= 0 ? &GLevel->Sectors[TmpIdx] : nullptr);
    } else if (Struct->Name == "line_t") {
      Ptr = (TmpIdx >= 0 ? &GLevel->Lines[TmpIdx] : nullptr);
    } else {
      if (developer) GCon->Logf(NAME_Warning, "Don't know how to handle pointer to %s", *Struct->Name);
      Ptr = nullptr;
    }
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class VSaveWriterStream : public VStream {
private:
  VVWadNewArchive *vwad;
  VStreamIOStrMapperWriter *strMapper;
  VStream *Stream;

private:
  // extended sections support
  struct ExtSection {
    VStream *strm;
    VStr name;
  };
  TArray<ExtSection> esections;

public:
  TArray<VName> Names;
  TArray<VObject *> Exports;
  TArray<vint32> NamesMap;
  TMapNC<vuint32, vint32> ObjectsMap; // key: object uid; value: internal index
  TArray</*VLevelScriptThinker*/VSerialisable *> AcsExports;
  bool skipPlayers;

private:
  inline VStream *GetCurrStream () const {
    return (esections.length() ? esections[esections.length() - 1].strm : Stream);
  }

  bool CloseLastESection () {
    bool res = false;
    const int eidx = esections.length() - 1;
    if (eidx >= 0) {
      VStream *strm = esections[eidx].strm;
      VStr name = esections[eidx].name;
      esections[eidx].strm = nullptr; esections[eidx].name.clear();
      esections.drop();
      if (strm) {
        if (IsError()) strm->SetError();
        // write it
        if (!strm->IsError()) {
          res = strm->Close();
          delete strm;
        }
      }
    }
    return res;
  }

  void WipeESections (bool setErr) {
    while (esections.length() != 0) {
      const int eidx = esections.length() - 1;
      VStream *strm = esections[eidx].strm;
      esections[eidx].strm = nullptr; esections[eidx].name.clear();
      esections.drop();
      if (strm) {
        if (setErr) strm->SetError();
        VStream::Destroy(strm);
      }
    }
  }

  // flush and destroy extended sections
  void FlushESections () {
    if (IsError()) { WipeESections(true); return; }
    if (!vwad) { vassert(esections.length() == 0); return; }
    while (esections.length() != 0) {
      if (!CloseLastESection()) {
        WipeESections(true);
        SetError();
      }
    }
  }

  void Init () {
    bLoading = false;
    NamesMap.setLength(VName::GetNumNames());
    for (int i = 0; i < VName::GetNumNames(); ++i) NamesMap[i] = -1;
  }

  static int GetCompressionLevel (VStr fname) {
    int level = save_compression_level.asInt();
    if (level < VWADWR_COMP_DISABLE || level > VWADWR_COMP_BEST) {
      level = VWADWR_COMP_FAST;
      save_compression_level = level;
    }
    // just in case
    if (fname.endsWithNoCase(".vwad")) level = VWADWR_COMP_DISABLE;
    return level;
  }

  // this automatically closes old file
  // it is safe to call this in non-vwad mode
  bool CreateFile (VStr name, bool buffit) {
    if (vwad) {
      if (Stream != nullptr) CloseFile();
      #if 0
      GCon->Logf(NAME_Debug, "CREATE: %s", *name);
      #endif
      if (!IsError()) {
        vassert(Stream == nullptr);
        while (name.startsWith("/")) name.chopLeft(1);
        if (buffit) {
          Stream = vwad->CreateFileBuffered(name, GetCompressionLevel(name));
        } else {
          Stream = vwad->CreateFileDirect(name, GetCompressionLevel(name));
        }
        if (Stream) {
          Stream->AttachStringMapper(strMapper);
        } else {
          SetError();
        }
      }
    }
    return !IsError();
  }


public:
  VV_DISABLE_COPY(VSaveWriterStream)

  // takes ownership of the passed stream
  VSaveWriterStream (VStream *InStream)
    : vwad(nullptr)
    , strMapper(nullptr)
    , Stream(InStream)
  {
    Init();
  }

  // takes ownership of the passed archive object
  // will properly close and destroy the archive
  VSaveWriterStream (VVWadNewArchive *avwad)
    : vwad(avwad)
    , strMapper(nullptr)
    , Stream(nullptr)
  {
    Init();
    #if 1
    strMapper = new VStreamIOStrMapperWriter();
    AttachStringMapper(strMapper);
    #endif
  }

  virtual ~VSaveWriterStream () override {
    Close();
    delete Stream; Stream = nullptr;
    delete strMapper; strMapper = nullptr;
  }

  inline bool IsNewFormat () const noexcept { return (vwad != nullptr); }

  // it is safe to call this in non-vwad mode
  void CloseFile () {
    if (vwad && Stream) {
      #if 0
      GCon->Logf(NAME_Debug, "CLOSE: %s", *Stream->GetName());
      #endif
      bool err = IsError();
      if (!err) {
        Stream->Close();
        err = Stream->IsError();
      }
      delete Stream; Stream = nullptr;
      if (err) SetError();
    }
  }

  // this automatically closes old file
  // it is safe to call this in non-vwad mode
  inline bool CreateFileBuffered (VStr name) { return CreateFile(name, true); }
  inline bool CreateFileDirect (VStr name) { return CreateFile(name, false); }

  // stream interface
  virtual bool IsExtendedFormat () const noexcept override {
    return IsNewFormat();
  }

  virtual bool OpenExtendedSection (VStr name, bool seekable) override {
    if (IsNewFormat() && !IsError()) {
      if (name.isEmpty()) {
        #ifdef VXX_DEBUG_SECTION_READER
        GCon->Log(NAME_Debug, "WRX: trying to open nameless section");
        #endif
        SetError();
      } else {
        int sidx = 0;
        while (sidx < esections.length() && !esections[sidx].name.strEquCI(name)) {
          sidx += 1;
        }
        if (sidx >= esections.length()) {
          // new section
          ExtSection es;
          es.name = name;
          if (seekable) {
            es.strm = vwad->CreateFileBuffered(name, GetCompressionLevel(name));
          } else {
            es.strm = vwad->CreateFileDirect(name, GetCompressionLevel(name));
          }
          if (es.strm == nullptr) {
            #ifdef VXX_DEBUG_SECTION_WRITER
            GCon->Logf(NAME_Debug, "WRX: error creating section '%s'", *name);
            #endif
            SetError();
          } else {
            #ifdef VXX_DEBUG_SECTION_WRITER
            GCon->Logf(NAME_Debug, "WRX: created section '%s'", *name);
            #endif
            esections.Append(es);
          }
        } else {
          #ifdef VXX_DEBUG_SECTION_READER
          GCon->Logf(NAME_Debug, "WRX: trying to opened duplicate section '%s'", *name);
          #endif
          SetError();
        }
      }
    }
    return !IsError();
  }

  bool CloseExtendedSection () override {
    if (IsNewFormat() && !IsError()) {
      if (!CloseLastESection()) {
        WipeESections(true);
        SetError();
      }
    }
    return !IsError();
  }

  virtual void SetError () override {
    VStream::Destroy(Stream);
    WipeESections(true);
    if (vwad) { delete vwad; vwad = nullptr; }
    delete strMapper; strMapper = nullptr;
    VStream::SetError();
  }

  virtual bool IsError () const noexcept override {
    if (bError) return true;
    VStream *s = GetCurrStream();
    return ((s && s->IsError()) || (Stream && Stream->IsError()));
  }

  virtual bool Close () override {
    bool err = IsError();
    if (vwad) {
      if (!err && Stream) {
        Stream->Close();
        err = Stream->IsError();
      }
      delete Stream; Stream = nullptr;
      if (err) {
        WipeESections(true);
      } else {
        FlushESections();
        err = IsError();
      }
      if (!err) {
        if (strMapper) {
          int level = save_compression_level.asInt();
          if (level < VWADWR_COMP_DISABLE || level > VWADWR_COMP_BEST) {
            level = VWADWR_COMP_FAST;
            save_compression_level = level;
          }
          VStream *wo = vwad->CreateFileDirect(NEWFMT_FNAME_MAP_STRTBL, level);
          if (wo) {
            strMapper->WriteStrings(wo);
            err = !wo->Close();
            VStream::Destroy(wo);
          } else {
            err = true;
          }
        }
        if (!err) err = !vwad->Close();
      }
      delete vwad; vwad = nullptr;
    } else {
      if (Stream) { err = !Stream->Close(); Stream = nullptr; }
    }
    if (err) SetError(); // just in case
    return !err;
  }

  virtual void Serialise (void *Data, int Len) override {
    VStream *s = GetCurrStream();
    if (s) {
      s->Serialise(Data, Len);
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
  }

  virtual void Seek (int Pos) override {
    VStream *s = GetCurrStream();
    if (s) {
      s->Seek(Pos);
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
  }

  virtual int Tell () override {
    VStream *s = GetCurrStream();
    int res = 0;
    if (s) {
      res = s->Tell();
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
    return res;
  }

  virtual int TotalSize () override {
    VStream *s = GetCurrStream();
    int res = 0;
    if (s) {
      res = s->TotalSize();
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
    return res;
  }

  virtual bool AtEnd () override {
    VStream *s = GetCurrStream();
    bool res = true;
    if (s) {
      res = s->AtEnd();
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
    return res;
  }

  virtual void Flush () override {
    VStream *s = GetCurrStream();
    if (s) {
      s->Flush();
      if (s->IsError()) SetError();
    } else {
      SetError();
    }
  }

  void RegisterObject (VObject *o) {
    if (!o) return;
    if (ObjectsMap.has(o->GetUniqueId())) return;
    if (skipPlayers) {
      VEntity *mobj = Cast<VEntity>(o);
      if (mobj != nullptr && (mobj->EntityFlags&VEntity::EF_IsPlayer)) {
        // skipping player mobjs
        if (dbg_save_verbose&0x01) {
          GCon->Logf("*** SKIP(0) PLAYER MOBJ: <%s>", *mobj->GetClass()->GetFullName());
        }
        return;
      }
    }
    if (dbg_save_verbose&0x02) {
      GCon->Logf("*** unique object (%u : %s)", o->GetUniqueId(), *o->GetClass()->GetFullName());
    }
    Exports.Append(o);
    ObjectsMap.put(o->GetUniqueId(), Exports.length());
  }

  virtual void io (VSerialisable *&Ref) override {
    vint32 scpIndex = 0;
    if (Ref) {
      if (Ref->GetClassName() != "VAcs") {
        Host_Error("trying to save unknown serialisable of class `%s`", *Ref->GetClassName());
      }
      while (scpIndex < AcsExports.length() && AcsExports[scpIndex] != Ref) ++scpIndex;
      if (scpIndex >= AcsExports.length()) {
        scpIndex = AcsExports.length();
        AcsExports.append(Ref);
      }
      ++scpIndex;
    }
    //GCon->Logf("SAVING: VSerialisable<%s>(%p); idx=%d", (Ref ? *Ref->GetClassName() : "[none]"), (void *)Ref, scpIndex);
    *this << STRM_INDEX(scpIndex);
  }

  virtual void io (VName &Name) override {
    int nidx = Name.GetIndex();
    const int olen = NamesMap.length();
    if (olen <= nidx) {
      NamesMap.setLength(nidx+1);
      for (int f = olen; f <= nidx; ++f) NamesMap[f] = -1;
    }
    if (NamesMap[nidx] == -1) NamesMap[nidx] = Names.Append(Name);
    *this << STRM_INDEX(NamesMap[nidx]);
  }

  virtual void io (VObject *&Ref) override {
    vint32 TmpIdx;
    if (!Ref /*|| !Ref->IsGoingToDie()*/) {
      TmpIdx = 0;
    } else {
      //TmpIdx = ObjectsMap[Ref->GetObjectIndex()];
      auto ppp = ObjectsMap.get(Ref->GetUniqueId());
      if (!ppp) {
        if (skipPlayers) {
          VEntity *mobj = Cast<VEntity>(Ref);
          if (mobj != nullptr && (mobj->EntityFlags&VEntity::EF_IsPlayer)) {
            // skipping player mobjs
            if (dbg_save_verbose&0x04) {
              GCon->Logf("*** SKIP(1) PLAYER MOBJ: <%s> -- THIS IS HARMLESS", *mobj->GetClass()->GetFullName());
            }
            TmpIdx = 0;
            *this << STRM_INDEX(TmpIdx);
            return;
          }
        }
        if ((dbg_save_verbose&0x08) /*|| true*/) {
          GCon->Logf("*** unknown object (%u : %s) -- THIS IS HARMLESS", Ref->GetUniqueId(), *Ref->GetClass()->GetFullName());
        }
        TmpIdx = 0; // that is how it was done in the previous version of the code
      } else {
        TmpIdx = *ppp;
      }
      //TmpIdx = *ObjectsMap.get(Ref->GetUniqueId());
    }
    *this << STRM_INDEX(TmpIdx);
  }

  // need to expose others too (all overloaded versions should be explicitly overriden)
  virtual void io (VStr &s) override {
    VStream::io(s);
  }

  // this should not be used (because VObject takes care of serialising its contents)
  virtual void io (VMemberBase *&o) override {
    #if 0
    if (o == nullptr) {
      GCon->Log(NAME_Debug, "writing `none` VMember");
    } else {
      GCon->Logf(NAME_Debug, "writing VMember `%s` from %s",
                 o->GetName(), *o->Loc.toStringNoCol());
    }
    #endif
    VStream::io(o);
  }

  virtual void SerialiseStructPointer (void *&Ptr, VStruct *Struct) override {
    vint32 TmpIdx;
    if (Struct->Name == "sector_t") {
      TmpIdx = (Ptr ? (int)((sector_t *)Ptr-GLevel->Sectors) : -1);
    } else if (Struct->Name == "line_t") {
      TmpIdx = (Ptr ? (int)((line_t *)Ptr-GLevel->Lines) : -1);
    } else {
      if (developer) GCon->Logf(NAME_Dev, "Don't know how to handle pointer to %s", *Struct->Name);
      TmpIdx = -1;
    }
    *this << STRM_INDEX(TmpIdx);
  }
};


// because dedicated server cannot save games yet
#ifdef CLIENT
static bool skipCallbackInited = false;
static VName oldPlrClassName = NAME_None;
static VName newPlrClassName = NAME_None;
static VName clsPlayerExName = NAME_None;


//==========================================================================
//
//  checkSkipClassCB
//
//==========================================================================
static bool checkSkipClassCB (VObject *self, VName clsname) {
  // as gore mod is build into the engine now, we don't need to subclass `VLevel` anymore
  // this allows loading of old saves: all old gore mod data will be simply ignored
  if (clsname == NAME_Level_K8BDW || clsname == NAME_Level_K8Gore) {
    // allow any level descendant
    for (VClass *cls = self->GetClass(); cls; cls = cls->GetSuperClass()) {
      //if (VStr::strEqu(cls->GetName(), "VLevel")) return true;
      if (cls->GetVName() == NAME_VLevel) {
        GCon->Logf(NAME_Debug, "VLevel subclass `%s` replaced with `%s` (this is normal gore fix).",
                   *clsname, self->GetClass()->GetName());
        return true;
      }
    }
    return false;
  }

  //TODO: skip any Gore Mod related things?
  if (VStr::strEqu(*clsname, "K8Gore_Blood_SplatterReplacer")) {
    // K8Gore_Blood_SplatterReplacer (it is removed)
    for (VClass *cls = self->GetClass(); cls; cls = cls->GetSuperClass()) {
      if (VStr::strEqu(cls->GetName(), "K8Gore_Blood")) return true;
      //if (VStr::strEqu(cls->GetName(), "K8Gore_BloodBase")) return true;
    }
  }

  if (ListLoaderCanSkipClass.has(clsname)) return true;

  return false;
}


//==========================================================================
//
//  ldrTranslatePlayerClassName
//
//==========================================================================
static VName ldrTranslatePlayerClassName (VObject *self, VName clsname) {
  if (clsname != oldPlrClassName) return NAME_None;
  // check if it is a descendant of "PlayerEx"
  for (VClass *cls = self->GetClass(); cls; cls = cls->GetSuperClass()) {
    if (cls->GetVName() == clsPlayerExName) {
      GCon->Logf(NAME_Debug, "PlayerEx subclass `%s` translated to `%s` (this is normal loader fix).",
                 *clsname, *newPlrClassName);
      return newPlrClassName;
    }
  }
  GCon->Logf(NAME_Debug, "class `%s` is not a child of `PlayerEx`, skipped translation (this is normal loader fix).",
             *clsname);
  return NAME_None;
}


//==========================================================================
//
//  SV_SetupSkipCallback
//
//==========================================================================
static void SV_SetupSkipCallback () {
  if (skipCallbackInited) return;
  skipCallbackInited = true;
  VObject::CanSkipReadingClassCBList.append(&checkSkipClassCB);
  // translate `Player` to `K8VPlayer`
  oldPlrClassName = VName("Player");
  newPlrClassName = VName("K8VPlayer");
  clsPlayerExName = VName("PlayerEx");
  VObject::ClassNameTranslationCBList.append(&ldrTranslatePlayerClassName);
  // translate old `Level` class to `VLevel`
  // nope, don't do that
  //VObject::IOClassNameTranslation.put(VName("Level"), NAME_VLevel);
}
#endif


//==========================================================================
//
//  SV_GetSavesDir
//
//==========================================================================
static VStr SV_GetSavesDir () {
  return FL_GetSavesDir();
}


//==========================================================================
//
//  GetSaveSlotDirectoryPrefixOld
//
//==========================================================================
static VStr GetSaveSlotCommonDirectoryPrefixOld0 () {
  vuint32 hash;
  (void)SV_GetModListHashOld(&hash);
  VStr pfx = VStr::buf2hex(&hash, 4);
  return pfx;
}


//==========================================================================
//
//  GetSaveSlotCommonDirectoryPrefixOld1
//
//==========================================================================
static VStr GetSaveSlotCommonDirectoryPrefixOld1 () {
  vuint64 hash = SV_GetModListHashOld(nullptr);
  VStr pfx = VStr::buf2hex(&hash, 8);
  return pfx;
}


//==========================================================================
//
//  GetSaveSlotDirectoryPrefix
//
//==========================================================================
static VStr GetSaveSlotCommonDirectoryPrefix () {
  vuint64 hash = SV_GetModListHash(nullptr);
  VStr pfx = VStr::buf2hex(&hash, 8);
  return pfx;
}


//==========================================================================
//
//  UpgradeSaveDirectories
//
//  rename old save directory
//
//==========================================================================
static void UpgradeSaveDirectories () {
  VStr xdir = SV_GetSavesDir();
  VStr newpath = xdir.appendPath(GetSaveSlotCommonDirectoryPrefix());

  if (!newpath.IsEmpty()) {
    VStr oldpath;

    oldpath = xdir.appendPath(GetSaveSlotCommonDirectoryPrefixOld0());
    //GCon->Logf("OLD0: <%s> -> <%s>", *oldpath, *newpath);
    if (!oldpath.IsEmpty() && oldpath != newpath) rename(*oldpath, *newpath);

    oldpath = xdir.appendPath(GetSaveSlotCommonDirectoryPrefixOld1());
    //GCon->Logf("OLD1: <%s> -> <%s>", *oldpath, *newpath);
    if (!oldpath.IsEmpty() && oldpath != newpath) rename(*oldpath, *newpath);
  }
}


//==========================================================================
//
//  GetDiskSavesPath
//
//==========================================================================
static VStr GetDiskSavesPath () {
  UpgradeSaveDirectories();
  return SV_GetSavesDir().appendPath(GetSaveSlotCommonDirectoryPrefix());
}


//==========================================================================
//
//  GetDiskSavesPathNoHash
//
//==========================================================================
static VStr GetDiskSavesPathNoHash () {
  UpgradeSaveDirectories();
  return SV_GetSavesDir();
}


//==========================================================================
//
//  SV_GetSaveHash
//
//  returns hash of savegame directory
//
//==========================================================================
VStr SV_GetSaveHash () {
  return GetSaveSlotCommonDirectoryPrefix();
}


//==========================================================================
//
//  isBadSlotIndex
//
//  checking for "bad" index is more common
//
//==========================================================================
static inline bool isBadSlotIndex (int slot) {
  return (slot != QUICKSAVE_SLOT && (slot < -64 || slot > 63));
}


//==========================================================================
//
//  UpdateSaveDirWadList
//
//  writes text file with list of active wads.
//  this file is not used by the engine, and is written solely for user.
//
//==========================================================================
static void UpdateSaveDirWadList () {
  VStr svpfx = GetDiskSavesPath();
  FL_CreatePath(svpfx); // just in case
  //GCon->Logf(NAME_Debug, "UpdateSaveDirWadList: svpfx=<%s>", *svpfx);
  svpfx = svpfx.appendPath("wadlist.txt");
  VStream *res = FL_OpenSysFileWrite(svpfx);
  if (res) {
    res->writef("%s\n", "# automatically generated, and purely informational");
    auto wadlist = FL_GetWadPk3ListSmall();
    for (auto &&wadname : wadlist) {
      //GCon->Logf(NAME_Debug, "  wad=<%s>", *wadname);
      res->writef("%s\n", *wadname);
    }
    VStream::Destroy(res);
  }
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
  if (isBadSlotIndex(slot)) return VStr();
  VStr pfx = GetSaveSlotCommonDirectoryPrefix();
  if (slot == QUICKSAVE_SLOT) pfx += "/quicksave";
  else if (slot < 0) pfx += va("/autosave_%02d", -slot);
  else pfx += va("/normsave_%02d", slot+1);
  return pfx;
}


//==========================================================================
//
//  SV_OpenSlotFileRead
//
//  open savegame slot file if it exists
//  sets `saveFileBase`
//
//==========================================================================
static VStream *SV_OpenSlotFileRead (int slot) {
  saveFileBase.clear();
  if (isBadSlotIndex(slot)) return nullptr;

  VStr seenVWad, seenVsg;

  // search save subdir
  auto svdir = GetDiskSavesPath();
  auto dir = Sys_OpenDir(svdir);
  if (dir) {
    auto svpfx = GetSaveSlotBaseFileName(slot).extractFileName();
    for (;;) {
      VStr fname = Sys_ReadDir(dir);
      if (fname.isEmpty()) break;
      if (fname.startsWithNoCase(svpfx)) {
        if (seenVsg.isEmpty() && fname.endsWithNoCase(".vsg")) seenVsg = fname;
        else if (seenVWad.isEmpty() && fname.endsWithNoCase(".vwad")) seenVWad = fname;
        if (!seenVWad.isEmpty()) break;
      }
    }
    Sys_CloseDir(dir);
    if (!seenVWad.isEmpty()) {
      saveFileBase = svdir.appendPath(seenVWad);
    } else if (!seenVsg.isEmpty()) {
      saveFileBase = svdir.appendPath(seenVsg);
    } else {
      saveFileBase.clear();
    }
    if (!saveFileBase.isEmpty()) {
      VStream *st = FL_OpenSysFileRead(saveFileBase);
      if (st != nullptr) return st;
      saveFileBase.clear();
    }
  }

  return nullptr;
}


//==========================================================================
//
//  SV_OpenSlotFileReadWithFmt
//
//  sets `arc` to `nullptr` for old-style saves
//  old format header is skipped
//
//  return `nullptr` on error, otherwise a stream
//  WARNING! returned stream is owned by `arc`, if `arc` is not `nullptr`!
//
//==========================================================================
static VStream *SV_OpenSlotFileReadWithFmt (int Slot, VVWadArchive *&vwad) {
  VStream *Strm = nullptr;
  char VersionText[SAVE_VERSION_TEXT_LENGTH+1];
  vwad = nullptr;

  #if 0
  GCon->Logf(NAME_Debug, "SV_OpenSlotFileReadWithFmt: Slot=%d...", Slot);
  #endif
  Strm = SV_OpenSlotFileRead(Slot);

  if (Strm) {
    #if 0
    GCon->Log(NAME_Debug, "...checking");
    #endif

    // is it a vwad?
    memset(VersionText, 0, 4);

    Strm->Serialise(VersionText, 4);
    if (Strm->IsError()) {
      VStream::Destroy(Strm);
      saveFileBase.clear();
      return nullptr;
    }

    if (memcmp(VersionText, "VWAD", 4) == 0) {
      #if 0
      GCon->Log(NAME_Debug, "....VWAD");
      #endif
      Strm->Seek(0);
      if (Strm->IsError()) { VStream::Destroy(Strm); return nullptr; }
      vwad = new VVWadArchive(VStr(va("slot_%02d_main", Slot)), Strm, true);
      if (!vwad->IsOpen()) {
        // the stream is already destroyed
        delete vwad; vwad = nullptr;
        saveFileBase.clear();
        return nullptr;
      }
      // check author
      if (vwad->GetAuthor() != NEWFMT_VWAD_AUTHOR) {
        #if 0
        GCon->Log(NAME_Debug, "invalid save VWAD signature");
        #endif
        // the stream will be destroyed automatically
        delete vwad; vwad = nullptr;
        saveFileBase.clear();
        return nullptr;
      }
    } else {
      #if 0
      GCon->Log(NAME_Debug, "....VSG!");
      #endif
      vassert(SAVE_VERSION_TEXT_LENGTH > 4);
      memset(VersionText + 4, 0, sizeof(VersionText) - 4);
      // check the version text
      Strm->Serialise(VersionText + 4, SAVE_VERSION_TEXT_LENGTH - 4);
      if (VStr::Cmp(VersionText, SAVE_VERSION_TEXT) /*&& VStr::Cmp(VersionText, SAVE_VERSION_TEXT_NO_DATE)*/) {
        VStream::Destroy(Strm);
        saveFileBase.clear();
        return nullptr;
      }
    }
  }

  return Strm;
}


//==========================================================================
//
//  removeSlotSaveFiles
//
//  user can rename file to different case
//  kill 'em all!
//
//==========================================================================
static bool removeSlotSaveFiles (int slot, VStr keepFileName) {
  TArray<VStr> tokill;
  if (isBadSlotIndex(slot)) return false;

  // search save subdir
  auto svdir = GetDiskSavesPath();
  auto dir = Sys_OpenDir(svdir);
  if (dir) {
    auto svpfx = GetSaveSlotBaseFileName(slot).extractFileName();
    for (;;) {
      VStr fname = Sys_ReadDir(dir);
      if (fname.isEmpty()) break;
      if (fname.startsWithNoCase(svpfx) &&
          (fname.endsWithNoCase(".vsg") || fname.endsWithNoCase(".vwad") ||
           fname.endsWithNoCase(".lmap")))
      {
        VStr fn = svdir.appendPath(fname);
        if (fn != keepFileName) tokill.append(fn);
      } else if (fname.endsWith(".$$$")) {
        // various broken temp saves
        VStr fn = svdir.appendPath(fname);
        tokill.append(fn);
      }
    }
    Sys_CloseDir(dir);
  }

  for (int f = 0; f < tokill.length(); ++f) Sys_FileDelete(tokill[f]);
  return true;
}


//==========================================================================
//
//  SV_CreateSlotFileWrite
//
//  create new savegame slot file
//  also, removes any existing savegame file for the same slot
//  sets `saveFileBase`
//
//  returned stream name should be proper disk file name
//  it is temporary, and should be renamed on success
//
//==========================================================================
static VStream *SV_CreateSlotFileWrite (int slot, VStr descr, bool asNew) {
  saveFileBase.clear();
  if (isBadSlotIndex(slot)) return nullptr;
  if (slot == QUICKSAVE_SLOT) descr = VStr();

  auto svpfx = GetDiskSavesPathNoHash().appendPath(GetSaveSlotBaseFileName(slot));
  FL_CreatePath(svpfx.extractFilePath()); // just in case

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
  if (newdesc.length()) { svpfx += "_"; svpfx += newdesc; }

  if (asNew) svpfx += ".vwad"; else svpfx += ".vsg";
  //GCon->Logf(NAME_Debug, "SAVE: <%s>", *svpfx);
  saveFileBase = svpfx;
  VStream *res = FL_OpenSysFileWrite(svpfx + ".$$$");
  /*
  if (res) {
    VStream::Destroy(res);
    removeSlotSaveFiles(slot);
    res = FL_OpenSysFileWrite(svpfx);
    if (res) UpdateSaveDirWadList();
  }
  */
  return res;
}


//==========================================================================
//
//  SV_SaveFailed
//
//  call this to properly close the stream, and remove temp file
//
//==========================================================================
static void SV_SaveFailed (VStr fname, int Slot) {
  vassert(!fname.IsEmpty());
  unlink(*fname);
}


//==========================================================================
//
//  SV_SaveSuccess
//
//  call this to properly close the stream, and rename temp file
//
//==========================================================================
static void SV_SaveSuccess (VStr fname, int Slot) {
  vassert(!fname.IsEmpty());
  vassert(!saveFileBase.IsEmpty());
  if (fname != saveFileBase) {
    #ifdef WIN32
    unlink(*saveFileBase);
    #endif
    if (rename(*fname, *saveFileBase) != 0) {
      GCon->Logf(NAME_Error, "Cannot rename save file for slot #%d!", Slot);
      return;
    }
  }
  removeSlotSaveFiles(Slot, saveFileBase);
  UpdateSaveDirWadList();
}


#ifdef CLIENT
//==========================================================================
//
//  SV_DeleteSlotFile
//
//==========================================================================
static bool SV_DeleteSlotFile (int slot) {
  if (isBadSlotIndex(slot)) return false;
  return removeSlotSaveFiles(slot, VStr::EmptyString);
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
#ifndef STK_TIMET_FIX
  if (localtime_r(&tv.tv_sec, &ctm)) {
#else
  time_t tsec = tv.tv_sec;
  if (localtime_r(&tsec, &ctm)) {
#endif
    if (forAutosave) {
      // for autosave
      return VStr(va("%02d:%02d", (int)ctm.tm_hour, (int)ctm.tm_min));
    } else {
      // full
      return VStr(va("%04d/%02d/%02d %02d:%02d:%02d",
        (int)(ctm.tm_year+1900),
        (int)ctm.tm_mon+1,
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
      Strm->Serialise(&tv, sizeof(tv));
      continue;
    }

    if (id == SAVE_EXTDATA_ID_DATESTR && size > 0 && size < 64) {
      char buf[65];
      memset(buf, 0, sizeof(buf));
      Strm->Serialise(buf, size);
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
      Strm->Serialise(tv, sizeof(tv));
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
//  VSaveSlot::Clear
//
//==========================================================================
void VSaveSlot::Clear (bool asNewFormat) {
  Description.Clean();
  CurrentMap = NAME_None;
  SavedSkill = -1;
  for (int i = 0; i < Maps.length(); ++i) { delete Maps[i]; Maps[i] = nullptr; }
  Maps.Clear();
  CheckPoint.Clear();
  //if (vwad) { delete vwad; vwad = nullptr; }
  newFormat = asNewFormat;
}


//==========================================================================
//
//  CheckWadCompName
//
//==========================================================================
static bool CheckWadCompName (VStr s, VStr wl) {
  if (s == wl) return true;
  if (s.strEquCI(wl)) return true;
  VStr sext = s.ExtractFileExtension();
  if (!s.strEquCI(".pk3") && !s.strEquCI(".vwad")) return false;
  VStr wext = wl.ExtractFileExtension();
  if (!wext.strEquCI(".pk3") && !wext.strEquCI(".vwad")) return false;
  s = s.StripExtension();
  wl = wl.StripExtension();
  return s.strEquCI(wl);
}


//==========================================================================
//
//  CheckModList
//
//  load mod list, and compare with the current one
//
//==========================================================================
static bool CheckModList (VStream *Strm, int Slot, bool oldFormat=false, bool silent=false) {
  vassert(Strm != nullptr);

  vint32 wcount = -1;
  *Strm << wcount;

  if (Strm->IsError()) {
    if (!silent) {
      GCon->Logf(NAME_Error, "Invalid savegame #%d (error reading modlist)", Slot);
    }
    return !(oldFormat || !dbg_load_ignore_wadlist.asBool());
  }

  if (wcount < 1 || wcount > 8192) {
    if (!silent) {
      GCon->Logf(NAME_Error, "Invalid savegame #%d (bad number of mods: %d)", Slot, wcount);
    }
    return false;
  }

  TArray<VStr> xwadlist;
  for (int f = 0; f < wcount; ++f) {
    VStr s;
    *Strm << s;
    if (Strm->IsError()) {
      if (!silent) {
        GCon->Logf(NAME_Error, "Invalid savegame #%d (error reading modlist)", Slot);
      }
      return !(oldFormat || !dbg_load_ignore_wadlist.asBool());
    }
    xwadlist.Append(s);
  }

  if (!dbg_load_ignore_wadlist.asBool()) {
    bool ok = false;
    auto wadlist = FL_GetWadPk3List();

    if (wcount == wadlist.length()) {
      ok = true;
      for (int f = 0; ok && f < wcount; ++f) ok = CheckWadCompName(xwadlist[f], wadlist[f]);
    }

    if (!ok) {
      auto wadlistNew = FL_GetWadPk3ListSmall();
      if (wcount == wadlistNew.length()) {
        ok = true;
        for (int f = 0; ok && f < wcount; ++f) ok = CheckWadCompName(xwadlist[f], wadlistNew[f]);
      }
    }

    if (!ok) {
      if (!silent) {
        GCon->Logf(NAME_Error, "Invalid savegame #%d (bad modlist)", Slot);
      }
      return false;
    }
  }

  return true;
}


//==========================================================================
//
//  VSaveSlot::LoadSlotOld
//
//  doesn't destroy stream on error
//
//==========================================================================
bool VSaveSlot::LoadSlotOld (int Slot, VStream *Strm) {
  Clear(false);

  *Strm << Description;
  if (Strm->IsError()) return false;

  // skip extended data
  if (true/*VStr::Cmp(VersionText, SAVE_VERSION_TEXT) == 0*/) {
    if (!SkipExtData(Strm) || Strm->IsError()) {
      return false;
    }
  }

  // check list of loaded modules
  if (!CheckModList(Strm, Slot, true)) return false;

  VStr TmpName;
  *Strm << TmpName;
  CurrentMap = *TmpName;

  vint32 NumMaps;
  *Strm << STRM_INDEX(NumMaps);
  for (int i = 0; i < NumMaps; ++i) {
    VSavedMap *Map = new VSavedMap(false);
    Maps.Append(Map);
    Map->Index = Maps.length() - 1;
    vassert(Map->Index == i);
    vassert(!Map->IsNewFormat());
    vint32 DataLen;
    *Strm << TmpName << Map->Compressed << STRM_INDEX(Map->DecompressedSize) << STRM_INDEX(DataLen);
    Map->Name = *TmpName;
    Map->Data.setLength(DataLen);
    Strm->Serialise(Map->Data.Ptr(), Map->Data.length());
    #if 0
    GCon->Logf(NAME_Debug, "Map #%d: %s (cp:%d; ds:%d; uds:%d)", i, *Map->Name,
               Map->Compressed, DataLen, Map->DecompressedSize);
    #endif
  }

  //HACK: if `NumMaps` is 0, we're loading a checkpoint
  if (NumMaps == 0) {
    // load players inventory
    VSavedCheckpoint &cp = CheckPoint;
    cp.Serialise(Strm);
    SavedSkill = cp.Skill;
  } else {
    VSavedCheckpoint &cp = CheckPoint;
    cp.Clear();
    if (!Strm->AtEnd()) {
      vuint32 seg = 0xffffffff;
      *Strm << STRM_INDEX_U(seg);
      if (!Strm->IsError() && seg == GSLOT_DATA_START) {
        for (;;) {
          seg = 0xffffffff;
          *Strm << STRM_INDEX_U(seg);
          if (Strm->IsError()) break;
          if (seg == GSLOT_DATA_END) break;
          if (seg == GSLOT_DATA_SKILL) {
            vint32 sk;
            *Strm << STRM_INDEX(sk);
            if (sk < 0 || sk > 31) {
              GCon->Logf(NAME_Warning, "Invalid savegame #%d skill (%d)", Slot, sk);
            } else {
              SavedSkill = sk;
            }
            continue;
          }
          GCon->Logf(NAME_Warning, "Invalid savegame #%d extra segment (%u)", Slot, seg);
          return false;
        }
      }
    }
  }

  bool err = Strm->IsError();

  Host_ResetSkipFrames();

  if (err) {
    GCon->Logf(NAME_Error, "Error loading savegame #%d data", Slot);
    return false;
  }

  vassert(!saveFileBase.isEmpty());
  return true;
}


//==========================================================================
//
//  VSaveSlot::LoadSlotNew
//
//  `vwad` should be set
//
//==========================================================================
bool VSaveSlot::LoadSlotNew (int Slot, VVWadArchive *vwad) {
  Clear(true);

  VStream *Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_HEADER);
  if (!Strm) return false;
  /*
  if (Strm->GetGroupName() != "<savegame>") {
    VStream::Destroy(Strm);
    GCon->Logf(NAME_Error, "Invalid savegame #%d header", Slot);
    return false;
  }
  */

  vuint8 savever = 255;
  *Strm << savever;
  if (Strm->IsError()) { VStream::Destroy(Strm); return false; }
  VStream::Destroy(Strm);
  if (savever != 0) {
    GCon->Logf(NAME_Error, "Invalid savegame #%d version", Slot);
    return false;
  }

  Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_DESCR);
  if (!Strm) return false;
  *Strm << Description;
  if (Strm->IsError()) { VStream::Destroy(Strm); return false; }
  VStream::Destroy(Strm);

  // check list of loaded modules
  if (!dbg_load_ignore_wadlist.asBool()) {
    Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_WADLIST);
    if (!Strm) return false;
    const bool modok = CheckModList(Strm, Slot);
    VStream::Destroy(Strm);
    if (!modok) return false;
  }

  // load current map name
  Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_CURRMAP);
  if (!Strm) return false;
  VStr TmpName;
  *Strm << TmpName;
  if (Strm->IsError()) { VStream::Destroy(Strm); return false; }
  VStream::Destroy(Strm);

  CurrentMap = *TmpName;

  // load map list
  vint32 NumMaps = -1;
  Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_MAPLIST);
  if (Strm) {
    *Strm << STRM_INDEX(NumMaps);
    if (Strm->IsError() || NumMaps < 0 || NumMaps > 1024) { VStream::Destroy(Strm); return false; }
    for (int i = 0; i < NumMaps; ++i) {
      *Strm << TmpName;
      if (Strm->IsError()) { VStream::Destroy(Strm); return false; }
      VSavedMap *Map = new VSavedMap(true);
      Maps.Append(Map);
      Map->Name = *TmpName;
      Map->Index = Maps.length() - 1;
    }
    VStream::Destroy(Strm);

    // now load map vwads
    for (int i = 0; i < NumMaps; ++i) {
      VSavedMap *Map = Maps[i];
      vassert(Map->Index == i);
      vassert(Map->IsNewFormat());
      VStr vname = Map->GenVWadName();
      Strm = vwad->OpenFile(vname);
      if (Strm->IsError() || Strm->TotalSize() < 16) { VStream::Destroy(Strm); return false; }
      Map->Data.setLength(Strm->TotalSize());
      Strm->Serialise(Map->Data.Ptr(), Map->Data.length());
      if (Strm->IsError()) { VStream::Destroy(Strm); return false; }
      VStream::Destroy(Strm);
    }
  } else {
    // for checkpoint, we don't have map list at all
    NumMaps = 0;
  }

  //HACK: if `NumMaps` is 0, we're loading a checkpoint
  if (NumMaps == 0) {
    // load players inventory
    VSavedCheckpoint &cp = CheckPoint;
    cp.Clear();
    Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_CPOINT);
    if (!Strm) return false;
    cp.Serialise(Strm);
    if (Strm->IsError()) { VStream::Destroy(Strm); return false; }
    VStream::Destroy(Strm);
    SavedSkill = cp.Skill;
  } else {
    VSavedCheckpoint &cp = CheckPoint;
    cp.Clear();
    // load skill level
    SavedSkill = -1;
    Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_SKILL);
    if (Strm) {
      vint32 sk = -1;
      *Strm << sk;
      if (Strm->IsError()) { VStream::Destroy(Strm); return false; }
      VStream::Destroy(Strm);
      if (sk < 0 || sk > 31) {
        GCon->Logf(NAME_Warning, "Invalid savegame #%d skill (%d)", Slot, sk);
      } else {
        SavedSkill = sk;
      }
    }
  }

  Host_ResetSkipFrames();

  vassert(!saveFileBase.isEmpty());
  return true;
}


//==========================================================================
//
//  VSaveSlot::LoadSlot
//
//==========================================================================
bool VSaveSlot::LoadSlot (int Slot) {
  Clear(!dbg_save_in_old_format.asBool());
  saveFileBase.clear();

  VVWadArchive *vwad = nullptr;
  VStream *Strm = SV_OpenSlotFileReadWithFmt(Slot, vwad);
  if (!Strm) {
    saveFileBase.clear();
    GCon->Logf(NAME_Error, "Savegame #%d file doesn't exist", Slot);
    return false;
  }

  bool res;

  if (vwad) {
    res = LoadSlotNew(Slot, vwad);
    delete vwad;
  } else {
    res = LoadSlotOld(Slot, Strm);
    if (res && Strm->IsError()) res = false;
    VStream::Destroy(Strm);
  }

  if (!res) {
    saveFileBase.clear();
    GCon->Logf(NAME_Error, "Savegame #%d could not be read", Slot);
    Clear(!dbg_save_in_old_format.asBool());
  }

  return res;
}


//==========================================================================
//
//  VSaveSlot::SaveToSlotOld
//
//==========================================================================
bool VSaveSlot::SaveToSlotOld (int Slot, VStr &savefilename) {
  saveFileBase.clear();
  savefilename.clear();

  VStream *Strm = SV_CreateSlotFileWrite(Slot, Description, false);
  if (!Strm) {
    GCon->Logf(NAME_Error, "cannot save to slot #%d!", Slot);
    return false;
  }
  savefilename = Strm->GetName();

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
    Strm->Serialise(&tv, sizeof(tv));

    // date string
    VStr dstr = TimeVal2Str(&tv);
    id = SAVE_EXTDATA_ID_DATESTR;
    *Strm << STRM_INDEX(id);
    size = dstr.length();
    *Strm << STRM_INDEX(size);
    Strm->Serialise(*dstr, size);

    // end of data marker
    id = SAVE_EXTDATA_ID_END;
    *Strm << STRM_INDEX(id);
  }

  // write list of loaded modules
  auto wadlist = FL_GetWadPk3ListSmall();
  //GCon->Logf("====================="); for (int f = 0; f < wadlist.length(); ++f) GCon->Logf("  %d: %s", f, *wadlist[f]);
  vint32 wcount = wadlist.length();
  *Strm << wcount;
  for (int f = 0; f < wadlist.length(); ++f) *Strm << wadlist[f];

  // write current map
  VStr TmpName(CurrentMap);
  *Strm << TmpName;

  // write map list
  vint32 NumMaps = Maps.length();
  *Strm << STRM_INDEX(NumMaps);
  for (int i = 0; i < Maps.length(); ++i) {
    VSavedMap *Map = Maps[i];
    vassert(Map->Index == i);
    vassert(!Map->IsNewFormat());
    TmpName = VStr(Map->Name);
    vint32 DataLen = Map->Data.length();
    *Strm << TmpName << Map->Compressed << STRM_INDEX(Map->DecompressedSize) << STRM_INDEX(DataLen);
    Strm->Serialise(Map->Data.Ptr(), Map->Data.length());
  }

  //HACK: if `NumMaps` is 0, we're saving checkpoint
  if (NumMaps == 0) {
    // save players inventory
    VSavedCheckpoint &cp = CheckPoint;
    cp.Serialise(Strm);
    SavedSkill = cp.Skill;
  } else {
    if (SavedSkill >= 0 && SavedSkill < 32) {
      // save extra data
      vuint32 seg = GSLOT_DATA_START;
      *Strm << STRM_INDEX_U(seg);
      // skill
      seg = GSLOT_DATA_SKILL;
      *Strm << STRM_INDEX_U(seg);
      vint32 sk = SavedSkill;
      *Strm << STRM_INDEX(sk);
      // done
      seg = GSLOT_DATA_END;
      *Strm << STRM_INDEX_U(seg);
    }
  }

  bool err = Strm->IsError();
  Strm->Close();
  err = err || Strm->IsError();
  delete Strm; // done in failed

  Host_ResetSkipFrames();

  if (err) {
    GCon->Logf(NAME_Error, "error saving to slot %d, savegame is corrupted!", Slot);
    return false;
  }

  vassert(!saveFileBase.isEmpty());
  return true;
}


#define CLOSE_VWAD_FILE()  do { \
  vassert(Strm != nullptr); \
  Strm->Close(); \
  const bool xserr = Strm->IsError(); \
  delete Strm; Strm = nullptr; \
  if (xserr || vmain->IsError()) { \
    delete vmain; vmain = nullptr; \
    return false; \
  } \
} while (0)


#define CREATE_VWAD_FILE(xxfname)  do { \
  vassert(Strm == nullptr); \
  int level = save_compression_level.asInt(); \
  if (level < VWADWR_COMP_DISABLE || level > VWADWR_COMP_BEST) { \
    level = VWADWR_COMP_FAST; \
    save_compression_level = level; \
  } \
  VStr xyname = VStr(xxfname); \
  if (xyname.endsWithNoCase(".vwad")) level = VWADWR_COMP_DISABLE; \
  Strm = vmain->CreateFileDirect(xyname, level); \
  vassert(Strm != nullptr); \
} while (0)


//==========================================================================
//
//  VSaveSlot::SaveToSlotNew
//
//==========================================================================
bool VSaveSlot::SaveToSlotNew (int Slot, VStr &savefilename) {
  TTimeVal tv;
  GetTimeOfDay(&tv);

  saveFileBase.clear();
  savefilename.clear();

  VStream *ArcStrm = SV_CreateSlotFileWrite(Slot, Description, true);
  if (!ArcStrm) {
    GCon->Logf(NAME_Error, "cannot save to slot %d!", Slot);
    return false;
  }
  savefilename = ArcStrm->GetName();

  VVWadNewArchive *vmain = new VVWadNewArchive("<main-save>",
                                               NEWFMT_VWAD_AUTHOR,
                                               Description + " | "+TimeVal2Str(&tv),
                                               ArcStrm, true/*owned*/);
  if (vmain->IsError()) {
    delete vmain;
    GCon->Logf(NAME_Error, "cannot create save archive for slot %d!", Slot);
    return false;
  }

  VStream *Strm = nullptr;

  // version
  CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_HEADER);
  vuint8 savever = 0;
  *Strm << savever;
  CLOSE_VWAD_FILE();

  CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_DESCR);
  *Strm << Description;
  CLOSE_VWAD_FILE();

  // extended data: date value and date string
  // date value
  CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_DATE);
  *Strm << tv.secs << tv.usecs << tv.secshi;
  // date string (unused, but nice to have)
  VStr dstr = TimeVal2Str(&tv);
  *Strm << dstr;
  CLOSE_VWAD_FILE();

  // write list of loaded modules
  {
    CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_WADLIST);
    auto wadlist = FL_GetWadPk3ListSmall();
    vint32 wcount = wadlist.length();
    *Strm << wcount;
    for (int f = 0; f < wcount; ++f) *Strm << wadlist[f];
    CLOSE_VWAD_FILE();

    // write human-readable list of loaded modules
    // (it is purely informative)
    VStr sres;
    sres = "# automatically generated, and purely informational\n";
    for (VStr w : wadlist) { sres += w; sres += "\n"; }
    CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_HWADLIST);
    Strm->Serialise((void *)sres.getCStr(), sres.length());
    CLOSE_VWAD_FILE();
  }

  // write current map name
  CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_CURRMAP);
  VStr TmpName(CurrentMap);
  *Strm << TmpName;
  CLOSE_VWAD_FILE();

  // write map list
  CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_MAPLIST);
  vint32 NumMaps = Maps.length();
  *Strm << STRM_INDEX(NumMaps);
  for (int i = 0; i < Maps.length(); ++i) {
    VSavedMap *Map = Maps[i];
    vassert(Map->Index == i);
    vassert(Map->IsNewFormat());
    TmpName = VStr(Map->Name);
    *Strm << TmpName;
  }
  CLOSE_VWAD_FILE();

  // write map vwads
  for (int i = 0; i < NumMaps; ++i) {
    VSavedMap *Map = Maps[i];
    vassert(Map->Index == i);
    vassert(Map->IsNewFormat());
    vassert(Map->Data.length() >= 16);
    VStr vname = Map->GenVWadName();
    CREATE_VWAD_FILE(vname);
    Strm->Serialise(Map->Data.Ptr(), Map->Data.length());
    CLOSE_VWAD_FILE();
  }

  //HACK: if `NumMaps` is 0, we're loading a checkpoint
  if (NumMaps == 0) {
    // save players inventory
    VSavedCheckpoint &cp = CheckPoint;
    CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_CPOINT);
    cp.Serialise(Strm);
    CLOSE_VWAD_FILE();
    SavedSkill = cp.Skill;
  } else {
    VSavedCheckpoint &cp = CheckPoint;
    cp.Clear();
    // write skill level
    if (SavedSkill >= 0 && SavedSkill < 32) {
      CREATE_VWAD_FILE(NEWFMT_FNAME_SAVE_SKILL);
      vint32 sk = SavedSkill;
      *Strm << sk;
      CLOSE_VWAD_FILE();
    }
  }

  // finish vwad
  const bool xres = vmain->Close();
  delete vmain;
  #if 0
  GCon->Logf(NAME_Debug, "finished VWAD archive! xres=%d (%s)", (int)xres, *ArcStrm->GetName());
  #endif
  if (!xres) {
    GCon->Logf(NAME_Error, "cannot finalize savegame archive");
  }

  return xres;
}


//==========================================================================
//
//  VSaveSlot::SaveToSlot
//
//==========================================================================
bool VSaveSlot::SaveToSlot (int Slot) {
  VStr savefilename;

  const bool res = (IsNewFormat() ? SaveToSlotNew(Slot, savefilename)
                                  : SaveToSlotOld(Slot, savefilename));
  if (!res) {
    SV_SaveFailed(savefilename, Slot);
    saveFileBase.clear();
    //removeSlotSaveFiles(Slot);
  } else {
    SV_SaveSuccess(savefilename, Slot);
  }

  return res;
}


//==========================================================================
//
//  VSaveSlot::FindMap
//
//==========================================================================
VSavedMap *VSaveSlot::FindMap (VName Name) {
  for (int i = 0; i < Maps.length(); ++i) if (Maps[i]->Name == Name) return Maps[i];
  return nullptr;
}


//==========================================================================
//
//  SV_GetSaveStringOld
//
//==========================================================================
static bool SV_GetSaveStringOld (int Slot, VStr &Desc, VStream *Strm) {
  bool goodSave = true;
  Desc = "???";
  *Strm << Desc;
  // skip extended data
  if (true/*VStr::Cmp(VersionText, SAVE_VERSION_TEXT) == 0*/) {
    if (!SkipExtData(Strm) || Strm->IsError()) goodSave = false;
  }
  if (goodSave) {
    // check list of loaded modules
    goodSave = CheckModList(Strm, Slot, true, true);
  }
  if (!goodSave) Desc = "*"+Desc;
  return /*true*/goodSave;
}


//==========================================================================
//
//  SV_GetSaveStringNew
//
//==========================================================================
static bool SV_GetSaveStringNew (int Slot, VStr &Desc, VVWadArchive *vwad) {
  VStream *Strm;

  Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_DESCR);
  if (!Strm) return false;
  *Strm << Desc;
  if (Strm->IsError()) { VStream::Destroy(Strm); return false; }
  VStream::Destroy(Strm);

  #if 0
  const bool goodSave = true;
  #else
  Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_WADLIST);
  if (!Strm) return false;
  const bool goodSave = CheckModList(Strm, Slot, false, true);
  VStream::Destroy(Strm);
  #endif

  if (!goodSave) Desc = "*"+Desc;
  return /*true*/goodSave;
}


//==========================================================================
//
//  SV_GetSaveString
//
//==========================================================================
bool SV_GetSaveString (int Slot, VStr &Desc) {
  VVWadArchive *vwad = nullptr;
  VStream *Strm = SV_OpenSlotFileReadWithFmt(Slot, vwad);
  bool res;
  if (vwad) {
    res = SV_GetSaveStringNew(Slot, Desc, vwad);
    delete vwad;
  } else if (Strm) {
    res = SV_GetSaveStringOld(Slot, Desc, Strm);
    if (res && Strm->IsError()) res = false;
    VStream::Destroy(Strm);
  } else {
    res = false;
  }
  if (!res) Desc = EMPTYSTRING;
  return res;
}



//==========================================================================
//
//  SV_GetSaveDateString
//
//==========================================================================
void SV_GetSaveDateString (int Slot, VStr &datestr) {
  VVWadArchive *vwad = nullptr;
  VStream *Strm = SV_OpenSlotFileReadWithFmt(Slot, vwad);
  bool res;
  if (vwad) {
    res = false;
    Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_DATE);
    if (Strm) {
      TTimeVal tv;
      memset((void *)&tv, 0, sizeof(tv));
      *Strm << tv.secs << tv.usecs << tv.secshi;
      res = !Strm->IsError();
      VStream::Destroy(Strm);
      if (res) datestr = TimeVal2Str(&tv);
    }
    delete vwad;
  } else if (Strm) {
    VStr Desc;
    *Strm << Desc;
    res = !Strm->IsError();
    if (res) {
      datestr = LoadDateStrExtData(Strm);
      if (datestr.length() == 0) datestr = "UNKNOWN";
      if (res && Strm->IsError()) res = false;
    }
    VStream::Destroy(Strm);
  } else {
    res = false;
  }
  if (!res) datestr = "UNKNOWN";
}


//==========================================================================
//
//  SV_GetSaveDateTVal
//
//  false: slot is empty or invalid
//
//==========================================================================
static bool SV_GetSaveDateTVal (int Slot, TTimeVal *tv) {
  //memset((void *)tv, 0, sizeof(*tv));

  VVWadArchive *vwad = nullptr;
  VStream *Strm = SV_OpenSlotFileReadWithFmt(Slot, vwad);
  bool res;
  if (vwad) {
    res = false;
    Strm = vwad->OpenFile(NEWFMT_FNAME_SAVE_DATE);
    if (Strm) {
      TTimeVal tv;
      memset((void *)&tv, 0, sizeof(tv));
      *Strm << tv.secs << tv.usecs << tv.secshi;
      res = !Strm->IsError();
      VStream::Destroy(Strm);
    }
    delete vwad;
  } else if (Strm) {
    VStr Desc;
    *Strm << Desc;
    res = !Strm->IsError();
    if (res) {
      res = LoadDateTValExtData(Strm, tv);
    }
    VStream::Destroy(Strm);
  } else {
    res = false;
  }
  if (!res) memset((void *)tv, 0, sizeof(*tv));
  return res;
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
  for (int slot = 1; slot <= NUM_AUTOSAVES; ++slot) {
    if (!SV_GetSaveDateTVal(-slot, &tv)) {
      //fprintf(stderr, "AUTOSAVE: free slot #%d found!\n", slot);
      bestslot = -slot;
      break;
    }
    if (!bestslot || tv < besttv) {
      //GCon->Logf("AUTOSAVE: better slot #%d found [%s] : old id #%d [%s]!", slot, *TimeVal2Str(&tv), -bestslot, (bestslot ? *TimeVal2Str(&besttv) : ""));
      bestslot = -slot;
      besttv = tv;
    } else {
      //GCon->Logf("AUTOSAVE: skipped slot #%d [%s] (%d,%d,%d)!", slot, *TimeVal2Str(&tv), tv.secshi, tv.secs, tv.usecs);
    }
  }
  return bestslot;
}


//==========================================================================
//
//  AssertSegment
//
//==========================================================================
static inline void AssertSegment (VStream &Strm, gameArchiveSegment_t segType) {
  if (Streamer<vint32>(Strm) != (int)segType) {
    Host_Error("Corrupted save game: Segment [%d] failed alignment check", segType);
  }
}


//==========================================================================
//
//  ArchiveNames
//
//==========================================================================
static void ArchiveNames (VSaveWriterStream *Saver) {
  if (Saver->IsNewFormat()) {
    if (!Saver->CreateFileDirect(NEWFMT_FNAME_MAP_NAMES)) return;
  } else {
    // write offset to the names in the beginning of the file
    vint32 NamesOffset = Saver->Tell();
    Saver->Seek(0);
    *Saver << NamesOffset;
    Saver->Seek(NamesOffset);
  }

  // serialise names
  vint32 Count = Saver->Names.length();
  *Saver << STRM_INDEX(Count);
  for (int i = 0; i < Count; ++i) {
    //*Saver << *VName::GetEntry(Saver->Names[i].GetIndex());
    const char *EName = *Saver->Names[i];
    vuint8 len = (vuint8)VStr::Length(EName);
    *Saver << len;
    if (len) Saver->Serialise((void *)EName, len);
  }
  Saver->CloseFile();

  // with new format, write it to the single file (i.e. not here)
  if (!Saver->IsNewFormat()) {
    vint32 numScripts = Saver->AcsExports.length();
    //if (!Saver->CreateFileDirect(NEWFMT_FNAME_MAP_ACS_EXPT)) return;
    // serialise number of ACS exports
    *Saver << STRM_INDEX(numScripts);
    //Saver->CloseFile();
  }
}


//==========================================================================
//
//  UnarchiveNames
//
//==========================================================================
static void UnarchiveNames (VSaveLoaderStream *Loader) {
  vint32 NamesOffset = -1;
  vint32 TmpOffset = 0;

  if (Loader->IsNewFormat()) {
    Loader->OpenFile(NEWFMT_FNAME_MAP_NAMES);
  } else {
    *Loader << NamesOffset;
    TmpOffset = Loader->Tell();
    Loader->Seek(NamesOffset);
  }

  vint32 Count;
  *Loader << STRM_INDEX(Count);
  Loader->NameRemap.setLength(Count);
  for (int i = 0; i < Count; ++i) {
    char EName[NAME_SIZE+1];
    vuint8 len = 0;
    *Loader << len;
    vassert(len <= NAME_SIZE);
    if (len) Loader->Serialise(EName, len);
    EName[len] = 0;
    Loader->NameRemap[i] = VName(EName);
  }

  // unserialise number of ACS exports
  vint32 numScripts = -1;
  if (Loader->IsNewFormat()) {
    if (Loader->HasExtendedSection(NEWFMT_FNAME_MAP_ACS_EXPT)) {
      Loader->OpenFile(NEWFMT_FNAME_MAP_ACS_EXPT);
      *Loader << STRM_INDEX(numScripts);
    } else {
      numScripts = 0;
    }
  } else {
    *Loader << STRM_INDEX(numScripts);
  }
  if (numScripts < 0 || numScripts >= 1024*1024*2) { // arbitrary limit
    Host_Error("invalid number of ACS scripts (%d)", numScripts);
  }
  Loader->AcsExports.setLength(numScripts);

  // create empty script objects
  for (vint32 f = 0; f < numScripts; ++f) Loader->AcsExports[f] = AcsCreateEmptyThinker();

  if (!Loader->IsNewFormat()) {
    Loader->Seek(TmpOffset);
  }
}


//==========================================================================
//
//  ArchiveThinkers
//
//==========================================================================
static void ArchiveThinkers (VSaveWriterStream *Saver, bool SavingPlayers) {
  if (!Saver->IsNewFormat()) {
    vint32 Seg = ASEG_WORLD;
    *Saver << Seg;
  }

  Saver->skipPlayers = !SavingPlayers;

  // add level
  Saver->RegisterObject(GLevel);

  // add world info
  if (!Saver->CreateFileDirect(NEWFMT_FNAME_MAP_WORDINFO)) return;
  vuint8 WorldInfoSaved = (vuint8)SavingPlayers;
  *Saver << WorldInfoSaved;
  if (WorldInfoSaved) Saver->RegisterObject(GGameInfo->WorldInfo);
  Saver->CloseFile();

  if (!Saver->CreateFileDirect(NEWFMT_FNAME_MAP_ACTPLYS)) return;
  // add players
  {
    vassert(MAXPLAYERS >= 0 && MAXPLAYERS <= 254);
    vuint8 mpl = MAXPLAYERS;
    *Saver << mpl;
  }
  for (int i = 0; i < MAXPLAYERS; ++i) {
    vuint8 Active = (vuint8)(SavingPlayers && GGameInfo->Players[i]);
    *Saver << Active;
    if (!Active) continue;
    Saver->RegisterObject(GGameInfo->Players[i]);
  }
  Saver->CloseFile();

  // add thinkers
  int ThinkersStart = Saver->Exports.length();
  for (TThinkerIterator<VThinker> Th(GLevel); Th; ++Th) {
    // players will be skipped by `Saver`
    Saver->RegisterObject(*Th);
    /*
    if ((*Th)->IsA(VEntity::StaticClass())) {
      VEntity *e = (VEntity *)(*Th);
      if (e->EntityFlags&VEntity::EF_IsPlayer) GCon->Logf("PLRSAV: FloorZ=%f; CeilingZ=%f; Floor=%p; Ceiling=%p", e->FloorZ, e->CeilingZ, e->Floor, e->Ceiling);
    }
    */
  }

  if (!Saver->CreateFileDirect(NEWFMT_FNAME_MAP_EXPOBJNS)) return;
  // write exported object names
  vint32 NumObjects = Saver->Exports.length()-ThinkersStart;
  *Saver << STRM_INDEX(NumObjects);
  for (int i = ThinkersStart; i < Saver->Exports.length(); ++i) {
    VName CName = Saver->Exports[i]->GetClass()->GetVName();
    *Saver << CName;
  }
  Saver->CloseFile();

  #if 0
  // purely informational
  if (!Saver->CreateFileDirect("obj_classes.txt")) return;
  for (int i = 0; i < Saver->Exports.length(); ++i) {
    VStr s = VStr(va("%d: ", i)) + Saver->Exports[i]->GetClass()->GetFullName() +
             " : " + Saver->Exports[i]->GetClass()->Loc.toStringNoCol() +
             "\n";
    Saver->Serialise((void *)s.getCStr(), s.length());
  }
  Saver->CloseFile();
  #endif

  if (!Saver->CreateFileBuffered(NEWFMT_FNAME_MAP_THINKERS)) return;
  // serialise objects
  for (int i = 0; i < Saver->Exports.length(); ++i) {
    if (dbg_save_verbose&0x10) GCon->Logf("** SR #%d: <%s>", i, *Saver->Exports[i]->GetClass()->GetFullName());
    Saver->Exports[i]->Serialise(*Saver);
  }
  Saver->CloseFile();

  //GCon->Logf("dbg_save_verbose=0x%04x (%s) %d", dbg_save_verbose.asInt(), *dbg_save_verbose.asStr(), dbg_save_verbose.asInt());

  if (!Saver->CreateFileBuffered(NEWFMT_FNAME_MAP_ACS_STATE)) return;
  // collect acs scripts, serialize acs level
  GLevel->Acs->Serialise(*Saver);
  Saver->CloseFile();

  // save collected VAcs objects contents
  vint32 numASX = Saver->AcsExports.length();
  if (numASX) {
    if (!Saver->CreateFileBuffered(NEWFMT_FNAME_MAP_ACS_EXPT)) return;
    if (Saver->IsNewFormat()) {
      // write counter for the new format
      *Saver << STRM_INDEX(numASX);
    }
    for (vint32 f = 0; f < numASX; ++f) {
      Saver->AcsExports[f]->Serialise(*Saver);
    }
    Saver->CloseFile();
  }
}


//==========================================================================
//
//  UnarchiveThinkers
//
//==========================================================================
static void UnarchiveThinkers (VSaveLoaderStream *Loader) {
  VObject *Obj = nullptr;

  if (!Loader->IsNewFormat()) {
    AssertSegment(*Loader, ASEG_WORLD);
  }

  // add level
  Loader->Exports.Append(GLevel);

  // add world info
  Loader->OpenFile(NEWFMT_FNAME_MAP_WORDINFO);
  vuint8 WorldInfoSaved;
  *Loader << WorldInfoSaved;
  if (WorldInfoSaved) Loader->Exports.Append(GGameInfo->WorldInfo);

  // add players
  Loader->OpenFile(NEWFMT_FNAME_MAP_ACTPLYS);
  {
    vuint8 mpl = 255;
    *Loader << mpl;
    if (mpl != MAXPLAYERS) Host_Error("Invalid number of players in save");
  }
  sv_load_num_players = 0;
  for (int i = 0; i < MAXPLAYERS; ++i) {
    vuint8 Active;
    *Loader << Active;
    if (Active) {
      ++sv_load_num_players;
      Loader->Exports.Append(GPlayersBase[i]);
    }
  }

  TArray<VEntity *> elist;
  #ifdef VAVOOM_LOADER_CAN_SKIP_CLASSES
  TMapNC<VObject *, bool> deadThinkers;
  TMapNC<VName, bool> deadThinkersWarned;
  #endif

  bool hasSomethingToRemove = false;

  Loader->OpenFile(NEWFMT_FNAME_MAP_EXPOBJNS);
  vint32 NumObjects;
  *Loader << STRM_INDEX(NumObjects);
  if (NumObjects < 0) Host_Error("invalid number of VM objects");
  for (int i = 0; i < NumObjects; ++i) {
    // get params
    VName CName;
    *Loader << CName;
    VClass *Class = VClass::FindClass(*CName);
    if (!Class) {
      #ifdef VAVOOM_LOADER_CAN_SKIP_CLASSES
      if (ListLoaderCanSkipClass.has(CName)) {
        if (!deadThinkersWarned.put(CName, true)) {
          GCon->Logf("I/O WARNING: No such class '%s'", *CName);
        }
        //Loader->Exports.Append(nullptr);
        Class = VThinker::StaticClass();
        Obj = VObject::StaticSpawnNoReplace(Class);
        //deadThinkers.append((VThinker *)Obj);
        deadThinkers.put(Obj, false);
      } else {
        Sys_Error("I/O ERROR: No such class '%s'", *CName);
      }
      #else
      Sys_Error("I/O ERROR: No such class '%s'", *CName);
      #endif
    } else {
      // allocate object and copy data
      Obj = VObject::StaticSpawnNoReplace(Class);
    }
    // reassign server uids
    if (Obj && Obj->IsA(VThinker::StaticClass())) ((VThinker *)Obj)->ServerUId = Obj->GetUniqueId();

    // handle level info
    if (Obj->IsA(VLevelInfo::StaticClass())) {
      GLevelInfo = (VLevelInfo *)Obj;
      GLevelInfo->Game = GGameInfo;
      GLevelInfo->World = GGameInfo->WorldInfo;
      GLevel->LevelInfo = GLevelInfo;
    } else if (Obj->IsA(VEntity::StaticClass())) {
      VEntity *e = (VEntity *)Obj;
      if (!hasSomethingToRemove && (e->EntityFlags&VEntity::EF_KillOnUnarchive) != 0) hasSomethingToRemove = true;
      elist.append(e);
    }

    Loader->Exports.Append(Obj);
  }

  GLevelInfo->Game = GGameInfo;
  GLevelInfo->World = GGameInfo->WorldInfo;

  Loader->OpenFile(NEWFMT_FNAME_MAP_THINKERS);
  for (int i = 0; i < Loader->Exports.length(); ++i) {
    vassert(Loader->Exports[i]);
    #ifdef VAVOOM_LOADER_CAN_SKIP_CLASSES
    auto dpp = deadThinkers.get(Loader->Exports[i]);
    if (dpp) {
      //GCon->Logf(NAME_Debug, "!!! %d: %s", i, Loader->Exports[i]->GetClass()->GetName());
      Loader->Exports[i]->Serialise(*Loader);
    } else
    #endif
    {
      Loader->Exports[i]->Serialise(*Loader);
    }
  }
  #ifdef VAVOOM_LOADER_CAN_SKIP_CLASSES
  for (auto it = deadThinkers.first(); it; ++it) {
    ((VThinker *)it.getKey())->DestroyThinker();
  }
  #endif

  Loader->OpenFile(NEWFMT_FNAME_MAP_ACS_STATE);
  // unserialise acs script
  GLevel->Acs->Serialise(*Loader);

  if (Loader->AcsExports.length()) {
    Loader->OpenFile(NEWFMT_FNAME_MAP_ACS_EXPT);
    vint32 numScripts = -1;
    if (Loader->IsNewFormat()) {
      *Loader << STRM_INDEX(numScripts);
      if (numScripts != Loader->AcsExports.length()) {
        Host_Error("wtf?!");
      }
    }
    // load collected VAcs objects contents
    for (vint32 f = 0; f < Loader->AcsExports.length(); ++f) {
      Loader->AcsExports[f]->Serialise(*Loader);
    }
  }

  // `LinkToWorld()` in `VEntity::SerialiseOther()` will find the correct floor

  /*
  for (int i = 0; i < elist.length(); ++i) {
    VEntity *e = elist[i];
    GCon->Logf("ENTITY <%s>: org=(%f,%f,%f); flags=0x%08x", *e->GetClass()->GetFullName(), e->Origin.x, e->Origin.y, e->Origin.z, e->EntityFlags);
  }
  */

  // remove unnecessary entities
  if (hasSomethingToRemove && !loader_ignore_kill_on_unarchive) {
    for (int i = 0; i < elist.length(); ++i) if (elist[i]->EntityFlags&VEntity::EF_KillOnUnarchive) elist[i]->DestroyThinker();
  }

  GLevelInfo->eventAfterUnarchiveThinkers();
  GLevel->eventAfterUnarchiveThinkers();
}


//==========================================================================
//
//  ArchiveSounds
//
//==========================================================================
static void ArchiveSounds (VSaveWriterStream *Saver) {
  if (Saver->IsNewFormat()) {
    #ifdef CLIENT
    if (!Saver->CreateFileDirect(NEWFMT_FNAME_MAP_SOUNDS)) return;
    GAudio->SerialiseSounds(*Saver);
    Saver->CloseFile();
    #endif
  } else {
    vint32 Seg = ASEG_SOUNDS;
    *Saver << Seg;
    #ifdef CLIENT
    GAudio->SerialiseSounds(*Saver);
    #else
    vint32 Dummy = 0;
    *Saver << Dummy;
    #endif
  }
}


//==========================================================================
//
//  UnarchiveSounds
//
//==========================================================================
static void UnarchiveSounds (VSaveLoaderStream *Loader) {
  if (Loader->IsNewFormat()) {
    #ifdef CLIENT
    Loader->OpenFile(NEWFMT_FNAME_MAP_SOUNDS);
    GAudio->SerialiseSounds(*Loader);
    #endif
  } else {
    AssertSegment(*Loader, ASEG_SOUNDS);
    #ifdef CLIENT
    GAudio->SerialiseSounds(*Loader);
    #else
    vint32 count = 0;
    *Loader << count;
    //FIXME: keep this in sync with VAudio
    //Strm.Seek(Strm.Tell()+Dummy*36); //FIXME!
    if (count < 0) Sys_Error("invalid sound sequence data");
    while (count-- > 0) {
      vuint8 xver = 0; // current version is 0
      *Loader << xver;
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
      *Loader << STRM_INDEX(Sequence)
        << STRM_INDEX(OriginId)
        << Origin
        << STRM_INDEX(CurrentSoundID)
        << DelayTime
        << STRM_INDEX(DidDelayOnce)
        << Volume
        << Attenuation
        << STRM_INDEX(ModeNum);

      vint32 Offset;
      *Loader << STRM_INDEX(Offset);

      vint32 Count;
      *Loader << STRM_INDEX(Count);
      if (Count < 0) Sys_Error("invalid sound sequence data");
      for (int i = 0; i < Count; ++i) {
        VName SeqName;
        *Loader << SeqName;
      }

      vint32 ParentSeqIdx;
      vint32 ChildSeqIdx;
      *Loader << STRM_INDEX(ParentSeqIdx) << STRM_INDEX(ChildSeqIdx);
    }
    #endif
  }
}


//==========================================================================
//
//  SV_SaveMap
//
//==========================================================================
static void SV_SaveMap (bool savePlayers) {
  // make sure we don't have any garbage
  Host_CollectGarbage(true);

  VSaveWriterStream *Saver;

  // if we have no maps, or only one map that we will replace,
  // convert the save to the new format

  if (!BaseSlot.IsNewFormat()) {
    if (BaseSlot.Maps.length() == 0 ||
        (BaseSlot.Maps.length() == 1 && BaseSlot.Maps[0]->Name == GLevel->MapName))
    {
      GCon->Logf(NAME_Debug, "converted save game to new format.");
      BaseSlot.ForceNewFormat();
    }
  }

  // open the output file
  if (!BaseSlot.IsNewFormat()) {
    // old format
    VSavedMap *Map = BaseSlot.FindMap(GLevel->MapName);
    if (!Map) {
      Map = new VSavedMap(false);
      BaseSlot.Maps.Append(Map);
      Map->Name = GLevel->MapName;
      Map->Index = BaseSlot.Maps.length() - 1;
    } else {
      Map->ClearData(false);
    }

    VMemoryStream *InStrm = new VMemoryStream();
    Saver = new VSaveWriterStream(InStrm);

    vint32 NamesOffset = 0;
    *Saver << NamesOffset;

    // place a header marker
    vint32 Seg = ASEG_MAP_HEADER;
    *Saver << Seg;

    // write the level timer
    *Saver << GLevel->Time << GLevel->TicTime;

    // write main data
    ArchiveThinkers(Saver, savePlayers);
    ArchiveSounds(Saver);

    // place a termination marker
    Seg = ASEG_END;
    *Saver << Seg;

    ArchiveNames(Saver);

    // close the output file
    Saver->Close();

    TArrayNC<vuint8> &Buf = InStrm->GetArray();

    // compress map data
    Map->DecompressedSize = Buf.length();
    Map->Data.Clear();
    if (save_compression_level.asInt() < 0) {
      Map->Compressed = 0;
      Map->Data.setLength(Buf.length());
      if (Buf.length()) memcpy(Map->Data.ptr(), Buf.ptr(), Buf.length());
    } else {
      Map->Compressed = 1;
      VArrayStream *ArrStrm = new VArrayStream("<savemap>", Map->Data);
      ArrStrm->BeginWrite();
      int level = save_compression_level.asInt() * 3;
      if (level < 3) level = 3;
      VZLibStreamWriter *ZipStrm = new VZLibStreamWriter(ArrStrm, level);
      ZipStrm->Serialise(Buf.Ptr(), Buf.length());
      bool wasErr = ZipStrm->IsError();
      if (!ZipStrm->Close()) wasErr = true;
      delete ZipStrm;
      ArrStrm->Close();
      delete ArrStrm;
      if (wasErr) Host_Error("error compressing savegame data");
    }

    delete Saver;
  } else {
    vassert(BaseSlot.IsNewFormat());

    VMemoryStream *InStrm = new VMemoryStream();
    VVWadNewArchive *vwad = new VVWadNewArchive("<map-data>", "k8vavoom engine", "saved map data",
                                                InStrm, false/*not owned*/);
    if (vwad->IsError()) {
      delete vwad;
      delete InStrm;
      Host_Error("error creating saved map archive");
    }

    // remove old map
    VSavedMap *Map = BaseSlot.FindMap(GLevel->MapName);
    if (!Map) {
      Map = new VSavedMap(true);
      BaseSlot.Maps.Append(Map);
      Map->Name = GLevel->MapName;
      Map->Index = BaseSlot.Maps.length() - 1;
    } else {
      Map->ClearData(true);
    }

    // create saver
    Saver = new VSaveWriterStream(vwad);

    // write the level timer
    if (Saver->CreateFileDirect(NEWFMT_FNAME_MAP_GINFO)) {
      *Saver << GLevel->Time << GLevel->TicTime;
      Saver->CloseFile();
    }

    // write main data
    ArchiveThinkers(Saver, savePlayers);
    ArchiveSounds(Saver);
    ArchiveNames(Saver);

    // close the output file
    const bool ok = Saver->Close();
    delete Saver;

    if (ok) {
      vassert(Map->IsNewFormat());
      TArrayNC<vuint8> &Buf = InStrm->GetArray();
      Map->Data.Clear();
      Map->Data.setLength(Buf.length());
      if (Buf.length()) memcpy(Map->Data.ptr(), Buf.ptr(), Buf.length());
      delete InStrm;
    } else {
      Map->Data.Clear();
      delete InStrm;
      Host_Error("error writing saved map archive");
    }
  }
}


//==========================================================================
//
//  SV_SaveCheckpoint
//
//==========================================================================
static bool SV_SaveCheckpoint () {
  if (!GGameInfo) return false;
  if (GGameInfo->NetMode != NM_Standalone) return false; // oops
  // do not create checkpoints if we have several player classes
  if (GGameInfo->PlayerClasses.length() != 1) return false;

  VBasePlayer *plr = nullptr;
  // check if checkpoints are possible
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (GGameInfo->Players[i]) {
      if (!GGameInfo->Players[i]->IsCheckpointPossible()) return false;
      if (plr) return false;
      plr = GGameInfo->Players[i];
    }
  }
  if (!plr || !plr->MO) return false;

  QS_StartPhase(QSPhase::QSP_Save);
  VSavedCheckpoint &cp = BaseSlot.CheckPoint;
  cp.Clear();
  cp.Skill = GGameInfo->WorldInfo->GameSkill;
  VEntity *rwe = plr->eventGetReadyWeapon();

  if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: === creating ===");
  for (VEntity *invFirst = plr->MO->QS_GetEntityInventory();
       invFirst;
       invFirst = invFirst->QS_GetEntityInventory())
  {
    cp.AddEntity(invFirst);
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: inventory item '%s'", invFirst->GetClass()->GetName());
  }
  if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: getting properties");

  plr->QS_Save();
  for (auto &&qse : cp.EList) qse.ent->QS_Save();
  cp.QSList = QS_GetCurrentArray();

  // count entities, build entity list
  for (int f = 0; f < cp.QSList.length(); ++f) {
    QSValue &qv = cp.QSList[f];
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: property #%d of '%s': %s", f, (qv.ent ? qv.ent->GetClass()->GetName() : "player"), *qv.toString());
    if (!qv.ent) {
      qv.objidx = 0;
    } else {
      qv.objidx = cp.FindEntity(qv.ent);
      if (rwe == qv.ent) cp.ReadyWeapon = cp.FindEntity(rwe);
    }
  }

  QS_StartPhase(QSPhase::QSP_None);

  if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: game skill is %d, cp skill is %d", GGameInfo->WorldInfo->GameSkill, cp.Skill);
  if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: === complete ===");

  return true;
}


//==========================================================================
//
//  SV_LoadMap
//
//  returns `true` if checkpoint was loaded
//
//==========================================================================
static bool SV_LoadMap (VName MapName, bool allowCheckpoints, bool hubTeleport) {
  bool isCheckpoint = (BaseSlot.Maps.length() == 0);
  if (isCheckpoint && !allowCheckpoints) {
    Host_Error("Trying to load checkpoint in hub game!");
  }
#ifdef CLIENT
  if (isCheckpoint && svs.max_clients != 1) {
    Host_Error("Checkpoints aren't supported in networked games!");
  }
  // if we are loading a checkpoint, simulate normal map start
  if (isCheckpoint) sv_loading = false;
#else
  // standalone server
  if (isCheckpoint) {
    Host_Error("Checkpoints aren't supported on dedicated servers!");
  }
#endif

  // load a base level (spawn thinkers if this is checkpoint save)
  if (!hubTeleport) SV_ResetPlayers();
  try {
    VBasePlayer::isCheckpointSpawn = isCheckpoint;
#ifdef CLIENT
    // setup skill for server here, so checkpoints will start with a right one
    if (isCheckpoint) {
      VSavedCheckpoint &cp = BaseSlot.CheckPoint;
      if (dbg_checkpoints) GCon->Logf(NAME_Debug, "*** CP SKILL: %d", cp.Skill);
      if (cp.Skill >= 0) {
        if (dbg_checkpoints) GCon->Logf(NAME_Debug, "*** setting skill from a checkpoint: %d", cp.Skill);
        Skill = cp.Skill;
      }
    }
#endif
    SV_SpawnServer(*MapName, isCheckpoint/*spawn thinkers*/);
  } catch (...) {
    VBasePlayer::isCheckpointSpawn = false;
    throw;
  }

#ifdef CLIENT
  if (isCheckpoint) {
    sv_loading = false; // just in case
    try {
      VBasePlayer::isCheckpointSpawn = true;
      CL_SetupLocalPlayer();
    } catch (...) {
      VBasePlayer::isCheckpointSpawn = false;
      throw;
    }
    VBasePlayer::isCheckpointSpawn = false;

    Host_ResetSkipFrames();

    // do this here so that clients have loaded info, not initial one
    SV_SendServerInfoToClients();

    VSavedCheckpoint &cp = BaseSlot.CheckPoint;

    // put inventory
    VBasePlayer *plr = nullptr;
    for (int i = 0; i < MAXPLAYERS; ++i) {
      if (!GGameInfo->Players[i] || !GGameInfo->Players[i]->MO) continue;
      plr = GGameInfo->Players[i];
      break;
    }
    if (!plr) Host_Error("active player not found");
    VEntity *rwe = nullptr; // ready weapon

    QS_StartPhase(QSPhase::QSP_Load);

    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: === loading ===");
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (starting inventory)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    plr->MO->QS_ClearEntityInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (cleared inventory)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: ---");

    // create inventory items
    // have to do it backwards due to the way `AttachToOwner()` works
    for (int f = cp.EList.length()-1; f >= 0; --f) {
      VSavedCheckpoint::EntityInfo &ei = cp.EList[f];
      VEntity *inv = plr->MO->QS_SpawnEntityInventory(VName(*ei.ClassName));
      if (!inv) Host_Error("cannot spawn inventory item '%s'", *ei.ClassName);
      if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: spawned '%s'", inv->GetClass()->GetName());
      ei.ent = inv;
      if (cp.ReadyWeapon == f+1) rwe = inv;
    }
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (spawned inventory)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: ---");

    for (int f = 0; f < cp.QSList.length(); ++f) {
      QSValue &qv = cp.QSList[f];
      if (qv.objidx == 0) {
        qv.ent = nullptr;
        if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS:  #%d:player: %s", f, *qv.toString());
      } else {
        qv.ent = cp.EList[qv.objidx-1].ent;
        vassert(qv.ent);
        if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS:  #%d:%s: %s", f, qv.ent->GetClass()->GetName(), *qv.toString());
      }
      QS_EnterValue(qv);
    }

    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (inventory before setting properties)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: ---");
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: calling loaders");
    // call player loader, then entity loaders
    plr->QS_Load();
    for (int f = 0; f < cp.EList.length(); ++f) {
      VSavedCheckpoint::EntityInfo &ei = cp.EList[f];
      ei.ent->QS_Load();
    }

    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: --- (final inventory)");
    if (dbg_checkpoints) plr->CallDumpInventory();
    if (dbg_checkpoints) GCon->Logf(NAME_Debug, "QS: === done ===");
    QS_StartPhase(QSPhase::QSP_None);

    plr->eventAfterUnarchiveThinkers();

    plr->PlayerState = PST_LIVE;
    if (rwe) plr->eventSetReadyWeapon(rwe, true); // instant

    Host_ResetSkipFrames();
    return true;
  }
#endif

  Host_ResetSkipFrames();

  VSavedMap *Map = BaseSlot.FindMap(MapName);
  vassert(Map);

  VSaveLoaderStream *Loader = nullptr;

  // oops, it should be here
  TArrayNC<vuint8> DecompressedData;

  if (Map->IsNewFormat()) {
    VMemoryStreamRO *mst = new VMemoryStreamRO("<savemap:mapdata>",
                                               Map->Data.ptr(), Map->Data.length(), false);
    VVWadArchive *vmap = new VVWadArchive("<savemap:mapdata>", mst, true);
    if (!vmap->IsOpen()) {
      Host_Error("error opening savegame arhive");
    } else {
      Loader = new VSaveLoaderStream(vmap);
    }
  } else {
    // decompress map data
    if (!Map->Compressed) {
      DecompressedData.setLength(Map->Data.length());
      if (Map->Data.length()) memcpy(DecompressedData.ptr(), Map->Data.ptr(), Map->Data.length());
    } else {
      VArrayStream *ArrStrm = new VArrayStream("<savemap:mapdata>", Map->Data);
      /*VZLibStreamReader*/
      VStream *ZipStrm = new VZLibStreamReader(ArrStrm,
                                               Map->Data.length()/*VZLibStreamReader::UNKNOWN_SIZE*/,
                                               Map->DecompressedSize);
      DecompressedData.setLength(Map->DecompressedSize);
      ZipStrm->Serialise(DecompressedData.Ptr(), DecompressedData.length());
      const bool wasErr = ZipStrm->IsError();
      VStream::Destroy(ZipStrm);
      ArrStrm->Close(); delete ArrStrm;
      if (wasErr) Host_Error("error decompressing savegame data");
    }
    Loader = new VSaveLoaderStream(new VArrayStream("<savemap:mapdata>", DecompressedData));
  }

  // load names
  UnarchiveNames(Loader);

  // read the level timer
  if (Loader->IsNewFormat()) {
    Loader->OpenFile(NEWFMT_FNAME_MAP_GINFO);
  } else {
    AssertSegment(*Loader, ASEG_MAP_HEADER);
  }
  *Loader << GLevel->Time << GLevel->TicTime;

  UnarchiveThinkers(Loader);
  UnarchiveSounds(Loader);

  if (!Loader->IsNewFormat()) {
    AssertSegment(*Loader, ASEG_END);
  }

  // free save buffer
  Loader->Close();
  delete Loader;

  Host_ResetSkipFrames();

  // do this here so that clients have loaded info, not initial one
  SV_SendServerInfoToClients();

  Host_ResetSkipFrames();

  return false;
}


//==========================================================================
//
//  SV_SaveGame
//
//==========================================================================
static void SV_SaveGame (int slot, VStr Description, bool checkpoint, bool isAutosave) {
  BaseSlot.Description = Description;
  BaseSlot.CurrentMap = GLevel->MapName;
  BaseSlot.SavedSkill = GGameInfo->WorldInfo->GameSkill;

  // save out the current map
  if (checkpoint) {
    // if we have no maps in our base slot, checkpoints are enabled
    if (BaseSlot.Maps.length() != 0) {
      GCon->Logf("AUTOSAVE: cannot use checkpoints, perform a full save sequence (this is not a bug!)");
      checkpoint = false;
    } else {
      GCon->Logf("AUTOSAVE: checkpoints might be allowed.");
    }
  }

  #ifdef CLIENT
  // perform full update, so lightmap cache will be valid
  if (!checkpoint && GLevel->Renderer && GLevel->Renderer->isNeedLightmapCache()) {
    GLevel->Renderer->FullWorldUpdate(true);
  }
  #endif

  SV_SendBeforeSaveEvent(isAutosave, checkpoint);

  if (checkpoint) {
    // player state save
    if (!SV_SaveCheckpoint()) {
      GCon->Logf("AUTOSAVE: cannot use checkpoints, perform a full save sequence (this is not a bug!)");
      checkpoint = false;
      SV_SaveMap(true); // true = save player info
    }
  } else {
    // full save
    SV_SaveMap(true); // true = save player info
  }

  // write data to destination slot
  if (BaseSlot.SaveToSlot(slot)) {
    // checkpoints using normal map cache
    if (!checkpoint && !saveFileBase.isEmpty()) {
      VStr ccfname = saveFileBase+".lmap";
      #ifdef CLIENT
      bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);
      #else
      enum { doPrecalc = false };
      #endif
      #ifdef CLIENT
      if (!GLevel->Renderer || !GLevel->Renderer->isNeedLightmapCache() || !loader_cache_data || !doPrecalc) {
        // no rendered usually means that this is some kind of server (the thing that should not be, but...)
        Sys_FileDelete(ccfname);
      } else {
        GLevel->cacheFileBase = saveFileBase;
        GLevel->cacheFlags &= ~VLevel::CacheFlag_Ignore;
        VStream *lmc = FL_OpenSysFileWrite(ccfname);
        if (lmc) {
          GCon->Logf("writing lightmap cache to '%s'", *ccfname);
          GLevel->Renderer->saveLightmaps(lmc);
          bool err = lmc->IsError();
          lmc->Close();
          err = (err || lmc->IsError());
          delete lmc;
          if (err) {
            GCon->Logf(NAME_Warning, "removed broken lightmap cache '%s'", *ccfname);
            Sys_FileDelete(ccfname);
          }
        } else {
          GCon->Logf(NAME_Warning, "cannot create lightmap cache file '%s'", *ccfname);
        }
      }
      #endif
    }
    SV_SendAfterSaveEvent(isAutosave, checkpoint);
  }

  Host_ResetSkipFrames();
}


#ifdef CLIENT
//==========================================================================
//
//  SV_LoadGame
//
//==========================================================================
static void SV_LoadGame (int slot) {
  SV_ShutdownGame();

  // temp hack
  SV_SetupSkipCallback();

  if (!BaseSlot.LoadSlot(slot)) return;

  if (BaseSlot.SavedSkill >= 0) {
    //GCon->Logf(NAME_Debug, "*** SAVED SKILL: %d", BaseSlot.SavedSkill);
    Skill = BaseSlot.SavedSkill;
  }

  sv_loading = true;

  // load the current map
  if (!SV_LoadMap(BaseSlot.CurrentMap, true/*allowCheckpoints*/, false/*hubTeleport*/)) {
    // not a checkpoint
    GLevel->cacheFileBase = saveFileBase;
    GLevel->cacheFlags &= ~VLevel::CacheFlag_Ignore;
    //GCon->Logf(NAME_Debug, "**********************: <%s>", *GLevel->cacheFileBase);
    #ifdef CLIENT
    if (GGameInfo->NetMode != NM_DedicatedServer) CL_SetupLocalPlayer();
    #endif
    // launch waiting scripts
    if (!svs.deathmatch) GLevel->Acs->CheckAcsStore();

    //GCon->Logf(NAME_Debug, "************************** (%d)", svs.max_clients);
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *Player = GGameInfo->Players[i];
      if (!Player) {
        //GCon->Logf(NAME_Debug, "*** no player #%d", i);
        continue;
      }
      Player->eventAfterUnarchiveThinkers();
    }
  } else {
    // checkpoint
    GLevel->cacheFlags &= ~VLevel::CacheFlag_Ignore;
  }

  SV_SendLoadedEvent();
}
#endif


//==========================================================================
//
//  SV_ClearBaseSlot
//
//==========================================================================
void SV_ClearBaseSlot () {
  BaseSlot.Clear(!dbg_save_in_old_format.asBool());
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
  TArray<VThinker *> TravelObjs;

  if (newskill >= 0 && (flags&CHANGELEVEL_CHANGESKILL) != 0) {
    GCon->Logf("SV_MapTeleport: new skill is %d", newskill);
    Skill = newskill;
    flags &= ~CHANGELEVEL_CHANGESKILL; // clear flag
  }

  // we won't show intermission anyway, so remove this flag
  flags &= ~CHANGELEVEL_NOINTERMISSION;

  if (flags&~(CHANGELEVEL_KEEPFACING|CHANGELEVEL_RESETINVENTORY|CHANGELEVEL_RESETHEALTH|CHANGELEVEL_PRERAISEWEAPON|CHANGELEVEL_REMOVEKEYS)) {
    GCon->Logf("SV_MapTeleport: unimplemented flag set: 0x%04x", (unsigned)flags);
  }

  TAVec plrAngles[MAXPLAYERS];
  memset((void *)plrAngles, 0, sizeof(plrAngles));

  // call PreTravel event
  for (int i = 0; i < MAXPLAYERS; ++i) {
    if (!GGameInfo->Players[i]) continue;
    plrAngles[i] = GGameInfo->Players[i]->ViewAngles;
    GGameInfo->Players[i]->eventPreTravel();
  }

  // collect list of thinkers that will go to the new level (player inventory)
  for (VThinker *Th = GLevel->ThinkerHead; Th; Th = Th->Next) {
    VEntity *vent = Cast<VEntity>(Th);
    if (vent && vent->Owner && vent->Owner->IsPlayer()) {
      TravelObjs.Append(vent);
      GLevel->RemoveThinker(vent);
      vent->UnlinkFromWorld();
      GLevel->DelSectorList();
      vent->StopSound(0); // stop all sounds
      //GCon->Logf(NAME_Debug, "SV_MapTeleport: saved player inventory item '%s'", vent->GetClass()->GetName());
      continue;
    }
    if (Th->IsA(VPlayerReplicationInfo::StaticClass())) {
      TravelObjs.Append(Th);
      GLevel->RemoveThinker(Th);
    }
  }

  if (!svs.deathmatch) {
    const VMapInfo &old_info = P_GetMapInfo(GLevel->MapName);
    const VMapInfo &new_info = P_GetMapInfo(mapname);
    // all maps in cluster 0 are treated as in different clusters
    if (old_info.Cluster && old_info.Cluster == new_info.Cluster &&
        (P_GetClusterDef(old_info.Cluster)->Flags&CLUSTERF_Hub))
    {
      // same cluster: save map without saving player mobjs
      SV_SaveMap(false);
    } else {
      // entering new cluster: clear base slot
      if (dbg_save_verbose&0x20) GCon->Logf("**** NEW CLUSTER ****");
      BaseSlot.Clear(!dbg_save_in_old_format.asBool());
    }
  }

  vuint8 oldNoMonsters = GGameInfo->nomonsters;
  if (flags&CHANGELEVEL_NOMONSTERS) GGameInfo->nomonsters = 1;

  sv_map_travel = true;
  if (!svs.deathmatch && BaseSlot.FindMap(mapname)) {
    // unarchive map
    SV_LoadMap(mapname, false/*allowCheckpoints*/, true/*hubTeleport*/); // don't allow checkpoints
  } else {
    // new map
    SV_SpawnServer(*mapname, true/*spawn thinkers*/);
    // if we spawned a new server, there is no need to reset inventory, health or keys
    flags &= ~(CHANGELEVEL_RESETINVENTORY|CHANGELEVEL_RESETHEALTH|CHANGELEVEL_REMOVEKEYS);
  }

  if (flags&CHANGELEVEL_NOMONSTERS) GGameInfo->nomonsters = oldNoMonsters;

  // add traveling thinkers to the new level
  for (int i = 0; i < TravelObjs.length(); ++i) {
    //GCon->Logf(NAME_Debug, "SV_MapTeleport: adding back player inventory item '%s'", TravelObjs[i]->GetClass()->GetName());
    GLevel->AddThinker(TravelObjs[i]);
    VEntity *Ent = Cast<VEntity>(TravelObjs[i]);
    if (Ent) Ent->LinkToWorld(true);
  }

  Host_ResetSkipFrames();

#ifdef CLIENT
  bool doSaveGame = false;
  if (GGameInfo->NetMode == NM_TitleMap ||
      GGameInfo->NetMode == NM_Standalone ||
      GGameInfo->NetMode == NM_ListenServer)
  {
    CL_SetupStandaloneClient();
    doSaveGame = sv_new_map_autosave;
  }

  if (doSaveGame && fsys_hasMapPwads && fsys_PWadMaps.length()) {
    // do not autosave on iwad maps
    doSaveGame = false;
    for (auto &&lmp : fsys_PWadMaps) {
      if (lmp.mapname.strEquCI(*mapname)) {
        doSaveGame = true;
        break;
      }
    }
    if (!doSaveGame) GCon->Logf(NAME_Warning, "autosave skipped due to iwad map");
  }
#else
  const bool doSaveGame = false;
#endif

  if (flags&(CHANGELEVEL_KEEPFACING|CHANGELEVEL_RESETINVENTORY|CHANGELEVEL_RESETHEALTH|CHANGELEVEL_PRERAISEWEAPON|CHANGELEVEL_REMOVEKEYS)) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      VBasePlayer *plr = GGameInfo->Players[i];
      if (!plr) continue;

      if (flags&CHANGELEVEL_KEEPFACING) {
        plr->ViewAngles = plrAngles[i];
        plr->eventClientSetAngles(plrAngles[i]);
        plr->PlayerFlags &= ~VBasePlayer::PF_FixAngle;
      }
      if (flags&CHANGELEVEL_RESETINVENTORY) plr->eventResetInventory();
      if (flags&CHANGELEVEL_RESETHEALTH) plr->eventResetHealth();
      if (flags&CHANGELEVEL_PRERAISEWEAPON) plr->eventPreraiseWeapon();
      if (flags&CHANGELEVEL_REMOVEKEYS) plr->eventRemoveKeys();
    }
  }

  // launch waiting scripts
  if (!svs.deathmatch) GLevel->Acs->CheckAcsStore();

  if (doSaveGame) GCmdBuf << "AutoSaveEnter\n";

  /*
  for (int i = 0; i < MAXPLAYERS; ++i) {
    VBasePlayer *plr = GGameInfo->Players[i];
    if (!plr) continue;
    VEntity *sve = plr->ehGetSavedInventory();
    if (!sve) continue;
    GCon->Logf(NAME_Debug, "+++ player #%d saved inventory +++", i);
    sve->DebugDumpInventory(true);
  }
  */
}


#ifdef CLIENT
void Draw_SaveIcon ();
void Draw_LoadIcon ();


//==========================================================================
//
//  CheckIfLoadIsAllowed
//
//==========================================================================
static bool CheckIfLoadIsAllowed () {
  if (svs.deathmatch) {
    GCon->Log("Can't load in deathmatch game");
    return false;
  }

  return true;
}
#endif


//==========================================================================
//
//  CheckIfSaveIsAllowed
//
//==========================================================================
static bool CheckIfSaveIsAllowed () {
  if (svs.deathmatch) {
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


//==========================================================================
//
//  BroadcastSaveText
//
//==========================================================================
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


//==========================================================================
//
//  SV_AutoSave
//
//==========================================================================
void SV_AutoSave (bool checkpoint) {
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

  SV_SaveGame(aslot, svname, checkpoint, true);
  Host_ResetSkipFrames();

  BroadcastSaveText(va("Game autosaved to slot #%d", -aslot));
}


#ifdef CLIENT
//==========================================================================
//
//  SV_AutoSaveOnLevelExit
//
//==========================================================================
void SV_AutoSaveOnLevelExit () {
  if (!dbg_save_on_level_exit) return;

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

  SV_SaveGame(aslot, svname, false, true); // not a checkpoint, obviously
  Host_ResetSkipFrames();

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
  if (Args.length() != 3) {
    GCon->Log("usage: save slotindex description");
    return;
  }

  if (!CheckIfSaveIsAllowed()) return;

  if (Args[2].Length() >= 32) {
    BroadcastSaveText("Description too long!");
    return;
  }

  Draw_SaveIcon();

  SV_SaveGame(VStr::atoi(*Args[1]), Args[2], false, false); // not a checkpoint
  Host_ResetSkipFrames();

  BroadcastSaveText("Game saved.");
}


//==========================================================================
//
//  COMMAND DeleteSavedGame <slotidx|quick>
//
//==========================================================================
COMMAND(DeleteSavedGame) {
  //GCon->Logf("DeleteSavedGame: argc=%d", Args.length());

  if (Args.length() != 2) return;

  if (!CheckIfLoadIsAllowed()) return;

  VStr numstr = Args[1].xstrip();
  if (numstr.isEmpty()) return;

  //GCon->Logf("DeleteSavedGame: <%s>", *numstr);

  if (/*numstr.ICmp("quick") == 0*/numstr.startsWithCI("q")) {
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
  if (neg && slot == abs(QUICKSAVE_SLOT)) {
    slot = QUICKSAVE_SLOT;
  } else {
    if (slot < 0 || slot > 99) return;
    if (neg) slot = -slot;
  }

  if (SV_DeleteSlotFile(slot)) {
         if (slot == QUICKSAVE_SLOT) BroadcastSaveText("Quicksave deleted.");
    else if (slot < 0) BroadcastSaveText(va("Autosave #%d deleted", -slot));
    else BroadcastSaveText(va("Savegame #%d deleted", slot));
  }
}


//==========================================================================
//
//  COMMAND Load
//
//==========================================================================
COMMAND(Load) {
  if (Args.length() != 2) return;

  if (!CheckIfLoadIsAllowed()) return;

  int slot = VStr::atoi(*Args[1]);
  VStr desc;
  if (!SV_GetSaveString(slot, desc)) {
    BroadcastSaveText("Empty slot!");
    return;
  }
  GCon->Logf("Loading \"%s\"", *desc);

  Draw_LoadIcon();
  SV_LoadGame(slot);
  Host_ResetSkipFrames();

  //if (GGameInfo->NetMode == NM_Standalone) SV_UpdateRebornSlot(); // copy the base slot to the reborn slot
  BroadcastSaveText(va("Loaded save \"%s\".", *desc));
}


//==========================================================================
//
//  COMMAND QuickSave
//
//==========================================================================
COMMAND(QuickSave) {
  if (!CheckIfSaveIsAllowed()) return;

  Draw_SaveIcon();

  SV_SaveGame(QUICKSAVE_SLOT, "quicksave", false, false); // not a checkpoint
  Host_ResetSkipFrames();

  BroadcastSaveText("Game quicksaved.");
}


//==========================================================================
//
//  COMMAND QuickLoad
//
//==========================================================================
COMMAND(QuickLoad) {
  if (!CheckIfLoadIsAllowed()) return;

  VStr desc;
  if (!SV_GetSaveString(QUICKSAVE_SLOT, desc)) {
    BroadcastSaveText("Empty quicksave slot");
    return;
  }
  GCon->Log("Loading quicksave");

  Draw_LoadIcon();
  SV_LoadGame(QUICKSAVE_SLOT);
  Host_ResetSkipFrames();
  // don't copy to reborn slot -- this is quickload after all!

  BroadcastSaveText("Quicksave loaded.");
}


//==========================================================================
//
//  COMMAND AutoSaveEnter
//
//==========================================================================
COMMAND(AutoSaveEnter) {
  // there is no reason to autosave on standard maps when we have pwads
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

  SV_SaveGame(aslot, svname, sv_autoenter_checkpoints, true);
  Host_ResetSkipFrames();

  BroadcastSaveText(va("Game autosaved to slot #%d", -aslot));
}


//==========================================================================
//
//  COMMAND AutoSaveLeave
//
//==========================================================================
COMMAND(AutoSaveLeave) {
  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) return;
  SV_AutoSaveOnLevelExit();
  Host_ResetSkipFrames();
}


//==========================================================================
//
//  COMMAND ShowSavePrefix
//
//==========================================================================
COMMAND(ShowSavePrefix) {
  auto wadlist = FL_GetWadPk3ListSmall();
  GCon->Log("==== MODS ====");
  for (auto &&mname: wadlist) GCon->Logf("  %s", *mname);
  GCon->Log("----");
  wadlist = FL_GetWadPk3List();
  GCon->Log("==== MODS (full) ====");
  for (auto &&mname: wadlist) GCon->Logf("  %s", *mname);
  GCon->Log("----");
  vuint64 hash = SV_GetModListHash(nullptr);
  VStr pfx = VStr::buf2hex(&hash, 8);
  GCon->Logf("save prefix: %s", *pfx);
}
#endif


#endif
