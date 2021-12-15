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
#ifndef VAVOOM_VLEVEL_HEADER
#define VAVOOM_VLEVEL_HEADER


#define MAXPLAYERS  (8)

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger, but we do not have any moving sectors nearby
// k8: old value
//#define MAXRADIUS  (32.0f)
// k8: it's not really used anymore
//#define MAXRADIUS  (64.0f)

// mapblocks are used to check movement against lines and things
#define MapBlock(x_)  (((int)floorf(x_))>>7)

class VDecalDef;
class VDecalAnim;
struct opening_t;

extern int validcount;
extern vint64 validcountState;

struct VBootPrintDecalParams {
  VName Animator;
  vint32 Translation;
  vint32 Shade;
  float Alpha;
  float MarkTime;
};

// WARNING! keep in sync with VavoomC code!
enum {
  // vanilla flags
  MTF_AMBUSH                = 0x0008, // deaf monsters/do not react to sound
  MTF_UNTRANSLATED_MP       = 0x0010,
  // Boom
  MTF_UNTRANSLATED_NO_DM    = 0x0020,
  MTF_UNTRANSLATED_NO_COOP  = 0x0040,
  // MBF
  MTF_UNTRANSLATED_FRIENDLY = 0x0080,

  MTF_DORMANT     = 0x0010, // the thing is dormant

  // translated k8vavoom flags
  MTF_GSINGLE     = 0x0100, // appearing in game modes
  MTF_GCOOP       = 0x0200,
  MTF_GDEATHMATCH = 0x0400,
  MTF_SHADOW      = 0x0800,
  MTF_ALTSHADOW   = 0x1000,
  MTF_FRIENDLY    = 0x2000,
  MTF_STANDSTILL  = 0x4000,

  MTF2_CLASS_BASE = 0x000010000,
  MTF2_CLASS_MASK = 0x0ffff0000,
  MTF2_FIGHTER    = 0x000010000, // thing appearing in player classes
  MTF2_CLERIC     = 0x000020000,
  MTF2_MAGE       = 0x000040000,
};


//==========================================================================
//
//  Structures for level network replication
//
//==========================================================================
struct rep_line_t {
  float alpha;
};

struct rep_side_t {
  side_tex_params_t Top;
  side_tex_params_t Bot;
  side_tex_params_t Mid;
  int TopTexture;
  int BottomTexture;
  int MidTexture;
  vuint32 Flags;
  int Light;
};

struct rep_sector_t {
  int floor_pic;
  float floor_dist;
  float floor_xoffs;
  float floor_yoffs;
  float floor_XScale;
  float floor_YScale;
  float floor_Angle;
  float floor_BaseAngle;
  float floor_BaseXOffs;
  float floor_BaseYOffs;
  float floor_PObjCX;
  float floor_PObjCY;
  float floor_MirrorAlpha;
  VEntity *floor_SkyBox;
  int ceiling_pic;
  float ceiling_dist;
  float ceiling_xoffs;
  float ceiling_yoffs;
  float ceiling_XScale;
  float ceiling_YScale;
  float ceiling_Angle;
  float ceiling_BaseAngle;
  float ceiling_BaseXOffs;
  float ceiling_BaseYOffs;
  float ceiling_PObjCX;
  float ceiling_PObjCY;
  float ceiling_MirrorAlpha;
  VEntity *ceiling_SkyBox;
  int Sky;
  sec_params_t params;
};

struct rep_polyobj_t {
  TVec startSpot;
  float angle;
};

struct rep_light_t {
  TVec Origin;
  float Radius;
  vuint32 Color;
  //VEntity *Owner;
  vuint32 OwnerUId; // 0: no owner
  TVec ConeDir;
  float ConeAngle;
  sector_t *LevelSector;
  float LevelScale;
  //int NextUIdIndex; // -1 or next light for this OwnerUId
  //WARNING! keep in sync with `dlight_t`!
  enum {
    PlayerLight   = 1u<<0,
    NoSelfShadow  = 1u<<1,
    NoShadow      = 1u<<2,
    NoSelfLight   = 1u<<3,
    NoActorLight  = 1u<<4,
    NoActorShadow = 1u<<5,
    Additive      = 1u<<6, // this does nothing
    Subtractive   = 1u<<7, // this does nothing
    Disabled      = 1u<<8,
    NoGeoClip     = 1u<<9, // don't clip with map geometry, don't cast geometry shadows
    LightSource   = 1u<<10, // this light comes from the light source thing (i.e. not a "converted" one)
    // other
    LightChanged = 1u<<29,
    LightActive  = 1u<<30,
  };
  vuint32 Flags;

  inline bool IsChanged () const noexcept { return (Flags&LightChanged); }
  inline bool IsActive () const noexcept { return (Flags&LightActive); }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VSndSeqInfo {
  VName Name;
  vint32 OriginId;
  TVec Origin;
  vint32 ModeNum;
  TArray<VName> Choices;
};


// ////////////////////////////////////////////////////////////////////////// //
struct VCameraTextureInfo {
  VEntity *Camera;
  int TexNum;
  int FOV;
};


// ////////////////////////////////////////////////////////////////////////// //
class VLevel;
class VLevelInfo;
struct VPathTraverseWorkData;


class VLevelScriptThinker : public VSerialisable {
public:
  bool destroyed; // script `Destroy()` method should set this (and check to avoid double destroying)
  VLevel *XLevel; // level object
  VLevelInfo *Level; // level info object

public:
  inline VLevelScriptThinker () : destroyed(false), XLevel(nullptr), Level(nullptr) {}
  virtual ~VLevelScriptThinker ();

  inline void DestroyThinker () { Destroy(); }

  // it is guaranteed that `Destroy()` will be called by master before deleting the object
  virtual void Destroy () = 0;
  virtual void ClearReferences () = 0;
  virtual void Tick (float DeltaTime) = 0;

  virtual VName GetName () = 0;
  virtual int GetNumber () = 0;

  virtual VStr DebugDumpToString () = 0;
};

extern VLevelScriptThinker *AcsCreateEmptyThinker ();
//FIXME
extern void AcsSuspendScript (VAcsLevel *acslevel, int number, int map);
extern void AcsTerminateScript (VAcsLevel *acslevel, int number, int map);
extern bool AcsHasScripts (VAcsLevel *acslevel);


//==========================================================================
//
//                  LEVEL
//
//==========================================================================
enum {
  MAX_LEVEL_TRANSLATIONS      = 0xffff,
  MAX_BODY_QUEUE_TRANSLATIONS = 0xff,
};


// ////////////////////////////////////////////////////////////////////////// //
struct VSectorLink {
  vint32 index;
  vint32 mts; // bit 30: set if ceiling; low byte: movetype
  vint32 next; // index, or -1
};


// ////////////////////////////////////////////////////////////////////////// //
struct VCtl2DestLink {
  vint32 src; // sector number, just in case (can be -1 for unused slots)
  vint32 dest; // sector number (can be -1 for unused slots)
  vint32 next; // index in `ControlLinks` or -1
};


// ////////////////////////////////////////////////////////////////////////// //
struct VLightParams {
  TVec Origin;
  float Radius;
  vuint32 Color;
  TVec coneDirection;
  float coneAngle;
  sector_t *LevelSector;
  float LevelScale; // <0: attenuated

  inline VLightParams () noexcept
    : Origin(0.0f, 0.0f, 0.0f)
    , Radius(0.0f)
    , Color(0u)
    , coneDirection(0.0f, 0.0f, 0.0f)
    , coneAngle(0.0f)
    , LevelSector(nullptr)
    , LevelScale(0.0f)
  {}

