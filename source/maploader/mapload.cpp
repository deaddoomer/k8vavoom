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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
//**    Do all the WAD I/O, get map description, set up initial state and
//**  misc. LUTs.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../text.h"
#ifdef CLIENT
# include "../automap.h"
#endif
#include "../mapinfo.h"


static VCvarB dbg_show_map_hash("dbg_show_map_hash", false, "Show map hash?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);

static VCvarI r_fakecontrast("r_fakecontrast", "1", "Controls fake contrast/smooth lighting for walls (0: disable; 1: fake contrast; 2: smooth lighting)?", CVAR_Archive);
static VCvarB r_fakecontrast_ignore_mapinfo("r_fakecontrast_ignore_mapinfo", false, "Controls fake contrast/smooth lighting for walls (0: disable; 1: fake contrast; 2: smooth lighting)?", CVAR_Archive|CVAR_NoShadow);

static VCvarB loader_cache_ignore_one("loader_cache_ignore_one", false, "Ignore (and remove) cache for next map loading?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB loader_cache_rebuild_data("loader_cache_rebuild_data", true, "Cache rebuilt nodes, pvs, blockmap, and so on?", CVAR_Archive|CVAR_NoShadow);

VCvarB loader_cache_data("loader_cache_data", true, "Cache built level data?", CVAR_Archive|CVAR_NoShadow);
VCvarF loader_cache_time_limit("loader_cache_time_limit", "3", "Cache data if building took more than this number of seconds.", CVAR_Archive|CVAR_NoShadow);

static VCvarB loader_xeep_vanilla_horizon_hack("loader_xeep_vanilla_horizon_hack", true, "Allow Xeep's \"Vanilla Horizon\" hack?", CVAR_Archive|CVAR_NoShadow);


//extern VCvarI nodes_builder_type;
#ifdef CLIENT
extern VCvarI r_max_portal_depth;
extern VCvarI r_max_portal_depth_override;
extern int ldr_extrasamples_override; // -1: no override; 0: disable; 1: enable
extern int r_precalc_static_lights_override; // <0: not set
extern int r_precache_textures_override; // <0: not set
#endif


// lump order in a map WAD: each map needs a couple of lumps
// to provide a complete scene geometry description
enum {
  ML_LABEL,    // a separator, name, ExMx or MAPxx
  ML_THINGS,   // monsters, items
  ML_LINEDEFS, // linedefs, from editing
  ML_SIDEDEFS, // sidedefs, from editing
  ML_VERTEXES, // vertices, edited and BSP splits generated
  ML_SEGS,     // linesegs, from linedefs split by BSP
  ML_SSECTORS, // subsectors, list of linesegs
  ML_NODES,    // BSP nodes
  ML_SECTORS,  // sectors, from editing
  ML_REJECT,   // LUT, sector-sector visibility
  ML_BLOCKMAP, // LUT, motion clipping, walls/grid element
  ML_BEHAVIOR, // ACS scripts
};

// lump order from "GL-Friendly Nodes" specs
enum {
  ML_GL_LABEL, // a separator name, GL_ExMx or GL_MAPxx
  ML_GL_VERT,  // extra Vertices
  ML_GL_SEGS,  // segs, from linedefs & minisegs
  ML_GL_SSECT, // subsectors, list of segs
  ML_GL_NODES, // gl bsp nodes
};


// ////////////////////////////////////////////////////////////////////////// //
static TMap<VStr, bool> mapTextureWarns;


// ////////////////////////////////////////////////////////////////////////// //
struct AuxiliaryCloser {
public:
  bool doCloseAux;

public:
  VV_DISABLE_COPY(AuxiliaryCloser)
  AuxiliaryCloser () : doCloseAux(false) {}
  ~AuxiliaryCloser () { if (doCloseAux) W_CloseAuxiliary(); doCloseAux = false; }
};


// ////////////////////////////////////////////////////////////////////////// //
struct LoadingTiming {
  const char *name;
  double time;
  int msecs;
};

#define MAX_LOADING_TIMINGS  (64)
static LoadingTiming loadingTimings[MAX_LOADING_TIMINGS];
static unsigned int loadingTimingsCount = 0;


//==========================================================================
//
//  ResetLoadingTimings
//
//==========================================================================
static void ResetLoadingTimings () {
  loadingTimingsCount = 0;
}


//==========================================================================
//
//  AddLoadingTiming
//
//==========================================================================
static void AddLoadingTiming (const char *name, double time) {
  if (!name || !name[0]) return;
  if (loadingTimingsCount == MAX_LOADING_TIMINGS) return;
  LoadingTiming *tm = &loadingTimings[loadingTimingsCount++];
  tm->name = name;
  tm->time = time;
  if (time < 0) {
    tm->msecs = 0;
  } else {
    tm->msecs = (int)(time*1000+0.5);
  }
}


//==========================================================================
//
//  DumpLoadingTimings
//
//==========================================================================
static void DumpLoadingTimings () {
  if (loadingTimingsCount == 0) return;
  size_t maxLabelLength = 0;
  for (unsigned int f = 0; f < loadingTimingsCount; ++f) {
    const LoadingTiming &lt = loadingTimings[f];
    if (lt.msecs == 0) continue;
    size_t sl = strlen(lt.name);
    if (maxLabelLength < sl) maxLabelLength = sl;
  }
  if (maxLabelLength == 0) return; // nothing to do
  if (maxLabelLength > 64) return; // just in case
  GCon->Log("-------");
  char buf[256];
  for (unsigned int f = 0; f < loadingTimingsCount; ++f) {
    const LoadingTiming &lt = loadingTimings[f];
    if (lt.msecs == 0) continue;
    memset(buf, 32, sizeof(buf));
    strcpy(buf, lt.name);
    buf[strlen(lt.name)] = 32;
    snprintf(buf+maxLabelLength+1, sizeof(buf)-maxLabelLength-1, "%3d.%03d", lt.msecs/1000, lt.msecs%1000);
    GCon->Log(buf);
  }
}


//==========================================================================
//
//  VLevel::FixKnownMapErrors
//
//==========================================================================
void VLevel::FixKnownMapErrors () {
  eventKnownMapBugFixer();
#ifdef CLIENT
  if (LevelFlags&LF_ForceNoTexturePrecache) r_precache_textures_override = 0;
  if (LevelFlags&LF_ForceNoPrecalcStaticLights) r_precalc_static_lights_override = 0;
#endif
  // check for xeep "vanilla hoziron" trick
  if (loader_xeep_vanilla_horizon_hack.asBool()) {
    for (auto &&line : allLines()) {
      if (line.lineTag != 999 || line.special) continue;
      side_t *side;
      if (line.frontside) {
        if (line.backside) continue;
        side = line.frontside;
      } else if (line.backside) {
        if (line.frontside) continue;
        side = line.backside;
      } else {
        continue;
      }
      if (side->Mid.TextureOffset != 180.0f) continue;
      GCon->Logf(NAME_Warning, "XEEP VANILLA HORIZON FX on line #%d", (int)(ptrdiff_t)(&line-&Lines[0]));
      #if 0
      GCon->Logf(NAME_Warning, "VANILLA HORIZON! xofs=%g; yofs=%g; scalex=%g; scaley=%g",
                 side->Mid.TextureOffset,
                 side->Mid.RowOffset,
                 side->Mid.ScaleX,
                 side->Mid.ScaleY);
      #endif
      line.special = 9; // SPEC_LineHorizon
    }
  }
}


//==========================================================================
//
//  hashLump
//
//==========================================================================
static bool hashLump (RIPEMD160_Ctx *ripectx, MD5Context *md5ctx, int lumpnum) {
  if (lumpnum < 0) return false;
  static vuint8 buf[65536];
  VStream *strm = W_CreateLumpReaderNum(lumpnum);
  if (!strm) return false;
  VCheckedStream st(strm);
  auto left = st.TotalSize();
  while (left > 0) {
    int rd = left;
    if (rd > (int)sizeof(buf)) rd = (int)sizeof(buf);
    st.Serialise(buf, rd);
    if (st.IsError()) { VStream::Destroy(strm); return false; }
    if (ripectx) ripemd160_put(ripectx, buf, (unsigned)rd);
    if (md5ctx) md5ctx->Update(buf, (unsigned)rd);
    //if (xxhash) XXH32_update(xxhash, buf, (unsigned)rd);
    left -= rd;
  }
  return true;
}


//==========================================================================
//
//  VLevel::LoadMap
//
//==========================================================================
void VLevel::LoadMap (VName AMapName) {
  fsys_report_added_paks_logtype = NAME_Log; // it is time for this...
  AuxiliaryCloser auxCloser;

  bool killCache = loader_cache_ignore_one;
  cacheFlags = (loader_cache_ignore_one ? CacheFlag_Ignore : 0);
  loader_cache_ignore_one = false;
  bool AuxiliaryMap = false;
  int lumpnum, xmaplumpnum = -1;
  VName MapLumpName;
  vassert(!decanimlist);

  mapTextureWarns.clear();
  // clear automap marks; save loader will restore them from a save
  #ifdef CLIENT
  AM_ClearMarks();
  #endif

  if (csTouched) Z_Free(csTouched);
  csTouchCount = 0;
  csTouched = nullptr;

load_again:
  cacheFileBase.clear();
  ResetLoadingTimings();
  GTextureManager.ResetMapTextures();

  #ifdef CLIENT
  r_max_portal_depth_override = -1;
  ldr_extrasamples_override = -1;
  r_precalc_static_lights_override = -1;
  r_precache_textures_override = -1;
  #endif

  double TotalTime = -Sys_Time();
  double InitTime = -Sys_Time();
  MapName = AMapName;
  MapHash = VStr();
  MapHashMD5 = VStr();
  // If working with a devlopment map, reload it.
  // k8: nope, it doesn't work this way: it looks for "maps/xxx.wad" in zips,
  //     and "complete.pk3" takes precedence over any pwads
  //     so let's do it backwards
  // Find map and GL nodes.
  lumpnum = W_CheckNumForName(MapName);
  MapLumpName = MapName;
  int wadlumpnum = W_CheckNumForFileName(va("maps/%s.wad", *MapName));
  if (wadlumpnum > lumpnum) lumpnum = -1;
  // if there is no map lump, try map wad
  if (lumpnum < 0) {
    // check if map wad is here
    VStr aux_file_name = va("maps/%s.wad", *MapName);
    if (FL_FileExists(aux_file_name)) {
      // append map wad to list of wads (it will be deleted later)
      xmaplumpnum = W_CheckNumForFileName(va("maps/%s.wad", *MapName));
      lumpnum = W_OpenAuxiliary(aux_file_name);
      if (lumpnum >= 0) {
        auxCloser.doCloseAux = true;
        MapLumpName = W_LumpName(lumpnum);
        AuxiliaryMap = true;
      }
    }
  } else {
    xmaplumpnum = lumpnum;
  }
  if (lumpnum < 0) Host_Error("Map \"%s\" not found", *MapName);

  // some idiots embeds wads into wads
  if (!AuxiliaryMap && lumpnum >= 0 && W_LumpLength(lumpnum) > 128 && W_LumpLength(lumpnum) < 1024*1024) {
    VStream *lstrm = W_CreateLumpReaderNum(lumpnum);
    if (lstrm) {
      char sign[4];
      lstrm->Serialise(sign, 4);
      if (!lstrm->IsError() && memcmp(sign, "PWAD", 4) == 0) {
        lstrm->Seek(0);
        xmaplumpnum = lumpnum;
        lumpnum = W_AddAuxiliaryStream(lstrm, WAuxFileType::VFS_Wad);
        if (lumpnum >= 0) {
          auxCloser.doCloseAux = true;
          MapLumpName = W_LumpName(lumpnum);
          AuxiliaryMap = true;
        } else {
          Host_Error("cannot open pwad for \"%s\"", *MapName);
        }
      } else {
        delete lstrm;
      }
    }
  }

  //FIXME: reload saved background screen from FBO
  R_OSDMsgReset(OSD_MapLoading);
  R_OSDMsgShowMain("LOADING");

  bool saveCachedData = false;
  int ThingsLump = -1;
  int LinesLump = -1;
  int SidesLump = -1;
  int VertexesLump = -1;
  int SectorsLump = -1;
  int RejectLump = -1;
  int BehaviorLump = -1;
  int DialogueLump = -1;
  bool NeedNodesBuild = true;
  char GLNodesHdr[4];
  const VMapInfo &MInfo = P_GetMapInfo(MapName);
  memset(GLNodesHdr, 0, sizeof(GLNodesHdr));

  RIPEMD160_Ctx ripectx;
  MD5Context md5ctx;

  ripemd160_init(&ripectx);
  md5ctx.Init();

  //sha224: k8vavoom hash
  //md5: gzdoom-compatible hash (only for non-UDMF, UDMF hashes are not compatible)

  bool mapHashValid = false;
  VStr cacheFileName;
  VStr cacheDir = getCacheDir();

  // check for UDMF map
  if (W_LumpName(lumpnum+1) == NAME_textmap) {
    LevelFlags |= LF_TextMap;
    NeedNodesBuild = true;
    for (int i = 2; true; ++i) {
      VName LName = W_LumpName(lumpnum+i);
      if (LName == NAME_endmap) break;
      if (LName == NAME_None || LName == NAME_textmap) Host_Error("Map %s is not a valid UDMF map", *MapName);
           if (LName == NAME_behavior) BehaviorLump = lumpnum+i;
      else if (LName == NAME_blockmap) {}
      else if (LName == NAME_reject) RejectLump = lumpnum+i;
      else if (LName == NAME_dialogue) DialogueLump = lumpnum+i;
      else if (LName == NAME_znodes) {}
    }
    mapHashValid = hashLump(nullptr, &md5ctx, lumpnum); // hash map header
    if (mapHashValid) mapHashValid = hashLump(&ripectx, &md5ctx, lumpnum+1); // hash map text
  } else {
    // find all lumps
    int LIdx = lumpnum+1;
    if (W_LumpName(LIdx) == NAME_things) ThingsLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_linedefs) LinesLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_sidedefs) SidesLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_vertexes) VertexesLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_segs) LIdx++;
    if (W_LumpName(LIdx) == NAME_ssectors) LIdx++;
    if (W_LumpName(LIdx) == NAME_nodes) LIdx++;
    if (W_LumpName(LIdx) == NAME_sectors) SectorsLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_reject) RejectLump = LIdx++;
    if (W_LumpName(LIdx) == NAME_blockmap) LIdx++;

    // verify that it's a valid map
    if (ThingsLump == -1 || LinesLump == -1 || SidesLump == -1 ||
        VertexesLump == -1 || SectorsLump == -1)
    {
      VStr nf = "missing lumps:";
      if (ThingsLump == -1) nf += " things";
      if (LinesLump == -1) nf += " lines";
      if (SidesLump == -1) nf += " sides";
      if (VertexesLump == -1) nf += " vertexes";
      if (SectorsLump == -1) nf += " sectors";
      Host_Error("Map '%s' is not a valid map (%s), %s", *MapName, *W_FullLumpName(lumpnum), *nf);
    }

    mapHashValid = hashLump(nullptr, &md5ctx, lumpnum); // map header: only in md5
    if (mapHashValid) mapHashValid = hashLump(nullptr, &md5ctx, ThingsLump); // things: only in md5
    if (mapHashValid) mapHashValid = hashLump(&ripectx, &md5ctx, LinesLump); // lines: both
    if (mapHashValid) mapHashValid = hashLump(&ripectx, &md5ctx, SidesLump); // sides: both
    if (mapHashValid) mapHashValid = hashLump(&ripectx, nullptr, VertexesLump); // vertices: only sha
    if (mapHashValid) mapHashValid = hashLump(&ripectx, &md5ctx, SectorsLump); // sectors: both

    // determine level format
    if (W_LumpName(LIdx) == NAME_behavior) {
      LevelFlags |= LF_Extended;
      BehaviorLump = LIdx++;
      if (mapHashValid) mapHashValid = hashLump(nullptr, &md5ctx, BehaviorLump); // compiled scrips: only md5
    }
  }
  InitTime += Sys_Time();

  if (AuxiliaryMap) GCon->Log("loading map from nested wad");

  if (mapHashValid) {
    vuint8 ripe[RIPEMD160_BYTES];
    ripemd160_finish(&ripectx, ripe);
    MapHash = VStr::buf2hex(ripe, RIPEMD160_BYTES);

    vuint8 md5digest[MD5Context::DIGEST_SIZE];
    md5ctx.Final(md5digest);
    MapHashMD5 = VStr::buf2hex(md5digest, MD5Context::DIGEST_SIZE);

    if (dbg_show_map_hash) {
      GCon->Logf("map hash, md5: %s", *MapHashMD5);
      GCon->Logf("map hash, ripemd-160: %s", *MapHash);
    } else if (developer) {
      GCon->Logf(NAME_Dev, "map hash, md5: %s", *MapHashMD5);
      GCon->Logf(NAME_Dev, "map hash, ripemd-160: %s", *MapHash);
    }
    // BadApple.wad hack
    // md5: 3cca5044d82cf1d1a91eca7933d5b4f6
    if (MapHashMD5 == "3cca5044d82cf1d1a91eca7933d5b4f6") LevelFlags |= LF_IsBadApple;
  }

  bool cachedDataLoaded = false;
  if (mapHashValid && cacheDir.length()) {
    cacheFileName = VStr("mapcache_")+MapHash+".cache";
    cacheFileName = cacheDir+"/"+cacheFileName;
  } else {
    mapHashValid = false;
  }

  bool hasCacheFile = false;

  //FIXME: load cache file into temp buffer, and process it later
  if (mapHashValid) {
    if (killCache) {
      Sys_FileDelete(cacheFileName);
    } else {
      VStream *strm = FL_OpenSysFileRead(cacheFileName);
      hasCacheFile = !!strm;
      VStream::Destroy(strm);
    }
  }

  double VertexTime = 0;
  double SectorsTime = 0;
  double LinesTime = 0;
  double ThingsTime = 0;
  double TranslTime = 0;
  double SidesTime = 0;
  double DecalProcessingTime = 0;
  double FloodFixTime = 0;
  double SectorListTime = 0;
  double MapHashingTime = 0;

  {
    auto texLock = GTextureManager.LockMapLocalTextures();

    // begin processing map lumps
    if (LevelFlags&LF_TextMap) {
      VertexTime = -Sys_Time();
      LoadTextMap(lumpnum+1, MInfo);
      VertexTime += Sys_Time();
    } else {
      // Note: most of this ordering is important
      VertexTime = -Sys_Time();
      LoadVertexes(VertexesLump);
      VertexTime += Sys_Time();
      SectorsTime = -Sys_Time();
      LoadSectors(SectorsLump);
      SectorsTime += Sys_Time();
      LinesTime = -Sys_Time();
      if (!(LevelFlags&LF_Extended)) {
        LoadLineDefs1(LinesLump, MInfo);
        LinesTime += Sys_Time();
        ThingsTime = -Sys_Time();
        LoadThings1(ThingsLump);
      } else {
        LoadLineDefs2(LinesLump, MInfo);
        LinesTime += Sys_Time();
        ThingsTime = -Sys_Time();
        LoadThings2(ThingsLump);
      }
      ThingsTime += Sys_Time();

      TranslTime = -Sys_Time();
      if (!(LevelFlags&LF_Extended)) {
        // translate level to Hexen format
        GGameInfo->eventTranslateLevel(this);
      }
      TranslTime += Sys_Time();
      // set up textures after loading lines because for some Boom line
      // specials there can be special meaning of some texture names
      SidesTime = -Sys_Time();
      LoadSideDefs(SidesLump);
      SidesTime += Sys_Time();
    }
  }

  // do it here, as it fixes bad two-sided lines and such
  // we have to do it before building nodes
  FinaliseLines();

  double Lines2Time = -Sys_Time();
  FixKnownMapErrors();
  Lines2Time += Sys_Time();

  //HACK! fix things skill settings
  SetupThingsFromMapinfo();

  if (hasCacheFile) {
    //GCon->Logf("using cache file: %s", *cacheFileName);
    VStream *strm = FL_OpenSysFileRead(cacheFileName);
    cachedDataLoaded = LoadCachedData(strm);
    if (!cachedDataLoaded) {
      GCon->Logf("cache data is obsolete or in invalid format");
      VStream::Destroy(strm);
      Sys_FileDelete(cacheFileName);
      ClearAllMapData();
      goto load_again;
    }
    VStream::Destroy(strm);
    if (cachedDataLoaded) {
      NeedNodesBuild = false;
      // touch cache file, so it will survive longer
      Sys_Touch(cacheFileName);
    }
  }

  bool forceNewBlockmap = false;
  double NodesTime = -Sys_Time();
  // and again; sorry!
  if (NeedNodesBuild) {
    GCon->Logf("building GL nodes");
    //R_OSDMsgShowSecondary("BUILDING NODES");
    BuildNodes();
    forceNewBlockmap = true;
    saveCachedData = true;
  }
  PostLoadNodes();

  HashSectors();
  HashLines();
  //FinaliseLines(); // done above
  // postload segs MUST be called after `FinaliseLines()`!
  PostLoadSegs();
  PostLoadSubsectors();

  for (int nidx = 0; nidx < NumNodes; ++nidx) Nodes[nidx].index = nidx;

  if (forceNewBlockmap) {
    delete[] BlockMapLump;
    BlockMapLump = nullptr;
    BlockMapLumpSize = 0;
  }

  NodesTime += Sys_Time();

  double BlockMapTime = -Sys_Time();
  if (!BlockMapLump || BlockMapLumpSize <= 0) {
    CreateBlockMap();
    saveCachedData = true;
  }
  vassert(BlockMapLump);
  {
    BlockMapOrgX = BlockMapLump[0];
    BlockMapOrgY = BlockMapLump[1];
    BlockMapWidth = BlockMapLump[2];
    BlockMapHeight = BlockMapLump[3];
    BlockMap = BlockMapLump+4;

    // clear out mobj chains
    int count = BlockMapWidth*BlockMapHeight;
    delete[] BlockLinks;
    BlockLinks = new VEntity *[count];
    memset(BlockLinks, 0, sizeof(VEntity *)*count);
  }
  BlockMapTime += Sys_Time();

  // load reject table
  double RejectTime = -Sys_Time();
  if (!RejectMatrix) {
    LoadReject(RejectLump);
    saveCachedData = true;
  }
  RejectTime += Sys_Time();


  // update cache
  if (loader_cache_data && saveCachedData && mapHashValid && TotalTime+Sys_Time() > loader_cache_time_limit) {
    VStream *strm = FL_OpenSysFileWrite(cacheFileName);
    if (strm) {
      bool err = !SaveCachedData(strm);
      if (strm->IsError()) err = true;
      if (!strm->Close()) err = true;
      delete strm;
      if (err) Sys_FileDelete(cacheFileName);
    }
  }
  doCacheCleanup();


  // ACS object code
  double AcsTime = -Sys_Time();
  LoadACScripts(BehaviorLump, xmaplumpnum);
  AcsTime += Sys_Time();

  double GroupLinesTime = -Sys_Time();
  GroupLines();
  GroupLinesTime += Sys_Time();

  double FloodZonesTime = -Sys_Time();
  FloodZones();
  FloodZonesTime += Sys_Time();

  double ConvTime = -Sys_Time();
  // load conversations
  LoadRogueConScript(GGameInfo->GenericConScript, -1, GenericSpeeches, NumGenericSpeeches);
  if (DialogueLump >= 0) {
    LoadRogueConScript(NAME_None, DialogueLump, LevelSpeeches, NumLevelSpeeches);
  } else {
    LoadRogueConScript(GGameInfo->eventGetConScriptName(MapName), -1, LevelSpeeches, NumLevelSpeeches);
  }
  ConvTime += Sys_Time();

  // "decal processor" must be called before creating polyobjects
  // this is because it creates necessary lists

  // this creates list of segs for each line
  DecalProcessingTime = -Sys_Time();
  PostProcessForDecals();
  DecalProcessingTime += Sys_Time();

  //GCon->Logf("Building Lidedef VV list");
  double LineVVListTime = -Sys_Time();
  BuildDecalsVVList();
  LineVVListTime += Sys_Time();

  // set up polyobjs, slopes, 3D floors and some other static stuff
  GCon->Log("spawning the world...");
  double SpawnWorldTime = -Sys_Time();
  GGameInfo->eventSpawnWorld(this);
  // hash it all again, 'cause spawner may change something
  HashSectors();
  HashLines();
  SpawnWorldTime += Sys_Time();
  GCon->Log("world spawning complete");

  double InitPolysTime = -Sys_Time();
  InitPolyobjs(); // Initialise the polyobjs
  if (NumPolyObjs) {
    // hash it all again, 'cause spawner may change something
    HashSectors();
    HashLines();
  }
  InitPolysTime += Sys_Time();

  double MinMaxTime = -Sys_Time();
  // we need this for client
  for (int i = 0; i < NumSectors; ++i) {
    //GCon->Logf("MINMAX: %d/%d %3d%%", i, NumSectors, 100*i/NumSectors);
    CalcSecMinMaxs(&Sectors[i]/*, true*/);
  }
  MinMaxTime += Sys_Time();

  // fake contrast
  double WallShadesTime = -Sys_Time();
  const int fctype = (r_fakecontrast_ignore_mapinfo || MInfo.FakeContrast == 0 ? r_fakecontrast.asInt() : (MInfo.FakeContrast+1)%3);
  if (fctype > 0 && (MInfo.HorizWallShade|MInfo.VertWallShade) != 0) {
    for (auto &&line : allLines()) {
      const int shadeChange =
        !line.normal.x ? MInfo.HorizWallShade :
        !line.normal.y ? MInfo.VertWallShade :
        0;
      const int smoothChange =
        !line.normal.x ? 0 :
        (int)(MInfo.HorizWallShade+fabsf(atanf(line.normal.y/line.normal.x)/1.57079f)*(MInfo.VertWallShade-MInfo.HorizWallShade)); // xs_RoundToInt()
      if (shadeChange || smoothChange) {
        for (int sn = 0; sn < 2; ++sn) {
          const int sidx = line.sidenum[sn];
          if (sidx >= 0) {
            side_t *side = &Sides[sidx];
            if (side->Flags&SDF_NOFAKECTX) continue; // UDMF flag
            if (side->Flags&SDF_SMOOTHLIT) { side->Light += smoothChange; continue; } // UDMF flag
            // apply mapinfo-defined shading
            side->Light += (fctype > 1 ? smoothChange : shadeChange);
          }
        }
      }
    }
  }
  WallShadesTime += Sys_Time();

  double RepBaseTime = -Sys_Time();
  CreateRepBase();
  RepBaseTime += Sys_Time();

  // end of map lump processing
  if (AuxiliaryMap || auxCloser.doCloseAux) {
    // close the auxiliary file(s)
    auxCloser.doCloseAux = false;
    W_CloseAuxiliary();
  }

  // this will remove polyobject and invalid lines from blockmap
  // it will also fill blockmap subsector info
  CleanupBlockMap();

  // do it here, so it won't touch sloped floors
  // it will set `othersec` for sectors too
  // also, it will detect "transparent door" sectors
  FloodFixTime = -Sys_Time();
  DetectHiddenSectors();
  FixTransparentDoors();
  FixDeepWaters();
  FloodFixTime += Sys_Time();

  // this must be called after deepwater fixes
  SectorListTime = -Sys_Time();
  BuildSectorLists();
  SectorListTime += Sys_Time();

  // calculate xxHash32 of various map parts

  // hash of linedefs, sidedefs, sectors (in this order)
  MapHashingTime = -Sys_Time();
  {
    //GCon->Logf("*** LSSHash: 0x%08x (%d:%d:%d)", LSSHash, NumLines, NumSides, NumSectors);
    //XXH32_state_t *lssXXHash = XXH32_createState();
    XXH32_state_t lssXXHash;
    XXH32_reset(&lssXXHash, (unsigned)(NumLines+NumSides+NumSectors));
    for (int f = 0; f < NumLines; ++f) xxHashLinedef(&lssXXHash, Lines[f]);
    for (int f = 0; f < NumSides; ++f) xxHashSidedef(&lssXXHash, Sides[f]);
    for (int f = 0; f < NumSectors; ++f) xxHashSectordef(&lssXXHash, Sectors[f]);
    LSSHash = XXH32_digest(&lssXXHash);
    //XXH32_freeState(lssXXHash);
    //GCon->Logf("*** LSSHash: 0x%08x", LSSHash);
  }

  // hash of segs
  {
    //GCon->Logf("*** SegHash: 0x%08x (%d)", SegHash, NumSegs);
    //XXH32_state_t *segXXHash = XXH32_createState();
    XXH32_state_t segXXHash;
    XXH32_reset(&segXXHash, (unsigned)NumSegs);
    for (int f = 0; f < NumSegs; ++f) xxHashSegdef(&segXXHash, Segs[f]);
    SegHash = XXH32_digest(&segXXHash);
    //XXH32_freeState(segXXHash);
    //GCon->Logf("*** SegHash: 0x%08x", SegHash);
  }
  MapHashingTime += Sys_Time();


  TotalTime += Sys_Time();

  AddLoadingTiming("Level loaded in", TotalTime);
  AddLoadingTiming("Initialisation", InitTime);
  AddLoadingTiming("Vertexes", VertexTime);
  AddLoadingTiming("Sectors", SectorsTime);
  AddLoadingTiming("Lines", LinesTime);
  AddLoadingTiming("Things", ThingsTime);
  AddLoadingTiming("Translation", TranslTime);
  AddLoadingTiming("Sides", SidesTime);
  AddLoadingTiming("Error fixing", Lines2Time);
  AddLoadingTiming("Nodes", NodesTime);
  AddLoadingTiming("Blockmap", BlockMapTime);
  AddLoadingTiming("ACS loading", AcsTime);
  AddLoadingTiming("Group lines", GroupLinesTime);
  AddLoadingTiming("Flood zones", FloodZonesTime);
  AddLoadingTiming("Conversations", ConvTime);
  AddLoadingTiming("Reject", RejectTime);
  AddLoadingTiming("Spawn world", SpawnWorldTime);
  AddLoadingTiming("Polyobjs", InitPolysTime);
  AddLoadingTiming("Sector minmaxs", MinMaxTime);
  AddLoadingTiming("Wall shades", WallShadesTime);
  AddLoadingTiming("Linedef VV list", LineVVListTime);
  AddLoadingTiming("Decal processing", DecalProcessingTime);
  AddLoadingTiming("Sector min/max", MinMaxTime);
  AddLoadingTiming("Floodbug fixing", FloodFixTime);
  AddLoadingTiming("Sector lists", SectorListTime);
  AddLoadingTiming("Map hashing", MapHashingTime);

  DumpLoadingTimings();

  mapTextureWarns.clear();

  RecalcWorldBBoxes();

  cacheFileBase = cacheFileName;

  #if 0
  // nope, some lines don't need fullsegs
  for (auto &&ld : allLines()) {
    // setup side references
    vassert((ld.sidenum[0] < 0 && !ld.frontside) || (ld.sidenum[0] >= 0 && ld.frontside));
    vassert((ld.sidenum[1] < 0 && !ld.backside) || (ld.sidenum[1] >= 0 && ld.backside));
  }
  #endif

  eventAfterLevelLoaded();
}


