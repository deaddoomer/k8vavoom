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
//**  Defines shared by refresh and drawer
//**
//**************************************************************************
#ifndef VAVOOM_RENDER_SHARED_H
#define VAVOOM_RENDER_SHARED_H

#include "../drawer.h"
#include "../level/beamclip.h"

//#define VV_DEBUG_LMAP_ALLOCATOR
#define VAVOOM_GLMODEL_32BIT_VIDX

// this is required for huge voxel models (like Cheello voxels)
#ifdef VAVOOM_GLMODEL_32BIT_VIDX
# define VV_GLM_VIDX  vuint32
#else
# define VV_GLM_VIDX  vuint16
#endif


// ////////////////////////////////////////////////////////////////////////// //
// lightmap texture dimensions
#define BLOCK_WIDTH      (128)
#define BLOCK_HEIGHT     (128)
// maximum lightmap textures
#define NUM_BLOCK_SURFS  (64)


extern int light_mem;


// ////////////////////////////////////////////////////////////////////////// //
// color maps
enum {
  CM_Default,
  CM_Inverse,
  CM_Gold,
  CM_Red,
  CM_Green,
  CM_Mono,
  CM_BeRed,
  CM_Blue,

  CM_Max,
};

// simulate light fading using dark fog
enum {
  FADE_LIGHT = 0xff010101u,
};


// ////////////////////////////////////////////////////////////////////////// //
struct texinfo_t {
  TVec saxis;
  float soffs;
  TVec taxis;
  float toffs;
  TVec saxisLM; // for lightmaps
  TVec taxisLM; // for lightmaps
  VTexture *Tex;
  vint32 noDecals;
  // 1.1f for solid surfaces
  // alpha for masked surfaces
  // not always right, tho (1.0f vs 1.1f); need to be checked and properly fixed
  float Alpha;
  vint32 Additive;
  vuint8 ColorMap;

  inline bool isEmptyTexture () const noexcept { return (!Tex || Tex->Type == TEXTYPE_Null); }

  // call this to check if we need to change OpenGL texture
  inline bool needChange (const texinfo_t &other, const vuint32 upframe) const noexcept {
    if (&other == this) return false;
    return
      Tex != other.Tex ||
      ColorMap != other.ColorMap ||
      FASI(saxis.x) != FASI(other.saxis.x) ||
      FASI(saxis.y) != FASI(other.saxis.y) ||
      FASI(saxis.z) != FASI(other.saxis.z) ||
      FASI(soffs) != FASI(other.soffs) ||
      FASI(taxis.x) != FASI(other.taxis.x) ||
      FASI(taxis.y) != FASI(other.taxis.y) ||
      FASI(taxis.z) != FASI(other.taxis.z) ||
      FASI(toffs) != FASI(other.toffs) ||
      (Tex && Tex->lastUpdateFrame != upframe);
  }

  // call this to cache info for `needChange()`
  inline void updateLastUsed (const texinfo_t &other) noexcept {
    if (&other == this) return;
    Tex = other.Tex;
    ColorMap = other.ColorMap;
    saxis = other.saxis;
    soffs = other.soffs;
    taxis = other.taxis;
    toffs = other.toffs;
    // just in case
    saxisLM = other.saxisLM;
    taxisLM = other.taxisLM;
    // other fields doesn't matter
  }

  inline void resetLastUsed () noexcept {
    Tex = nullptr; // it is enough, but to be sure...
    ColorMap = 255; // impossible colormap
  }

