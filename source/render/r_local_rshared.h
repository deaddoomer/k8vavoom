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
#ifndef VAVOOM_R_LOCAL_HEADER_RSHARED
#define VAVOOM_R_LOCAL_HEADER_RSHARED

#include "../client/cl_local.h"

// wall quad vertex indicies
enum {
  QUAD_V1_BOTTOM = 0,
  QUAD_V1_TOP    = 1,
  QUAD_V2_TOP    = 2,
  QUAD_V2_BOTTOM = 3,
};



// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelShared : public VRenderLevelDrawer {
protected:
  friend class VPortal;
  friend class VSkyPortal;
  friend class VSkyBoxPortal;
  friend class VSectorStackPortal;
  friend class VMirrorPortal;
  friend struct AutoSavedView;

  struct BSPVisInfo {
    //enum { UNKNOWN = -1, INVISIBLE = 0, VISIBLE = 1, };
    vuint32 framecount; // data validity checking
    //float radius;
    //vuint8 vis;
  };

public:
  enum {
    MAX_PARTICLES = 2048, // default max # of particles at one time
    ABSOLUTE_MIN_PARTICLES = 512, // no fewer than this no matter what's on the command line
    MAX_DLIGHTS = 32, // should fit into vuint32, 'cause subsector is using that as bitmask for active lights
  };

  struct world_surf_t {
    surface_t *Surf;
    vuint8 Type;
  };

  struct light_t {
    TVec origin;
    float radius;
    vuint32 color;
    //VEntity *dynowner; // for dynamic
    vuint32 ownerUId; // for static
    int leafnum;
    bool active; // for filtering
    TVec coneDirection;
    float coneAngle;
    // all subsectors touched by this static light
    // this is used to trigger static lightmap updates
    TArray<subsector_t *> touchedSubs;
    unsigned invalidateFrame; // to avoid double-processing lights; using `currDLightFrame`
    unsigned dlightframe; // set to `currDLightFrame` if BSP renderer touched any subsector that is connected with this light
    sector_t *levelSector; // for `DLTYPE_Sector`, or `nullptr`
    float levelScale; // scale for `DLTYPE_Sector` light, <0: attenuated
    vuint32 sectorLightLevel; // to detect changes
    vuint32 flags; // dlight_t::XXX
    //TArray<polyobj_t *> touchedPolys;
  };

protected:
  struct DLightInfo {
    int needTrace; // <0: no; >1: yes; 0: invisible
    int leafnum; // -1: unknown yet

    inline bool isVisible () const noexcept { return (needTrace != 0); }
    inline bool isNeedTrace () const noexcept { return (needTrace > 0); }
  };

protected:
  //VLevel *Level; // moved to `VRenderLevelPublic()`
  VEntity *ViewEnt;
  VPortal *CurrPortal;

  //unsigned FrustumIndexes[5][6];
  int MirrorLevel;
  int PortalLevel;

  unsigned BspVisFrame;
  unsigned *BspVisData;
  unsigned *BspVisSectorData; // for whole sectors

  subsector_t *r_viewleaf;
  subsector_t *r_oldviewleaf;
  float old_fov;
  int prev_aspect_ratio;
  bool prev_vertical_fov_flag;

  // bumped light from gun blasts
  int ExtraLight;
  int FixedLight;
  int ColorMap;
  // some code checks for `ColorMap == CM_Default` to resed fixed light
  // this is now wrong, as we may render colormaps with postprocess shader
  bool ColorMapFixedLight;

  int NumParticles;
  particle_t *Particles;
  particle_t *ActiveParticles;
  particle_t *FreeParticles;

  // sky variables
  int CurrentSky1Texture;
  int CurrentSky2Texture;
  bool CurrentDoubleSky;
  bool CurrentLightning;
  VSky BaseSky;

  // world render variables
  VViewClipper ViewClip;
  TArray<world_surf_t> WorldSurfs;
  TArray<VPortal *> Portals;
  TArray<VSky *> SideSkies;

  sec_plane_t sky_plane;
  float skyheight;
  surface_t *free_wsurfs;
  void *AllocatedWSurfBlocks;
  subregion_t *AllocatedSubRegions;
  drawseg_t *AllocatedDrawSegs;
  segpart_t *AllocatedSegParts;
  subregion_info_t *SubRegionInfo; // always allocated and inited with `AllocatedSubRegions`

  // static lights
  TArray<light_t> Lights;
  TMapNC<vuint32, vint32> StOwners; // key: object id; value: index in `Lights`

  struct SubStaticLigtInfo {
    TMapNC<int, bool> touchedStatic;
    vuint32 invalidateFrame; // to avoid double-processing lights; using `currDLightFrame`
  };
  TArray<SubStaticLigtInfo> SubStaticLights; // length == Level->NumSubsectors

  // dynamic lights
  dlight_t DLights[MAX_DLIGHTS];
  DLightInfo dlinfo[MAX_DLIGHTS];
  TMapNC<vuint32, vuint32> dlowners; // key: object id; value: index in `DLights`

  // server uid -> object
  // no need, VLevel now has it
  //TMapNC<vuint32, VEntity *> suid2ent;

  // moved here so that model rendering methods can be merged
  TVec CurrLightPos;
  float CurrLightRadius;
  bool CurrLightInFrustum;
  bool CurrLightInFront;
  bool CurrLightCalcUnstuck; // set to `true` to calculate "unstuck" distance in `CalcLightVis()`
  TVec CurrLightUnstuckPos; // set in `CalcLightVis()` if `CurrLightCalcUnstuck` is `true`
  vuint32 CurrLightBit; // tag (bitor) subsectors with this in lightvis builder

  // for spotlights
  bool CurrLightSpot; // is current light a spotlight?
  TVec CurrLightConeDir; // current spotlight direction
  float CurrLightConeAngle; // current spotlight cone angle
  //TPlane CurrLightSpotPlane; // CurrLightSpotPlane.SetPointNormal3D(CurrLightPos, CurrLightConeDir);
  TFrustum CurrLightConeFrustum; // current spotlight frustum

  bool inWorldCreation; // are we creating world surfaces now?

  // mark all updated subsectors with this; increment on each new frame
  vuint32 updateWorldFrame;

  // those arrays are filled in `BuildVisibleObjectsList()`
  TArray<VEntity *> visibleObjects;
  TArray<VEntity *> visibleAliasModels;
  TArray<VEntity *> visibleSprites;
  TArray<VEntity *> allShadowModelObjects; // used in advrender
  bool useInCurrLightAsLight; // use `mobjsInCurrLightModels()` list to render models in light?

  TArray<int> renderedSectors; // sector numbers
  TArray<int> renderedSectorMarks; // contiains counter; sized by number of sectors
  int renderedSectorCounter;
  vuint32 renderedLineCounter;
  TMapNC<VEntity *, bool> renderedThingMarks;

  BSPVisInfo *bspVisRadius;
  vuint32 bspVisRadiusFrame;
  float bspVisLastCheckRadius;

  // `CalcLightVis()` working variables
  unsigned LightFrameNum;
  VViewClipper LightClip; // internal working clipper
  VViewClipper LightShadowClip; // internal working clipper
  unsigned *LightVis; // this will be allocated if necessary; set to `LightFrameNum` for subsectors touched in `CalcLightVis()`
  unsigned *LightBspVis; // this will be allocated if necessary; set to `LightFrameNum` for subsectors touched, and marked in `BspVisData`
  bool HasLightIntersection; // set by `CalcLightVisCheckNode()`: is any touched subsector also marked in `BspVisData`?
  // set `LitCalcBBox` to true to calculate bbox of all hit surfaces, and all following flags
  bool LitCalcBBox; // set this to `false` disables `LitSurfaces`/`LitSurfaceHit` calculation, and all other arrays
  TVec LitBBox[2]; // bounding box for all hit surfaces
  bool LitSurfaceHit; // hit any surface in visible subsector?
  // nope, not used for now
  //bool HasBackLit; // if there's no backlit surfaces, use zpass

  bool doShadows; // for current light

  // renderer info; put here to avoid passing it around
  // k8: it was a bunch of globals; i will eventually got rid of this
  //bool MirrorClipSegs;

  // set in `CreateWorldSurfaces()`
  int NumSegParts;
  // used in `CreateWorldSurfaces()` and friends
  segpart_t *pspart;
  int pspartsLeft;
  bool lastRenderQuality;

  bool createdFullSegs;

  vuint32 currVisFrame;

  // chasecam
  double prevChaseCamTime = -1.0;
  TVec prevChaseCamPos;

  // automap
  TArray<sec_surface_t *> amSurfList;
  TArray<TVec> amTmpVerts;
  bool amDoFloors;
  VTexture *amSkyTex;
  AMCheckSubsectorCB amCheckSubsector;
  float amX, amY, amX2, amY2;

  // temp array for `FixSegSurfaceTJunctions()`
  TArray<seg_t *> adjSegs;
  TArray<subsector_t *> adjSubs;