//==========================================================================
//
//  texForceLoad
//
//==========================================================================
static int texForceLoad (const char *name, int Type, bool CMap) {
  if (!name || !name[0]) return 0; // just in case
  if (name[0] == '-' && !name[1]) return 0; // just in case
  //if (VStr::strEquCI(name, "aashitty") || VStr::strEquCI(name, "aastinky")) return 0;
  VName tmpname = VName(name, VName::FindLower);
  if (tmpname != NAME_None && VTextureManager::IsDummyTextureName(tmpname)) return 0;
  int i = -1;

  //GCon->Logf("texForceLoad(*): <%s>", name);

  #if 0
  VName loname = NAME_None;
  // try filename if slash is found
  const char *slash = strchr(name, '/');
  const char *dot = nullptr;
  for (int f = 0; name[f]; ++f) if (name[f] == '.') dot = name+f;
  if (slash && slash[1]) {
    loname = VName(slash+1, VName::AddLower);
    //GCon->Logf("texForceLoad(**): <%s>", *loname);
    //i = GTextureManager.AddFileTextureChecked(loname, Type);
  } else if (!dot) {
    loname = VName(name, VName::AddLower);
    //GCon->Logf("texForceLoad(***): <%s>", *loname);
    //i = GTextureManager.AddFileTextureChecked(loname, Type);
  } else if (dot) {
    loname = VName(dot+1, VName::AddLower);
  }

  if (loname != NAME_None) {
    i = GTextureManager.CheckNumForName(loname, Type, true);
    if (i >= 0) return i;
    if (CMap) return 0;
    if (!allowForceLoad) return GTextureManager.DefaultTexture;
    //i = GTextureManager.AddFileTextureChecked(loname, Type);
    //if (i != -1) GCon->Logf("texForceLoad(0): <%s><%s> (%d)", *loname, name, i);
  }

  if (i == -1) {
    VName loname8((dot ? dot+1 : slash ? slash+1 : name), VName::AddLower8);
    i = GTextureManager.CheckNumForName(loname8, Type, true);
    //if (i != -1) GCon->Logf("texForceLoad(1): <%s><%s> (%d)", *loname8, name, i);
    if (i == -1 && CMap) return 0;
  }

  //if (i == -1) i = GTextureManager.CheckNumForName(VName(name, VName::AddLower), Type, true, true);
  //if (i == -1 && VStr::length(name) > 8) i = GTextureManager.AddFileTexture(VName(name, VName::AddLower), Type);

  if (i == -1 /*&& !slash*/ && allowForceLoad) {
    //GCon->Logf("texForceLoad(x): <%s>", name);
    /*
    if (!slash && loname != NAME_None) {
      if (loname == "ftub3") {
        GCon->Log("===========");
        i = GTextureManager.CheckNumForName(loname, Type, true);
        //i = GTextureManager.CheckNumForName(loname, TEXTYPE_Flat, false);
        GCon->Logf("*********** FTUB3 (%s)! i=%d", VTexture::TexTypeToStr(Type), i);
      }
    }
    */
    if (i == -1) i = GTextureManager.AddFileTextureChecked(VName(name, VName::AddLower), Type);
    //if (i != -1) GCon->Logf("texForceLoad(2): <%s> (%d)", name, i);
    if (i == -1 && loname != NAME_None) {
      i = GTextureManager.AddFileTextureChecked(loname, Type);
      //if (i != -1) GCon->Logf("texForceLoad(3): <%s> (%d)", name, i);
    }
  }
  #else
  i = GTextureManager.FindOrLoadFullyNamedTexture(VStr(name), nullptr, Type, /*overload*/true, /*silent*/true);
  #endif

  if (i == -1) {
    VStr nn = VStr(name);
    if (!mapTextureWarns.has(nn)) {
      mapTextureWarns.put(nn, true);
      GCon->Logf(NAME_Warning, "MAP TEXTURE NOT FOUND: '%s'", name);
    }
    i = (CMap ? 0 : GTextureManager.DefaultTexture);
  }
  return i;
}