  inline void initLastUsed () noexcept {
    saxis = taxis = saxisLM = taxisLM = TVec(-99999.0f, -99999.0f, -99999.0f);
    soffs = toffs = -99999.0f;
    Tex = nullptr;
    noDecals = false;
    Alpha = -666;
    Additive = 666;
    ColorMap = 255;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
//WARNING! take care of setting heights to non-zero, or glow shaders will fail!
struct GlowParams {
public:
  vuint32 glowCC, glowCF; // glow colors
  float floorZ, ceilingZ;
  float floorGlowHeight, ceilingGlowHeight;

public:
  VVA_FORCEINLINE GlowParams () noexcept : glowCC(0), glowCF(0), floorZ(0.0f), ceilingZ(0.0f), floorGlowHeight(128.0f), ceilingGlowHeight(128.0f) {}
  VVA_FORCEINLINE bool isActive () const noexcept { return (glowCC|glowCF); }
  VVA_FORCEINLINE void clear () noexcept { glowCC = glowCF = 0; floorGlowHeight = ceilingGlowHeight = 128.0f; }
  VVA_FORCEINLINE bool operator == (const GlowParams &other) const noexcept {
    return
      glowCC == other.glowCC &&
      glowCF == other.glowCF &&
      FASI(floorZ) == FASI(other.floorZ) &&
      FASI(ceilingZ) == FASI(other.ceilingZ) &&
      FASI(floorGlowHeight) == FASI(other.floorGlowHeight) &&
      FASI(ceilingGlowHeight) == FASI(other.ceilingGlowHeight);
  }
  VVA_FORCEINLINE bool operator != (const GlowParams &other) const noexcept {
    return
      glowCC != other.glowCC ||
      glowCF != other.glowCF ||
      FASI(floorZ) != FASI(other.floorZ) ||
      FASI(ceilingZ) != FASI(other.ceilingZ) ||
      FASI(floorGlowHeight) != FASI(other.floorGlowHeight) ||
      FASI(ceilingGlowHeight) != FASI(other.ceilingGlowHeight);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct surface_t {
public:
  enum {
    MAXWVERTS = 8+8, // maximum number of vertices in wsurf (world/wall surface)
  };

  enum {
    DF_MASKED       = 1u<<0, // this surface has "masked" texture
    DF_WSURF        = 1u<<1, // is this world/wall surface? such surfs are guaranteed to have space for `MAXWVERTS`
    DF_FIX_CRACKS   = 1u<<2, // this surface must be processed with t-junction fixer
    DF_CALC_LMAP    = 1u<<3, // calculate static lightmap
    DF_NO_FACE_CULL = 1u<<4, // ignore face culling
    DF_MIRROR       = 1u<<5, // this is floor/ceiling mirror surface (set in renderer)
    // call t-junction fixer before rendering this
    DF_CHECK_TJUNK  = 1u<<29,
    // temp flag for shadowmap collector
    DF_SMAP_FLIP    = 1u<<30,
    // cached visibility flag, set in main BSP collector (VRenderLevelShared::SurfCheckAndQueue)
    DF_PL_VISIBLE   = 1u<<31,
  };

  enum {
    TF_TOP     = 1u<<0,
    TF_BOTTOM  = 1u<<1,
    TF_MIDDLE  = 1u<<2,
    TF_FLOOR   = 1u<<3,
    TF_CEILING = 1u<<4,
    TF_TOPHACK = 1u<<5,
    TF_3DFLOOR = 1u<<6, // this surface comes from gzdoom-style non-solid 3d floor (and it is guaranteed to be floor/ceiling)
    TF_3DPOBJ  = 1u<<7, // this surface comes from 3d polyobject
  };

  // internal allocator flags (allocflags)
  enum {
    // allocation type; it prolly doesn't belong there, but meh
    ALLOC_WORLD = 1u<<0,
    // used in lightmap t-junction fixing after subdivision
    // set if this surface was already converted to centroid-based triangle fan
    // in this case vertex 0 is a centroid
    // also, last vertex is a duplicate of `verts[1]` (this is required for correct fan)
    CENTROID_CREATED = 1u<<1,
  };

public:
  surface_t *next; // subdivisions can create surface chain
  texinfo_t *texinfo; // points to owning `sec_surface_t`
  TPlane plane; // was pointer
  sec_plane_t *HorizonPlane; // used in horizon rendering
  vuint32 Light; // light level and color
  vuint32 Fade; // fog color, (a)rgb (`FADE_LIGHT` is "normal Doom fading")
  float glowFloorHeight;
  float glowCeilingHeight;
  vuint32 glowFloorColor;
  vuint32 glowCeilingColor;
  subsector_t *subsector; // owning subsector
  seg_t *seg; // owning seg (can be `nullptr` for floor/ceiling)
  subregion_t *sreg; // owning subsector region (can be `nullptr` for walls)
  vuint32 typeFlags; // TF_xxx
  // not exposed to VC
  int lmsize, lmrgbsize; // to track static lightmap memory
  vuint8 *lightmap; // only intensity
  rgb_t *lightmap_rgb; // colored light
  unsigned queueframe; // this is used to prevent double queuing
  unsigned dlightframe; // if equal to `currDLightFrame`, then `dlightbits` are valid
  unsigned dlightbits; // bit N set if the corresponding dynamic light affects this surface
  unsigned drawflags; // DF_XXX
  unsigned allocflags; // see enum above
  int texturemins[2]; // used in lightmapping
  int extents[2]; // used in lightmapping
  surfcache_t *CacheSurf; // lightmap atlas cache
  int shaderClass; // used in renderers
  int firstIndex; // address in VBO buffer; only used in world rendering
  GlowParams gp; // used in renderer to cache glow info
  int alloted; // number of allocated vertices (valid even for world surfaces)
  int count; // number of used vertices
  SurfVertex verts[1]; // dynamic array of vertices

public:
  inline bool isWorldAllocated () const noexcept { return !!(allocflags&ALLOC_WORLD); }

  inline bool isCentroidCreated () const noexcept { return !!(allocflags&CENTROID_CREATED); }
  inline void setCentroidCreated () noexcept { allocflags |= CENTROID_CREATED; }
  inline void resetCentroidCreated () noexcept { allocflags &= ~CENTROID_CREATED; }

  inline bool IsFixCracks () const noexcept { return !!(drawflags&DF_FIX_CRACKS); }
  inline void SetFixCracks () noexcept { drawflags |= DF_FIX_CRACKS; }
  inline void ResetFixCracks () noexcept { drawflags &= ~DF_FIX_CRACKS; }

  // used to fill subdivided surfaces
  inline void copyRequiredFrom (const surface_t &other) noexcept {
    if (&other != this) {
      texinfo = other.texinfo;
      plane = other.plane;
      HorizonPlane = other.HorizonPlane;
      subsector = other.subsector;
      seg = other.seg;
      sreg = other.sreg;
      typeFlags = other.typeFlags;
      drawflags = other.drawflags;
    }
  }


  // there should be enough room for vertex
  void InsertVertexAt (int idx, const TVec &p, int tjflags) noexcept;
  void RemoveVertexAt (int idx) noexcept;

  void RemoveCentroid () noexcept;

  // there should be enough room for two new points
  void AddCentroid () noexcept;

  // remove all vertices with tjflags > 0
  void RemoveTJVerts () noexcept;

  inline bool NeedRecalcStaticLightmap () const noexcept { return (drawflags&DF_CALC_LMAP); }
  inline bool IsMasked () const noexcept { return (drawflags&DF_MASKED); }
  inline bool IsTwoSided () const noexcept { return (drawflags&DF_NO_FACE_CULL); }

  // to use in renderer
  inline void SetPlVisible (const bool v) noexcept {
    if (v) drawflags |= DF_PL_VISIBLE; else drawflags &= ~DF_PL_VISIBLE;
  }

  // to use in renderer
  inline unsigned IsPlVisible () const noexcept { return (drawflags&DF_PL_VISIBLE); }

  // to use in renderer
  inline bool IsVisibleFor (const TVec &point) const noexcept {
    //k8: mirror surfaces should be rendered too, but i don't yet know why
    return (!(drawflags&(DF_NO_FACE_CULL|DF_MIRROR)) ? !plane.PointOnSide(point) : (plane.PointOnSide2(point) != 2));
  }

  inline int PointOnSide (const TVec &point) const noexcept { return plane.PointOnSide(point); }
  inline bool SphereTouches (const TVec &center, float radius) const noexcept { return plane.SphereTouches(center, radius); }
  inline float GetNormalZ () const noexcept { return plane.normal.z; }
  inline const TVec &GetNormal () const noexcept { return plane.normal; }
  inline float GetDist () const noexcept { return plane.dist; }
  inline float PointDistance (const TVec &p) const noexcept { return plane.PointDistance(p); }

  inline void GetPlane (TPlane *p) const noexcept { *p = plane; }

  void FreeLightmaps () noexcept;
  void FreeRGBLightmap () noexcept;

  void ReserveMonoLightmap (int sz) noexcept;
  void ReserveRGBLightmap (int sz) noexcept;

private:
  // calculated centroid may be wrong for invalid surfaces

  TVec CalculateFastCentroid () const noexcept;
  TVec CalculateRealCentroid () const noexcept;
};
static_assert(sizeof(unsigned) >= 4, "struct surface_t expects `unsigned` of at least 4 bytes");


// ////////////////////////////////////////////////////////////////////////// //
template<typename TBase> class V2DCache {
public:
  struct Item : public TBase {
    // position in light surface
    //int s, t; // must present in `TBase` (for now)
    // size
    int width, height;
    // line list in block
    Item *bprev;
    Item *bnext;
    // cache list in line
    Item *lprev;
    Item *lnext;
    // next free block in `blockbuf`
    Item *freeChain;
    // light surface index
    vuint32 atlasid;
    Item **owner;
    vuint32 lastframe;
  };

  struct AtlasInfo {
    int width;
    int height;

    inline AtlasInfo () noexcept : width(0), height(0) {}
    inline bool isValid () const noexcept { return (width > 0 && height > 0 && width <= 4096 && height <= 4096); }
  };

protected:

  // block pool
  struct VBlockPool {
  public:
    // number of `Item` items in one pool entry
    enum { NUM_CACHE_BLOCKS_IN_POOL_ENTRY = 4096u };

  public:
    struct PoolEntry {
      Item page[NUM_CACHE_BLOCKS_IN_POOL_ENTRY];
      PoolEntry *prev;
    };

  public:
    PoolEntry *tail;
    unsigned tailused;
    unsigned entries; // full

  public:
    VV_DISABLE_COPY(VBlockPool)
    inline VBlockPool () noexcept : tail(nullptr), tailused(0), entries(0) {}
    inline ~VBlockPool () noexcept { clear(); }

    inline unsigned itemCount () noexcept { return entries*NUM_CACHE_BLOCKS_IN_POOL_ENTRY+tailused; }

    void clear () noexcept {
      while (tail) {
        PoolEntry *c = tail;
        tail = c->prev;
        Z_Free(c);
      }
      tailused = 0;
      entries = 0;
    }

    Item *alloc () noexcept {
      if (tail && tailused < NUM_CACHE_BLOCKS_IN_POOL_ENTRY) return &tail->page[tailused++];
      if (tail) ++entries; // full entries counter
      // allocate new pool entry
      PoolEntry *c = (PoolEntry *)Z_Calloc(sizeof(PoolEntry));
      c->prev = tail;
      tail = c;
      tailused = 1;
      return &tail->page[0];
    }

    void resetFrames () noexcept {
      for (PoolEntry *c = tail; c; c = c->prev) {
        Item *s = &c->page[0];
        for (unsigned count = NUM_CACHE_BLOCKS_IN_POOL_ENTRY; count--; ++s) s->lastframe = 0;
      }
    }
  };

  struct Atlas {
    int width;
    int height;
    vuint32 id;
    Item *blocks;

    inline bool isValid () const noexcept { return (width > 0 && height > 0 && width <= 4096 && height <= 4096); }
  };

protected:
  TArray<Atlas> atlases;
  Item *freeblocks;
  VBlockPool blockpool;
  vuint32 lastOldFreeFrame;

public:
  vuint32 cacheframecount;

protected:
  Item *performBlockVSplit (int width, int height, Item *block, bool forceAlloc=false) {
    vassert(block->height >= height);
    vassert(!block->lnext);
    const vuint32 aid = block->atlasid;

    if (block->height > height) {
      #ifdef VV_DEBUG_LMAP_ALLOCATOR
      GLog.Logf(NAME_Debug, "  vsplit: aid=%d; w=%d; h=%d", aid, block->width, block->height);
      #endif
      Item *other = getFreeBlock(forceAlloc);
      if (!other) return nullptr;
      other->s = 0;
      other->t = block->t+height;
      other->width = block->width;
      other->height = block->height-height;
      other->lnext = nullptr;
      other->lprev = nullptr;
      other->bnext = block->bnext;
      if (other->bnext) other->bnext->bprev = other;
      block->bnext = other;
      other->bprev = block;
      block->height = height;
      other->owner = nullptr;
      other->atlasid = aid;
      forceAlloc = true; // we need second block unconditionally
    }

    {
      Item *other = getFreeBlock(forceAlloc);
      if (!other) return nullptr;
      other->s = block->s+width;
      other->t = block->t;
      other->width = block->width-width;
      other->height = block->height;
      other->lnext = nullptr;
      block->lnext = other;
      other->lprev = block;
      block->width = width;
      other->owner = nullptr;
      other->atlasid = aid;
    }

    return block;
  }

  Item *performBlockHSplit (int width, Item *block, bool forceAlloc=false) {
    if (block->width < width) return nullptr; // just in case
    if (block->width == width) return block; // nothing to do
    vassert(block->width > width);
    #ifdef VV_DEBUG_LMAP_ALLOCATOR
    GLog.Logf(NAME_Debug, "  hsplit: aid=%u; w=%d; h=%d", block->atlasid, block->width, block->height);
    #endif
    Item *other = getFreeBlock(forceAlloc);
    if (!other) return nullptr;
    other->s = block->s+width;
    other->t = block->t;
    other->width = block->width-width;
    other->height = block->height;
    other->lnext = block->lnext;
    if (other->lnext) other->lnext->lprev = other;
    block->lnext = other;
    other->lprev = block;
    block->width = width;
    other->owner = nullptr;
    other->atlasid = block->atlasid;
    return block;
  }

  Item *getFreeBlock (bool forceAlloc) {
    Item *res = freeblocks;
    if (res) {
      freeblocks = res->freeChain;
    } else {
      // no free blocks
      if (!forceAlloc && blockpool.itemCount() >= 32768) {
        // too many blocks
        flushOldCaches();
        res = freeblocks;
        if (res) freeblocks = res->freeChain; else res = blockpool.alloc(); // force-allocate anyway
      } else {
        // allocate new block
        res = blockpool.alloc();
      }
    }
    if (res) memset(res, 0, sizeof(Item));
    return res;
  }

public:
  void debugDump () noexcept {
    GLog.Logf("=== V2DCache: %d atlases ===", atlases.length());
    for (auto &&it : atlases) {
      GLog.Logf(" -- atlas #%u size=(%d,%d)--", it.id, it.width, it.height);
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        vassert(blines->atlasid == it.id);
        GLog.Logf("  line: ofs=(%d,%d); size=(%d,%d)", blines->s, blines->t, blines->width, blines->height);
        for (Item *block = blines; block; block = block->lnext) {
          GLog.Logf("   block: ofs=(%d,%d); size=(%d,%d)", block->s, block->t, block->width, block->height);
        }
      }
    }
  }

public:
  VV_DISABLE_COPY(V2DCache)
  inline V2DCache () noexcept : atlases(), freeblocks(nullptr), blockpool(), lastOldFreeFrame(0), cacheframecount(0) {}
  inline ~V2DCache () noexcept { /*clear();*/ }

  inline int getAtlasCount () const noexcept { return atlases.length(); }

  // this resets all allocated blocks (but doesn't deallocate atlases)
  void reset () noexcept {
    clearCacheInfo();
    // clear blocks
    blockpool.clear();
    freeblocks = nullptr;
    //initLightChain();
    // release all lightmap atlases
    for (int idx = atlases.length()-1; idx >= 0; --idx) releaseAtlas(atlases[idx].id);
    atlases.reset();
    /*
    // setup lightmap atlases (no allocations, all atlases are free)
    for (auto &&it : atlases) {
      vassert(it.isValid());
      Item *block = blockpool.alloc();
      memset((void *)block, 0, sizeof(Item));
      block->width = it.width;
      block->height = it.height;
      block->atlasid = it.id;
      it.blocks = block;
    }
    */
  }

  // zero all `lastframe` fields
  void zeroFrames () noexcept {
    for (auto &&it : atlases) {
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        for (Item *block = blines; block; block = block->lnext) {
          block->lastframe = 0;
        }
      }
    }
    lastOldFreeFrame = 0;
  }

  virtual void releaseAtlas (vuint32 id) noexcept = 0;

  virtual void resetBlock (Item *block) noexcept = 0;

  // called when we need to clear all cache pointers
  // calls `resetBlock()`
  void resetAllBlocks () noexcept {
    for (auto &&it : atlases) {
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        for (Item *block = blines; block; block = block->lnext) {
          resetBlock(block);
        }
      }
    }
  }

  // `FreeSurfCache()` calls this with `true`
  Item *freeBlock (Item *block, bool checkLines) noexcept {
    resetBlock(block);

    if (block->owner) {
      *block->owner = nullptr;
      block->owner = nullptr;
    }

    if (block->lnext && !block->lnext->owner) {
      Item *other = block->lnext;
      block->width += other->width;
      block->lnext = other->lnext;
      if (block->lnext) block->lnext->lprev = block;
      other->freeChain = freeblocks;
      freeblocks = other;
    }

    if (block->lprev && !block->lprev->owner) {
      Item *other = block;
      block = block->lprev;
      block->width += other->width;
      block->lnext = other->lnext;
      if (block->lnext) block->lnext->lprev = block;
      other->freeChain = freeblocks;
      freeblocks = other;
    }

    if (block->lprev || block->lnext || !checkLines) return block;

    if (block->bnext && !block->bnext->lnext) {
      Item *other = block->bnext;
      block->height += other->height;
      block->bnext = other->bnext;
      if (block->bnext) block->bnext->bprev = block;
      other->freeChain = freeblocks;
      freeblocks = other;
    }

    if (block->bprev && !block->bprev->lnext) {
      Item *other = block;
      block = block->bprev;
      block->height += other->height;
      block->bnext = other->bnext;
      if (block->bnext) block->bnext->bprev = block;
      other->freeChain = freeblocks;
      freeblocks = other;
    }

    return block;
  }

  void flushOldCaches () noexcept {
    if (lastOldFreeFrame == cacheframecount) return; // already done
    for (auto &&it : atlases) {
      vassert(it.isValid());
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        for (Item *block = blines; block; block = block->lnext) {
          if (block->owner && cacheframecount != block->lastframe) block = freeBlock(block, false);
        }
        if (!blines->owner && !blines->lprev && !blines->lnext) blines = freeBlock(blines, true);
      }
      //FIXME: release free atlases here
    }
    if (!freeblocks) {
      //Sys_Error("No more free blocks");
      GLog.Logf(NAME_Warning, "Surface cache overflow, and no old surfaces found");
    } else {
      GLog.Logf(NAME_Debug, "Surface cache overflow, old caches flushed");
    }
  }

