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
#ifndef VAVOOM_R_LOCAL_HEADER_RADV
#define VAVOOM_R_LOCAL_HEADER_RADV


// ////////////////////////////////////////////////////////////////////////// //
class VRenderLevelShadowVolume : public VRenderLevelShared {
private:
  vuint32 CurrLightColor;
  // built in `BuildMobjsInCurrLight()`
  // used in rendering of shadow and light things (only)
  TArrayNC<VEntity *> mobjsInCurrLightModels;
  TArrayNC<VEntity *> mobjsInCurrLightSprites;
  int LightsRendered;
  int DynLightsRendered;
  // set this to true before calling `RenderLightShadows()` to indicate dynamic light
  bool DynamicLights;

  // keep 'em here, so we don't have to traverse BSP several times
  TArrayNC<surface_t *> shadowSurfacesSolid;
  TArrayNC<surface_t *> shadowSurfacesMasked;
  TArrayNC<surface_t *> lightSurfacesSolid;
  TArrayNC<surface_t *> lightSurfacesMasked;

  // used to avoid double-checking; sized by NumSectors
  struct FlatSectorShadowInfo {
    enum {
      NoFloor   = 1u<<0,
      NoCeiling = 1u<<1,
      //
      NothingZero = 0u,
    };
    unsigned renderFlag;
    unsigned frametag;
  };

  TArray<FlatSectorShadowInfo> fsecCheck;

  #ifdef VV_CHECK_1S_CAST_SHADOW
  struct Line1SShadowInfo {
    int canShadow;
    unsigned frametag;
  };

  TArray<Line1SShadowInfo> flineCheck;
  #endif

  unsigned fsecCounter;

  inline unsigned fsecCounterGen () noexcept {
    if ((++fsecCounter) == 0 || fsecCheck.length() != Level->NumSectors
        #ifdef VV_CHECK_1S_CAST_SHADOW
        || flineCheck.length() != Level->NumLines
        #endif
       ) {
      if (fsecCheck.length() != Level->NumSectors) fsecCheck.setLength(Level->NumSectors);
      for (auto &&it : fsecCheck) it.frametag = 0;
      #ifdef VV_CHECK_1S_CAST_SHADOW
      if (flineCheck.length() != Level->NumLines) flineCheck.setLength(Level->NumLines);
      for (auto &&it : flineCheck) it.frametag = 0;
      #endif
      fsecCounter = 1;
    }
    return fsecCounter;
  }

  // to avoid checking sectors twice in `CheckShadowingFlats`
  TArray<unsigned> fsecSeenSectors;
  unsigned fsecSeenSectorsCounter;

  inline unsigned fsecSeenSectorsGen () noexcept {
    if ((++fsecSeenSectorsCounter) == 0 || fsecSeenSectors.length() != Level->NumSectors) {
      if (fsecSeenSectors.length() != Level->NumSectors) fsecSeenSectors.setLength(Level->NumSectors);
      for (auto &&it : fsecSeenSectors) it = 0;
      fsecSeenSectorsCounter = 1;
    }
    return fsecSeenSectorsCounter;
  }


  // returns `renderFlag`
  unsigned CheckShadowingFlats (subsector_t *sub);

  #ifdef VV_CHECK_1S_CAST_SHADOW
  bool CheckCan1SCastShadow (line_t *line);
  #endif

protected:
  virtual void RefilterStaticLights () override;

  void RenderSceneLights (const refdef_t *RD, const VViewClipper *Range);
  void RenderSceneStaticLights (const refdef_t *RD, const VViewClipper *Range);
  void RenderSceneDynamicLights (const refdef_t *RD, const VViewClipper *Range);

  // general
  virtual void RenderScene (const refdef_t *, const VViewClipper *) override;

  // surface fixer for lightmaps, does nothing here
  virtual surface_t *FixSegSurfaceTJunctions (surface_t *surf, seg_t *seg) override;