public:
  inline bool AM_isBBox3DVisible (const float bbox3d[6]) const noexcept {
    return
      amX2 >= bbox3d[0+0] && amY2 >= bbox3d[0+1] &&
      amX <= bbox3d[3+0] && amY <= bbox3d[3+1];
  }

  // also, resets list of rendered sectors
  inline void nextRenderedSectorsCounter () noexcept {
    if (++renderedSectorCounter == MAX_VINT32) {
      renderedSectorCounter = 1;
      for (auto &&rs : renderedSectorMarks) rs = 0;
    }
    renderedSectors.resetNoDtor();
  }

  inline void nextRenderedLineCounter () noexcept {
    if (++renderedLineCounter == 0) {
      renderedLineCounter = 1;
      for (auto &&sd : Level->allSides()) sd.rendercount = 0;
    }
  }

  inline void markSectorRendered (int secnum) noexcept {
    if (secnum < 0 || secnum >= Level->NumSectors) return; // just in case
    if (renderedSectorMarks.length() == 0) {
      renderedSectorMarks.setLength(Level->NumSectors);
      for (auto &&rs : renderedSectorMarks) rs = 0;
    }
    vassert(renderedSectorMarks.length() == Level->NumSectors);
    if (renderedSectorMarks[secnum] != renderedSectorCounter) {
      renderedSectorMarks[secnum] = renderedSectorCounter;
      renderedSectors.append(secnum);
    }
  }

private:
  VDirtyArea unusedDirty; // required to return reference to it

private:
  inline segpart_t *SurfCreatorGetPSPart () {
    if (pspartsLeft == 0) Sys_Error("internal level surface creation bug");
    --pspartsLeft;
    return pspart++;
  }

protected:
  // it is of `Level->NumLines` size, to avoid marking lines twice in `SectorModified`; marked with `updateWorldFrame`
  vuint32 *tjLineMarkCheck; // some segment of this line was processed in `MarkTJunctions()`
  vuint32 *tjLineMarkFix; // this line was fully processed in `MarkTJunctions()` (i.e. as adjacent line)

protected:
  void NewBSPFloodVisibilityFrame () noexcept;

  bool CheckBSPFloodVisibilitySub (const TVec &org, const float radius, const subsector_t *currsub, const seg_t *firsttravel) noexcept;
  bool CheckBSPFloodVisibility (const TVec &org, float radius, const subsector_t *sub=nullptr) noexcept;

  // mostly used to check for dynamic light visibility
  bool CheckBSPVisibilityBoxSub (int bspnum) noexcept;
  bool CheckBSPVisibilityBox (const TVec &org, float radius, const subsector_t *sub=nullptr) noexcept;

  void ResetVisFrameCount () noexcept;
  inline void IncVisFrameCount () noexcept {
    if ((++currVisFrame) == 0x7fffffffu) ResetVisFrameCount();
  }

  void ResetDLightFrameCount () noexcept;
  inline void IncDLightFrameCount () noexcept {
    if ((++currDLightFrame) == 0) ResetDLightFrameCount();
  }

  void ResetUpdateWorldFrame () noexcept;
  inline void IncUpdateWorldFrame () noexcept {
    if ((++updateWorldFrame) == 0) ResetUpdateWorldFrame();
  }

  inline void IncQueueFrameCount () noexcept {
    //if ((++currQueueFrame) == 0) ResetQueueFrameCount();
    if ((++currQueueFrame) == 0) {
      // there is nothing we can do with this yet
      // resetting this is too expensive, and doesn't worth the efforts
      // i am not expecting for someone to play one level for that long in one seat
      // it is more than a half of a year for 200 FPS, so should be pretty safe (around 240 days)
      Sys_Error("*************** WARNING!!! QUEUE FRAME COUNT OVERFLOW!!! ***************");
    }
  }

  inline void IncLightFrameNum () noexcept {
    if (++LightFrameNum == 0) {
      LightFrameNum = 1;
      memset(LightVis, 0, sizeof(LightVis[0])*Level->NumSubsectors);
      memset(LightBspVis, 0, sizeof(LightBspVis[0])*Level->NumSubsectors);
    }
  }

  // WARNING! NO CHECKS!
  VVA_CHECKRESULT inline bool IsSubsectorLitVis (int sub) const noexcept { return (LightVis[(unsigned)sub] == LightFrameNum); }
  VVA_CHECKRESULT inline bool IsSubsectorLitBspVis (int sub) const noexcept { return (LightBspVis[(unsigned)sub] == LightFrameNum); }

  // clears render queues
  virtual void ClearQueues ();