  Item *allocBlock (int width, int height) noexcept {
    vassert(width > 0);
    vassert(height > 0);
    #ifdef VV_DEBUG_LMAP_ALLOCATOR
    GLog.Logf(NAME_Debug, "V2DCache::allocBlock: w=%d; h=%d", width, height);
    #endif

    Item *splitBlock = nullptr;

    for (auto &&it : atlases) {
      for (Item *blines = it.blocks; blines; blines = blines->bnext) {
        if (blines->height < height) continue;
        if (blines->height == height) {
          for (Item *block = blines; block; block = block->lnext) {
            if (block->owner) continue;
            if (block->width < width) continue;
            block = performBlockHSplit(width, block);
            vassert(block->atlasid == it.id);
            return block;
          }
        }
        // possible vertical split?
        if (!splitBlock && !blines->lnext && blines->height >= height) {
          splitBlock = blines;
          vassert(splitBlock->atlasid == it.id);
        }
      }
    }

    if (splitBlock) {
      Item *other = performBlockVSplit(width, height, splitBlock);
      if (other) return other;
    }

    // try to get a new atlas
    AtlasInfo nfo = allocAtlas((vuint32)atlases.length(), width, height);
    if (!nfo.isValid()) return nullptr;

    // setup new atlas
    Atlas &atp = atlases.alloc();
    atp.width = nfo.width;
    atp.height = nfo.height;
    atp.id = (vuint32)atlases.length()-1;
    atp.blocks = blockpool.alloc();
    memset((void *)atp.blocks, 0, sizeof(Item));
    atp.blocks->width = atp.width;
    atp.blocks->height = atp.height;
    atp.blocks->atlasid = atp.id;

    // first split ever
    {
      Item *blines = atp.blocks;
      if (blines->height < height || blines->width < width) return nullptr; // something is wrong with this atlas
      Item *other;
      if (blines->height == height) {
        other = performBlockHSplit(width, blines, true);
      } else {
        other = performBlockVSplit(width, height, blines, true);
      }
      if (other) return other;
    }

    GLog.Logf(NAME_Error, "Surface cache overflow!");
    return nullptr;
  }