//==========================================================================
//
//  LdrTexNumForName
//
//  native int LdrTexNumForName (string name, int Type, optional bool CMap);
//
//==========================================================================
IMPLEMENT_FUNCTION(VLevel, LdrTexNumForName) {
  VStr name;
  int Type;
  VOptParamBool CMap(false);
  vobjGetParamSelf(name, Type, CMap);
  RET_INT(Self->TexNumForName(*name, Type, CMap));
}


//==========================================================================
//
//  VLevel::TexNumForName
//
//  Retrieval, get a texture or flat number for a name.
//
//==========================================================================
int VLevel::TexNumForName (const char *name, int Type, bool CMap) const {
  return texForceLoad(name, Type, CMap);
}


//==========================================================================
//
//  VLevel::TexNumOrColor
//
//==========================================================================
int VLevel::TexNumOrColor (const char *name, int Type, bool &GotColor, vuint32 &Col) const {
  if (VStr::strEquCI(name, "WATERMAP")) {
    GotColor = true;
    Col = M_ParseColor("#004FA5");
    Col = (Col&0xffffffU)|0x80000000U;
    return 0;
  }
  VName Name(name, VName::FindLower8);
  int i = (Name != NAME_None ? GTextureManager.CheckNumForName(Name, Type, true) : -1);
  if (i == -1) {
    char tmpname[9];
    strncpy(tmpname, name, 8);
    tmpname[8] = 0;
    Col = M_ParseColor(tmpname, true); // return zero if invalid
    if (Col == 0) {
      if (tmpname[0] == '#') tmpname[7] = 0; else tmpname[6] = 0;
      Col = M_ParseColor(tmpname, true); // return zero if invalid
    }
    GotColor = (Col != 0);
    Col &= 0xffffffU; // so it won't be fullbright
    i = 0;
  } else {
    GotColor = false;
    Col = 0;
  }
  return i;
}