protected:
  void setupCurrentLight (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle) noexcept;

  // spotlight should be properly initialised!
  inline bool isSurfaceInSpotlight (const surface_t *surf) const noexcept {
    //if (!CurrLightSpot || !surf || surf->count < 3) return false;
    /*
    const SurfVertex *vv = surf->verts;
    //for (unsigned f = (unsigned)surf->count; f--; ++vv) if (vv->vec().isInSpotlight(LightPos, coneDir, coneAngle)) { splhit = true; break; }
    for (unsigned f = (unsigned)surf->count; f--; ++vv) {
      if (!spotPlane.PointOnSide(vv->vec())) return true;
    }
    return false;
    */
    // check bounding box instead?
    return (!CurrLightSpot || CurrLightConeFrustum.checkPolyInterlaced(surf->verts->vecptr(), sizeof(SurfVertex), (unsigned)surf->count));
    /*
    float bbox[6];
    const SurfVertex *vv = surf->verts;
    bbox[BOX3D_MINX] = bbox[BOX3D_MAXX] = vv->vec().x;
    bbox[BOX3D_MINY] = bbox[BOX3D_MAXY] = vv->vec().y;
    bbox[BOX3D_MINZ] = bbox[BOX3D_MAXZ] = vv->vec().z;
    ++vv;
    for (unsigned f = (unsigned)surf->count-1u; f--; ++vv) {
      const TVec &v = vv->vec();
      bbox[BOX3D_MINX] = min2(bbox[BOX3D_MINX], v.x);
      bbox[BOX3D_MAXX] = max2(bbox[BOX3D_MAXX], v.x);
      bbox[BOX3D_MINY] = min2(bbox[BOX3D_MINY], v.y);
      bbox[BOX3D_MAXY] = max2(bbox[BOX3D_MAXY], v.y);
      bbox[BOX3D_MINZ] = min2(bbox[BOX3D_MINZ], v.z);
      bbox[BOX3D_MAXZ] = max2(bbox[BOX3D_MAXZ], v.z);
    }
    return coneFrustum.checkBox(bbox);
    */
  }

  inline bool isSpriteInSpotlight (const TVec *cv) const noexcept {
    /*
    for (unsigned f = 4; f--; ++cv) if (!spotPlane.PointOnSide(*cv)) return true;
    return false;
    */
    return (!CurrLightSpot || CurrLightConeFrustum.checkQuad(cv[0], cv[1], cv[2], cv[3]));
  }

protected:
  // entity must not be `nullptr`, and must have `SubSector` set
  // also, `viewfrustum` should be valid here
  // this is usually called once for each entity, but try to keep it reasonably fast anyway
  bool IsThingVisible (VEntity *ent) const noexcept;

  /*
  static inline bool IsThingRenderable (VEntity *ent) noexcept {
    return
      ent && ent->State && !ent->IsGoingToDie() &&
      !(ent->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible)) &&
      ent->BaseSubSector;
  }
  */

public:
  struct TransPolyState {
    bool pofsEnabled;
    int pofs;
    float lastpdist; // for sprites: use polyofs for the same dist

    // options
    bool allowTransPolys;
    bool sortWithOfs;

    inline void reset () noexcept {
      pofsEnabled = false;
      pofs = 0;
      lastpdist = -1e12f; // for sprites: use polyofs for the same dist
    }
  };

protected:
  TransPolyState transSprState;

  void DrawTransSpr (trans_sprite_t &spr);

public:
  bool forceDisableShadows; // override for "r_shadows"

  inline bool IsShadowsEnabled () const noexcept { return (forceDisableShadows ? false : r_shadows.asBool()); }

public:
  virtual bool IsNodeRendered (const node_t *node) const noexcept override;
  virtual bool IsSubsectorRendered (const subsector_t *sub) const noexcept override;

  // check if `invUId` is in inventory of `owner`
  bool IsInInventory (VEntity *owner, vuint32 invUId) const;

  void CalcEntityStaticLightingFromOwned (VEntity *lowner, const TVec &p, float radius, float height, float &l, float &lr, float &lg, float &lb, unsigned flags);
  void CalcEntityDynamicLightingFromOwned (VEntity *lowner, const TVec &p, float radius, float height, float &l, float &lr, float &lg, float &lb, unsigned flags);

  enum {
    LP_IgnoreSelfLights = 1u<<0,
    LP_IgnoreFixedLight = 1u<<1,
    LP_IgnoreExtraLight = 1u<<2,
    LP_IgnoreDynLights  = 1u<<3,
    LP_IgnoreStatLights = 1u<<4,
    LP_IgnoreGlowLights = 1u<<5,
    LP_IgnoreAmbLight   = 1u<<6,
  };

  // defined only after `PushDlights()`
  // `radius` is used for visibility raycasts
  vuint32 LightPoint (VEntity *lowner, const TVec p, float radius, float height, const subsector_t *psub=nullptr, unsigned flags=0u);
  // `radius` is used for... nothing yet
  vuint32 LightPointAmbient (VEntity *lowner, const TVec p, float radius, float height, const subsector_t *psub=nullptr, unsigned flags=0u);

  // `dflags` is `VDrawer::ELFlag_XXX` set
  // returns 0 for unknown
  virtual vuint32 CalcEntityLight (VEntity *lowner, unsigned dflags) override;

  virtual void UpdateSubsectorFlatSurfaces (subsector_t *sub, bool dofloors, bool doceils, bool forced=false) override;

  virtual void PrecacheLevel () override;
  virtual void UncacheLevel () override;

  // lightmap chain iterator (used in renderer)
  // block number+1 or 0
  virtual vuint32 GetLightChainHead () override;
  // block number+1 or 0
  virtual vuint32 GetLightChainNext (vuint32 bnum) override;

  virtual VDirtyArea &GetLightBlockDirtyArea (vuint32 bnum) override;
  virtual VDirtyArea &GetLightAddBlockDirtyArea (vuint32 bnum) override;
  virtual rgba_t *GetLightBlock (vuint32 bnum) override;
  virtual rgba_t *GetLightAddBlock (vuint32 bnum) override;
  virtual surfcache_t *GetLightChainFirst (vuint32 bnum) override;

  virtual void NukeLightmapCache () override;

  virtual void FullWorldUpdate (bool forceClientOrigin) override;

  virtual int GetNumberOfStaticLights () override;