  // this is called in `reset()` to perform necessary cleanups
  // it is called before any other actions are done
  virtual void clearCacheInfo () noexcept = 0;

  // this method will be called when allocator runs out of atlases
  // return invalid atlas info if no more free atlases available
  // `aid` is the number of this atlas (starting from zero)
  // it can be used to manage atlas resources
  virtual AtlasInfo allocAtlas (vuint32 aid, int minwidth, int minheight) noexcept = 0;
};


// ////////////////////////////////////////////////////////////////////////// //
struct surfcache_t {
  // position in light surface
  int s, t;
  surfcache_t *chain; // list of drawable surfaces for the atlas
  vuint32 Light; // checked for strobe flash
  int dlight;
  surface_t *surf;
};


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelShared;


// ////////////////////////////////////////////////////////////////////////// //
// base class for view portals
class VPortal {
public:
  VRenderLevelShared *RLev;
  TArrayNC<surface_t *> Surfs;
  int Level;
  float bbox3d[6];
  bool needBBox; // default is `true`

public:
  VPortal (VRenderLevelShared *ARLev) noexcept;
  virtual ~VPortal ();

  virtual bool NeedsDepthBuffer () const noexcept;
  virtual bool IsSky () const noexcept;
  virtual bool IsSkyBox () const noexcept;
  virtual bool IsMirror () const noexcept;
  virtual bool IsStack () const noexcept;

