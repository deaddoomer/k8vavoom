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
#include "../gamedefs.h"
#include "../text.h"
#include "../server/server.h"
#include "../client/client.h"
#include "r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
static int constexpr cestlen (const char *s, int pos=0) noexcept { return (s && s[pos] ? 1+cestlen(s, pos+1) : 0); }
static constexpr const char *LMAP_CACHE_DATA_SIGNATURE = "VAVOOM CACHED LMAP VERSION 002.\n";
enum { CDSLEN = cestlen(LMAP_CACHE_DATA_SIGNATURE) };
static_assert(CDSLEN == 32, "oops!");


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB r_precalc_static_lights;
extern int r_precalc_static_lights_override; // <0: not set

extern VCvarB r_lmap_recalc_static;
extern VCvarB r_lmap_bsp_trace_static;

extern VCvarB loader_cache_data;
VCvarF loader_cache_time_limit_lightmap("loader_cache_time_limit_lightmap", "3", "Cache lightmap data if building took more than this number of seconds.", CVAR_Archive);
VCvarI loader_cache_compression_level_lightmap("loader_cache_compression_level_lightmap", "6", "Lightmap cache file compression level [0..9]", CVAR_Archive);

static VCvarB dbg_cache_lightmap_always("dbg_cache_lightmap_always", false, "Always cache lightmaps?", /*CVAR_Archive|*/CVAR_PreInit);
static VCvarB dbg_cache_lightmap_dump_missing("dbg_cache_lightmap_dump_missing", false, "Dump missing lightmaps?", /*CVAR_Archive|*/CVAR_PreInit);


// ////////////////////////////////////////////////////////////////////////// //
extern int light_mem;


//**************************************************************************
//
// lightmap cache saving
//
//**************************************************************************