public:
  static vuint32 CountSurfacesInChain (const surface_t *s) noexcept;
  static vuint32 CountSegSurfacesInChain (const segpart_t *sp) noexcept;
  vuint32 CountAllSurfaces () const noexcept;

  static VName GetClassNameForModel (VEntity *mobj) noexcept;

  // fuckery to avoid having friends, because i am asocial
  inline void CallTransformFrustum () { TransformFrustum(); }

  static inline __attribute__((const)) float TextureSScale (const VTexture *pic) noexcept { return (pic ? pic->TextureSScale() : 1.0f); }
  static inline __attribute__((const)) float TextureTScale (const VTexture *pic) noexcept { return (pic ? pic->TextureTScale() : 1.0f); }
  static inline __attribute__((const)) float TextureOffsetSScale (const VTexture *pic) noexcept { return (pic ? pic->TextureOffsetSScale() : 1.0f); }
  static inline __attribute__((const)) float TextureOffsetTScale (const VTexture *pic) noexcept { return (pic ? pic->TextureOffsetTScale() : 1.0f); }

  inline void IncrementBspVis () {
    if (++BspVisFrame == 0) {
      memset(BspVisData, 0, Level->NumSubsectors*sizeof(BspVisData[0]));
      memset(BspVisSectorData, 0, Level->NumSectors*sizeof(BspVisSectorData[0]));
      Level->ResetPObjRenderCounts();
    }
  }

  inline bool IsBspVis (int idx) const noexcept { return (idx >= 0 && idx < Level->NumSubsectors ? (BspVisData[(unsigned)idx] == BspVisFrame) : false); }
  inline bool IsBspVisSector (int idx) const noexcept { return (idx >= 0 && idx < Level->NumSectors ? (BspVisSectorData[(unsigned)idx] == BspVisFrame) : false); }

  inline void MarkBspVis (int idx) noexcept { if (idx >= 0 && idx < Level->NumSubsectors) BspVisData[(unsigned)idx] = BspVisFrame; }
  inline void MarkBspVisSector (int idx) noexcept { if (idx >= 0 && idx < Level->NumSectors) BspVisSectorData[(unsigned)idx] = BspVisFrame; }

protected:
  VRenderLevelShared (VLevel *ALevel);
  ~VRenderLevelShared ();

  void UpdateTextureOffsets (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const TPlane *plane=nullptr);
  void UpdateTextureOffsetsEx (subsector_t *sub, seg_t *seg, segpart_t *sp, const side_tex_params_t *tparam, const side_tex_params_t *tparam2); // for 3d floors
  void UpdateDrawSeg (subsector_t *sub, drawseg_t *dseg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void UpdateSubRegions (subsector_t *sub);
  void UpdatePObj (polyobj_t *po); // should be used for polyobjects instead of normal updaters

  void UpdateFakeSectors (subsector_t *viewleaf=nullptr);
  void InitialWorldUpdate ();

  void UpdateBBoxWithSurface (TVec bbox[2], surface_t *surfs, const texinfo_t *texinfo,
                              VEntity *SkyBox, bool CheckSkyBoxAlways);
  void UpdateBBoxWithLine (TVec bbox[2], VEntity *SkyBox, const drawseg_t *dseg);

  // the following should not be called directly
  void CalcLightVisUnstuckLightSubsector (const subsector_t *sub);
  // the following should not be called directly
  void CalcLightVisCheckSubsector (const unsigned subidx);
  // the following should not be called directly
  void CalcLightVisCheckNode (int bspnum, const float *bbox, const float *lightbbox);

  // main entry point for lightvis calculation
  // note that this should be called with filled `BspVisData`
  // if we will use this for dynamic lights, they will have one-frame latency
  // sets `CurrLightPos` and `CurrLightRadius`, and other lvis fields
  // returns `false` if the light is invisible
  // this also sets the list of touched subsectors for dynamic light
  // (this is used in `LightPoint()` and such
  bool CalcLightVis (const TVec &org, const float radius, const TVec &aconeDir, const float aconeAngle, int dlnum=-1);
  inline bool CalcPointLightVis (const TVec &org, const float radius, int dlnum=-1) { return CalcLightVis(org, radius, TVec(0.0f, 0.0f, 0.0f), 0.0f, dlnum); }

  // does some sanity checks, possibly moves origin a little
  // returns `false` if this light can be dropped
  // `sec` should be valid, null sector means "no checks"
  static bool CheckValidLightPosRough (TVec &lorg, const sector_t *sec);

  // yes, non-virtual
  // dlinfo::leafnum must be set (usually this is done in `PushDlights()`)
  //void MarkLights (dlight_t *light, vuint32 bit, int bspnum, int lleafnum);

  virtual void RenderScene (const refdef_t *, const VViewClipper *) = 0;

  // this calculates final dlight visibility, list of affected subsectors, and so on
  void PushDlights ();

  // returns attenuation multiplier (0 means "out of cone")
  static float CheckLightPointCone (VEntity *lowner, const TVec &p, const float radius, const float height, const TVec &coneOrigin, const TVec &coneDir, const float coneAngle);

  // base fixer, need not to be virtual
  surface_t *FixSegTJunctions (surface_t *surf, seg_t *seg);

  // surface fixer for lightmaps
  virtual surface_t *FixSegSurfaceTJunctions (surface_t *surf, seg_t *seg) = 0;

  virtual void InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub, seg_t *seg, subregion_t *sreg) = 0;
  virtual surface_t *SubdivideFace (surface_t *InF, subregion_t *sreg, sec_surface_t *ssf, const TVec &axis, const TVec *nextaxis, const TPlane *plane, bool doSubdivisions=true) = 0;
  virtual surface_t *SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis, seg_t *seg) = 0;

  virtual void QueueWorldSurface (surface_t *surf) = 0;
  // this does BSP traversing, and collect world surfaces into various lists to drive GPU rendering
  virtual void RenderCollectSurfaces (const refdef_t *rd, const VViewClipper *Range);

  virtual void FreeSurfCache (surfcache_t *&block);
  // this is called after surface queues built, so lightmap renderer can calculate new lightmaps
  // it is called right before starting world drawing
  virtual void ProcessCachedSurfaces ();

  virtual void PrepareWorldRender (const refdef_t *, const VViewClipper *);
  // this should be called after `RenderCollectSurfaces()`
  virtual void BuildVisibleObjectsList (bool doShadows);

  // general
  static float CalcEffectiveFOV (float fov, const refdef_t &refdef);
  void SetupRefdefWithFOV (refdef_t *refdef, float fov);

  void ExecuteSetViewSize ();
  void TransformFrustum ();
  void SetupFrame ();
  void SetupCameraFrame (VEntity *, VTexture *, int, refdef_t *);
  void MarkLeaves ();
  bool UpdateCameraTexture (VEntity *Camera, int TexNum, int FOV); // returns `true` if camera texture was updated
  vuint32 GetFade (sec_region_t *reg, bool isFloorCeiling=false);

  enum {
    CST_Normal,
    CST_OnlyBlood,
    CST_OnlyBloodAndPSprites,
  };
  int CollectSpriteTextures (TMapNC<vint32, vint32> &texturetype, int limit, int cstmode); // this is actually private, but meh... returns number of new textures

  VTextureTranslation *GetTranslation (int);
  void BuildPlayerTranslations ();

  // particles
  void InitParticles ();
  void ClearParticles ();
  void UpdateParticles (float);
  void DrawParticles ();

  // sky methods
  void InitSky ();
  void AnimateSky (float);