  virtual bool MatchSky (class VSky *) const noexcept;
  virtual bool MatchSkyBox (VEntity *) const noexcept;
  virtual bool MatchMirror (const TPlane *) const noexcept;

  // most portals will override `DrawContents()`, but for sky we could skip all the heavy mechanics
  // `Draw()` will save rendering state (view), and call `DrawContents()`
  // this will also turn off shadows for portals
  // (because for shadows we usually need stencils, and also for speed)
  virtual void Draw (bool UseStencil);

  // portal content renderer (K.O.)
  virtual void DrawContents () = 0;

  // this also updates bbox
  void AppendSurface (surface_t *surf) noexcept;

protected:
  void SetupRanges (const refdef_t &refdef, VViewClipper &Range, bool Revert, bool SetFrustum);
};


// ////////////////////////////////////////////////////////////////////////// //
// one alias model frame
struct VMeshFrame {
  VStr Name;
  TVec Scale;
  TVec Origin;
  TVec *Verts;
  TVec *Normals;
  TPlane *Planes;
  vuint32 TriCount;
  // cached offsets on OpenGL buffer
  vuint32 VertsOffset;
  vuint32 NormalsOffset;
};

// note that the following structs are uploaded to GPU
// so they must be packed, and data sizes must not be changed
#pragma pack(push,1)
// texture coordinates
struct VMeshSTVert {
  float S;
  float T;
};