  // surf methods
  virtual void InitSurfs (bool recalcStaticLightmaps, surface_t *ASurfs, texinfo_t *texinfo, const TPlane *plane, subsector_t *sub, seg_t *seg, subregion_t *sreg) override;
  virtual surface_t *SubdivideFace (surface_t *InF, subregion_t *sreg, sec_surface_t *ssf, const TVec &axis, const TVec *nextaxis, const TPlane *plane, bool doSubdivisions) override;
  virtual surface_t *SubdivideSeg (surface_t *InSurf, const TVec &axis, const TVec *nextaxis, seg_t *seg) override;

  virtual void ProcessCachedSurfaces () override;

  // world BSP rendering

  // called to put surface into queue
  // surface is either solid, or masked, but never translucent/additive/etc.
  virtual void QueueWorldSurface (surface_t *surf) override;

  void AddPolyObjToLightClipper (VViewClipper &clip, subsector_t *sub, int asShadow);

  bool collectorForShadowMaps; // used in `CollectAdv*()`, set in `CollectLightShadowSurfaces()`
  int collectorShadowType; // for clipper, used in `CollectAdv*()`, set in `CollectLightShadowSurfaces()`

  // we can collect surfaces for lighting and shadowing in one pass
  // don't forget to reset `shadowSurfaces` and `lightSurfaces`
  void CollectAdvLightSurfaces (const seg_t *seg, surface_t *InSurfs, texinfo_t *texinfo,
                                VEntity *SkyBox, bool CheckSkyBoxAlways, int SurfaceType,
                                unsigned int ssflag, const bool distInFront, bool flipAllowed);
  void CollectAdvLightLine (subsector_t *sub, sec_region_t *secregion, drawseg_t *dseg, unsigned int ssflag);
  void CollectAdvLightSecSurface (sec_region_t *secregion, sec_surface_t *ssurf, VEntity *SkyBox, unsigned int ssflag, const bool paperThin=false);
  void CollectAdvLightPolyObj (subsector_t *sub, unsigned int ssflag);
  void CollectAdvLightSubRegion (subsector_t *sub, unsigned int ssflag);
  void CollectAdvLightSubsector (int num, unsigned int ssflag);
  void CollectAdvLightBSPNode (int bspnum, const float *bbox, unsigned int ssflag);

  // collector entry point
  // `CurrLightPos` and `CurrLightRadius` should be set
  void CollectLightShadowSurfaces (bool doShadows);

  void RenderShadowSurfaceList ();
  void RenderLightSurfaceList ();

  void DrawShadowSurfaces (surface_t *InSurfs, texinfo_t *texinfo, VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross);
  void DrawLightSurfaces (surface_t *InSurfs, texinfo_t *texinfo, VEntity *SkyBox, bool CheckSkyBoxAlways, int LightCanCross);

  // WARNING! may modify `Pos`
  void RenderLightShadows (VEntity *ent, vuint32 dlflags, const refdef_t *RD, const VViewClipper *Range,
                           TVec &Pos, float Radius, float LightMin, vuint32 Color,
                           TVec coneDir=TVec(0.0f, 0.0f, 0.0f), float coneAngle=0.0f, bool forceRender=false);

  // things
  void BuildMobjsInCurrLight (bool doShadows, bool collectSprites);

  void RenderMobjsAmbient ();
  void RenderMobjsTextures ();
  void RenderMobjsLight (VEntity *owner, vuint32 dlflags);
  void RenderMobjsShadow (VEntity *owner, vuint32 dlflags);
  void RenderMobjsShadowMap (VEntity *owner, const unsigned int facenum, vuint32 dlflags);
  void RenderMobjsFog ();

  void RenderMobjSpriteShadowMap (VEntity *owner, const unsigned int facenum, int spShad, vuint32 dlflags);
  // doesn't do any checks, just renders it
  void RenderMobjShadowMapSprite (VEntity *ent, const unsigned int facenum, const bool allowRotating);

  bool IsTouchedByCurrLight (const VEntity *ent) const noexcept;

public:
  VRenderLevelShadowVolume (VLevel *);
  ~VRenderLevelShadowVolume ();

  virtual void PreRender () override;

  virtual void BuildLightMap (surface_t *) override;
  virtual bool IsShadowMapRenderer () const noexcept override;
};


#endif