public:
  // checks if surface is not queued twice, sets various flags
  // returns `false` if the surface should not be queued
  bool SurfPrepareForRender (surface_t *surf);

  // world BSP rendering
  //void QueueTranslucentSurf (surface_t *surf);

  // `SurfPrepareForRender()` should be done by the caller
  // `IsPlVisible()` should be checked by the caller
  // i.e. it does no checks at all
  void QueueSimpleSurf (surface_t *surf);

  void QueueSkyPortal (surface_t *surf);
  void QueueHorizonPortal (surface_t *surf);

  // main dispatcher (it calls other queue methods)
  enum SFCType {
    SFCT_World,
    SFCT_Sky,
    SFCT_Horizon,
  };
  void CommonQueueSurface (surface_t *surf, SFCType type);

protected:
  // used in `RenderSubRegions()`
  TArray<subregion_t *> sortedRegs;

public:
  void DrawSurfaces (subsector_t *sub, sec_region_t *secregion, seg_t *seg, surface_t *InSurfs,
                     texinfo_t *texinfo, VEntity *SkyBox, int LightSourceSector, int SideLight,
                     bool AbsSideLight, bool CheckSkyBoxAlways, bool hasAlpha=false);

  void GetFlatSetToRender (subsector_t *sub, subregion_t *region, sec_surface_t *surfs[4]);
  void ChooseFlatSurfaces (sec_surface_t *&destf0, sec_surface_t *&destf1, sec_surface_t *realflat, sec_surface_t *fakeflat);

  // called by `RenderLine()`, to mark segs/lines/subsectors on the automap
  void RenderSegMarkMapped (subsector_t *sub, seg_t *seg);

  void RenderHorizon (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, drawseg_t *dseg);
  void RenderMirror (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg);
  void RenderLine (subsector_t *sub, sec_region_t *secregion, subregion_t *subregion, seg_t *seg);
  void RenderSecFlatSurfaces (subsector_t *sub, sec_region_t *secregion, sec_surface_t *flat0, sec_surface_t *flat1, VEntity *SkyBox);
  void RenderSecSurface (subsector_t *sub, sec_region_t *secregion, sec_surface_t *ssurf, VEntity *SkyBox, bool hasAlpha);
  void AddPolyObjToClipper (VViewClipper &clip, subsector_t *sub);
  void RenderPolyObj (subsector_t *sub);
  void RenderSubRegions (subsector_t *sub);
  void RenderSubsector (int num, bool onlyClip);
  void RenderBSPNode (int bspnum, const float *bbox, unsigned AClipflags, bool onlyClip);
  void RenderBSPTree ();
  virtual void RenderBspWorld (const refdef_t *rd, const VViewClipper *Range);
  virtual void RenderPortals ();