// one mesh triangle
struct VMeshTri {
  /*vuint16*/VV_GLM_VIDX VertIndex[3];
};
#pragma pack(pop)


struct VMeshEdge {
  /*vuint16*/VV_GLM_VIDX Vert1;
  /*vuint16*/VV_GLM_VIDX Vert2;
  vint16 Tri1;
  vint16 Tri2;
};


struct VMeshModel {
  //WARNING! HACK! this should be the same as OpenGL constants!
  enum {
    GlNone = 0, // so we can't use GL_POINTS; meh.
    GlTriangles = 0x0004,
    GlTriangleStrip = 0x0005,
    GlTriangleFan = 0x0006,
  };

  struct SkinInfo {
    VName fileName;
    int textureId; // -1: not loaded yet
    int shade; // -1: none
  };

  // note that the number of normals must be the same
  // as the number of vertices, because normals are per-vertex
  // this is also true for `STVerts`
  // `Edges` are used only in stencil shadows mode
  // if `TriVerts` has any elements, it will be used with `GlMode`
  // note that `Tris` should still be constructed, because they are used in stencil shadows
  VStr Name;
  int MeshIndex;
  TArray<SkinInfo> Skins;
  TArray<VMeshFrame> Frames;
  TArray<TVec> AllVerts;
  TArray<TVec> AllNormals; // normals are per-vertex!
  TArray<VMeshSTVert> STVerts;
  TArray<VMeshTri> Tris; // vetex indices
  TArray</*vuint16*/VV_GLM_VIDX> TriVerts; // vetex indices for triangle strips and such, with 65535 as restart index
  // the following two arrays are used only in stencil shadows
  // they are lazily created
  TArray<TPlane> AllPlanes; // for `Tris`
  TArray<VMeshEdge> Edges; // for `Tris`
  unsigned GlMode; // GL_TRIANGLES, etc.