//==========================================================================
//
//  VRenderLevelLightmap::isNeedLightmapCache
//
//==========================================================================
bool VRenderLevelLightmap::isNeedLightmapCache () const noexcept {
  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
struct SurfaceInfoBlock {
  TPlane plane;
  subsector_t *subsector; // owning subsector
  seg_t *seg; // owning seg (can be `nullptr` for floor/ceiling)
  vint32 subidx;
  vint32 segidx;
  vuint32 typeFlags; // TF_xxx
  vuint32 lmsize, lmrgbsize; // to track static lightmap memory
  vuint8 *lightmap;
  rgb_t *lightmap_rgb;
  vint32 count;
  vint32 texturemins[2];
  vint32 extents[2];
  TVec vert0; // dynamic array
  bool lmowned;

  SurfaceInfoBlock () noexcept
    : subsector(nullptr)
    , seg(nullptr)
    , subidx(-1)
    , segidx(-1)
    , typeFlags(0)
    , lmsize(0)
    , lmrgbsize(0)
    , lightmap(nullptr)
    , lightmap_rgb(nullptr)
    , count(0)
    , vert0(0, 0, 0)
    , lmowned(false)
  {
    plane.normal = TVec(0, 0, 0);
    plane.dist = 0;
    texturemins[0] = texturemins[1] = 0;
    extents[0] = extents[1] = 0;
  }

  ~SurfaceInfoBlock () noexcept { clear(); }

  inline bool isValid () const noexcept { return (count > 0); }

  inline void disownLightmaps () noexcept { lmowned = false; }

  void dump () {
    GCon->Log(NAME_Debug, "=== SURFACE INFO ===");
    GCon->Logf(NAME_Debug, "  plane: (%g,%g,%g) : %g", plane.normal.x, plane.normal.y, plane.normal.z, plane.dist);
    GCon->Logf(NAME_Debug, "  subsector/seg: %d / %d", subidx, segidx);
    GCon->Logf(NAME_Debug, "  typeFlags: %u", typeFlags);
    GCon->Logf(NAME_Debug, "  vertices: %d : (%g,%g,%g)", count, vert0.x, vert0.y, vert0.z);
    GCon->Logf(NAME_Debug, "  tmins/extents: (%d,%d) / (%d,%d)", texturemins[0], texturemins[1], extents[0], extents[1]);
  }

  void clear () noexcept {
    if (lmowned) {
      if (lightmap) Z_Free(lightmap);
      if (lightmap_rgb) Z_Free(lightmap_rgb);
    }
    subsector = nullptr;
    seg = nullptr;
    subidx = -1;
    segidx = -1;
    typeFlags = 0;
    lmsize = lmrgbsize = 0;
    lightmap = nullptr;
    lightmap_rgb = nullptr;
    count = 0;
    vert0 = TVec(0, 0, 0);
    plane.normal = TVec(0, 0, 0);
    plane.dist = 0;
    texturemins[0] = texturemins[1] = 0;
    extents[0] = extents[1] = 0;
    lmowned = false;
  }

  inline bool equalTo (const surface_t *sfc) noexcept {
    if (!sfc || count < 1 || count != sfc->count) return false;
    return
      typeFlags == sfc->typeFlags &&
      texturemins[0] == sfc->texturemins[0] &&
      texturemins[1] == sfc->texturemins[1] &&
      extents[0] == sfc->extents[0] &&
      extents[1] == sfc->extents[1] &&
      subsector == sfc->subsector &&
      seg == sfc->seg &&
      plane.normal == sfc->plane.normal &&
      plane.dist == sfc->plane.dist &&
      vert0 == sfc->verts[0].vec();
  }

  void initWith (VLevel *Level, const surface_t *sfc) noexcept {
    vassert(sfc);
    clear();
    plane = sfc->plane;
    subsector = sfc->subsector;
    subidx = (sfc->subsector ? (vint32)(ptrdiff_t)(sfc->subsector-&Level->Subsectors[0]) : -1);
    seg = sfc->seg;
    segidx = (sfc->seg ? (vint32)(ptrdiff_t)(sfc->seg-&Level->Segs[0]) : -1);
    typeFlags = sfc->typeFlags;
    if (sfc->lightmap) {
      lmsize = sfc->lmsize;
      lightmap = sfc->lightmap;
      if (sfc->lightmap_rgb) {
        lmrgbsize = sfc->lmrgbsize;
        lightmap_rgb = sfc->lightmap_rgb;
      } else {
        lmrgbsize = 0;
        lightmap_rgb = nullptr;
      }
    } else {
      lmsize = 0;
      lightmap = nullptr;
      lmrgbsize = 0;
      lightmap_rgb = nullptr;
    }
    count = sfc->count;
    vert0 = (sfc->count > 0 ? sfc->verts[0].vec() : TVec(0, 0, 0));
    texturemins[0] = sfc->texturemins[0];
    texturemins[1] = sfc->texturemins[1];
    extents[0] = sfc->extents[0];
    extents[1] = sfc->extents[1];
    lmowned = false;
  }

  // must be initialised with `initWith()`
  void writeTo (VStream *strm, VLevel *Level) {
    vassert(strm);
    vassert(!strm->IsLoading());
    vuint8 flag = 0;
    // monochrome lightmap is always there when rgb lightmap is there
    if (!lightmap) {
      vassert(!lightmap_rgb);
    } else {
      flag |= 1u;
      vassert(lmsize > 0);
      if (lightmap_rgb) {
        vassert(lmrgbsize > 0);
        flag |= 2u;
      }
    }
    *strm << flag;
    // surface check data
    // plane
    *strm << plane.normal.x << plane.normal.y << plane.normal.z << plane.dist;
    // subsector
    *strm << subidx;
    // seg
    *strm << segidx;
    // type
    *strm << typeFlags;
    // vertices
    *strm << count;
    // extents
    *strm << texturemins[0];
    *strm << texturemins[1];
    *strm << extents[0];
    *strm << extents[1];
    // first vertex
    *strm << vert0.x << vert0.y << vert0.z;
    // write lightmaps
    if (flag&1u) {
      *strm << lmsize;
      strm->Serialise(lightmap, lmsize);
      if (flag&2u) {
        *strm << lmrgbsize;
        strm->Serialise(lightmap_rgb, lmrgbsize);
      }
    }
  }

  // must be initialised with `initWith()`
  bool readFrom (VStream *strm, VLevel *Level) {
    vassert(strm);
    vassert(strm->IsLoading());
    clear();
    lmowned = true;
    vuint8 flag = 0xff;
    *strm << flag;
    if ((flag&~3u) != 0 || flag == 2u) { GCon->Log(NAME_Warning, "invalid lightmap cache surface flags"); return false; }
    //if (!flag) return true; // it is valid, but empty
    // plane
    *strm << plane.normal.x << plane.normal.y << plane.normal.z << plane.dist;
    // subsector
    *strm << subidx;
    // seg
    *strm << segidx;
    // type
    *strm << typeFlags;
    // vertices
    *strm << count;
    // extents
    *strm << texturemins[0];
    *strm << texturemins[1];
    *strm << extents[0];
    *strm << extents[1];
    // first vertex
    *strm << vert0.x << vert0.y << vert0.z;
    // lightmaps
    if (flag&1u) {
      *strm << lmsize;
      if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); clear(); return false; }
      if (count < 0) { GCon->Log(NAME_Warning, "invalid lightmap cache surface vertex count"); clear(); return false; }
      if (lmsize == 0 || lmsize > BLOCK_WIDTH*BLOCK_HEIGHT) { GCon->Log(NAME_Warning, "invalid lightmap cache surface lightmap size"); clear(); return false; }
      lightmap = (vuint8 *)Z_Malloc(lmsize);
      strm->Serialise(lightmap, lmsize);
      if (flag&2u) {
        *strm << lmrgbsize;
        if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); clear(); return false; }
        if (lmrgbsize == 0 || lmrgbsize > BLOCK_WIDTH*BLOCK_HEIGHT*3) { GCon->Log(NAME_Warning, "invalid lightmap cache surface lightmap size"); clear(); return false; }
        lightmap_rgb = (rgb_t *)Z_Malloc(lmrgbsize);
        strm->Serialise(lightmap_rgb, lmrgbsize);
      }
    }
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); clear(); return false; }
    // link to subsector
         if (subidx == -1) subsector = nullptr;
    else if (subidx < 0 || subidx >= Level->NumSubsectors) { GCon->Log(NAME_Warning, "invalid lightmap cache surface subsector"); clear(); return false; }
    else subsector = &Level->Subsectors[subidx];
    // link to seg
         if (segidx == -1) seg = nullptr;
    else if (segidx < 0 || segidx >= Level->NumSegs) { GCon->Log(NAME_Warning, "invalid lightmap cache surface seg"); clear(); return false; }
    else seg = &Level->Segs[segidx];
    // done
    return true;
  }
};