public:
  void SetupOneSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupOneSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);

  void SetupTwoSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedTopWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedBotWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);

  void SetupTwoSidedTopWSurf3DPObj (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedBotWSurf3DPObj (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);
  void SetupTwoSidedMidWSurf3DPObj (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);

  void SetupTwoSidedMidExtraWSurf (sec_region_t *reg, subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling);

  // surf methods
  void SetupSky ();

public:
  void FlushSurfCaches (surface_t *InSurfs) noexcept;

  // `ssurf` can be `nullptr`, and it will be allocated, otherwise changed
  // this is used both to create initial surfaces, and to update changed surfaces
  sec_surface_t *CreateSecSurface (sec_surface_t *ssurf, subsector_t *sub, TSecPlaneRef InSplane, subregion_t *sreg, bool fake=false);
  // subsector is not changed, but we need it non-const
  //enum { USS_Normal, USS_Force, USS_IgnoreCMap, USS_ForceIgnoreCMap };
  void UpdateSecSurface (sec_surface_t *ssurf, TSecPlaneRef RealPlane, subsector_t *sub, subregion_t *sreg, bool ignoreColorMap=false, bool fake=false);

  // call this ONLY when vcount is known and won't be increased later! use `FreeWSurfs()` to free this surface
  surface_t *NewWSurf (int vcount) noexcept;
  // frees *the* *whole* *surface* *chain*!
  void FreeWSurfs (surface_t *&InSurfs) noexcept;
  surface_t *ReallocSurface (surface_t *surfs, int vcount) noexcept;

  // used to grow surface in lightmap t-junction fixer
  // returns new surface, and may modify `listhead` (and `pref->next`)
  // if `prev` is `nullptr`, will try to find the proper `prev`
  // vertex counter is not changed
  surface_t *EnsureSurfacePoints (surface_t *surf, int vcount, surface_t *&listhead, surface_t *prev) noexcept;

public:
  // can be called to recreate all world surfaces
  // call only after world surfaces were created for the first time!
  void InvaldateAllSegParts () noexcept;

  static void InvalidateSegPart (segpart_t *sp) noexcept;
  static void InvalidateWholeSeg (seg_t *seg) noexcept;

  void MarkAdjacentTJunctions (const sector_t *fsec, const line_t *line) noexcept;
  void MarkTJunctions (seg_t *seg) noexcept;

public:
  surface_t *CreateWSurf (TVec *wv, texinfo_t *texinfo, seg_t *seg, subsector_t *sub, int wvcount, vuint32 typeFlags) noexcept;

  // WARNING! this may modify `quad`
  void CreateWorldSurfFromWV (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec quad[4], vuint32 typeFlags) noexcept;

  // cut quad with regions (used to cut out parts obscured by 3d floors)
  // WARNING! this may modify `quad`
  void CreateWorldSurfFromWVSplit (subsector_t *sub, seg_t *seg, segpart_t *sp, TVec quad[4], vuint32 typeFlags,
                                   bool onlySolid=true, const sec_region_t *ignorereg=nullptr) noexcept;

  // cut quad with regions (used to cut out parts obscured by 3d floors)
  // WARNING! this may modify `quad`
  void CreateWorldSurfFromWVSplitFromReg (sec_region_t *frontreg, sec_region_t *backreg, subsector_t *sub, seg_t *seg, segpart_t *sp,
                                          TVec quad[4], vuint32 typeFlags,
                                          bool onlySolid, const sec_region_t *ignorereg=nullptr) noexcept;

protected:
  // to pass only one pointer
  struct QSplitInfo {
    subsector_t *sub;
    seg_t *seg;
    segpart_t *sp;
    vuint32 typeFlags;
    bool onlySolid;
    const sec_region_t *ignorereg;
    vuint32 mask;
  };

  void RecursiveQuadSplit (const QSplitInfo &nfo, sec_region_t *frontreg, sec_region_t *backreg, TVec quad[4]);

public:
  enum {
    QInvalid = -1, // invalid region
    QIntersect = 0, // the quad intersects the region
    QTop = 1, // the quad is completely above the region
    QBottom = 2, // the quad is completely below the region
    QInside = 3, // the quad is completely inside the region
  };

  static inline const char *GetQTypeStr (int cc) noexcept {
    switch (cc) {
      case QInvalid: return "QInvalid";
      case QIntersect: return "QIntersect";
      case QTop: return "QTop";
      case QBottom: return "QBottom";
      case QInside: return "QInside";
      default: break;
    }
    return "QWutafuck";
  }

protected:
  // doesn't really check quad direction, only valid z order
  // quad points are:
  //   [0]: left bottom point
  //   [1]: left top point
  //   [2]: right top point
  //   [3]: right bottom point
  static inline bool isValidQuad (const TVec quad[4]) noexcept {
    // is z ordering valid?
    if (quad[QUAD_V1_BOTTOM].z > quad[QUAD_V1_TOP].z) return false;
    if (quad[QUAD_V2_BOTTOM].z > quad[QUAD_V2_TOP].z) return false;
    // degenerate surface (non-vertical line)?
    if (quad[QUAD_V1_BOTTOM].z == quad[QUAD_V1_TOP].z && quad[QUAD_V1_TOP].z == quad[QUAD_V2_TOP].z && quad[QUAD_V2_TOP].z == quad[QUAD_V2_BOTTOM].z) return false;
    // degenerate surface (thin line)?
    if (quad[QUAD_V1_BOTTOM].z == quad[QUAD_V1_TOP].z && quad[QUAD_V2_TOP].z == quad[QUAD_V2_BOTTOM].z) return false;
    // degenerate surface (thin line)?
    if (quad[QUAD_V1_BOTTOM].z == quad[QUAD_V2_TOP].z && quad[QUAD_V1_TOP].z == quad[QUAD_V2_BOTTOM].z) return false;
    return true;
  }

  // exactly on the floor/ceiling is "inside" too
  static bool isPointInsideSolidReg (const TVec lv, const float pz, const sec_region_t *streg, const sec_region_t *ignorereg) noexcept;

  static void DumpQuad (const char *name, const TVec quad[4]) noexcept {
    GCon->Logf(NAME_Debug, " %s: v1bot=(%g,%g,%g); v1top=(%g,%g,%g); v2top=(%g,%g,%g); v2bot=(%g,%g,%g); valid=%d (v0=%d; v1=%d)", name,
      quad[QUAD_V1_BOTTOM].x, quad[QUAD_V1_BOTTOM].y, quad[QUAD_V1_BOTTOM].z,
      quad[QUAD_V1_TOP].x, quad[QUAD_V1_TOP].y, quad[QUAD_V1_TOP].z,
      quad[QUAD_V2_TOP].x, quad[QUAD_V2_TOP].y, quad[QUAD_V2_TOP].z,
      quad[QUAD_V2_BOTTOM].x, quad[QUAD_V2_BOTTOM].y, quad[QUAD_V2_BOTTOM].z,
      (int)isValidQuad(quad),
      (int)(!(quad[QUAD_V1_BOTTOM].z > quad[QUAD_V1_TOP].z)),
      (int)(!(quad[QUAD_V2_BOTTOM].z > quad[QUAD_V2_TOP].z)));
  }

  // quad must be valid; returns one of the `Q*` constants above
  static int ClassifyQuadVsRegion (const TVec quad[4], const sec_region_t *reg) noexcept;

  // splits quad with a non-vertical plane
  // returns `false` if there is no bottom quad
  // plane orientation doesn't matter (but it must not be vertical), "top" is always on top
  // `qbottom` and/or `qtop` can be `nullptr`
  // `qbottom` or `qtop` can be the same as `quad`
  // see comments in "r_surf_opening.cpp"
  // input quad must be valid
  static bool SplitQuadWithPlane (const TVec quad[4], TPlane pl, TVec qbottom[4], TVec qtop[4]) noexcept;

  static inline bool SplitQuadWithPlane (const TVec quad[4], const TSecPlaneRef &pl, TVec qbottom[4], TVec qtop[4]) noexcept {
    TPlane pp(pl.GetNormal(), pl.GetDist());
    return SplitQuadWithPlane(quad, pp, qbottom, qtop);
  }

public:
  static inline void SetupFakeDistances (const seg_t *seg, segpart_t *sp) noexcept {
    if (seg->frontsector->heightsec) {
      sp->frontFakeFloorDist = seg->frontsector->heightsec->floor.dist;
      sp->frontFakeCeilDist = seg->frontsector->heightsec->ceiling.dist;
    } else {
      sp->frontFakeFloorDist = 0.0f;
      sp->frontFakeCeilDist = 0.0f;
    }

    if (seg->backsector && seg->backsector->heightsec) {
      sp->backFakeFloorDist = seg->backsector->heightsec->floor.dist;
      sp->backFakeCeilDist = seg->backsector->heightsec->ceiling.dist;
    } else {
      sp->backFakeFloorDist = 0.0f;
      sp->backFakeCeilDist = 0.0f;
    }
  }

public:
  // HACK: sector with height of 1, and only middle
  // masked texture is "transparent door"
  //
  // actually, 2s "door" wall without top/bottom textures, and with masked
  // midtex is "transparent door"
  static bool IsTransDoorHack (const seg_t *seg, bool fortop) noexcept {
    const sector_t *secs[2] = { seg->frontsector, seg->backsector };
    if (!secs[0] || !secs[1]) return false;
    const side_t *sidedef = seg->sidedef;
    if (!GTextureManager.IsEmptyTexture(fortop ? sidedef->TopTexture : sidedef->BottomTexture)) return false;
    // if we have don't have a midtex, it is not a door hack
    //if (GTextureManager.IsEmptyTexture(sidedef->MidTexture)) return false;
    VTexture *tex = GTextureManager[sidedef->MidTexture];
    if (!tex || tex->Type == TEXTYPE_Null) return false;
    // should be "see through", or line should have alpha
    if (seg->linedef->alpha >= 1.0f && !tex->isSeeThrough()) return false;
    // check for slopes
    if (secs[0]->floor.normal.z != 1.0f || secs[0]->ceiling.normal.z != -1.0f) return false;
    if (secs[1]->floor.normal.z != 1.0f || secs[1]->ceiling.normal.z != -1.0f) return false;
    // ok, looks like it
    return true;
  }

  static inline bool IsTransDoorHackTop (const seg_t *seg) noexcept { return IsTransDoorHack(seg, true); }
  static inline bool IsTransDoorHackBot (const seg_t *seg) noexcept { return IsTransDoorHack(seg, false); }

public:
  static void SetupTextureAxesOffsetDummy (texinfo_t *texinfo, VTexture *tex, const bool resetAlpha=true);

  enum TAxType {
    TAX_TOP,
    TAX_BOTTOM,
    // middle texture for two-sided walls
    TAX_MID2S,
    // middle texture for one-sided walls
    TAX_MID1S,
    // middle texture for "extra floor" (aka 3d floor)
    TAX_MIDES,
    // for 3d polyobjects
    TAX_TOPPO,
    TAX_BOTTOMPO,
    TAX_MIDPO,
  };

  static inline bool IsTAxWrapFlagActive (const TAxType type) noexcept { return (type == TAX_MID2S); }

  void SetupTextureAxesOffsetEx (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam, TAxType type);

  inline void SetupTextureAxesOffsetTop (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam=nullptr) { SetupTextureAxesOffsetEx(seg, texinfo, tex, tparam, TexZ, segparam, TAX_TOP); }
  inline void SetupTextureAxesOffsetBot (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam=nullptr) { SetupTextureAxesOffsetEx(seg, texinfo, tex, tparam, TexZ, segparam, TAX_BOTTOM); }
  inline void SetupTextureAxesOffsetMid2S (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam=nullptr) { SetupTextureAxesOffsetEx(seg, texinfo, tex, tparam, TexZ, segparam, TAX_MID2S); }
  inline void SetupTextureAxesOffsetMid1S (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam=nullptr) { SetupTextureAxesOffsetEx(seg, texinfo, tex, tparam, TexZ, segparam, TAX_MID1S); }
  inline void SetupTextureAxesOffsetMidES (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam=nullptr) { SetupTextureAxesOffsetEx(seg, texinfo, tex, tparam, TexZ, segparam, TAX_MIDES); }

  inline void SetupTextureAxesOffsetTopPO (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam=nullptr) { SetupTextureAxesOffsetEx(seg, texinfo, tex, tparam, TexZ, segparam, TAX_TOPPO); }
  inline void SetupTextureAxesOffsetBotPO (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam=nullptr) { SetupTextureAxesOffsetEx(seg, texinfo, tex, tparam, TexZ, segparam, TAX_BOTTOMPO); }
  inline void SetupTextureAxesOffsetMidPO (seg_t *seg, texinfo_t *texinfo, VTexture *tex, const side_tex_params_t *tparam, float &TexZ, const side_tex_params_t *segparam=nullptr) { SetupTextureAxesOffsetEx(seg, texinfo, tex, tparam, TexZ, segparam, TAX_MIDPO); }

public:
  int CountSegParts (const seg_t *);
  void CreateSegParts (subsector_t *sub, drawseg_t *dseg, seg_t *seg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, sec_region_t *curreg);
  void CreateWorldSurfaces ();
  bool CopyPlaneIfValid (sec_plane_t *, const sec_plane_t *, const sec_plane_t *);
  void UpdateFakeFlats (sector_t *);
  void UpdateDeepWater (sector_t *);
  void UpdateFloodBug (sector_t *sec);

  // free all surfaces except the first one, clear first, set number of vertices to vcount
  void FreeSurfaces (surface_t *) noexcept;
  void FreeSegParts (segpart_t *) noexcept;

public:
  // models
  bool DrawAliasModel (VEntity *mobj, const TVec &Org, const TAVec &Angles,
    float ScaleX, float ScaleY, VModel *Mdl,
    const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
    VTextureTranslation *Trans, int Version,
    const RenderStyleInfo &ri,
    bool IsViewModel, float Inter, bool Interpolate,
    ERenderPass Pass);

  bool DrawAliasModel (VEntity *mobj, VName clsName, const TVec &Org, const TAVec &Angles,
    float ScaleX, float ScaleY,
    const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame, //old:VState *State, VState *NextState,
    VTextureTranslation *Trans, int Version,
    const RenderStyleInfo &ri,
    bool IsViewModel, float Inter, bool Interpolate,
    ERenderPass Pass);

  bool DrawEntityModel (VEntity *Ent, const RenderStyleInfo &ri, float Inter, ERenderPass Pass);

  bool CheckAliasModelFrame (VEntity *Ent, float Inter);
  bool HasEntityAliasModel (VEntity *Ent) const;
  static bool IsAliasModelAllowedFor (VEntity *Ent);
  bool IsShadowAllowedFor (VEntity *Ent);

  // things
  void QueueTranslucentSurface (surface_t *surf, const RenderStyleInfo &ri);

  void FixSpriteOffset (int fixAlgo, VEntity *thing, VTexture *Tex, const int TexHeight, const float scaleY, int &TexTOffset, int &origTOffset);

  void QueueSpritePoly (VEntity *thing, const TVec *sv, int lump, const RenderStyleInfo &ri,
                        int translation, const TVec &normal, float pdist, const TVec &saxis,
                        const TVec &taxis, const TVec &texorg, int priority, const TVec &sprOrigin,
                        vuint32 objid);

  void QueueSprite (VEntity *thing, RenderStyleInfo &ri, bool onlyShadow=false); // this can modify `ri`!
  void QueueTranslucentAliasModel (VEntity *mobj, const RenderStyleInfo &ri, float TimeFrac);
  bool RenderAliasModel (VEntity *mobj, const RenderStyleInfo &ri, ERenderPass Pass);
  void RenderThing (VEntity *, ERenderPass);
  void RenderMobjs (ERenderPass);
  void DrawTranslucentPolys ();
  void RenderPSprite (float SX, float SY, const VAliasModelFrameInfo &mfi, float PSP_DIST, const RenderStyleInfo &ri);
  bool RenderViewModel (VViewState *VSt, float SX, float SY, const RenderStyleInfo &ri);
  void DrawPlayerSprites ();

  // used in things rendering to calculate lighting in `ri`
  void SetupRIThingLighting (VEntity *ent, RenderStyleInfo &ri, bool asAmbient, bool allowBM);

  // used in sprite lighting checks
  bool RadiusCastRay (bool textureCheck, const subsector_t *subsector, const TVec &org, const subsector_t *destsubsector, const TVec &dest, float radius);

protected:
  // use drawer's vieworg, so it can be called only when rendering a scene
  // it's not exact!
  // returns `false` if the light is invisible (or too small, with radius < 8)
  // in this case, `w`, and `h` are zeroes
  // both `w` and `h` can be `nullptr`
  bool CalcScreenLightDimensions (const TVec &LightPos, const float LightRadius, int *w, int *h) noexcept;

  // does very primitive Z clipping
  // returns `false` if fully out of screen
  bool CalcBBox3DScreenPosition (const float bbox3d[6], int *x0, int *y0, int *x1, int *y1) noexcept;

protected:
  static int prevCrosshairPic;

  virtual void RenderCrosshair () override;

protected:
  virtual void RefilterStaticLights ();

  // used to get a floor to sample lightmap
  // WARNING! can return `nullptr`!
  //sec_surface_t *GetNearestFloor (const subsector_t *sub, const TVec &p);

  // this is common code for light point calculation
  // pass light values from ambient pass
  void CalculateDynLightSub (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, float height, unsigned flags);

  // calculate subsector's ambient light (light variables must be initialized)
  void CalculateSubAmbient (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, float height, unsigned flags);

  // calculate subsector's light from static light sources (light variables must be initialized)
  void CalculateSubStatic (VEntity *lowner, float &l, float &lr, float &lg, float &lb, const subsector_t *sub, const TVec &p, float radius, float height, unsigned flags);

  void CalcBSPNodeLMaps (int slindex, light_t &sl, int bspnum, const float *bbox);
  void CalcStaticLightTouchingSubs (int slindex, light_t &sl);

  // called when some surface from the given subsector changed
  // invalidates lightmaps for all touching lights
  virtual void InvalidateStaticLightmapsSubs (subsector_t *sub) override;
  // only polyobject itself
  virtual void InvalidatePObjLMaps (polyobj_t *po) override;

  virtual void InvalidateStaticLightmaps (const TVec &org, float radius, bool relight);

public:
  virtual particle_t *NewParticle (const TVec &porg) override;

  virtual int GetStaticLightCount () const noexcept override;
  virtual LightInfo GetStaticLight (int idx) const noexcept override;

  virtual int GetDynamicLightCount () const noexcept override;
  virtual LightInfo GetDynamicLight (int idx) const noexcept override;

  virtual void RenderPlayerView () override;

  // this will NOT take ownership, nor create any clones!
  // will include decal into list of decals for the given subregion
  virtual void AppendFlatDecal (decal_t *dc) override;
  // call this before destroying the decal
  virtual void RemoveFlatDecal (decal_t *dc) override;

  virtual decal_t *GetSRegFloorDecalHead (const subregion_t *sreg) noexcept override;
  virtual decal_t *GetSRegCeilingDecalHead (const subregion_t *sreg) noexcept override;

  // fix polyobject segs
  void SegMoved (seg_t *seg); // do not call this directly, it will be called from `PObjModified()`
  virtual void PObjModified (polyobj_t *po) override;

  virtual void SectorModified (sector_t *sec) override; // this sector *may* be moved

  virtual void SetupFakeFloors (sector_t *) override;

  virtual void ResetStaticLights () override;
  virtual void AddStaticLightRGB (vuint32 OwnerUId, const VLightParams &lpar, const vuint32 flags) override;
  virtual void MoveStaticLightByOwner (vuint32 OwnerUId, const TVec &origin) override;
  virtual void RemoveStaticLightByOwner (vuint32 OwnerUId) override;

  void RemoveStaticLightByIndex (int slidx);

  virtual void ClearReferences () override;

  virtual dlight_t *AllocDlight (VThinker *Owner, const TVec &lorg, float radius, int lightid=-1) override;
  virtual dlight_t *FindDlightById (int lightid) override;
  virtual void DecayLights (float) override;

  virtual void RegisterAllThinkers () override;
  virtual void ThinkerAdded (VThinker *Owner) override;
  virtual void ThinkerDestroyed (VThinker *Owner) override;

  virtual void ResetLightmaps (bool recalcNow) override;
  virtual bool IsShadowMapRenderer () const noexcept override;

  virtual bool isNeedLightmapCache () const noexcept override;
  virtual void saveLightmaps (VStream *strm) override;
  virtual bool loadLightmaps (VStream *strm) override;

public: // automap
  virtual void RenderTexturedAutomap (
    float m_x, float m_y, float m_x2, float m_y2,
    bool doFloors, // floors or ceiling?
    float alpha,
    AMCheckSubsectorCB CheckSubsector,
    AMIsHiddenSubsectorCB IsHiddenSubsector,
    AMMapXYtoFBXYCB MapXYtoFBXY
  ) override;

private: // automap
  static sec_surface_t *AM_getFlatSurface (subregion_t *reg, bool doFloors);
  void amFlatsCheckSubsector (int num);
  void amFlatsCheckNode (int bspnum);

  void amFlatsCollectSurfaces ();

protected:
  // the one that is higher
  sec_region_t *GetHigherRegionAtZ (sector_t *sector, const float srcrtopz, const sec_region_t *reg2skip=nullptr) noexcept;
  sec_region_t *GetHigherRegion (sector_t *sector, sec_region_t *srcreg) noexcept;

public: // k8: so i don't have to fuck with friends
  struct PPNode {
    vuint8 *mem;
    int size;
    int used;
    PPNode *next;
  };

  struct PPMark {
    PPNode *curr;
    int currused;

    PPMark () : curr(nullptr), currused(-666) {}
    inline bool isValid () const { return (currused != -666); }
  };


  static PPNode *pphead;
  static PPNode *ppcurr;
  static int ppMinNodeSize;

  static void CreatePortalPool ();
  static void KillPortalPool ();

public:
  static void ResetPortalPool (); // called on frame start
  static void SetMinPoolNodeSize (int minsz);

  static void MarkPortalPool (PPMark *mark);
  static void RestorePortalPool (PPMark *mark);
  static vuint8 *AllocPortalPool (int size);

  friend struct AutoSavedBspVis;
};


#endif