  bool loaded; // is this model loaded?
  bool Uploaded; // is this model uploaded to GPU?
  bool HadErrors; // set if this model had some errors (exclude from stencil shadowing)

  // voxel options
  bool isVoxel; // should this model be loaded as a voxel?
  bool useVoxelPivotZ; // should the voxel be z-centered using the pivot point?
  int voxOptLevel;
  bool voxFixTJunk;
  bool voxHollowFill;
  int voxSkinTextureId;

  // OpenGL handles
  vuint32 VertsBuffer;
  vuint32 IndexBuffer;

  // next mipmap level, or NULL
  VMeshModel *nextMip;
  float mipXScale, mipYScale, mipZScale;

public:
  // does nothing if `loaded` is `true`
  // bombs out on invalid model data
  void LoadFromData (const vuint8 *Data, int DataSize);

  // does nothing if `loaded` is `true`
  // bombs out on invalid model data
  void LoadFromWad ();

private:
  void Load_MD2 (const vuint8 *Data, int DataSize);
  void Load_MD3 (const vuint8 *Data, int DataSize);
  void Load_KVX (const vuint8 *Data, int DataSize);

  void PrepareMip (VMeshModel &src);
  void SetupMip (void *voxdata);
  void ClearData ();

  bool Load_KVXCacheMdl (VStream *st);
  void Save_KVXCacheMdl (VStream *st);