//==========================================================================
//
//  VLevel::LoadRogueConScript
//
//==========================================================================
void VLevel::LoadRogueConScript (VName LumpName, int ALumpNum, FRogueConSpeech *&SpeechList, int &NumSpeeches) const {
  bool teaser = false;
  // clear variables
  SpeechList = nullptr;
  NumSpeeches = 0;

  int LumpNum = ALumpNum;
  if (LumpNum < 0) {
    // check for empty name
    if (LumpName == NAME_None) return;
    // get lump num
    LumpNum = W_CheckNumForName(LumpName);
    if (LumpNum < 0) return; // not here
  }

  // load them

  // first check the size of the lump, if it's 1516,
  // we are using a registered strife lump, if it's
  // 1488, then it's a teaser conversation script
  if (W_LumpLength(LumpNum)%1516 != 0) {
    NumSpeeches = W_LumpLength(LumpNum)/1488;
    teaser = true;
  } else {
    NumSpeeches = W_LumpLength(LumpNum)/1516;
  }

  SpeechList = new FRogueConSpeech[NumSpeeches];

  VStream *lumpstream = W_CreateLumpReaderNum(LumpNum);
  VCheckedStream Strm(lumpstream);
  for (int i = 0; i < NumSpeeches; ++i) {
    char Tmp[324];

    FRogueConSpeech &S = SpeechList[i];
    if (!teaser) {
      // parse non teaser speech
      Strm << S.SpeakerID << S.DropItem << S.CheckItem1 << S.CheckItem2
        << S.CheckItem3 << S.JumpToConv;

      // parse NPC name
      Strm.Serialise(Tmp, 16);
      Tmp[16] = 0;
      S.Name = Tmp;

      // parse sound name (if any)
      Strm.Serialise(Tmp, 8);
      Tmp[8] = 0;
      S.Voice = VName(Tmp, VName::AddLower8);
      if (S.Voice != NAME_None) S.Voice = va("svox/%s", *S.Voice);

      // parse backdrop pics (if any)
      Strm.Serialise(Tmp, 8);
      Tmp[8] = 0;
      S.BackPic = VName(Tmp, VName::AddLower8);

      // parse speech text
      Strm.Serialise(Tmp, 320);
      Tmp[320] = 0;
      S.Text = Tmp;
    } else {
      // parse teaser speech, which doesn't contain all fields
      Strm << S.SpeakerID << S.DropItem;

      // parse NPC name
      Strm.Serialise(Tmp, 16);
      Tmp[16] = 0;
      S.Name = Tmp;

      // parse sound number (if any)
      vint32 Num;
      Strm << Num;
      if (Num) S.Voice = va("svox/voc%d", Num);

      // also, teaser speeches don't have backdrop pics
      S.BackPic = NAME_None;

      // parse speech text
      Strm.Serialise(Tmp, 320);
      Tmp[320] = 0;
      S.Text = Tmp;
    }

    // parse conversation options for PC
    for (int j = 0; j < 5; ++j) {
      FRogueConChoice &C = S.Choices[j];
      Strm << C.GiveItem << C.NeedItem1 << C.NeedItem2 << C.NeedItem3
        << C.NeedAmount1 << C.NeedAmount2 << C.NeedAmount3;
      Strm.Serialise(Tmp, 32);
      Tmp[32] = 0;
      C.Text = Tmp;
      Strm.Serialise(Tmp, 80);
      Tmp[80] = 0;
      C.TextOK = Tmp;
      Strm << C.Next << C.Objectives;
      Strm.Serialise(Tmp, 80);
      Tmp[80] = 0;
      C.TextNo = Tmp;
    }
  }
}