  inline VLightParams (const rep_light_t &L) noexcept {
    Origin = L.Origin;
    Radius = L.Radius;
    Color = L.Color;
    coneDirection = L.ConeDir;
    coneAngle = L.ConeAngle;
    LevelSector = L.LevelSector;
    LevelScale = L.LevelScale;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VPolyLink3D {
  vint32 srcpid;
  vint32 destpid;
  enum {
    // for sequences, link all objects from `srcpid` to `destpid`
    Flag_NothingZero = 0u,
    Flag_Sequence = 1u<<0,
  };
  vuint32 flags;
};


// ////////////////////////////////////////////////////////////////////////// //
struct VCustomKeyInfo {
  struct Value {
    int i;
    float f;

    inline Value () noexcept : i(0), f(0.0f) {}
    inline Value (const int iv, const float fv) noexcept : i(iv), f(fv) {}
  };

  TArray<Value> allValues;

  TMapNC<VCustomKeyInfo_NamedValue, int> idMap; // value: index in `allValues`

  inline VCustomKeyInfo () noexcept : idMap() {}
  inline ~VCustomKeyInfo () noexcept { idMap.clear(); }

  inline void putKey (int id, const char *name, int iv, float fv) noexcept {
    if (!name || !name[0] || !id) return;
    VName n = VName(name, VName::AddLower);
    VCustomKeyInfo_NamedValue key(n, id);
    const auto mp = idMap.get(key);
    if (!mp) {
      // new value
      const int aidx = allValues.length();
      allValues.append(Value(iv, fv));
      idMap.put(key, aidx);
    } else {
      // replace old value
      allValues[*mp] = Value(iv, fv);
    }
  }

  inline const Value *findKey (int id, const char *name) const noexcept {
    if (!id || !name || !name[0]) return nullptr;
    VName n = VName(name, VName::FindLower);
    if (n == NAME_None) return nullptr;
    VCustomKeyInfo_NamedValue key(n, id);
    const auto mp = idMap.get(key);
    if (!mp) return nullptr;
    return &allValues[*mp];
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VDecalList {
  decal_t *head;
  decal_t *tail;
};


// ////////////////////////////////////////////////////////////////////////// //
class VLevel : public VGameObject {
  DECLARE_CLASS(VLevel, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VLevel)

  friend class VUdmfParser;

  VName MapName;
  VStr MapHash; // lowercase, RIPEMD-160, 20 bytes
  VStr MapHashMD5; // lowercase
  vuint32 LSSHash; // xxHash32 of linedefs, sidedefs, sectors (in this order)
  vuint32 SegHash; // xxHash32 of segs

  // flags
  enum {
    LF_ForServer = 1u<<0, // true if this level belongs to the server
    LF_Extended  = 1u<<1, // true if level was in Hexen format (or extended namespace for UDMF)
    LF_TextMap   = 1u<<2, // UDMF format map
    // used in map fixer
    LF_ForceUseZDBSP                    = 1u<<3, // force ZDBSP (it works better with some ancient tricky maps)
    LF_ForceAllowSeveralPObjInSubsector = 1u<<4, // this is not used anymore, but kept for compatibility
    LF_ForceNoTexturePrecache           = 1u<<5,
    LF_ForceNoPrecalcStaticLights       = 1u<<6,
    LF_ForceNoDeepwaterFix              = 1u<<7,
    LF_ForceNoFloorFloodfillFix         = 1u<<8,
    LF_ForceNoCeilingFloodfillFix       = 1u<<9,
    // used only in VavoomC
    LF_ConvertSectorLightsToStatic      = 1u<<10,
    // set by pobj spawner
    LF_Has3DPolyObjects                 = 1u<<11,
    // set by loader
    // DO NOT REMOVE!
    //LF_HasFullSegs                      = 1u<<12,
    // BadApple.wad ;-)
    LF_IsBadApple                       = 1u<<13,
  };
  vuint32 LevelFlags;

  VName UDMFNamespace; // locased

  // MAP related Lookup tables
  // store VERTEXES, LINEDEFS, SIDEDEFS, etc.

  TVec *Vertexes;
  vint32 NumVertexes;

  sector_t *Sectors;
  vint32 NumSectors;

  side_t *Sides;
  vint32 NumSides;

  line_t *Lines;
  vint32 NumLines;

  seg_t *Segs;
  vint32 NumSegs;

  subsector_t *Subsectors;
  vint32 NumSubsectors;

  node_t *Nodes;
  vint32 NumNodes;

  // !!! Used only during level loading
  mthing_t *Things;
  vint32 NumThings;

  // BLOCKMAP
  // created from axis aligned bounding box of the map, a rectangular array of blocks of size ...
  // used to speed up collision detection by spatial subdivision in 2D
  vint32 BlockMapLumpSize;
  vint32 *BlockMapLump; // offsets in blockmap are from here
  vint32 *BlockMap;   // int for larger maps
  vint32 BlockMapWidth;  // Blockmap size.
  vint32 BlockMapHeight; // size in mapblocks
  float BlockMapOrgX; // origin of block map
  float BlockMapOrgY;
  VEntity **BlockLinks;   // for thing chains
  polyblock_t **PolyBlockMap;

  // REJECT
  // for fast sight rejection
  // speeds up enemy AI by skipping detailed LineOf Sight calculation
  // without special effect, this could be used as a PVS lookup as well
  vuint8 *RejectMatrix;
  vint32 RejectMatrixSize;

  // strife conversations
  FRogueConSpeech *GenericSpeeches;
  vint32 NumGenericSpeeches;

  FRogueConSpeech *LevelSpeeches;
  vint32 NumLevelSpeeches;

  // list of all poly-objects on the level
  polyobj_t **PolyObjs;
  vint32 NumPolyObjs;

  // anchor points of poly-objects
  PolyAnchorPoint_t *PolyAnchorPoints;
  vint32 NumPolyAnchorPoints;

  // this is used only once, on pobj initialisation
  VPolyLink3D *PolyLinks3D;
  vint32 NumPolyLinks3D;

  TMapNC<int, int> *PolyTagMap; // key: tag; value: index in PolyObjs

  // sound environments for sector zones
  vint32 *Zones;
  vint32 NumZones;

  VThinker *ThinkerHead;
  VThinker *ThinkerTail;

  VLevelInfo *LevelInfo;
  VWorldInfo *WorldInfo;

  VAcsLevel *Acs;

  VRenderLevelPublic *Renderer;
  VNetContext *NetContext;

  rep_line_t *BaseLines;
  rep_side_t *BaseSides;
  rep_sector_t *BaseSectors;
  rep_polyobj_t *BasePolyObjs;

  TArray<rep_light_t> StaticLights;
  TMapNC<vuint32, int> *StaticLightsMap; // key: owner; value: index in `StaticLights`

  TArray<VSndSeqInfo> ActiveSequences;
  TArray<VCameraTextureInfo> CameraTextures;

  float NextTime; // current game time, in seconds (set *BEFORE* all tickers called)
  float Time; // current game time, in seconds (incremented *AFTER* all tickers called)
  vint32 TicTime; // game time, in tics (35 per second)

  msecnode_t *SectorList;
  // phares 3/21/98
  // Maintain a freelist of msecnode_t's to reduce memory allocs and frees
  msecnode_t *HeadSecNode;

  // Translations controlled by ACS scripts
  TArray<VTextureTranslation *> Translations;
  TArray<VTextureTranslation *> BodyQueueTrans;

  VState *CallingState;
  VStateCall *StateCall;

  vint32 NextSoundOriginID;

  decal_t *decanimlist;

  //vint32 tagHashesSaved; // for savegame compatibility
  TagHash *lineTags;
  TagHash *sectorTags;

  TArray<vint32> sectorlinkStart;
  TArray<VSectorLink> sectorlinks;

  TArray<VLevelScriptThinker *> scriptThinkers;

  vuint32 csTouchCount;
  // for ChangeSector; Z_Malloc'ed, NumSectors elements
  // bit 31 is return value from `ChangeSectorInternal()`
  // if other bits are equal to csTouchCount
  vuint32 *csTouched;

  // sectors with fake floors/ceilings, so world updater can skip iterating over all of them
  TArray<vint32> FakeFCSectors;
  //TArray<vint32> TaggedSectors;

  // this is used to keep control->dest 3d floor links
  // [0..NumSectors) is entry point, then follow next index (-1 means 'end')
  // order is undefined
  TArray<VCtl2DestLink> ControlLinks;

  // array of cached subsector centers (used in VC code, no need to save/load it)
  TArray<TVec> ssCenters;

  vint32 tsVisitedCount;
  // for sector height cache
  vint32 validcountSZCache;

  // set in `LoadMap()`, can be used by renderer to load/save lightmap cache
  VStr cacheFileBase; // can be empty, otherwise it is path and file name w/o extension
  enum {
    CacheFlag_Ignore = 1u<<0,
  };
  vuint32 cacheFlags;

  TArray<VMapMarkerInfo> MapMarkers;

  VCustomKeyInfo *UserLineKeyInfo;
  VCustomKeyInfo *UserThingKeyInfo;
  VCustomKeyInfo *UserSectorKeyInfo;
  VCustomKeyInfo *UserSideKeyInfo0;
  VCustomKeyInfo *UserSideKeyInfo1;
  VCustomKeyInfo *UserLineIdxKeyInfo; // key: line_index+1
  VCustomKeyInfo *UserSideIdxKeyInfo; // key: side_index+1

  // temporaries for `VPathTraverse`
  // it is safe to have only one set of those, because `VPathTraverse` is never called recursively
  vint32 *processedBMCells; // used in thing collector, contains validcount; size is `BlockMapWidth*BlockMapHeight`
  vint32 processedBMCellsSize;

  // each world tick will reset this
  // put here to avoid reallocations in `VPathTraverse`
  // each call to `VPathTraverse` will build a new list from `pathInterceptsUsed`
  // deleting iterator in order will lower `pathInterceptsUsed`
  intercept_t *pathIntercepts;
  vint32 pathInterceptsAlloted;
  vint32 pathInterceptsUsed;

  intercept_t *tempPathIntercepts;
  vint32 tempPathInterceptsAlloted;
  vint32 tempPathInterceptsUsed;

  // list of all sector floor/ceiling decals
  // `dc->slidesec` is used to store decal sector
  // this list will be passed to renderer, so it will be able to populate decal lists for subsectors
  // this list is kept up-to-date by renderer (via `dc->mainDecal` link)
  // note that animators will animate this list separately from renderer list (i.e. both lists will be animated)
  decal_t *subdecalhead;
  decal_t *subdecaltail;

  // server uid -> object
  // dynamically allocated, so it won't be scanned by GC
  // bookkeeping is done in `AddThinker()` and `RemoveThinker()`
  TMapNC<vuint32, VEntity *> *suid2ent;

  TMapNC<vuint32, VEntity *> *SoundIDMap;

  // unified list for floor and ceiling decals
  VDecalList *subsectorDecalList;

public:
  // sorry for those globals
  static TMapNC<VName, bool> baddecals;
  static TArrayNC<VEntity *> poRoughEntityList; // for polyobjects

public:
  // decal floodfill statics
  // "subsector touched" marks for portal spread
  static TArray<int> dcSubTouchMark;
  static int dcSubTouchCounter;
  static TMapNC<polyobj_t *, bool> dcPobjTouched;
  static bool dcPobjTouchedReset;

  void IncSubTouchCounter () noexcept;

  inline bool IsSubTouched (const subsector_t *sub) noexcept {
    return (sub ? (dcSubTouchMark.ptr()[(unsigned)(ptrdiff_t)(sub-&Subsectors[0])] == dcSubTouchCounter) : true);
  }

  inline void MarkSubTouched (const subsector_t *sub) noexcept {
    if (sub) dcSubTouchMark.ptr()[(unsigned)(ptrdiff_t)(sub-&Subsectors[0])] = dcSubTouchCounter;
  }

public:
  void RegisterEntitySoundID (VEntity *ent, int SoundOriginID);
  void RemoveEntitySoundID (int SoundOriginID);
  VEntity *FindEntityBySoundID (int SoundOriginID);

  sector_t *FindSectorBySoundID (int SoundOriginID);
  polyobj_t *FindPObjBySoundID (int SoundOriginID);

public:
  // decal segfill statics
  // "subsector touched" marks for portal spread
  static TArrayNC<int> dcLineTouchMark;
  static TArrayNC<int> dcSegTouchMark;
  static int dcLineTouchCounter; // and for segs too
  //static TMapNC<polyobj_t *, bool> dcPobjTouched;
  //static bool dcPobjTouchedReset;

  // temporary working set for decal spreader
  struct DecalLineInfo {
    line_t *nline;
    int nside;
    bool isv1;
    bool isbackside;
  };

  static TArray<DecalLineInfo> connectedLines;


  void IncLineTouchCounter () noexcept;

  inline bool IsLineTouched (const line_t *line) noexcept {
    return (line ? (dcLineTouchMark.ptr()[(unsigned)(ptrdiff_t)(line-&Lines[0])] == dcLineTouchCounter) : true);
  }

  inline void MarkLineTouched (const line_t *line) noexcept {
    if (line) dcLineTouchMark.ptr()[(unsigned)(ptrdiff_t)(line-&Lines[0])] = dcLineTouchCounter;
  }

  inline bool IsSegTouched (const seg_t *seg) noexcept {
    return (seg ? (dcSegTouchMark.ptr()[(unsigned)(ptrdiff_t)(seg-&Segs[0])] == dcLineTouchCounter) : true);
  }

  inline void MarkSegTouched (const seg_t *seg) noexcept {
    if (seg) dcSegTouchMark.ptr()[(unsigned)(ptrdiff_t)(seg-&Segs[0])] = dcLineTouchCounter;
  }

  inline bool IsBadApple () const noexcept { return (LevelFlags&LF_IsBadApple); }

public:
  inline void ResetTempPathIntercepts () noexcept { tempPathInterceptsUsed = 0; }

  inline int GetTempPathInterceptsCount () const noexcept { return tempPathInterceptsUsed; }

  inline intercept_t *GetTempPathInterceptFirst () noexcept { return tempPathIntercepts; }

  inline intercept_t *AllocTempPathIntercept () noexcept {
    if (tempPathInterceptsUsed >= tempPathInterceptsAlloted) {
      tempPathInterceptsAlloted += 8192; // why not?
      tempPathIntercepts = (intercept_t *)Z_Realloc(tempPathIntercepts, tempPathInterceptsAlloted*sizeof(tempPathIntercepts[0]));
    }
    return &tempPathIntercepts[tempPathInterceptsUsed++];
  }


  inline void ResetAllPathIntercepts () noexcept { pathInterceptsUsed = 0; }

  // call before start filling interception array; returns first index
  inline int StartPathInterception () const noexcept { return pathInterceptsUsed; }
  // call after finishing filling interception array; returns first unused index
  inline int EndPathInterception () const noexcept { return pathInterceptsUsed; }
  // call after finishing filling interception array; returns first unused index
  inline int CurrentPathInterceptionIndex () const noexcept { return pathInterceptsUsed; }

  inline void PopLastIntercept () noexcept { if (--pathInterceptsUsed < 0) pathInterceptsUsed = 0; }

  // pass results of the above methods here
  inline void ReleasePathInterception (const int start, const int end) noexcept {
    vassert(start <= end);
    vassert(start >= 0);
    vassert(end >= 0);
    vassert(end <= pathInterceptsUsed);
    // release pool items if we can
    if (end == pathInterceptsUsed) pathInterceptsUsed = start;
  }

  inline intercept_t *GetPathIntercept (const int idx) const noexcept {
    return (idx >= 0 && idx < pathInterceptsUsed ? &pathIntercepts[idx] : nullptr);
  }

  inline intercept_t *AllocPathIntercept () noexcept {
    if (pathInterceptsUsed >= pathInterceptsAlloted) {
      pathInterceptsAlloted += 8192; // why not?
      pathIntercepts = (intercept_t *)Z_Realloc(pathIntercepts, pathInterceptsAlloted*sizeof(pathIntercepts[0]));
    }
    return &pathIntercepts[pathInterceptsUsed++];
  }

  inline void EnsureProcessedBMCellsArray () {
    const int size = (int)(BlockMapWidth*BlockMapHeight);
    if (processedBMCellsSize < size) {
      processedBMCellsSize = size;
      processedBMCells = (vint32 *)Z_Realloc(processedBMCells, size*sizeof(processedBMCells[0]));
      if (size) memset(processedBMCells, 0, size*sizeof(processedBMCells[0]));
    }
  }

public:
  // iterator
  struct CtlLinkIter {
    VLevel *level;
    int srcidx;
    int curridx;

    CtlLinkIter () = delete;

    CtlLinkIter (VLevel *alevel, int asrcidx) : level(alevel), srcidx(asrcidx), curridx(asrcidx) {
      if (!alevel || asrcidx < 0 || asrcidx >= alevel->ControlLinks.length()) {
        curridx = -1;
      } else if (alevel->ControlLinks[asrcidx].dest < 0) {
        // no linked sectors for this source
        curridx = -1;
      }
    }

    inline bool isEmpty () const { return (!level || curridx < 0); }
    inline sector_t *getDestSector () const { return (level && curridx >= 0 ? level->Sectors+level->ControlLinks[curridx].dest : nullptr); }
    inline int getDestSectorIndex () const { return (level && curridx >= 0 ? level->ControlLinks[curridx].dest : -1); }
    inline void next () { if (level && curridx >= 0) curridx = level->ControlLinks[curridx].next; }
  };

private:
  // returns `false` for duplicate/invalid link
  bool AppendControlLink (const sector_t *src, const sector_t *dest);
  inline CtlLinkIter IterControlLinks (const sector_t *src) { return CtlLinkIter(this, (src ? (int)(ptrdiff_t)(src-Sectors) : -1)); }

  // `-1` for `crunch` means "ignore stuck mobj"
  bool ChangeSectorInternal (sector_t *sector, int crunch);
  void ChangeOneSectorInternal (sector_t *sector);

private:
  void xxHashLinedef (XXH32_state_t *xctx, const line_t &line) const {
    if (!xctx) return;
    // sides, front and back sectors
    XXH32_update(xctx, &line.sidenum[0], sizeof(line.sidenum[0]));
    XXH32_update(xctx, &line.sidenum[1], sizeof(line.sidenum[1]));
    vuint32 fsnum = (line.frontsector ? (vuint32)(ptrdiff_t)(line.frontsector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &fsnum, sizeof(fsnum));
    vuint32 bsnum = (line.backsector ? (vuint32)(ptrdiff_t)(line.backsector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &bsnum, sizeof(bsnum));
  }

  void xxHashSidedef (XXH32_state_t *xctx, const side_t &side) const {
    if (!xctx) return;
    // sector, line
    vuint32 secnum = (side.Sector ? (vuint32)(ptrdiff_t)(side.Sector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &secnum, sizeof(secnum));
    XXH32_update(xctx, &side.LineNum, sizeof(side.LineNum));
  }

  void xxHashSectordef (XXH32_state_t *xctx, const sector_t &sec) const {
    if (!xctx) return;
    // number of lines in sector
    XXH32_update(xctx, &sec.linecount, sizeof(sec.linecount));
  }

  void xxHashPlane (XXH32_state_t *xctx, const TPlane &pl) const {
    if (!xctx) return;
    XXH32_update(xctx, &pl.normal.x, sizeof(pl.normal.x));
    XXH32_update(xctx, &pl.normal.y, sizeof(pl.normal.y));
    XXH32_update(xctx, &pl.normal.z, sizeof(pl.normal.z));
    XXH32_update(xctx, &pl.dist, sizeof(pl.dist));
  }

  // seg hash is using to restore decals
  // it won't prevent level loading
  void xxHashSegdef (XXH32_state_t *xctx, const seg_t &seg) const {
    if (!xctx) return;
    xxHashPlane(xctx, seg);
    vuint32 v0idx = (vuint32)(ptrdiff_t)(seg.v1-Vertexes);
    XXH32_update(xctx, &v0idx, sizeof(v0idx));
    vuint32 v1idx = (vuint32)(ptrdiff_t)(seg.v2-Vertexes);
    XXH32_update(xctx, &v1idx, sizeof(v1idx));
    //!XXH32_update(xctx, &seg.offset, sizeof(seg.offset));
    //!XXH32_update(xctx, &seg.length, sizeof(seg.length));
    vuint32 sdnum = (seg.sidedef ? (vuint32)(ptrdiff_t)(seg.sidedef-Sides) : 0xffffffffU);
    XXH32_update(xctx, &sdnum, sizeof(sdnum));
    vuint32 ldnum = (seg.linedef ? (vuint32)(ptrdiff_t)(seg.linedef-Lines) : 0xffffffffU);
    XXH32_update(xctx, &ldnum, sizeof(ldnum));
    vuint32 fsnum = (seg.frontsector ? (vuint32)(ptrdiff_t)(seg.frontsector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &fsnum, sizeof(fsnum));
    vuint32 bsnum = (seg.backsector ? (vuint32)(ptrdiff_t)(seg.backsector-Sectors) : 0xffffffffU);
    XXH32_update(xctx, &bsnum, sizeof(bsnum));
    vuint32 pnum = (seg.partner ? (vuint32)(ptrdiff_t)(seg.partner-Segs) : 0xffffffffU);
    XXH32_update(xctx, &pnum, sizeof(pnum));
    XXH32_update(xctx, &seg.side, sizeof(seg.side));
  }

public:
  // if `ImmediateRun` is true, init some script variables, but don't register thinker
  void AddScriptThinker (VLevelScriptThinker *sth, bool ImmediateRun);
  void PromoteImmediateScriptThinker (VLevelScriptThinker *sth); // WARNING! does no checks!
  void RemoveScriptThinker (VLevelScriptThinker *sth); // won't call `Destroy()`, won't call `delete`
  void RunScriptThinkers (float DeltaTime);

  void SuspendNamedScriptThinkers (VStr name, int map);
  void TerminateNamedScriptThinkers (VStr name, int map);

public:
  // basic coldet code
  enum CD_HitType {
    CD_HT_None,
    CD_HT_Point,
    // sides
    CD_HT_Top,
    CD_HT_Bottom,
    CD_HT_Left,
    CD_HT_Right,
  };

  // negative clip epsilon means "use some default"
  static float SweepLinedefAABB (const line_t *ld, TVec vstart, TVec vend, TVec bmin, TVec bmax,
                                 TPlane *hitPlane=nullptr, TVec *contactPoint=nullptr, CD_HitType *hitType=nullptr,
                                 int *hitplanenum=nullptr, float clipEpsilon=-1.0f);

public:
  void ResetValidCount ();
  inline void IncrementValidCount () { if (++validcount == MAX_VINT32) ResetValidCount(); }

  void ResetSZValidCount ();
  inline void IncrementSZValidCount () { if (++validcountSZCache == MAX_VINT32) ResetSZValidCount(); }

  void ResetVisitedCount ();
  inline vint32 nextVisitedCount () { if (++tsVisitedCount == MAX_VINT32) ResetVisitedCount(); return tsVisitedCount; }

  // this saves everything except thinkers, so i can load it for further experiments
  void DebugSaveLevel (VStream &strm);

  // this is slow!
  float CalcSkyHeight () const;

  // some sectors (like doors) has floor and ceiling on the same level, so
  // we have to look at neighbour sector to get height.
  // note that if neighbour sector is closed door too, we can safely use
  // our zero height, as camera cannot see through top/bottom textures.
  //void CalcSectorBoundingHeight (sector_t *sector, float *minz, float *maxz);

  void UpdateSubsectorBBox (int num, float bbox[6], const float skyheight);
  void RecalcWorldNodeBBox (int bspnum, float bbox[6], const float skyheight);
  // this also fixes 2d node bounding boxes (those can be wrong due to integers in ajbsp)
  void RecalcWorldBBoxes ();

  // recalcs world bboxes if some cvars changed
  void CheckAndRecalcWorldBBoxes ();

  void UpdateSectorHeightCache (sector_t *sector);
  void GetSubsectorBBox (subsector_t *sub, float bbox[6]);
  void CalcSecMinMaxs (sector_t *sector, bool fixTexZ=false); // also, update BSP bounding boxes

  //inline bool VVA_CHECKRESULT HasFullSegs () const noexcept { return (LevelFlags&LF_HasFullSegs); }

private:
  static bool canReplaceBlood;
  static VName goreBloodDecalSplat;
  static VName goreBloodDecalSmear;
  static VName goreBloodDecalSplatRadius;
  static VName goreBloodDecalSmearRadius;

public:
  static void LevelStaticInit ();

  virtual void PostCtor () override;
  virtual void SerialiseOther (VStream &Strm) override;
  virtual void ClearReferences () override;
  virtual void Destroy () override;

  // map loader
  void LoadMap (VName MapName);

  // vanilla buggy algo
  subsector_t *PointInSubsector_Buggy (const TVec &point) const noexcept;
  // bugfixed algo
  subsector_t *PointInSubsector (const TVec &point) const noexcept;

  bool IsPointInSubsector2D (const subsector_t *sub, TVec in) const noexcept;
  // checks all subsectors
  bool IsPointInSector2D (const sector_t *sec, TVec in) const noexcept;

  inline vuint8 IsRejectedVis (const sector_t *from, const sector_t *dest) const noexcept {
    if (!from || !dest) return 0xff; // this is REJECT matrix, not ACCEPT
    if (RejectMatrix) {
      const unsigned s1 = (unsigned)(ptrdiff_t)(from-Sectors);
      const unsigned s2 = (unsigned)(ptrdiff_t)(dest-Sectors);
      const unsigned pnum = s1*(unsigned)NumSectors+s2;
      return RejectMatrix[pnum>>3]&(1u<<(pnum&7));
    }
    return 0x00; // this is REJECT matrix, not ACCEPT
  }

  void ResetStaticLights ();

  // returns static light id
  void AddStaticLightRGB (VEntity *Ent, const VLightParams &lpar, const vuint32 flags);
  void MoveStaticLightByOwner (VEntity *Ent, const TVec &Origin);
  void RemoveStaticLightByOwner (VEntity *Ent);

  // returns static light id
  void AddStaticLightRGB (vuint32 owneruid, const VLightParams &lpar, const vuint32 flags);
  void MoveStaticLightByOwner (vuint32 owneruid, const TVec &Origin);
  void RemoveStaticLightByOwner (vuint32 owneruid);

  VThinker *SpawnThinker (VClass *AClass, const TVec &AOrigin=TVec(0, 0, 0),
                          const TAVec &AAngles=TAVec(0, 0, 0), mthing_t *mthing=nullptr,
                          bool AllowReplace=true, vuint32 srvUId=0);

  void AddThinker (VThinker *Th);
  void RemoveThinker (VThinker *Th);
  void DestroyAllThinkers ();

  inline VEntity *GetEntityBySUId (vuint32 suid) const {
    VEntity **ee = (suid ? suid2ent->get(suid) : nullptr);
    return (ee ? *ee : nullptr);
  }

  // called from netcode for client connection, after `LevelInfo` was received
  void UpdateThinkersLevelInfo ();

  void TickDecals (float DeltaTime); // this should be called in `CL_UpdateMobjs()`
  void TickWorld (float DeltaTime);

  // polyobjects
  void SpawnPolyobj (mthing_t *thing, float x, float y, float height, int tag, bool crush, bool hurt);
  void AddPolyAnchorPoint (mthing_t *thing, float x, float y, float height, int tag);
  void Add3DPolyobjLink (mthing_t *thing, int srcpid, int destpid);
  void InitPolyobjs ();

  void ResetPObjRenderCounts () noexcept; // called from renderer

  polyobj_t *GetPolyobj (int polyNum) noexcept; // actually, tag
  int GetPolyobjMirror (int poly); // tag again

  //WARNING! keep in sync with ACS and "Level.vc"!
  enum {
    POFLAG_FORCED  = 1u<<0, // do not check position, do not carry objects
    POFLAG_NOLINK  = 1u<<1, // ignore linked polyobjects
    POFLAG_INDROT  = 1u<<2, // for linked polyobjects, don't use master pobj center, but rotate each pobj around it's own center
  };

  bool MovePolyobj (int num, float x, float y, float z=0.0f, unsigned flags=0u); // tag (GetPolyobj)
  bool RotatePolyobj (int num, float angle, unsigned flags=0u); // tag (GetPolyobj)

  // called from loader
  void FixPolyobjCachedFlags (polyobj_t *po);

  // do not call directly!
  bool CheckBSPB2DBoxNode (int bspnum, const float bbox2d[4], bool (*cb) (VLevel *level, subsector_t *sub, void *udata), void *udata) noexcept;
  // return `false` from `cb` to stop checking
  // will not invoke `cb` for original polyobject (sub)sectors
  void CheckBSPB2DBox (const float bbox2d[4], bool (*cb) (VLevel *level, subsector_t *sub, void *udata), void *udata) noexcept;

  // `-1` for `crunch` means "ignore stuck mobj"
  // returns `true` if movement was blocked
  bool ChangeSector (sector_t *sector, int crunch);

  // returns `false` on hit
  bool TraceLine (linetrace_t &Trace, const TVec &Start, const TVec &End, unsigned PlaneNoBlockFlags, unsigned moreLineBlockFlags=0);

  // doesn't check pvs or reject
  // `Sector` can be `nullptr`
  // if `orgdirRight` is zero vector, don't do extra checks
  bool CastCanSee (const subsector_t *SubSector, const TVec &org, float myheight, const TVec &orgdirFwd, const TVec &orgdirRight,
                   const TVec &dest, float radius, float height, bool skipBaseRegion, const subsector_t *DestSubSector,
                   bool allowBetterSight, bool ignoreBlockAll, bool ignoreFakeFloors);
  // this is used to trace light rays (via blockmap)
  bool CastLightRay (bool textureCheck, const subsector_t *SubSector, const TVec &org, const TVec &dest, const subsector_t *DestSubSector=nullptr);

  void SetCameraToTexture (VEntity *, VName, int);

  msecnode_t *AddSecnode (sector_t *, VEntity *, msecnode_t *);
  msecnode_t *DelSecnode (msecnode_t *);
  void DelSectorList ();

  int FindSectorFromTag (sector_t *&sector, int tag, int start=-1);
  line_t *FindLine (int, int *);
  void SectorSetLink (int controltag, int tag, int surface, int movetype);

  inline bool IsForServer () const { return !!(LevelFlags&LF_ForServer); }
  inline bool IsForClient () const { return !(LevelFlags&LF_ForServer); }

  bool SaveCachedData (VStream *strm); // returns success
  bool LoadCachedData (VStream *strm); // returns success
  void ClearAllMapData (); // call this if cache is corrupted

  void FixKnownMapErrors ();

  void AddExtraFloor (line_t *line, sector_t *dst);

  void CalcLine (line_t *line); // this calls `CalcLineCDPlanes()`
  void CalcLineCDPlanes (line_t *line);
  void CalcSegLenOfs (seg_t *seg); // only length and offset
  void CalcSegPlaneDir (seg_t *seg); // plane, direction, but length and offset should be already set

  // returns 0 for "unknown"
  // `flags` is `VDrawer::ELFlag_XXX` set
  vuint32 CalcEntityLight (VEntity *lowner, unsigned flags);

private:
  void AddExtraFloorSane (line_t *line, sector_t *dst); // k8vavoom
  void AddExtraFloorShitty (line_t *line, sector_t *dst); // gozzo

  void RegisterPolyObj (int poidx) noexcept; // NOT a tag!

public:
  static void dumpRegion (const sec_region_t *reg);
  static void dumpSectorRegions (const sector_t *dst);

  static bool CheckPlanePass (const TSecPlaneRef &plane, const TVec &linestart, const TVec &lineend, TVec &currhit, bool &isSky);

  // set `SPF_IGNORE_FAKE_FLOORS` is flagmask to ignore fake floors
  // set `SPF_IGNORE_BASE_REGION` is flagmask to ignore base region
  static bool CheckPassPlanes (sector_t *sector, TVec linestart, TVec lineend, unsigned flagmask,
                               TVec *outHitPoint, TVec *outHitNormal, bool *outIsSky, TPlane *outHitPlane, float *outTime=nullptr);

  // returns hit time
  // negative means "no hit"
  static float CheckPObjPassPlanes (const polyobj_t *po, const TVec &linestart, const TVec &lineend,
                                    TVec *outHitPoint=nullptr, TVec *outHitNormal=nullptr, TPlane *outHitPlane=nullptr);

  // this function is used to check planes of polyobject that *CONTAINS* `linestart`
  // WARNING! `linestart` must be inside a polyobject!
  // returns hit time
  // negative means "no hit"
  float CheckPObjPlanesPoint (const TVec &linestart, const TVec &lineend, const subsector_t *stsub=nullptr,
                              TVec *outHitPoint=nullptr, TVec *outHitNormal=nullptr, TPlane *outHitPlane=nullptr,
                              polyobj_t **po=nullptr);

  // returns `false` if seg is out of subsector
  // WARNING! this WILL MODIFY `seg->v1` and `seg->v2`!
  // will not modify `drawsegs`, etc.
  bool ClipPObjSegToSub (const subsector_t *sub, seg_t *seg) noexcept;

  VVA_CHECKRESULT inline bool Has3DPolyObjects () const noexcept { return (LevelFlags&LF_Has3DPolyObjects); }

  //WARNING! coords and bounding boxes MUST be valid for the following methods!
  // sctor and polyobject may be `nullptr` or non-3d, tho

  bool CanHave3DPolyObjAt2DPoint (const float x, float y) const noexcept;
  bool CanHave3DPolyObjAt3DPoint (const TVec &p) const noexcept;

  bool CanHave3DPolyObjAt2DBBox (const float bb2d[4]) const noexcept;
  bool CanHave3DPolyObjAt3DBBox (const float bb3d[6]) const noexcept;

  // if the point is exactly on a seg, it is inside
  bool IsPointInsideSubsector2D (const subsector_t *sub, const float x, const float y) const noexcept;
  bool IsBBox2DTouchingSubsector (const subsector_t *sub, const float bb2d[4]) const noexcept;

  // if the point is exactly on a seg, it is inside
  bool IsPointInsideSector2D (const sector_t *sec, const float x, const float y) const noexcept;
  bool IsBBox2DTouchingSector (const sector_t *sec, const float bb2d[4]) const noexcept;

  bool Is2DPointInside3DPolyObj (const polyobj_t *po, const float x, const float y) const noexcept;
  bool Is3DPointInside3DPolyObj (const polyobj_t *po, const TVec &pos) const noexcept;

  bool IsBBox2DTouching3DPolyObj (const polyobj_t *po, const float bb2d[4]) const noexcept;
  bool IsBBox3DTouching3DPolyObj (const polyobj_t *po, const float bb3d[6]) const noexcept;

public:
  // returns region to use as light param source, and additionally glow flags (`glowFlags` can be nullptr)
  // glowFlags: bit 0: floor glow allowed; bit 1: ceiling glow allowed
  // `sub` can be `nullptr`
  sec_region_t *PointRegionLight (const subsector_t *sub, const TVec &p, unsigned *glowFlags=nullptr);

public: // regions and openings internal functions
  static void DumpRegion (const sec_region_t *inregion, bool extendedflags=false) noexcept;
  static void DumpOpening (const opening_t *op) noexcept;
  static void DumpOpPlanes (const TArray<opening_t> &list) noexcept;

  // cut the solid region from the list of openings in `dest`
  // the list of openings will mainain its sorted order (from the lowest to the highest)
  // also, no openings will intersect each other
  void CutSolidRegion (TArray<opening_t> &dest, const float bottom, const float top,
                       const TSecPlaneRef &efloor, const TSecPlaneRef &eceiling);

  void Cut3DMidtex (TArray<opening_t> &dest, const sector_t *sector, const line_t *linedef);

  void GetBaseSectorOpening (opening_t &op, sector_t *sector, const TVec point, bool usePoint);

  enum {
    BSO_UsePoint = 1u<<0,
    BSO_Do3DMid  = 1u<<1,
    //
    BSO_NothingZero = 0u,
  };

  void BuildSectorOpenings (const line_t *xldef, TArray<opening_t> &dest, sector_t *sector, const TVec point,
                            unsigned NoBlockFlags, unsigned bsoFlags);

public: // regions
  // returns `CONTENTS_xxx` or sector content value
  int PointContents (const sector_t *sector, const TVec &p, bool dbgDump=false);

  const sec_region_t *PointContentsRegion (const sector_t *sector, const TVec &p);

  // find region for thing, and return best floor/ceiling
  // `p.z` is bottom
  void FindGapFloorCeiling (sector_t *sector, const TVec point, float height, TSecPlaneRef &floor, TSecPlaneRef &ceiling, bool debugDump=false);

  // find sector gap that contains the given point, and return its floor and ceiling
  void GetSectorGapCoords (sector_t *sector, const TVec point, float &floorz, float &ceilz);

  // it is used to find lowest sector point for silent teleporters
  // as silent teleport isn't passing a valid z coord here, there is no need
  // to check 3d floors yet
  float GetSectorFloorPointZ (sector_t *sector, const TVec &point);

  // returns top and bottom of the current line's mid texture
  // should be used ONLY for 3dmidtex non-wrapping lines
  // returns `false` if there is no midtex (or it is empty)
  bool GetMidTexturePosition (const line_t *linedef, int sideno, float *ptexbot, float *ptextop);

public: // openings
  // build list of openings for the given line and point
  // note that returned list can be reused on next call to `LineOpenings()`
  opening_t *LineOpenings (const line_t *linedef, const TVec point, unsigned NoBlockFlags, bool do3dmidtex=false, bool usePoint=true);

  // find "best fit" opening for the given coordz
  // `z1` is feet, `z2` is head
  static opening_t *FindOpening (opening_t *gaps, const float zbot, float ztop);

  // find "rel best fit" opening for the given coordz
  // `z1` is feet, `z2` is head
  // used in sector movement, so it tries hard to not leave current opening
  static opening_t *FindRelOpening (opening_t *gaps, float zbot, float ztop) noexcept;

public:
  #define VL_ITERATOR(arrname_,itername_,itertype_) \
    class arrname_##Iterator { \
      friend class VLevel; \
    private: \
      const VLevel *level; \
    public: \
      arrname_##Iterator (const VLevel *alevel) : level(alevel) {} \
    public: \
      inline itertype_ *begin () const { return &level->arrname_[0]; } \
      /*inline const itertype_ *begin () const { return &level->arrname_[0]; }*/ \
      inline itertype_ *end () const { return &level->arrname_[level->Num##arrname_]; } \
      /*inline const itertype_ *end () const { return &level->arrname_[level->Num##arrname_]; }*/ \
    }; \
    inline arrname_##Iterator itername_ () { return arrname_##Iterator(this); } \
    inline const arrname_##Iterator itername_ () const { return arrname_##Iterator(this); }

  // iterators: `for (auto &&it : XXX())` (`it` is a reference!)
  VL_ITERATOR(Vertexes, allVertices, TVec)
  VL_ITERATOR(Sectors, allSectors, sector_t)
  VL_ITERATOR(Sides, allSides, side_t)
  VL_ITERATOR(Lines, allLines, line_t)
  VL_ITERATOR(Segs, allSegs, seg_t)
  VL_ITERATOR(Subsectors, allSubsectors, subsector_t)
  VL_ITERATOR(Nodes, allNodes, node_t)
  VL_ITERATOR(Things, allThings, mthing_t)
  VL_ITERATOR(Zones, allZones, vint32)
  VL_ITERATOR(PolyObjs, allPolyobjects, polyobj_t *)
  VL_ITERATOR(PolyAnchorPoints, allPolyAnchorPoints, PolyAnchorPoint_t)
  VL_ITERATOR(GenericSpeeches, allGenericSpeeches, FRogueConSpeech)
  VL_ITERATOR(LevelSpeeches, allLevelSpeeches, FRogueConSpeech)

  inline VertexesIterator allVertexes () { return VertexesIterator(this); }
  inline const VertexesIterator allVertexes () const { return VertexesIterator(this); }

  #undef VL_ITERATOR


  #define VL_ITERATOR_INDEX(arrname_,itername_,itertype_) \
    class arrname_##IndexIterator { \
      friend class VLevel; \
    private: \
      const VLevel *level; \
      int idx; \
    public: \
      arrname_##IndexIterator (const VLevel *alevel, bool asEnd=false) : level(alevel), idx(asEnd ? alevel->Num##arrname_ : 0) {} \
      arrname_##IndexIterator (const arrname_##IndexIterator &it) : level(it.level), idx(it.idx) {} \
    public: \
      inline bool operator == (const arrname_##IndexIterator &b) const { return (idx == b.idx); } \
      inline bool operator != (const arrname_##IndexIterator &b) const { return (idx != b.idx); } \
      inline arrname_##IndexIterator operator * () const { return arrname_##IndexIterator(*this); } /* required for iterator */ \
      inline void operator ++ () { ++idx; } /* this is enough for iterator */ \
      inline arrname_##IndexIterator begin () { return arrname_##IndexIterator(*this); } \
      inline arrname_##IndexIterator end () { return arrname_##IndexIterator(level, true); } \
      inline itertype_ *value () const { return &level->arrname_[idx]; } \
      /*inline const itertype_ *value () const { return &level->arrname_[idx]; }*/ \
      inline int index () const { return idx; } \
    }; \
    inline arrname_##IndexIterator itername_ () { return arrname_##IndexIterator(this); } \
    inline arrname_##IndexIterator itername_ () const { return arrname_##IndexIterator(this); } \

  VL_ITERATOR_INDEX(Vertexes, allVerticesIdx, TVec)
  VL_ITERATOR_INDEX(Sectors, allSectorsIdx, sector_t)
  VL_ITERATOR_INDEX(Sides, allSidesIdx, side_t)
  VL_ITERATOR_INDEX(Lines, allLinesIdx, line_t)
  VL_ITERATOR_INDEX(Segs, allSegsIdx, seg_t)
  VL_ITERATOR_INDEX(Subsectors, allSubsectorsIdx, subsector_t)
  VL_ITERATOR_INDEX(Nodes, allNodesIdx, node_t)
  VL_ITERATOR_INDEX(Things, allThingsIdx, mthing_t)
  VL_ITERATOR_INDEX(Zones, allZonesIdx, vint32)
  VL_ITERATOR_INDEX(PolyObjs, allPolyObjsIdx, polyobj_t *)
  VL_ITERATOR_INDEX(PolyAnchorPoints, allPolyAnchorPointsIdx, PolyAnchorPoint_t)
  VL_ITERATOR_INDEX(GenericSpeeches, allGenericSpeechesIdx, FRogueConSpeech)
  VL_ITERATOR_INDEX(LevelSpeeches, allLevelSpeechesIdx, FRogueConSpeech)

  inline VertexesIndexIterator allVertexesIdx () { return VertexesIndexIterator(this); }
  inline VertexesIndexIterator allVertexesIdx () const { return VertexesIndexIterator(this); }

  #undef VL_ITERATOR_INDEX

  // subsector segs
  class SubSegsIndexIterator {
    friend class VLevel;
  private:
    const VLevel *level;
    seg_t *curr;
    seg_t *last;
  public:
    SubSegsIndexIterator (const VLevel *alevel, const subsector_t *sub) : level(alevel) {
      if (sub && sub->numlines > 0) {
        curr = &alevel->Segs[sub->firstline];
        last = curr+sub->numlines;
      } else {
        curr = nullptr;
        last = nullptr;
      }
    }
    //SubSegsIndexIterator (const SubSegsIndexIterator &it) : level(it.level), curr(it.curr), last(it.last) {}
  public:
    inline seg_t *begin () { return curr; }
    inline seg_t *end () { return last; }
  };

  inline SubSegsIndexIterator allSubSegs (const subsector_t *sub) const { return SubSegsIndexIterator(this, sub); }
  inline SubSegsIndexIterator allSubSegs (int subidx) const {
    const subsector_t *sub = (subidx >= 0 && subidx < NumSubsectors ? &Subsectors[subidx] : nullptr);
    return SubSegsIndexIterator(this, sub);
  }

  static constexpr float BigDecalWidth = 34.0f;
  static constexpr float BigDecalHeight = 34.0f;

public:
  // this will remove dead and over-the-limit decals (including animated)
  // called from `AddDecal()` (and currently from renderer, therefore it is public)
  void CleanupSegDecals (seg_t *seg);

  // currently called from renderer, therefore it is public
  void DestroyDecal (decal_t *dc); // this will also destroy decal and its animator!

  struct DecalParams {
    int translation;
    int shadeclr; // -2: don't change
    float alpha; // >0: override; >= 1000.0f -- mult by (alpha-1000.0f)
    VDecalAnim *animator; // force if not `nullptr`
    float angle; // force if finite
    bool forceFlipX;
    bool forcePermanent;
    unsigned orflags; // will be bitwise-ored with decal flags

    inline DecalParams () noexcept
      : translation(0)
      , shadeclr(-2)
      , alpha(-2.0f)
      , animator(nullptr)
      , angle(INFINITY)
      , forceFlipX(false)
      , forcePermanent(false)
      , orflags(0)
    {}
  };

  // used in some internal static functions. sigh.
  // if `alpha` >= 0: override decal alpha; >= 1000.0f -- mult by (alpha-1000.0f)
  // shadeclr==-2: don't change
  // alpha<0: don't change
  void NewFlatDecal (bool asFloor, subsector_t *sub, const int eregidx, const float wx, const float wy,
                     VDecalDef *dec, const DecalParams &params);

public:
  enum {
    BLINES_LINES = 1u<<0,
    BLINES_POBJ  = 1u<<1,
    BLINES_ALL = (BLINES_LINES|BLINES_POBJ),
  };

  struct VAllBlockLines {
  private:
    const VLevel *Level;
    polyblock_t *PolyLink;
    int PolySegIdx;
    vint32 *List;
    line_t *Line;

  private:
    void advance () noexcept;

  public:
    VAllBlockLines (const VLevel *ALevel, int x, int y, unsigned pobjMode) noexcept;

    inline VAllBlockLines (const VAllBlockLines &src, bool asEnd) noexcept {
      Level = src.Level;
      if (!asEnd) {
        PolyLink = src.PolyLink;
        PolySegIdx = src.PolySegIdx;
        List = src.List;
        Line = src.Line;
      } else {
        PolyLink = nullptr;
        PolySegIdx = 0;
        List = nullptr;
        Line = nullptr;
      }
    }

    inline bool operator == (const VAllBlockLines &b) const noexcept { return (Level == b.Level && PolyLink == b.PolyLink && PolySegIdx == b.PolySegIdx && List == b.List && Line == b.Line); } /* required for iterator */
    inline bool operator != (const VAllBlockLines &b) const noexcept { return !(*this == b); }
    inline VAllBlockLines operator * () const noexcept { return VAllBlockLines(*this, false); } /* required for iterator */
    inline void operator ++ () noexcept { advance(); } /* required for iterator */
    inline VAllBlockLines begin () noexcept { return VAllBlockLines(*this, false); } /* required for iterator */
    inline VAllBlockLines end () noexcept { return VAllBlockLines(*this, true); } /* required for iterator */

    inline VVA_CHECKRESULT line_t *line () const noexcept { return Line; }
  };

  // The validcount flags are used to avoid checking lines that are marked in
  // multiple mapblocks, so increment validcount before creating this object.
  inline VAllBlockLines allBlockLines (int x, int y, unsigned pobjMode=BLINES_ALL) const noexcept { return VAllBlockLines(this, x, y, pobjMode); }

private:
  // need to have this due to `VEntity` being defined after `VLevel`
  static VEntity *GetNextBlockmapEntity (VEntity *ent) noexcept;
  static VEntity *GetFirstBlockmapEntity (VEntity *ent) noexcept;

public:
  class VAllBlockThings {
  private:
    VEntity *Ent;

  public:
    inline VAllBlockThings (const VLevel *Level, int x, int y) noexcept {
      if (x < 0 || x >= Level->BlockMapWidth || y < 0 || y >= Level->BlockMapHeight) {
        Ent = nullptr;
      } else {
        Ent = GetFirstBlockmapEntity(Level->BlockLinks[y*Level->BlockMapWidth+x]);
      }
    }

    inline VAllBlockThings (const VAllBlockThings &src, bool asEnd) noexcept {
      Ent = (!asEnd ? src.Ent : nullptr);
    }

    inline bool operator == (const VAllBlockThings &b) const noexcept { return (Ent == b.Ent); } /* required for iterator */
    inline bool operator != (const VAllBlockThings &b) const noexcept { return (Ent != b.Ent); }
    inline VAllBlockThings operator * () const noexcept { return VAllBlockThings(*this, false); } /* required for iterator */
    inline void operator ++ () noexcept { Ent = GetNextBlockmapEntity(Ent); } /* required for iterator */
    inline VAllBlockThings begin () noexcept { return VAllBlockThings(*this, false); } /* required for iterator */
    inline VAllBlockThings end () noexcept { return VAllBlockThings(*this, true); } /* required for iterator */

    inline VEntity *entity () const noexcept { return Ent; }
  };

  inline VAllBlockThings allBlockThings (int bmx, int bmy) const noexcept { return VAllBlockThings(this, bmx, bmy); }

private:
  // map loaders
  void LoadVertexes (int Lump);
  void LoadSectors (int);
  void LoadSideDefs (int);
  void LoadLineDefs1 (int, const VMapInfo &);
  void LoadLineDefs2 (int, const VMapInfo &);
  void LoadReject (int);
  void LoadThings1 (int);
  void LoadThings2 (int);
  void LoadLoadACS (int lacsLump, int XMapLump); // load libraries from 'loadacs'
  void LoadACScripts (int BehLump, int XMapLump);
  void LoadTextMap (int, const VMapInfo &);
  // call this after loading things
  void SetupThingsFromMapinfo ();

  // call this after level is loaded or nodes are built
  void PostLoadNodes ();
  void PostLoadSegs ();
  void PostLoadSubsectors ();

  // called by map loader, in postprocessing stage
  // sector `deepref`, `othersecFloor`, and `othersecCeiling` fields
  // are not set for the following two, and should not be read or written
  void DetectHiddenSectors ();
  void FixTransparentDoors ();

  void FixDeepWaters ();
  void FixSelfRefDeepWater (); // called from `FixDeepWaters`

  vuint32 IsFloodBugSectorPart (sector_t *sec);
  vuint32 IsFloodBugSector (sector_t *sec);
  sector_t *FindGoodFloodSector (sector_t *sec, bool wantFloor);

  void BuildDecalsVVList ();

  // cache
  static VStr getCacheDir ();
  static void doCacheCleanup ();

  // map loading helpers
  int TexNumForName (const char *name, int Type, bool CMap=false) const;
  int TexNumOrColor (const char *, int, bool &, vuint32 &) const;
  void CreateSides ();
  void FinaliseLines ();
  void CreateRepBase ();
  void CreateBlockMap ();
  void CleanupBlockMap ();

  enum { BSP_AJ, BSP_ZD };
  int GetNodesBuilder () const; // valid only after `LevelFlags` are set
  void BuildNodesAJ ();
  void BuildNodesZD ();
  void BuildNodes ();

  bool CreatePortals (void *pvsinfo);
  void SimpleFlood (/*portal_t*/void *srcportalp, int leafnum, void *pvsinfo);
  bool LeafFlow (int leafnum, void *pvsinfo);
  void BasePortalVis (void *pvsinfo);
  void HashSectors ();
  void HashLines ();
  void BuildSectorLists ();

  static inline void ClearBox2D (float box2d[4]) {
    box2d[BOX2D_TOP] = box2d[BOX2D_RIGHT] = -999999.0f;
    box2d[BOX2D_BOTTOM] = box2d[BOX2D_LEFT] = 999999.0f;
  }

  static inline void AddToBox2D (float box2d[4], const float x, const float y) {
    if (x < box2d[BOX2D_LEFT]) box2d[BOX2D_LEFT] = x; else if (x > box2d[BOX2D_RIGHT]) box2d[BOX2D_RIGHT] = x;
    if (y < box2d[BOX2D_BOTTOM]) box2d[BOX2D_BOTTOM] = y; else if (y > box2d[BOX2D_TOP]) box2d[BOX2D_TOP] = y;
  }

  // post-loading routines
  void GroupLines ();
  void LinkNode (int BSPNum, node_t *pParent);
  void FloodZones ();
  void FloodZone (sector_t *, int);

  // loader of the Strife conversations
  void LoadRogueConScript (VName, int, FRogueConSpeech *&, int &) const;

  // internal polyobject methods
  static TVec CalcPolyobjCenter2D (polyobj_t *po) noexcept; // returns zero vector on error
  void TranslatePolyobjToStartSpot (PolyAnchorPoint_t *anchor);
  void UpdatePolySegs (polyobj_t *po);
  void InitPolyBlockMap ();
  void LinkPolyobj (polyobj_t *po, bool relinkMObjs);
  void Link3DPolyobjMObjs (polyobj_t *pofirst, bool skipLink);
  void UnlinkPolyobj (polyobj_t *po);

  // used in spawner
  bool IsGood3DPolyobj (polyobj_t *po);

  void ValidateNormalPolyobj (polyobj_t *po);
  void Validate3DPolyobj (polyobj_t *po);

  // this also fixes inner sector height (forces to pobj floor and ceiling)
  // does nothing for non-3d polyobjects
  void OffsetPolyobjFlats (polyobj_t *po, float z, bool forceRecreation);

  void PutPObjInSubsectors (polyobj_t *po) noexcept;

  bool PolyCheckMobjLineBlocking (const line_t *ld, polyobj_t *po, bool rotation);
  // we don't need the exact blocking line here
  bool PolyCheckMobjBlocked (polyobj_t *po, bool rotation);

  // internal TraceLine methods
  //bool CheckPlane (linetrace_t &, const TSecPlaneRef &Plane) const;
  //bool CheckPlanes (linetrace_t &, sector_t *) const;
  /*
  bool BSPCheckLine (linetrace_t &trace, line_t *line);
  bool BSPCrossSubsector (linetrace_t &trace, int num);
  bool CrossBSPNode (linetrace_t &trace, int BspNum);
  */

  int SetBodyQueueTrans (int, int);

public: // used in renderer for flat decals
  // add animated decal to the list of all animated decals
  // it is ok to call this with any `dc`, including `nullptr`
  void AddAnimatedDecal (decal_t *dc);
  void RemoveDecalAnimator (decal_t *dc); // this will also delete animator; safe to call on decals without an animator

  // check what kind of bootprint decal is required at `org`
  // returns `false` if none (params are undefined)
  // `sub` can be `nullptr`
  bool CheckBootPrints (TVec org, subsector_t *sub, VBootPrintDecalParams &params);

  // `sub` can be `nullptr`
  // `dgself` and `dgfunc` must be valid
  void CheckFloorDecalDamage (bool isPlayer, TVec org, subsector_t *sub, VObject *dgself, VMethod *dgfunc);

  void KillAllMapDecals ();

private:
  decal_t *AllocSegDecal (seg_t *seg, VDecalDef *dec, float alpha, VDecalAnim *animator);
  //decal_t *AllocSRegDecal (sector_t *sec, int eregidx, bool atFloor, VDecalDef *dec);

  void KillAllSubsectorDecals ();

  static float CalcDecalAlpha (const VDecalDef *dec, const float alpha) noexcept;
  static float CalcSwitchDecalAlpha (const VDecalDef *dec, const float ovralpha);

  // only for flat decals
  // decal must be fully initialised
  // decal animator may be set, but you SHOULD NOT manually add it to animator list (this method will do it for you)
  void AppendDecalToSubsectorList (decal_t *dc);

  void DestroyFlatDecal (decal_t *dc); // this will also destroy decal and its animator!

  void SpreadFlatDecalEx (const TVec org, float range, VDecalDef *dec, int level, DecalParams &params);

  // z coord matters!
  void AddFlatDecal (TVec org, VName dectype, float range, DecalParams &params);

  void AddDecal (TVec org, VName dectype, int side, line_t *li, int level, DecalParams &params);
  void AddDecalById (TVec org, int id, int side, line_t *li, int level, DecalParams &params);
  // called by `AddDecal()`
  // if angle is not finite, don't override
  void AddOneDecal (int level, TVec org, VDecalDef *dec, int side, line_t *li, DecalParams &params);

  // `flips` will be bitwise-ored with decal flags
  void PutDecalAtLine (const TVec &org, float lineofs, VDecalDef *dec, int side, line_t *li, DecalParams &params, bool skipMarkCheck);

  void PostProcessForDecals ();

  void processSoundSector (int validcount, TArrayNC<VEntity *> &elist, sector_t *sec, int soundblocks, VEntity *soundtarget, float maxdist, const TVec sndorigin);
  void doRecursiveSound (TArrayNC<VEntity *> &elist, sector_t *sec, VEntity *soundtarget, float maxdist, const TVec sndorigin);

  void eventAfterLevelLoaded () {
    static VMethodProxy method("AfterLevelLoaded");
    vobjPutParamSelf();
    VMT_RET_VOID(method);
  }

  void eventUpdateCachedCVars () {
    static VMethodProxy method("UpdateCachedCVars");
    vobjPutParamSelf();
    VMT_RET_VOID(method);
  }

  void eventBeforeWorldTick (float deltaTime) {
    if (deltaTime <= 0.0f) return;
    static VMethodProxy method("BeforeWorldTick");
    vobjPutParamSelf(deltaTime);
    VMT_RET_VOID(method);
  }

  void eventAfterWorldTick (float deltaTime) {
    if (deltaTime <= 0.0f) return;
    static VMethodProxy method("AfterWorldTick");
    vobjPutParamSelf(deltaTime);
    VMT_RET_VOID(method);
  }

  void eventEntitySpawned (VEntity *e) {
    if (e) {
      static VMethodProxy method("OnEntitySpawned");
      vobjPutParamSelf(e);
      VMT_RET_VOID(method);
    }
  }

  void eventEntityDying (VEntity *e) {
    if (e) {
      static VMethodProxy method("OnEntityDying");
      vobjPutParamSelf(e);
      VMT_RET_VOID(method);
    }
  }

  void eventKnownMapBugFixer () {
    static VMethodProxy method("KnownMapBugFixer");
    vobjPutParamSelf();
    VMT_RET_VOID(method);
  }

public:
  void eventAfterUnarchiveThinkers () {
    static VMethodProxy method("AfterUnarchiveThinkers");
    vobjPutParamSelf();
    VMT_RET_VOID(method);
  }

public:
  inline void putLineKey (int id, const char *name, int iv, float fv) noexcept {
    if (!id || !name || !name[0]) return;
    if (!UserLineKeyInfo) UserLineKeyInfo = new VCustomKeyInfo();
    UserLineKeyInfo->putKey(id, name, iv, fv);
  }

  inline void putThingKey (int id, const char *name, int iv, float fv) noexcept {
    if (!id || !name || !name[0]) return;
    if (!UserThingKeyInfo) UserThingKeyInfo = new VCustomKeyInfo();
    UserThingKeyInfo->putKey(id, name, iv, fv);
  }

  inline void putSectorKey (int id, const char *name, int iv, float fv) noexcept {
    if (!id || !name || !name[0]) return;
    if (!UserSectorKeyInfo) UserSectorKeyInfo = new VCustomKeyInfo();
    UserSectorKeyInfo->putKey(id, name, iv, fv);
  }

  inline void putSideKey (int sidenum, int id, const char *name, int iv, float fv) noexcept {
    if (sidenum < 0 || sidenum > 1 || !id || !name || !name[0]) return;
    if (sidenum == 0) {
      if (!UserSideKeyInfo0) UserSideKeyInfo0 = new VCustomKeyInfo();
      UserSideKeyInfo0->putKey(id, name, iv, fv);
    } else {
      if (!UserSideKeyInfo1) UserSideKeyInfo1 = new VCustomKeyInfo();
      UserSideKeyInfo1->putKey(id, name, iv, fv);
    }
  }

  inline void putLineIdxKey (int idx, const char *name, int iv, float fv) noexcept {
    if (idx < 0 || idx >= NumLines || !name || !name[0]) return;
    if (!UserLineIdxKeyInfo) UserLineIdxKeyInfo = new VCustomKeyInfo();
    UserLineIdxKeyInfo->putKey(idx+1, name, iv, fv);
  }

  inline void putSideIdxKey (int idx, const char *name, int iv, float fv) noexcept {
    if (idx < 0 || idx >= NumSides || !name || !name[0]) return;
    if (!UserSideIdxKeyInfo) UserSideIdxKeyInfo = new VCustomKeyInfo();
    UserSideIdxKeyInfo->putKey(idx+1, name, iv, fv);
  }


  inline const VCustomKeyInfo::Value *findLineKey (int id, const char *name) const noexcept {
    return (UserLineKeyInfo ? UserLineKeyInfo->findKey(id, name) : nullptr);
  }

  inline const VCustomKeyInfo::Value *findThingKey (int id, const char *name) const noexcept {
    return (UserThingKeyInfo ? UserThingKeyInfo->findKey(id, name) : nullptr);
  }

  inline const VCustomKeyInfo::Value *findSectorKey (int id, const char *name) const noexcept {
    return (UserSectorKeyInfo ? UserSectorKeyInfo->findKey(id, name) : nullptr);
  }

  inline const VCustomKeyInfo::Value *findSide0Key (int id, const char *name) const noexcept {
    return (UserSideKeyInfo0 ? UserSideKeyInfo0->findKey(id, name) : nullptr);
  }

  inline const VCustomKeyInfo::Value *findSide1Key (int id, const char *name) const noexcept {
    return (UserSideKeyInfo1 ? UserSideKeyInfo1->findKey(id, name) : nullptr);
  }

  inline const VCustomKeyInfo::Value *findSideKey (int sidenum, int id, const char *name) const noexcept {
         if (sidenum == 0) return findSide0Key(id, name);
    else if (sidenum == 1) return findSide1Key(id, name);
    else return nullptr;
  }

  inline const VCustomKeyInfo::Value *findLineIdxKey (int idx, const char *name) const noexcept {
    return (UserLineIdxKeyInfo && idx >= 0 && idx < NumLines ? UserLineIdxKeyInfo->findKey(idx+1, name) : nullptr);
  }

  inline const VCustomKeyInfo::Value *findSideIdxKey (int idx, const char *name) const noexcept {
    return (UserSideIdxKeyInfo && idx >= 0 && idx < NumSides ? UserSideIdxKeyInfo->findKey(idx+1, name) : nullptr);
  }


  inline int getLineKeyInt (int id, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findLineKey(id, name);
    return (v ? v->i : 0);
  }

  inline float getLineKeyFloat (int id, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findLineKey(id, name);
    return (v ? v->f : 0);
  }

  inline int getThingKeyInt (int id, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findThingKey(id, name);
    return (v ? v->i : 0);
  }

  inline float getThingKeyFloat (int id, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findThingKey(id, name);
    return (v ? v->f : 0);
  }

  inline int getSectorKeyInt (int id, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findSectorKey(id, name);
    return (v ? v->i : 0);
  }

  inline float getSectorKeyFloat (int id, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findSectorKey(id, name);
    return (v ? v->f : 0);
  }

  inline int getSideKeyInt (int sidenum, int id, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findSideKey(sidenum, id, name);
    return (v ? v->i : 0);
  }

  inline float getSideKeyFloat (int sidenum, int id, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findSideKey(sidenum, id, name);
    return (v ? v->f : 0);
  }

  inline int getLineIdxKeyInt (int idx, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findLineIdxKey(idx, name);
    return (v ? v->i : 0);
  }

  inline float getLineIdxKeyFloat (int idx, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findLineIdxKey(idx, name);
    return (v ? v->f : 0);
  }

  inline int getSideIdxKeyInt (int idx, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findSideIdxKey(idx, name);
    return (v ? v->i : 0);
  }

  inline float getSideIdxKeyFloat (int idx, const char *name) const noexcept {
    const VCustomKeyInfo::Value *v = findSideIdxKey(idx, name);
    return (v ? v->f : 0);
  }


private:
  DECLARE_FUNCTION(GetLineIndex)

  DECLARE_FUNCTION(PointInSector)
  DECLARE_FUNCTION(PointInSubsector)
  DECLARE_FUNCTION(PointInSectorRender)
  DECLARE_FUNCTION(PointInSubsectorRender)

  DECLARE_FUNCTION(PointContents)

  DECLARE_FUNCTION(BSPTraceLine)
  DECLARE_FUNCTION(BSPTraceLineEx)

  DECLARE_FUNCTION(ChangeSector)
  DECLARE_FUNCTION(ChangeOneSectorInternal)

  DECLARE_FUNCTION(AddExtraFloor)
  DECLARE_FUNCTION(SwapPlanes)

  DECLARE_FUNCTION(CastLightRay)

  DECLARE_FUNCTION(SetFloorPic)
  DECLARE_FUNCTION(SetCeilPic)
  DECLARE_FUNCTION(SetLineTexture)
  DECLARE_FUNCTION(SetLineAlpha)
  DECLARE_FUNCTION(SetFloorLightSector)
  DECLARE_FUNCTION(SetCeilingLightSector)
  DECLARE_FUNCTION(SetHeightSector)

  DECLARE_FUNCTION(FindSectorFromTag)
  DECLARE_FUNCTION(FindLine)
  DECLARE_FUNCTION(SectorSetLink)

  //  Polyobj functions
  DECLARE_FUNCTION(SpawnPolyobj)
  DECLARE_FUNCTION(AddPolyAnchorPoint)
  DECLARE_FUNCTION(Add3DPolyobjLink)
  DECLARE_FUNCTION(GetPolyobj)
  DECLARE_FUNCTION(GetPolyobjMirror)
  DECLARE_FUNCTION(MovePolyobj)
  DECLARE_FUNCTION(RotatePolyobj)

  //  ACS functions
  DECLARE_FUNCTION(StartACS)
  DECLARE_FUNCTION(SuspendACS)
  DECLARE_FUNCTION(TerminateACS)
  DECLARE_FUNCTION(StartTypedACScripts)

  DECLARE_FUNCTION(RunACS)
  DECLARE_FUNCTION(RunACSAlways)
  DECLARE_FUNCTION(RunACSWithResult)

  DECLARE_FUNCTION(RunNamedACS)
  DECLARE_FUNCTION(RunNamedACSAlways)
  DECLARE_FUNCTION(RunNamedACSWithResult)

  DECLARE_FUNCTION(SetBodyQueueTrans)

  DECLARE_FUNCTION(AddDecal)
  DECLARE_FUNCTION(AddDecalById)
  //DECLARE_FUNCTION(AddFloorDecal)
  //DECLARE_FUNCTION(AddCeilingDecal)
  DECLARE_FUNCTION(AddFlatDecal)

  DECLARE_FUNCTION(CheckBootPrints)
  DECLARE_FUNCTION(CheckFloorDecalDamage)

  DECLARE_FUNCTION(doRecursiveSound)

  DECLARE_FUNCTION(AllThinkers)
  DECLARE_FUNCTION(AllActivePlayers)

  DECLARE_FUNCTION(LdrTexNumForName)

  DECLARE_FUNCTION(CD_SweepLinedefAABB)

  DECLARE_FUNCTION(GetUDMFLineInt)
  DECLARE_FUNCTION(GetUDMFLineFloat)
  DECLARE_FUNCTION(GetUDMFThingInt)
  DECLARE_FUNCTION(GetUDMFThingFloat)
  DECLARE_FUNCTION(GetUDMFSectorInt)
  DECLARE_FUNCTION(GetUDMFSectorFloat)
  DECLARE_FUNCTION(GetUDMFSideInt)
  DECLARE_FUNCTION(GetUDMFSideFloat)

  DECLARE_FUNCTION(CheckPObjPlanesPoint)

  DECLARE_FUNCTION(GetSectorFloorPointZ)
  DECLARE_FUNCTION(GetSectorGapCoords)

  DECLARE_FUNCTION(FindOpening)
  DECLARE_FUNCTION(LineOpenings)

  DECLARE_FUNCTION(GetMidTexturePosition)

  DECLARE_FUNCTION(GetNextValidCount)
  DECLARE_FUNCTION(GetNextVisitedCount)
};


void SV_LoadLevel (VName MapName);
void CL_LoadLevel (VName MapName);
void SwapPlanes (sector_t *);

// WARNING! sometimes `GClLevel` set go `GLevel`, and sometimes only one of those is not `nullptr`
// the basic rule is that if there is full-featured playsim, then `GLevel` is set
// and if we need to render something, then `GClLevel` is set
// for standalone game, `GClLevel` is equal to `GLevel` (this is one weird special case)
//FIXME: need to check if those vars are properly managed everywhere
extern VLevel *GLevel; // server/standalone level
extern VLevel *GClLevel; // level for client-only games


// ////////////////////////////////////////////////////////////////////////// //
#define MAPBLOCKUNITS  (128)


struct VBlockMapWalker {
public:
  DDALineWalker<MAPBLOCKUNITS, MAPBLOCKUNITS> dda;
  int maxTileX, maxTileY;

public:
  inline VBlockMapWalker () noexcept {}

  // returns `false` if coords are out of bounds
  // note that if this method returned `false`, calling `next()` method is UB
  inline bool start (const VLevel *level, float x1, float y1, float x2, float y2) noexcept {
    maxTileX = level->BlockMapWidth;
    maxTileY = level->BlockMapHeight;
    if (maxTileX < 1 || maxTileY < 1) return false; // oops

    x1 -= level->BlockMapOrgX;
    y1 -= level->BlockMapOrgY;
    x2 -= level->BlockMapOrgX;
    y2 -= level->BlockMapOrgY;

    const int bmWidth = maxTileX*MAPBLOCKUNITS;
    const int bmHeight = maxTileY*MAPBLOCKUNITS;

    // clip to blockmap rectanle
    //GLog.Logf("WKLINE000: x1=%g; y1=%g; x2=%g; y2=%g; w=%d; h=%d", x1, y1, x2, y2, level->BlockMapWidth*MAPBLOCKUNITS, level->BlockMapHeight*MAPBLOCKUNITS);
    if (!ClipLineToRect0(&x1, &y1, &x2, &y2, bmWidth, bmHeight)) {
      // fully clipped away, so don't do anything here
      //GLog.Logf(NAME_Debug, "***CLIPPED!");
      return false;
    }
    //GLog.Logf("WKLINE001: x1=%g; y1=%g; x2=%g; y2=%g; w=%d; h=%d", x1, y1, x2, y2, level->BlockMapWidth*MAPBLOCKUNITS, level->BlockMapHeight*MAPBLOCKUNITS);

    // values should be valid here, but hey, why don't double-ensure it?
    const int ix1 = clampval((int)x1, 0, bmWidth-1);
    const int iy1 = clampval((int)y1, 0, bmHeight-1);
    const int ix2 = clampval((int)x2, 0, bmWidth-1);
    const int iy2 = clampval((int)y2, 0, bmHeight-1);

    dda.start(ix1, iy1, ix2, iy2);

    return true;
  }

  // returns `false` if we should stop (and map coords are undefined in this case)
  inline bool next (int &tileX, int &tileY) noexcept {
    while (dda.next(tileX, tileY)) {
      if (tileX >= 0 && tileX < maxTileX && tileY >= 0 && tileY < maxTileY) return true;
    }
    tileX = tileY = 0; // just to make sure that we won't leave uninitialised values there
    return false;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
TVec P_SectorClosestPoint (const sector_t *sec, const TVec in, line_t **resline=nullptr);


// ////////////////////////////////////////////////////////////////////////// //
extern int dbgEntityTickTotal;
extern int dbgEntityTickSimple;
extern int dbgEntityTickNoTick;


#endif