  bool Load_KVXCache (VStream *st);
  void Save_KVXCache (VStream *st);

  VStr GenKVXCacheName (const vuint8 *Data, int DataSize);

  bool VoxNeedReload ();

private:
  struct VTempEdge {
    /*vuint16*/VV_GLM_VIDX Vert1;
    /*vuint16*/VV_GLM_VIDX Vert2;
    vint16 Tri1;
    vint16 Tri2;
  };

  struct TVertMap {
    int VertIndex;
    int STIndex;
  };

  static void AddEdge (TArray<VTempEdge> &Edges, int Vert1, int Vert2, int Tri);
  static void CopyEdgesTo (TArray<VMeshEdge> &dest, TArray<VTempEdge> &src);
  static VStr getStrZ (const char *s, unsigned maxlen);

public:
  static bool IsKnownModelFormat (VStream *strm); // this also does simple check for known model format
  static bool LoadMD2Frames (VStr mdpath, TArray<VStr> &names);

public:
  // create `AllPlanes` and `Edges`, if necessary
  void EnsureEdgesPlanes ();
};


// ////////////////////////////////////////////////////////////////////////// //
void R_DrawViewBorder ();
void R_ParseGLDefSkyBoxesScript (VScriptParser *sc);

// for dark light mode
float R_CalcGlobVis ();


// ////////////////////////////////////////////////////////////////////////// //
//extern VCvarI r_fog;
extern VCvarF r_fog_r;
extern VCvarF r_fog_g;
extern VCvarF r_fog_b;
extern VCvarF r_fog_start;
extern VCvarF r_fog_end;
extern VCvarF r_fog_density;

extern VCvarB r_vsync;
extern VCvarB r_vsync_adaptive;

extern VCvarB r_fade_light;
extern VCvarF r_fade_factor;

extern VCvarF r_sky_bright_factor;

extern VCvarB r_hack_transparent_doors;

extern VCvarB r_fix_tjunctions;
extern VCvarB dbg_fix_tjunctions;
extern VCvarB warn_fix_tjunctions;

extern float PixelAspect;
extern float BaseAspect;
extern float PSpriteOfsAspect;
extern float EffectiveFOV;
extern float AspectFOVX;
extern float AspectEffectiveFOVX;

extern VTextureTranslation ColorMaps[CM_Max];


#endif