//==========================================================================
//
//  VLevel::CreateRepBase
//
//==========================================================================
void VLevel::CreateRepBase () {
  BaseLines = new rep_line_t[NumLines];
  for (int i = 0; i < NumLines; ++i) {
    line_t &L = Lines[i];
    rep_line_t &B = BaseLines[i];
    B.alpha = L.alpha;
  }

  BaseSides = new rep_side_t[NumSides];
  for (int i = 0; i < NumSides; ++i) {
    side_t &S = Sides[i];
    rep_side_t &B = BaseSides[i];
    B.Top.TextureOffset = S.Top.TextureOffset;
    B.Bot.TextureOffset = S.Bot.TextureOffset;
    B.Mid.TextureOffset = S.Mid.TextureOffset;
    B.Top.RowOffset = S.Top.RowOffset;
    B.Bot.RowOffset = S.Bot.RowOffset;
    B.Mid.RowOffset = S.Mid.RowOffset;
    B.TopTexture = S.TopTexture;
    B.BottomTexture = S.BottomTexture;
    B.MidTexture = S.MidTexture;
    B.Top.ScaleX = S.Top.ScaleX;
    B.Top.ScaleY = S.Top.ScaleY;
    B.Bot.ScaleX = S.Bot.ScaleX;
    B.Bot.ScaleY = S.Bot.ScaleY;
    B.Mid.ScaleX = S.Mid.ScaleX;
    B.Mid.ScaleY = S.Mid.ScaleY;
    B.Flags = S.Flags;
    B.Light = S.Light;
  }

  BaseSectors = new rep_sector_t[NumSectors];
  for (int i = 0; i < NumSectors; ++i) {
    sector_t &S = Sectors[i];
    rep_sector_t &B = BaseSectors[i];
    B.floor_pic = S.floor.pic;
    B.floor_dist = S.floor.dist;
    B.floor_xoffs = S.floor.xoffs;
    B.floor_yoffs = S.floor.yoffs;
    B.floor_XScale = S.floor.XScale;
    B.floor_YScale = S.floor.YScale;
    B.floor_Angle = S.floor.Angle;
    B.floor_BaseAngle = S.floor.BaseAngle;
    B.floor_BaseXOffs = S.floor.BaseXOffs;
    B.floor_BaseYOffs = S.floor.BaseYOffs;
    B.floor_SkyBox = nullptr;
    B.floor_MirrorAlpha = S.floor.MirrorAlpha;
    B.ceiling_pic = S.ceiling.pic;
    B.ceiling_dist = S.ceiling.dist;
    B.ceiling_xoffs = S.ceiling.xoffs;
    B.ceiling_yoffs = S.ceiling.yoffs;
    B.ceiling_XScale = S.ceiling.XScale;
    B.ceiling_YScale = S.ceiling.YScale;
    B.ceiling_Angle = S.ceiling.Angle;
    B.ceiling_BaseAngle = S.ceiling.BaseAngle;
    B.ceiling_BaseXOffs = S.ceiling.BaseXOffs;
    B.ceiling_BaseYOffs = S.ceiling.BaseYOffs;
    B.ceiling_SkyBox = nullptr;
    B.ceiling_MirrorAlpha = S.ceiling.MirrorAlpha;
    B.Sky = S.Sky;
    B.params = S.params;
  }

  BasePolyObjs = new rep_polyobj_t[NumPolyObjs];
  for (int i = 0; i < NumPolyObjs; ++i) {
    polyobj_t *P = PolyObjs[i];
    rep_polyobj_t &B = BasePolyObjs[i];
    B.startSpot = P->startSpot;
    B.angle = P->angle;
  }
}


//==========================================================================
//
//  VLevel::FloodZones
//
//==========================================================================
void VLevel::FloodZones () {
  for (int i = 0; i < NumSectors; ++i) {
    if (Sectors[i].Zone == -1) {
      FloodZone(&Sectors[i], NumZones);
      ++NumZones;
    }
  }

  Zones = new vint32[NumZones];
  for (int i = 0; i < NumZones; ++i) Zones[i] = 0;
}


//==========================================================================
//
//  VLevel::FloodZone
//
//==========================================================================
void VLevel::FloodZone (sector_t *Sec, int Zone) {
  Sec->Zone = Zone;
  for (int i = 0; i < Sec->linecount; ++i) {
    line_t *Line = Sec->lines[i];
    if (Line->flags&ML_ZONEBOUNDARY) continue;
    if (Line->frontsector && Line->frontsector->Zone == -1) FloodZone(Line->frontsector, Zone);
    if (Line->backsector && Line->backsector->Zone == -1) FloodZone(Line->backsector, Zone);
  }
}