//==========================================================================
//
//  WriteSurfaceLightmaps
//
//==========================================================================
static void WriteSurfaceLightmaps (VLevel *Level, VStream *strm, surface_t *s) {
  vuint32 cnt = 0;
  for (surface_t *t = s; t; t = t->next) ++cnt;
  *strm << cnt;
  SurfaceInfoBlock sib;
  for (; s; s = s->next) {
    sib.initWith(Level, s);
    sib.writeTo(strm, Level);
  }
}


//==========================================================================
//
//  WriteSegLightmaps
//
//==========================================================================
static void WriteSegLightmaps (VLevel *Level, VStream *strm, segpart_t *sp) {
  vuint32 cnt = 0;
  for (segpart_t *t = sp; t; t = t->next) ++cnt;
  *strm << cnt;
  for (; sp; sp = sp->next) WriteSurfaceLightmaps(Level, strm, sp->surfs);
}


//==========================================================================
//
//  VRenderLevelLightmap::saveLightmapsInternal
//
//==========================================================================
void VRenderLevelLightmap::saveLightmapsInternal (VStream *strm) {
  if (!strm) return;
  vuint32 surfCount = CountAllSurfaces();

  *strm << Level->NumSectors;
  *strm << Level->NumSubsectors;
  *strm << Level->NumSegs;
  *strm << surfCount;

  /*
  vuint32 atlasW = BLOCK_WIDTH, atlasH = BLOCK_HEIGHT;
  *strm << atlasW << atlasH;
  */

  for (int i = 0; i < Level->NumSubsectors; ++i) {
    subsector_t *sub = &Level->Subsectors[i];
    vuint32 ssnum = (vuint32)i;
    *strm << ssnum;
    // count regions (so we can skip them if necessary)
    vuint32 regcount = 0;
    for (subregion_t *r = sub->regions; r != nullptr; r = r->next) ++regcount;
    *strm << regcount;
    regcount = 0;
    for (subregion_t *r = sub->regions; r != nullptr; r = r->next, ++regcount) {
      *strm << regcount;
      WriteSurfaceLightmaps(Level, strm, (r->realfloor ? r->realfloor->surfs : nullptr));
      WriteSurfaceLightmaps(Level, strm, (r->realceil ? r->realceil->surfs : nullptr));
      WriteSurfaceLightmaps(Level, strm, (r->fakefloor ? r->fakefloor->surfs : nullptr));
      WriteSurfaceLightmaps(Level, strm, (r->fakeceil ? r->fakeceil->surfs : nullptr));
    }
    if (i%128 == 0) NET_SendNetworkHeartbeat();
  }

  for (int i = 0; i < Level->NumSegs; ++i) {
    seg_t *seg = &Level->Segs[i];
    vuint32 snum = (vuint32)i;
    *strm << snum;
    vuint8 dscount = (seg->drawsegs ? 1 : 0);
    *strm << dscount;
    if (seg->drawsegs) {
      drawseg_t *ds = seg->drawsegs;
      WriteSegLightmaps(Level, strm, ds->top);
      WriteSegLightmaps(Level, strm, ds->mid);
      WriteSegLightmaps(Level, strm, ds->bot);
      WriteSegLightmaps(Level, strm, ds->topsky);
      WriteSegLightmaps(Level, strm, ds->extra);
    }
    if (i%128 == 0) NET_SendNetworkHeartbeat();
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::saveLightmaps
//
//==========================================================================
void VRenderLevelLightmap::saveLightmaps (VStream *strm) {
  if (!strm) return;
  strm->Serialise(LMAP_CACHE_DATA_SIGNATURE, CDSLEN);
  VZLibStreamWriter *zipstrm = new VZLibStreamWriter(strm, (int)loader_cache_compression_level_lightmap);
  saveLightmapsInternal(zipstrm);
  zipstrm->Close();
  delete zipstrm;
}


//**************************************************************************
//
// lightmap cache loading
//
//**************************************************************************

//==========================================================================
//
//  SkipLightSurfaces
//
//==========================================================================
static bool SkipLightSurfaces (VLevel *Level, VStream *strm, vuint32 *number=nullptr) {
  vuint32 rd = 0;
  if (number) {
    rd = *number;
  } else {
    *strm << rd;
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
  }
  if (rd > 1024*1024) { GCon->Logf(NAME_Warning, "invalid lightmap cache surface chain count (%u)", rd); return false; }
  SurfaceInfoBlock sib;
  while (rd--) {
    sib.clear();
    if (!sib.readFrom(strm, Level)) return false;
    sib.clear();
  }
  return true;
}


//==========================================================================
//
//  SkipLightSegSurfaces
//
//==========================================================================
static bool SkipLightSegSurfaces (VLevel *Level, VStream *strm, vuint32 *number=nullptr) {
  vuint32 rd = 0;
  if (number) {
    rd = *number;
  } else {
    *strm << rd;
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
  }
  while (rd--) {
    if (!SkipLightSurfaces(Level, strm)) return false;
  }
  return true;
}


//==========================================================================
//
//  LoadLightSurfaces
//
//==========================================================================
static bool LoadLightSurfaces (VLevel *Level, VStream *strm, surface_t *s, unsigned &lmcacheUnknownSurfaceCount, bool &missingWarned) {
  vuint32 cnt = 0, rd = 0;
  for (surface_t *t = s; t; t = t->next) ++cnt;
  *strm << rd;
  if (rd != cnt) {
    GCon->Logf(NAME_Warning, "invalid lightmap cache surface chain count (%u instead of %u)", rd, cnt);
    //return false;
  }
  if (rd > 1024*1024) { GCon->Logf(NAME_Warning, "invalid lightmap cache surface chain count (%u)", rd); return false; }
  // just in case
  if (rd == 0) {
    lmcacheUnknownSurfaceCount += cnt;
    return true;
  }
  if (cnt == 0) return SkipLightSurfaces(Level, strm, &rd);
  // load surfaces
  TArray<SurfaceInfoBlock> sibs;
  sibs.setLength((int)rd);
  #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
  GCon->Logf(NAME_Debug, "============== ON-DISK (%u) ==============", rd);
  #endif
  for (int f = 0; f < (int)rd; ++f) {
    if (!sibs[f].readFrom(strm, Level)) return false;
    #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
    sibs[f].dump();
    #endif
  }
  #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
  GCon->Logf(NAME_Debug, "============== MEMORY (%u) ==============", cnt);
  for (surface_t *t = s; t; t = t->next) {
    SurfaceInfoBlock sss;
    sss.initWith(Level, t);
    sss.dump();
  }
  #endif
  // setup surfaces
  for (; s; s = s->next) {
    SurfaceInfoBlock *sp = nullptr;
    for (auto &&sb : sibs) {
      if (sb.isValid() && sb.equalTo(s)) {
        if (sp) { GCon->Log(NAME_Warning, "invalid lightmap cache surface: duplicate info!"); } else sp = &sb;
      } else {
        #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
        GCon->Logf(NAME_Debug, "::: compare ::: count=%d; typeFlags=%d; tm0=%d; tm1=%d; ext0=%d; ext1=%d; sub=%d; seg=%d; pnorm=%d; pdist=%d; vert=%d",
          (int)(s->count == sb.count),
          (int)(s->typeFlags == sb.typeFlags),
          (int)(s->texturemins[0] == sb.texturemins[0]),
          (int)(s->texturemins[1] == sb.texturemins[1]),
          (int)(s->extents[0] == sb.extents[0]),
          (int)(s->extents[1] == sb.extents[1]),
          (int)(s->subsector == sb.subsector),
          (int)(s->seg == sb.seg),
          (int)(s->plane.normal == sb.plane.normal),
          (int)(s->plane.dist == sb.plane.dist),
          (int)(s->verts[0].vec() == sb.vert0.vec()));
        #endif
      }
    }
    if (!sp) {
      if (!missingWarned) {
        missingWarned = true;
        GCon->Logf(NAME_Warning, "*** lightmap cache is missing some surface info");
      }
      ++lmcacheUnknownSurfaceCount;
      #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
      SurfaceInfoBlock sss;
      sss.initWith(Level, s);
      sss.dump();
      #elif defined(VV_DUMP_LMAP_CACHE_COMPARISON_FAIL)
      if (dbg_cache_lightmap_dump_missing) {
        GCon->Logf(NAME_Debug, "============== ON-DISK (%u) ==============", rd);
        for (auto &&ss : sibs) {
          if (ss.isValid()) {
            ss.dump();
          } else {
            GCon->Log(NAME_Debug, "*** SKIPPED");
          }
        }
        GCon->Log(NAME_Debug, "============== MEMORY (FAIL) ==============");
        SurfaceInfoBlock sss;
        sss.initWith(Level, s);
        sss.dump();
      }
      #endif
    } else {
      #ifdef VV_DUMP_LMAP_CACHE_COMPARISONS
      GCon->Log(NAME_Debug, "*** FOUND!");
      {
        GCon->Log(NAME_Debug, "--- surface ----");
        SurfaceInfoBlock sss;
        sss.initWith(Level, s);
        sss.dump();
        GCon->Log(NAME_Debug, "--- on-disk ----");
        sp->dump();
      }
      #endif
      if (sp->isValid() && sp->lightmap) {
        vassert(!s->lightmap);
        vassert(!s->lightmap_rgb);
        // monochrome
        s->lmsize = sp->lmsize;
        s->lightmap = sp->lightmap;
        light_mem += sp->lmsize;
        // rgb
        if (sp->lightmap_rgb) {
          s->lmrgbsize = sp->lmrgbsize;
          s->lightmap_rgb = sp->lightmap_rgb;
          light_mem += sp->lmrgbsize;
        }
        sp->disownLightmaps();
        sp->clear();
      }
      s->drawflags &= ~surface_t::DF_CALC_LMAP;
    }
  }
  // done
  return !strm->IsError();
}


//==========================================================================
//
//  LoadLightSegSurfaces
//
//==========================================================================
static bool LoadLightSegSurfaces (VLevel *Level, VStream *strm, segpart_t *sp, unsigned &lmcacheUnknownSurfaceCount, bool &missingWarned) {
  vuint32 cnt = 0, rd = 0;
  for (segpart_t *s = sp; s; s = s->next) ++cnt;
  *strm << rd;
  if (rd != cnt) {
    GCon->Logf(NAME_Warning, "invalid lightmap cache segment surface count (%u instead of %u)", rd, cnt);
    lmcacheUnknownSurfaceCount += cnt;
    return SkipLightSegSurfaces(Level, strm, &rd);
  } else {
    for (; sp; sp = sp->next) if (!LoadLightSurfaces(Level, strm, sp->surfs, lmcacheUnknownSurfaceCount, missingWarned)) return false;
    return true;
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::loadLightmapsInternal
//
//==========================================================================
bool VRenderLevelLightmap::loadLightmapsInternal (VStream *strm) {
  if (!strm || strm->IsError()) return false;

  vuint32 surfCount = CountAllSurfaces();

  // load and check header
  vuint32 seccount = 0, sscount = 0, sgcount = 0, sfcount = 0;
  *strm << seccount << sscount << sgcount << sfcount;
  if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
  if ((int)seccount != Level->NumSectors || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache sector count"); return false; }
  if ((int)sscount != Level->NumSubsectors || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache subsector count"); return false; }
  if ((int)sgcount != Level->NumSegs || strm->IsError()) { GCon->Log(NAME_Warning, "invalid lightmap cache seg count"); return false; }
  if (sfcount != surfCount || strm->IsError()) { GCon->Logf(NAME_Warning, "invalid lightmap cache surface count (%u instead of %u)", sfcount, surfCount); /*return false;*/ }
  GCon->Log(NAME_Debug, "lightmap cache validated, trying to load it");

  bool missingWarned = false;

  for (int i = 0; i < Level->NumSubsectors; ++i) {
    subsector_t *sub = &Level->Subsectors[i];
    // count regions (so we can skip them if necessary)
    vuint32 regcount = 0;
    for (subregion_t *r = sub->regions; r != nullptr; r = r->next) ++regcount;
    vuint32 snum = 0xffffffffu;
    *strm << snum;
    if ((int)snum != i) { GCon->Log(NAME_Warning, "invalid lightmap cache subsector number"); return false; }
    // check region count
    vuint32 ccregcount = 0xffffffffu;
    *strm << ccregcount;
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
    if (ccregcount != regcount) {
      GCon->Logf(NAME_Warning, "lightmap cache subsector #%d region count mismatch (%u instead of %u)", i, ccregcount, regcount);
      if (regcount != 0) {
        for (subregion_t *r = sub->regions; r != nullptr; r = r->next, ++regcount) {
          if (r->realfloor != nullptr) lmcacheUnknownSurfaceCount += CountSurfacesInChain(r->realfloor->surfs);
          if (r->realceil != nullptr) lmcacheUnknownSurfaceCount += CountSurfacesInChain(r->realceil->surfs);
          if (r->fakefloor != nullptr) lmcacheUnknownSurfaceCount += CountSurfacesInChain(r->fakefloor->surfs);
          if (r->fakeceil != nullptr) lmcacheUnknownSurfaceCount += CountSurfacesInChain(r->fakeceil->surfs);
        }
      }
      // skip them
      while (ccregcount--) {
        vuint32 n = 0xffffffffu;
        *strm << n;
        if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
        if (!SkipLightSurfaces(Level, strm)) return false; // realfloor
        if (!SkipLightSurfaces(Level, strm)) return false; // realceil
        if (!SkipLightSurfaces(Level, strm)) return false; // fakefloor
        if (!SkipLightSurfaces(Level, strm)) return false; // fakeceil
      }
    } else {
      regcount = 0;
      for (subregion_t *r = sub->regions; r != nullptr; r = r->next, ++regcount) {
        vuint32 n = 0xffffffffu;
        *strm << n;
        if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
        if (n != regcount) { GCon->Log(NAME_Warning, "invalid lightmap cache region number"); return false; }
        if (!LoadLightSurfaces(Level, strm, (r->realfloor ? r->realfloor->surfs : nullptr), lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSurfaces(Level, strm, (r->realceil ? r->realceil->surfs : nullptr), lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSurfaces(Level, strm, (r->fakefloor ? r->fakefloor->surfs : nullptr), lmcacheUnknownSurfaceCount, missingWarned)) return false;
        if (!LoadLightSurfaces(Level, strm, (r->fakeceil ? r->fakeceil->surfs : nullptr), lmcacheUnknownSurfaceCount, missingWarned)) return false;
      }
    }
    if (i%128 == 0) NET_SendNetworkHeartbeat();
  }

  for (int i = 0; i < Level->NumSegs; ++i) {
    seg_t *seg = &Level->Segs[i];
    // count drawsegs (so we can skip them if necessary)
    vuint8 dscount = (seg->drawsegs ? 1 : 0);
    vuint32 snum = 0xffffffffu;
    *strm << snum;
    if ((int)snum != i) { GCon->Log(NAME_Warning, "invalid lightmap cache seg number"); return false; }
    // check drawseg count
    vuint8 ccdscount = 0xffu;
    *strm << ccdscount;
    if (strm->IsError()) { GCon->Log(NAME_Error, "error reading lightmap cache"); return false; }
    if (ccdscount > 1) { GCon->Log(NAME_Error, "invalid lightmap cache drawsec count"); return false; }
    if (ccdscount != dscount) {
      GCon->Logf(NAME_Warning, "lightmap cache seg #%d drawseg count mismatch (%u instead of %u)", i, ccdscount, dscount);
      if (dscount) {
        vassert(ccdscount == 0);
        drawseg_t *ds = seg->drawsegs;
        vassert(ds);
        lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->top);
        lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->mid);
        lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->bot);
        lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->topsky);
        lmcacheUnknownSurfaceCount += CountSegSurfacesInChain(ds->extra);
      } else {
        // skip them
        vassert(ccdscount == 1);
        if (!SkipLightSegSurfaces(Level, strm)) return false; // top
        if (!SkipLightSegSurfaces(Level, strm)) return false; // mid
        if (!SkipLightSegSurfaces(Level, strm)) return false; // bot
        if (!SkipLightSegSurfaces(Level, strm)) return false; // topsky
        if (!SkipLightSegSurfaces(Level, strm)) return false; // extra
      }
    } else if (dscount) {
      vassert(dscount == 1);
      vassert(ccdscount == 1);
      drawseg_t *ds = seg->drawsegs;
      vassert(ds);
      if (!LoadLightSegSurfaces(Level, strm, ds->top, lmcacheUnknownSurfaceCount, missingWarned)) return false;
      if (!LoadLightSegSurfaces(Level, strm, ds->mid, lmcacheUnknownSurfaceCount, missingWarned)) return false;
      if (!LoadLightSegSurfaces(Level, strm, ds->bot, lmcacheUnknownSurfaceCount, missingWarned)) return false;
      if (!LoadLightSegSurfaces(Level, strm, ds->topsky, lmcacheUnknownSurfaceCount, missingWarned)) return false;
      if (!LoadLightSegSurfaces(Level, strm, ds->extra, lmcacheUnknownSurfaceCount, missingWarned)) return false;
    }
    if (i%128 == 0) NET_SendNetworkHeartbeat();
  }

  return !strm->IsError();
}


//==========================================================================
//
//  VRenderLevelLightmap::loadLightmaps
//
//==========================================================================
bool VRenderLevelLightmap::loadLightmaps (VStream *strm) {
  if (!strm || strm->IsError()) {
    lmcacheUnknownSurfaceCount = CountAllSurfaces();
    return false;
  }
  char sign[CDSLEN];
  strm->Serialise(sign, CDSLEN);
  if (strm->IsError() || memcmp(sign, LMAP_CACHE_DATA_SIGNATURE, CDSLEN) != 0) {
    GCon->Logf(NAME_Error, "invalid lightmap cache file signature");
    lmcacheUnknownSurfaceCount = CountAllSurfaces();
    return false;
  }
  lmcacheUnknownSurfaceCount = 0;
  /*VZLibStreamReader*/VStream *zipstrm = new VZLibStreamReader(true, strm, VZLibStreamReader::UNKNOWN_SIZE, VZLibStreamReader::UNKNOWN_SIZE/*Map->DecompressedSize*/);
  bool ok = loadLightmapsInternal(zipstrm);
  VStream::Destroy(zipstrm);
  if (!ok && !lmcacheUnknownSurfaceCount) lmcacheUnknownSurfaceCount = CountAllSurfaces();
  if (ok && lmcacheUnknownSurfaceCount > 0 && lmcacheUnknownSurfaceCount == CountAllSurfaces()) ok = false; // totally wrong
  return ok;
}


//**************************************************************************
//
// calculate static lightmaps
//
//**************************************************************************

//==========================================================================
//
//  LightSurfaces
//
//==========================================================================
static int LightSurfaces (VRenderLevelLightmap *rdr, surface_t *s, bool recalcNow, bool onlyMarked) {
  int res = 0;
  if (recalcNow) {
    for (; s; s = s->next) {
      ++res;
      if (onlyMarked && (s->drawflags&surface_t::DF_CALC_LMAP) == 0) continue;
      s->drawflags &= ~surface_t::DF_CALC_LMAP;
      if (s->count >= 3) rdr->LightFace(s);
    }
  } else {
    for (; s; s = s->next) {
      ++res;
      if (onlyMarked && (s->drawflags&surface_t::DF_CALC_LMAP) == 0) continue;
      if (s->count >= 3) {
        s->drawflags |= surface_t::DF_CALC_LMAP;
      } else {
        s->drawflags &= ~surface_t::DF_CALC_LMAP;
      }
    }
  }
  return res;
}


//==========================================================================
//
//  LightSegSurfaces
//
//==========================================================================
static int LightSegSurfaces (VRenderLevelLightmap *rdr, segpart_t *sp, bool recalcNow, bool onlyMarked) {
  int res = 0;
  for (; sp; sp = sp->next) res += LightSurfaces(rdr, sp->surfs, recalcNow, onlyMarked);
  return res;
}


//==========================================================================
//
//  VRenderLevelLightmap::RelightMap
//
//==========================================================================
void VRenderLevelLightmap::RelightMap (bool recalcNow, bool onlyMarked) {
  vuint32 surfCount = 0;

  if (recalcNow) {
    surfCount = CountAllSurfaces();
    R_PBarReset();
    R_PBarUpdate("Lightmaps", 0, (int)surfCount);
  }

  int processed = 0;
  for (auto &&sub : Level->allSubsectors()) {
    for (subregion_t *r = sub.regions; r != nullptr; r = r->next) {
      if (r->realfloor != nullptr) processed += LightSurfaces(this, r->realfloor->surfs, recalcNow, onlyMarked);
      if (r->realceil != nullptr) processed += LightSurfaces(this, r->realceil->surfs, recalcNow, onlyMarked);
      if (r->fakefloor != nullptr) processed += LightSurfaces(this, r->fakefloor->surfs, recalcNow, onlyMarked);
      if (r->fakeceil != nullptr) processed += LightSurfaces(this, r->fakeceil->surfs, recalcNow, onlyMarked);
    }
    if (recalcNow) R_PBarUpdate("Lightmaps", processed, (int)surfCount);
  }

  for (auto &&seg : Level->allSegs()) {
    drawseg_t *ds = seg.drawsegs;
    if (ds) {
      processed += LightSegSurfaces(this, ds->top, recalcNow, onlyMarked);
      processed += LightSegSurfaces(this, ds->mid, recalcNow, onlyMarked);
      processed += LightSegSurfaces(this, ds->bot, recalcNow, onlyMarked);
      processed += LightSegSurfaces(this, ds->topsky, recalcNow, onlyMarked);
      processed += LightSegSurfaces(this, ds->extra, recalcNow, onlyMarked);
      if (recalcNow) R_PBarUpdate("Lightmaps", processed, (int)surfCount);
    }
  }

  if (recalcNow) R_PBarUpdate("Lightmaps", (int)surfCount, (int)surfCount, true);
}


//==========================================================================
//
//  VRenderLevelLightmap::ResetLightmaps
//
//==========================================================================
void VRenderLevelLightmap::ResetLightmaps (bool recalcNow) {
  RelightMap(recalcNow, false);
}


//==========================================================================
//
//  VRenderLevelLightmap::PreRender
//
//==========================================================================
void VRenderLevelLightmap::PreRender () {
  c_subdivides = 0;
  c_seg_div = 0;
  light_mem = 0;
  inWorldCreation = true;

  RegisterAllThinkers();
  CreateWorldSurfaces();

  if (cl) FullWorldUpdate(true);

  // lightmapping
  bool doReadCache = (!Level->cacheFileBase.isEmpty() && loader_cache_data.asBool() && (Level->cacheFlags&VLevel::CacheFlag_Ignore) == 0);
  bool doWriteCache = (!Level->cacheFileBase.isEmpty() && loader_cache_data.asBool());
  Level->cacheFlags &= ~VLevel::CacheFlag_Ignore;

  bool doPrecalc = (r_precalc_static_lights_override >= 0 ? !!r_precalc_static_lights_override : r_precalc_static_lights);
  VStr ccfname = (Level->cacheFileBase.isEmpty() ? VStr::EmptyString : Level->cacheFileBase+".lmap");
  if (ccfname.isEmpty()) { doReadCache = doWriteCache = false; }
  if (!doPrecalc) doWriteCache = false;

  if (doReadCache || doWriteCache) GCon->Logf(NAME_Debug, "lightmap cache file: '%s'", *ccfname);

  if (doPrecalc || doReadCache || doWriteCache) {
    R_OSDMsgShowSecondary("CREATING LIGHTMAPS");

    bool recalcLight = true;
    if (doReadCache) {
      VStream *lmc = FL_OpenSysFileRead(ccfname);
      if (lmc) {
        recalcLight = !loadLightmaps(lmc);
        if (lmc->IsError()) recalcLight = true;
        VStream::Destroy(lmc);
        if (recalcLight) {
          Sys_FileDelete(ccfname);
        } else {
          // touch cache file, so it will survive longer
          Sys_Touch(ccfname);
        }
      }
    }

    if (recalcLight || lmcacheUnknownSurfaceCount) {
      if (lmcacheUnknownSurfaceCount) {
        GCon->Logf("calculating static lighting due to lightmap cache inconsistencies (%u out of %u surfaces)", lmcacheUnknownSurfaceCount, CountAllSurfaces());
      } else {
        GCon->Log("calculating static lighting");
      }
      double stt = -Sys_Time();
      RelightMap(true, true); // only marked
      stt += Sys_Time();
      GCon->Logf("static lighting calculated in %d.%d seconds (%s mode)", (int)stt, (int)(stt*1000)%1000, (r_lmap_bsp_trace_static ? "BSP" : "blockmap"));
      // cache
      if (doWriteCache) {
        const float tlim = loader_cache_time_limit_lightmap.asFloat();
        // if our lightmap cache is partially valid, rewrite it unconditionally
        if (dbg_cache_lightmap_always || lmcacheUnknownSurfaceCount || stt >= tlim) {
          VStream *lmc = FL_OpenSysFileWrite(ccfname);
          if (lmc) {
            GCon->Logf("writing lightmap cache to '%s'", *ccfname);
            saveLightmaps(lmc);
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
      }
    }
  }

  inWorldCreation = false;

  GCon->Logf("%d subdivides", c_subdivides);
  GCon->Logf("%d seg subdivides", c_seg_div);
  GCon->Logf("%dk light mem", light_mem/1024);
}


//==========================================================================
//
//  COMMAND NukeLightmapCache
//
//==========================================================================
COMMAND(NukeLightmapCache) {
  if (GClLevel && GClLevel->Renderer) {
    GClLevel->Renderer->NukeLightmapCache();
    GCon->Log("lightmap cache nuked.");
  }
}


//==========================================================================
//
//  COMMAND LightmapsReset
//
//==========================================================================
COMMAND_WITH_AC(LightmapsReset) {
  if (GClLevel && GClLevel->Renderer) {
    bool recalcNow = true;
    if (Args.length() > 1) recalcNow = false;
    GCon->Log("resetting lightmaps");
    double stt = -Sys_Time();
    GClLevel->Renderer->ResetLightmaps(recalcNow);
    stt += Sys_Time();
    GCon->Logf("static lighting calculated in %d.%d seconds (%s mode)", (int)stt, (int)(stt*1000)%1000, (r_lmap_bsp_trace_static ? "BSP" : "blockmap"));
    GClLevel->Renderer->NukeLightmapCache();
  }
}

COMMAND_AC(LightmapsReset) {
  if (aidx != 1) return VStr::EmptyString;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  TArray<VStr> list;
  list.append("defer");
  return AutoCompleteFromListCmd(prefix, list);
}
