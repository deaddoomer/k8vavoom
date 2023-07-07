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
#include "../gamedefs.h"
#include "beamclip.h"

// use seg coords for "is closed" check?
// this seems to be wrong!
//#define CLIPPER_USE_SEG_COORDS

//#define XXX_CLIPPER_DUMP_ADDED_RANGES

// if `Origin` is on a plane, we should consider this plane front-facing
// without this hack, rendering when the origin is exactly on a plane will
// skip some clipping, and "crack through walls". this is long-standing
// clipping bug; i knew about it for a long time, but never managed to
// investigate the cause of it.
#define CLIPPER_ONPLANE_HACK  true

#ifdef VAVOOM_CLIPPER_USE_REAL_ANGLES
# define MIN_ANGLE  ((VFloat)0)
# define MAX_ANGLE  ((VFloat)360)
#else
# ifdef VAVOOM_CLIPPER_USE_PSEUDO_INT
#  define MIN_ANGLE  (0)
//#  define MAX_ANGLE  (4u*ClipperPseudoResolution)
#  define MAX_ANGLE  (0xffffffffu)
#  define VAVOOM_CLIPPER_NO_ANGLE_CLAMP
# else
#  define MIN_ANGLE  ((VFloat)0)
#  define MAX_ANGLE  ((VFloat)4)
# endif
# ifdef VV_CLIPPER_FULL_CHECK
#  error "oops"
# endif
#endif

#ifdef XXX_CLIPPER_DUMP_ADDED_RANGES
# define RNGLOG(...)  if (dbg_clip_dump_added_ranges) GCon->Logf(__VA_ARGS__)
# define SBCLOG(...)  if (dbg_clip_dump_sub_checks) GCon->Logf((__VA_ARGS__)
#else
# define RNGLOG(...)  do {} while (0)
# define SBCLOG(...)  do {} while (0)
#endif


static VCvarB clip_hack_pointside("clip_hack_pointside", CLIPPER_ONPLANE_HACK, "Fix occasional \"through the wall\" bug? (EXPERIMENTAL!)", CVAR_NoShadow/*|CVAR_Archive*/);


// ////////////////////////////////////////////////////////////////////////// //
// returns side 0 (front, or on a plane) or 1 (back)
static VVA_FORCEINLINE VVA_CHECKRESULT int ClipPointOnSide2 (const TPlane &plane,
                                                             const TVec &point) noexcept
{
  if (clip_hack_pointside) {
    return (point.dot(plane.normal)-plane.dist < 0.0f);
  } else {
    return plane.PointOnSide2(point);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
#ifdef CLIENT
extern VCvarB r_draw_pobj;
#else
enum { r_draw_pobj = true };
#endif

static VCvarB clip_enabled("clip_enabled", true, "Do 1D geometry cliping optimizations?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB clip_bbox("clip_bbox", true, "Clip BSP bboxes with 1D clipper?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB clip_subregion("clip_subregion", true, "Clip subregions?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB clip_with_polyobj("clip_with_polyobj", true, "Do clipping with polyobjects?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB clip_platforms("clip_platforms", true, "Clip geometry behind some closed doors and lifts?", CVAR_PreInit|CVAR_NoShadow);
VCvarB clip_frustum("clip_frustum", true, "Clip geometry with frustum?", CVAR_PreInit|CVAR_NoShadow);
//VCvarB clip_frustum_mirror("clip_frustum_mirror", true, "Clip mirrored geometry with frustum?", CVAR_PreInit|CVAR_NoShadow);
VCvarB clip_frustum_init_range("clip_frustum_init_range", true, "Init clipper range with frustum?", CVAR_PreInit|CVAR_NoShadow);
// set to false, because 1d clipper can clip bboxes too, and node bsp z is miscalculated for some map trickery
VCvarB clip_frustum_bsp("clip_frustum_bsp", true, "Clip BSP geometry with frustum?", CVAR_PreInit|CVAR_NoShadow);
//VCvarB clip_frustum_bsp_segs("clip_frustum_bsp_segs", false, "Clip segs in BSP rendering with frustum?", CVAR_PreInit|CVAR_NoShadow);
//VCvarI clip_frustum_check_mask("clip_frustum_check_mask", TFrustum::LeftBit|TFrustum::RightBit|TFrustum::BackBit, "Which frustum planes we should check?", CVAR_PreInit|CVAR_NoShadow);
//VCvarI clip_frustum_check_mask("clip_frustum_check_mask", "19", "Which frustum planes we should check?", CVAR_PreInit|CVAR_NoShadow);
VCvarI clip_frustum_check_mask("clip_frustum_check_mask", "255", "Which frustum planes we should check?", CVAR_PreInit|CVAR_NoShadow);

static VCvarB clip_add_backface_segs("clip_add_backface_segs", false, "Add backfaced segs to 1D clipper (this prevents some clipping bugs, but makes \"noclip\" less usable)?", CVAR_PreInit|CVAR_NoShadow);

static VCvarB clip_skip_slopes_1side("clip_skip_slopes_1side", false, "Skip clipping with one-sided slopes?", CVAR_PreInit|CVAR_NoShadow);

// need to be public for portal rendering
VCvarB clip_height("clip_height", true, "Clip with top and bottom frustum?", CVAR_PreInit|CVAR_NoShadow);
VCvarB clip_midsolid("clip_midsolid", true, "Clip with solid midtex?", CVAR_PreInit|CVAR_NoShadow);

static VCvarB clip_use_transfers("clip_use_transfers", true, "Use transfer sectors to clip?", CVAR_PreInit|CVAR_NoShadow);

VCvarB clip_use_1d_clipper("clip_use_1d_clipper", true, "Use 1d clipper?", CVAR_PreInit|CVAR_NoShadow);

VCvarB dbg_clip_dump_added_ranges("dbg_clip_dump_added_ranges", false, "Dump added ranges in 1D clipper?", CVAR_PreInit|CVAR_NoShadow);
VCvarB dbg_clip_dump_sub_checks("dbg_clip_dump_sub_checks", false, "Dump subsector checks in 1D clipper?", CVAR_PreInit|CVAR_NoShadow);


// ////////////////////////////////////////////////////////////////////////// //
/*
  // Lines with stacked sectors must never block!
  if (backsector->portals[sector_t::ceiling] != NULL || backsector->portals[sector_t::floor] != NULL ||
      frontsector->portals[sector_t::ceiling] != NULL || frontsector->portals[sector_t::floor] != NULL)
  {
    return false;
  }
*/


//==========================================================================
//
//  CopyFloorPlaneIfValid
//
//==========================================================================
static inline bool CopyFloorPlaneIfValid (TPlane *dest, const TPlane *source, const TPlane *opp) noexcept {
  bool copy = false;

  // if the planes do not have matching slopes, then always copy them
  // because clipping would require creating new sectors
  if (source->normal != dest->normal) {
    copy = true;
  } else if (opp->normal != -dest->normal) {
    if (source->dist < dest->dist) copy = true;
  } else if (source->dist < dest->dist && source->dist > -opp->dist) {
    copy = true;
  }

  if (copy) *(TPlane *)dest = *(TPlane *)source;

  return copy;
}


//==========================================================================
//
//  CopyHeight
//
//==========================================================================
static inline void CopyHeight (const sector_t *sec, TPlane *fplane, TPlane *cplane, int *fpic, int *cpic, const sec_plane_t **sfpl=nullptr, const sec_plane_t **scpl=nullptr) noexcept {
  *cpic = sec->ceiling.pic;
  *fpic = sec->floor.pic;
  *fplane = *(TPlane *)&sec->floor;
  *cplane = *(TPlane *)&sec->ceiling;
  if (sfpl) *sfpl = &sec->floor;
  if (scpl) *scpl = &sec->ceiling;

  if (clip_use_transfers) {
    // check transferred (k8: do we need more checks here?)
    sector_t *hs = sec->heightsec;
    if (!hs) return;
    if (hs->SectorFlags&sector_t::SF_IgnoreHeightSec) return;

    if (hs->SectorFlags&sector_t::SF_ClipFakePlanes) {
      if (sec->floor.dist > hs->floor.dist) {
        if (!CopyFloorPlaneIfValid(fplane, &hs->floor, &sec->ceiling)) {
          if (hs->SectorFlags&sector_t::SF_FakeFloorOnly) return;
        }
        if (sfpl) *sfpl = &hs->floor;
        *fpic = hs->floor.pic;
      }
      if (-sec->ceiling.dist < -hs->ceiling.dist) {
        if (!CopyFloorPlaneIfValid(cplane, &hs->ceiling, &sec->floor)) {
          return;
        }
        if (scpl) *scpl = &hs->ceiling;
        *cpic = hs->ceiling.pic;
      }
    } else {
      if (hs->SectorFlags&sector_t::SF_FakeCeilingOnly) {
        if (-sec->ceiling.dist < -hs->ceiling.dist) {
          *cplane = *(TPlane *)&hs->ceiling;
          if (scpl) *scpl = &hs->ceiling;
        }
      } else if (hs->SectorFlags&sector_t::SF_FakeFloorOnly) {
        if (sec->floor.dist > hs->floor.dist) {
          *fplane = *(TPlane *)&hs->floor;
          if (sfpl) *sfpl = &hs->floor;
        }
      } else {
        if (-sec->ceiling.dist < -hs->ceiling.dist) {
          *cplane = *(TPlane *)&hs->ceiling;
          if (scpl) *scpl = &hs->ceiling;
        }
        if (sec->floor.dist > hs->floor.dist) {
          *fplane = *(TPlane *)&hs->floor;
          if (sfpl) *sfpl = &hs->floor;
        }
      }
    }
  }
}


//==========================================================================
//
//  CopyHeightServer
//
//==========================================================================
static inline void CopyHeightServer (VLevel *level, rep_sector_t *repsecs, const sector_t *sec, TPlane *fplane, TPlane *cplane, int *fpic, int *cpic) noexcept {
  *cpic = sec->ceiling.pic;
  *fpic = sec->floor.pic;
  *fplane = *(TPlane *)&sec->floor;
  *cplane = *(TPlane *)&sec->ceiling;

  int snum = (int)(ptrdiff_t)(sec-&level->Sectors[0]);
  if (repsecs[snum].floor_dist < sec->floor.dist) fplane->dist = repsecs[snum].floor_dist;
  if (repsecs[snum].ceiling_dist < sec->ceiling.dist) cplane->dist = repsecs[snum].ceiling_dist;
}


//==========================================================================
//
//  IsGoodSegForPoly
//
//==========================================================================
/*
static inline bool IsGoodSegForPoly (const VViewClipper &clip, const seg_t *seg) noexcept {
  const line_t *ldef = seg->linedef;
  if (!ldef) return true;

  if (!(ldef->flags&ML_TWOSIDED)) return true; // one-sided wall always blocks everything
  if (ldef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything

  // mirrors and horizons always block the view
  switch (ldef->special) {
    case LNSPEC_LineHorizon:
    case LNSPEC_LineMirror:
      return true;
  }

  auto fsec = ldef->frontsector;
  auto bsec = ldef->backsector;

  if (fsec == bsec) return false; // self-referenced sector

  int fcpic, ffpic;
  TPlane ffplane, fcplane;

  CopyHeight(fsec, &ffplane, &fcplane, &ffpic, &fcpic);
  if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return false;

  if (bsec) {
    TPlane bfplane, bcplane;
    int bcpic, bfpic;
    CopyHeight(bsec, &bfplane, &bcplane, &bfpic, &bcpic);
    if (bfplane.normal.z != 1.0f || bcplane.normal.z != -1.0f) return false;
  }

  return true;
}
*/


//==========================================================================
//
//  IsPObjSegAClosedSomething
//
//==========================================================================
static bool IsPObjSegAClosedSomething (VLevel *level, const TFrustum *Frustum, polyobj_t *pobj, const subsector_t *sub, const seg_t *seg, const TVec *lorg=nullptr, const float *lrad=nullptr) noexcept {
  (void)level; (void)Frustum; (void)lorg; (void)lrad;
  const line_t *ldef = seg->linedef;

  if ((ldef->flags&ML_ADDITIVE) != 0 || ldef->alpha < 1.0f) return false; // skip translucent walls
  if (ldef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything

  // mirrors and horizons always block the view
  switch (ldef->special) {
    case LNSPEC_LineHorizon:
    case LNSPEC_LineMirror:
      return true;
  }

  auto midTexType = GTextureManager.GetTextureType(seg->sidedef->MidTexture);

  if (midTexType != VTextureManager::TCT_SOLID) return false;

  #ifdef CLIPPER_USE_SEG_COORDS
  const TVec vv1 = *seg->v1;
  const TVec vv2 = *seg->v2;
  #else
  const TVec vv1 = *ldef->v1;
  const TVec vv2 = *ldef->v2;
  #endif

  // polyobject height
  float pocz1 = pobj->poceiling.GetPointZClamped(vv1);
  float pocz2 = pobj->poceiling.GetPointZClamped(vv2);
  float pofz1 = pobj->pofloor.GetPointZClamped(vv1);
  float pofz2 = pobj->pofloor.GetPointZClamped(vv2);

  if (pofz1 >= pocz1 || pofz2 >= pocz2) return false; // something strange

  if (ldef->pobj()->Is3D()) {
    // for wrapped, the whole wall is covered
    if (((ldef->flags&ML_WRAP_MIDTEX)|(seg->sidedef->Flags&SDF_WRAPMIDTEX)) == 0) {
      // non-wrapped
      const side_t *sidedef = seg->sidedef;
      // determine real height using midtex
      //const sector_t *sec = seg->backsector; //(!seg->side ? ldef->backsector : ldef->frontsector);
      VTexture *MTex = GTextureManager(sidedef->MidTexture);
      // here we should check if midtex covers the whole height, as it is not tiled vertically (if not wrapped)
      const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;
      // do more calculations only if the texture doesn't cover the whole wall
      if (texh < max2(pocz1-pofz1, pocz2-pofz2)) {
        float zOrg; // texture bottom
        if (ldef->flags&ML_DONTPEGBOTTOM) {
          // bottom of texture at bottom
          zOrg = pobj->pofloor.TexZ;
        } else {
          // top of texture at top
          zOrg = pobj->poceiling.TexZ-texh;
        }
        const float yofs = sidedef->Mid.RowOffset;
        if (yofs != 0.0f) zOrg += yofs/(MTex->TextureTScale()/MTex->TextureOffsetTScale()*sidedef->Mid.ScaleY);
        pofz1 = max2(pofz1, zOrg);
        pofz2 = max2(pofz2, zOrg);
        pocz1 = min2(pocz1, zOrg+texh);
        pocz2 = min2(pocz2, zOrg+texh);
      } else {
        // fully covered
        // check top texture
        VTexture *TTex = GTextureManager(sidedef->TopTexture);
        if (TTex && TTex->Type != TEXTYPE_Null && !TTex->isSeeThrough()) {
          //TODO: sloped 3d polyobjects!
          const float ttexh = TTex->GetScaledHeightF()/sidedef->Top.ScaleY;
          pocz1 += ttexh;
          pocz2 += ttexh;
        }
        // check bottom texture
        VTexture *BTex = GTextureManager(sidedef->BottomTexture);
        if (BTex && BTex->Type != TEXTYPE_Null && !BTex->isSeeThrough()) {
          //TODO: sloped 3d polyobjects!
          const float ttexh = BTex->GetScaledHeightF()/sidedef->Bot.ScaleY;
          pofz1 -= ttexh;
          pofz2 -= ttexh;
        }
      }
    }
  }

  // base sector height
  int fcpic, ffpic;
  TPlane ffplane, fcplane;

  const sec_plane_t *sfpl;
  const sec_plane_t *scpl;

  CopyHeight(sub->sector, &ffplane, &fcplane, &ffpic, &fcpic, &sfpl, &scpl);

  const float seccz1 = scpl->GetPointZClamped(vv1);
  const float seccz2 = scpl->GetPointZClamped(vv2);
  const float secfz1 = sfpl->GetPointZClamped(vv1);
  const float secfz2 = sfpl->GetPointZClamped(vv2);

  // is midtex fully covers the sector?
  if (pofz1 <= secfz1 && pofz2 <= secfz2 &&
      pocz1 >= seccz1 && pocz2 >= seccz2)
  {
    return true;
  }

  return false;
}


//==========================================================================
//
//  IsPObjSegAClosedSomethingServer
//
//==========================================================================
static bool IsPObjSegAClosedSomethingServer (VLevel *level, const TFrustum *Frustum, polyobj_t *pobj, const subsector_t *sub, const seg_t *seg, const TVec *lorg=nullptr, const float *lrad=nullptr) noexcept {
  return IsPObjSegAClosedSomething(level, Frustum, pobj, sub, seg, lorg, lrad);
}


// sorry for this, but i want to avoid some copy-pasting

#define CLIPPER_CHECK_CLOSED_SECTOR()  do { \
  /* now check for closed sectors */ \
  /* inspired by Zandronum code (actually, most sourceports has this, Zandronum was just a port i looked at) */ \
 \
  /* properly render skies (consider door "open" if both ceilings are sky) */ \
  /* k8: why? */ \
  if (bcpic == skyflatnum && fcpic == skyflatnum) return false; \
  /* closed door */ \
  if (backcz1 <= frontfz1 && backcz2 <= frontfz2) { \
    /* preserve a kind of transparent door/lift special effect */ \
    if (seg->frontsector == fsec) { \
      /* we are looking at toptex */ \
      if (topTexType != VTextureManager::TCT_SOLID) return false; \
    } else { \
      /* we are looking at bottex */ \
      if (botTexType != VTextureManager::TCT_SOLID) return false; \
    } \
    return true; \
  } \
 \
  if (frontcz1 <= backfz1 && frontcz2 <= backfz2) { \
    /* preserve a kind of transparent door/lift special effect */ \
    if (seg->frontsector == bsec) { \
      /* we are looking at toptex */ \
      if (topTexType != VTextureManager::TCT_SOLID) return false; \
    } else { \
      /* we are looking at bottex */ \
      if (botTexType != VTextureManager::TCT_SOLID) return false; \
    } \
    /*if (botTexType != VTextureManager::TCT_SOLID) return false;*/ \
    return true; \
  } \
 \
  /* if door is closed because back is shut */ \
  if (backcz1 <= backfz1 && backcz2 <= backfz2) { \
    /* properly render skies */ \
    /* k8: why? */ \
    if (bfpic == skyflatnum && ffpic == skyflatnum) return false; \
    /* preserve a kind of transparent door/lift special effect */ \
    if (seg->frontsector == bsec) { \
      /* we are inside a closed sector; check for a transparent door */ \
      /* if any of partner top/down texture is visible, and it is non-solid, this is non-blocking seg */ \
      /* see commented code above; it is wrong */ \
      if (GTextureManager.GetTextureType(seg->partner->sidedef->TopTexture) != VTextureManager::TCT_SOLID) { \
        if (backfz1 < frontcz1 || backfz2 < frontcz2) return false; \
      } \
      if (GTextureManager.GetTextureType(seg->partner->sidedef->BottomTexture) != VTextureManager::TCT_SOLID) { \
        if (backcz1 > frontfz1 || backcz2 > frontfz2) return false; \
      } \
      return true; \
    } \
    /* we are in front sector */ \
    if (backcz1 <= frontcz1 || backcz2 <= frontcz2) { \
      if (topTexType != VTextureManager::TCT_SOLID) return false; \
    } \
    if (backfz1 >= frontfz1 || backfz2 >= frontfz2) { \
      if (botTexType != VTextureManager::TCT_SOLID) return false; \
    } \
    return true; \
  } \
} while (0)


#ifdef CLIPPER_USE_SEG_COORDS
#define CLIPPER_GET_VV1_VV2  \
  const TVec vv1 = *seg->v1; \
  const TVec vv2 = *seg->v2;
#else
#define CLIPPER_GET_VV1_VV2  \
  const TVec vv1 = *ldef->v1; \
  const TVec vv2 = *ldef->v2;
#endif

#define CLIPPER_CALC_FCHEIGHTS  \
  CLIPPER_GET_VV1_VV2 \
 \
  const float frontcz1 = fcplane.GetPointZ(vv1); \
  const float frontcz2 = fcplane.GetPointZ(vv2); \
  const float frontfz1 = ffplane.GetPointZ(vv1); \
  const float frontfz2 = ffplane.GetPointZ(vv2); \
 \
  const float backcz1 = bcplane.GetPointZ(vv1); \
  const float backcz2 = bcplane.GetPointZ(vv2); \
  const float backfz1 = bfplane.GetPointZ(vv1); \
  const float backfz2 = bfplane.GetPointZ(vv2); \


//==========================================================================
//
//  VViewClipper::IsSegAClosedSomethingServer
//
//  prerequisite: has front and back sectors, has linedef
//
//==========================================================================
bool VViewClipper::IsSegAClosedSomethingServer (VLevel *level, rep_sector_t *repsecs, const seg_t *seg) noexcept {
  if (!clip_platforms) return false;
  if (!seg->backsector) return false;

  if (!seg->partner || seg->partner == seg) return false;

  const line_t *ldef = seg->linedef;

  if ((ldef->flags&ML_ADDITIVE) != 0 || ldef->alpha < 1.0f) return false; // skip translucent walls
  if (!(ldef->flags&ML_TWOSIDED)) return true; // one-sided wall always blocks everything
  if (ldef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything

  // mirrors and horizons always block the view
  switch (ldef->special) {
    case LNSPEC_LineHorizon:
    case LNSPEC_LineMirror:
      return true;
  }

  const sector_t *fsec = ldef->frontsector;
  const sector_t *bsec = ldef->backsector;

  if (fsec == bsec) return false; // self-referenced sector

  const side_t *sidedef = seg->sidedef;

  auto topTexType = GTextureManager.GetTextureType(sidedef->TopTexture);
  auto botTexType = GTextureManager.GetTextureType(sidedef->BottomTexture);
  auto midTexType = GTextureManager.GetTextureType(sidedef->MidTexture);

  // transparent door hack
  if (topTexType == VTextureManager::TCT_EMPTY && botTexType == VTextureManager::TCT_EMPTY) return false;

  if (topTexType == VTextureManager::TCT_SOLID || // a seg without top texture isn't a door
      botTexType == VTextureManager::TCT_SOLID || // a seg without bottom texture isn't an elevator/plat
      midTexType == VTextureManager::TCT_SOLID) // a seg without mid texture isn't a polyobj door
  {
    int fcpic, ffpic;
    int bcpic, bfpic;

    TPlane ffplane, fcplane;
    TPlane bfplane, bcplane;

    CopyHeightServer(level, repsecs, fsec, &ffplane, &fcplane, &ffpic, &fcpic);
    CopyHeightServer(level, repsecs, bsec, &bfplane, &bcplane, &bfpic, &bcpic);

    CLIPPER_CALC_FCHEIGHTS

    CLIPPER_CHECK_CLOSED_SECTOR();
  }

  return false;
}


//==========================================================================
//
//  VViewClipper::IsSegAClosedSomething
//
//  prerequisite: has front and back sectors, has linedef
//
//  returns `true` if blocking
//
//==========================================================================
bool VViewClipper::IsSegAClosedSomething (VLevel *level, const TFrustum *Frustum, const seg_t *seg, const TVec *lorg, const float *lrad) noexcept {
  (void)level;
  if (!clip_platforms) return false;
  if (!seg->backsector) return false;

  if (!seg->partner || seg->partner == seg) return false;

  const line_t *ldef = seg->linedef;

  if ((ldef->flags&ML_ADDITIVE) || ldef->alpha < 1.0f) return false; // skip translucent walls
  if (!(ldef->flags&ML_TWOSIDED)) return true; // one-sided wall always blocks everything
  if (ldef->flags&ML_3DMIDTEX) return false; // 3dmidtex never blocks anything
  if (ldef->pobj()) return false; // just in case

  // mirrors and horizons always block the view
  switch (ldef->special) {
    case LNSPEC_LineHorizon:
    case LNSPEC_LineMirror:
      return true;
  }

  if (ldef->sidenum[0] < 0 || ldef->sidenum[1] < 0) return true; // one-sided

  const sector_t *fsec = seg->frontsector;
  const sector_t *bsec = seg->backsector;

  if (fsec == bsec) return false; // self-referenced sector
  if (!fsec || !bsec) return true; // one-sided

  // skip clipping with transdoor sectors (really?)
  // we should mark transparent door tracks -- they should be used for clipping
  if ((fsec->SectorFlags|bsec->SectorFlags)&sector_t::SF_IsTransDoor) return false;

  const side_t *sidedef = seg->sidedef;

  auto topTexType = GTextureManager.GetTextureType(sidedef->TopTexture);
  auto botTexType = GTextureManager.GetTextureType(sidedef->BottomTexture);
  auto midTexType = GTextureManager.GetTextureType(sidedef->MidTexture);

  // just in case (transparent doors)
  if (topTexType != VTextureManager::TCT_SOLID &&
      botTexType != VTextureManager::TCT_SOLID &&
      midTexType != VTextureManager::TCT_SOLID)
  {
    return false;
  }

  int fcpic, ffpic;
  int bcpic, bfpic;

  TPlane ffplane, fcplane;
  TPlane bfplane, bcplane;

  CopyHeight(fsec, &ffplane, &fcplane, &ffpic, &fcpic);
  CopyHeight(bsec, &bfplane, &bcplane, &bfpic, &bcpic);

  //FIXME: check for sky textures here?

  CLIPPER_CALC_FCHEIGHTS

  const bool topvisible = (backcz1 < frontcz1 || backcz2 < frontcz2); // is some part of the top texture visible?
  const bool botvisible = (backfz1 > frontfz1 || backfz2 > frontfz2); // is some part of the bottom texture visible?

  // just in case (transparent doors)
  if ((!topvisible || topTexType != VTextureManager::TCT_SOLID) &&
      (!botvisible || botTexType != VTextureManager::TCT_SOLID) &&
      midTexType != VTextureManager::TCT_SOLID)
  {
    return false;
  }

  // turn invisible textures into solid ones (because they essentially are)
  if (!topvisible) topTexType = VTextureManager::TCT_SOLID;
  if (!botvisible) botTexType = VTextureManager::TCT_SOLID;

  // transparent door hack (not needed, we'll do it better)
  //if (topTexType == VTextureManager::TCT_EMPTY && botTexType == VTextureManager::TCT_EMPTY) return false;

  if (topTexType == VTextureManager::TCT_SOLID || // a seg without top texture isn't a door
      botTexType == VTextureManager::TCT_SOLID || // a seg without bottom texture isn't an elevator/plat
      midTexType == VTextureManager::TCT_SOLID) // a seg without mid texture isn't a polyobj door
  {
    CLIPPER_CHECK_CLOSED_SECTOR();

    // clip with solid midtex that covers the whole hole (both other textures should be solid/invisible)
    if (midTexType == VTextureManager::TCT_SOLID &&
        topTexType == VTextureManager::TCT_SOLID &&
        botTexType == VTextureManager::TCT_SOLID &&
        clip_midsolid.asBool())
    {
      //const sector_t *sec = seg->backsector; //(!seg->side ? ldef->backsector : ldef->frontsector);
      VTexture *MTex = GTextureManager(sidedef->MidTexture);
      // here we should check if midtex covers the whole height, as it is not tiled vertically (if not wrapped)
      const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;
      // do more calculations only if the texture doesn't cover the whole wall
      float zOrg; // texture bottom
      if (ldef->flags&ML_DONTPEGBOTTOM) {
        // bottom of texture at bottom
        zOrg = max2(fsec->floor.TexZ, bsec->floor.TexZ);
      } else {
        // top of texture at top
        zOrg = min2(fsec->ceiling.TexZ, bsec->ceiling.TexZ)-texh;
      }
      const bool midWrapped = ((ldef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
      if (!midWrapped) {
        const float yofs = sidedef->Mid.RowOffset;
        if (yofs != 0.0f) {
          zOrg += yofs/(MTex->TextureTScale()/MTex->TextureOffsetTScale()*sidedef->Mid.ScaleY);
        }
      }
      // use proper checks for slopes here?
      float floorz, ceilz;
      if (bsec == ldef->frontsector) {
        floorz = min2(frontfz1, frontfz2);
        ceilz = max2(frontcz1, frontcz2);
      } else {
        floorz = min2(backfz1, backfz2);
        ceilz = max2(backcz1, backcz2);
      }
      if (zOrg <= floorz && (midWrapped || zOrg+texh >= ceilz)) return true; // fully covered
    }

    if (clip_height &&
        /*((seg->frontsector->SectorFlags|seg->backsector->SectorFlags)&sector_t::SF_IsTransDoor) == 0 &&*/ /* (checked above) don't do this for transdoors */
        (topTexType == VTextureManager::TCT_SOLID || botTexType == VTextureManager::TCT_SOLID) &&
        (bfpic != skyflatnum && ffpic != skyflatnum) && /* ignore skies */
        ((fsec->SectorFlags|bsec->SectorFlags)&sector_t::SF_HangingBridge) == 0 &&
        (lorg || (Frustum && Frustum->isValid())) &&
        seg->partner && seg->partner != seg &&
        seg->partner->frontsub && seg->partner->frontsub != seg->frontsub /*&&*/
        /*(!midTexType || !GTextureManager.IsSeeThrough(sidedef->MidTexture))*/
        /*(midTexType == VTextureManager::TCT_EMPTY || midTexType == VTextureManager::TCT_SOLID)*/ //k8: why?
        )
    {
      // here we can check if midtex is in frustum; if it doesn't,
      // we can add this seg to clipper.
      // this way, we can clip alot of things when camera looks at
      // floor/ceiling, and we can clip away too high/low windows.

      //TODO: use proper checks for slopes here
      // midhole quad
      TVec verts[4];
      verts[0] = TVec(vv1.x, vv1.y, max2(frontfz1, backfz1));
      verts[1] = TVec(vv1.x, vv1.y, min2(frontcz1, backcz1));
      verts[2] = TVec(vv2.x, vv2.y, min2(frontcz2, backcz2));
      verts[3] = TVec(vv2.x, vv2.y, max2(frontfz2, backfz2));

      if (botTexType == VTextureManager::TCT_SOLID &&
          topTexType == VTextureManager::TCT_SOLID &&
          min2(verts[0].z, verts[3].z) >= max2(verts[1].z, verts[2].z)) return true; // definitely closed

      if (Frustum && Frustum->isValid()) {
        // check if midtex is visible at all
        if (!Frustum->checkQuad(verts[0], verts[1], verts[2], verts[3])) {
          //return true;
          // midtex is invisible; yet we still have to check if top or bottom is transparent, and can be visible
          //bool checkPassed = true;

          if (botvisible && botTexType != VTextureManager::TCT_SOLID) {
            // create bottom pseudo-quad (back floor is higher)
            verts[0] = TVec(vv1.x, vv1.y, backfz1);
            verts[1] = TVec(vv1.x, vv1.y, frontfz1);
            verts[2] = TVec(vv2.x, vv2.y, frontfz2);
            verts[3] = TVec(vv2.x, vv2.y, backfz2);
            if (Frustum->checkQuad(verts[0], verts[1], verts[2], verts[3])) {
              // bottom is visible
              //GCon->Logf(NAME_Debug, "line #%d: BOTTOM IS VISIBLE (type=%d); backfz=(%g : %g); topfz=(%g : %g)", (int)(ptrdiff_t)(ldef-&level->Lines[0]), botTexType, backfz1, backfz2, frontfz1, frontfz2);
              //checkPassed = false;
              return false;
            }
          }

          if (/*checkPassed &&*/ topvisible && topTexType != VTextureManager::TCT_SOLID) {
            // create top pseudo-quad (front ceiling is higher)
            verts[0] = TVec(vv1.x, vv1.y, frontcz1);
            verts[1] = TVec(vv1.x, vv1.y, backcz1);
            verts[2] = TVec(vv2.x, vv2.y, backcz2);
            verts[3] = TVec(vv2.x, vv2.y, frontcz2);
            if (Frustum->checkQuad(verts[0], verts[1], verts[2], verts[3])) {
              // top is visible
              //GCon->Logf(NAME_Debug, "line #%d: TOP IS VISIBLE (type=%d); backcz=(%g : %g); frontcz=(%g : %g); dz1=%a (%s); dz2=%a (%s)", (int)(ptrdiff_t)(ldef-&level->Lines[0]), topTexType, backcz1, backcz2, frontcz1, frontcz2, backcz1-frontcz1, *VStr(backcz1-frontcz1), backcz2-frontcz2, *VStr(backcz2-frontcz2));
              //checkPassed = false;
              return false;
            }
          }

          //if (checkPassed) return true;
          return true;
        }
      }

      // check if light can touch midtex
      //TODO: check top/bottom visibility and transparency?
      //TODO: use proper checks for slopes here
      if (lorg) {
        // checking light
        if (!ldef->SphereTouches(*lorg, *lrad)) return true;
        // min
        float bbox[6];
        bbox[BOX3D_MINX] = min2(vv1.x, vv2.x);
        bbox[BOX3D_MINY] = min2(vv1.y, vv2.y);
        bbox[BOX3D_MINZ] = min2(min2(min2(frontfz1, backfz1), frontfz2), backfz2);
        // max
        bbox[BOX3D_MAXX] = max2(vv1.x, vv2.x);
        bbox[BOX3D_MAXY] = max2(vv1.y, vv2.y);
        bbox[BOX3D_MAXZ] = max2(max2(max2(frontcz1, backcz1), frontcz2), backcz2);
        FixBBoxZ(bbox);

        if (bbox[BOX3D_MINZ] >= bbox[BOX3D_MAXZ]) return true; // definitely closed

        if (!CheckSphereVsAABB(bbox, *lorg, *lrad)) return true; // cannot see midtex, can block
      }
    }
  }

  return false;
}



//==========================================================================
//
//  VViewClipper::VViewClipper
//
//==========================================================================
VViewClipper::VViewClipper () noexcept
  : FreeClipNodes(nullptr)
  , ClipHead(nullptr)
  , ClipTail(nullptr)
  , ClearClipNodesCalled(false)
  , RepSectors(nullptr)
{
}


//==========================================================================
//
//  VViewClipper::~VViewClipper
//
//==========================================================================
VViewClipper::~VViewClipper () noexcept {
  ClearClipNodes(TVec(), nullptr);
  VClipNode *Node = FreeClipNodes;
  while (Node) {
    VClipNode *Next = Node->Next;
    delete Node;
    Node = Next;
  }
}


//==========================================================================
//
//  VViewClipper::NewClipNode
//
//==========================================================================
VViewClipper::VClipNode *VViewClipper::NewClipNode () noexcept {
  VClipNode *res = FreeClipNodes;
  if (res) FreeClipNodes = res->Next; else res = new VClipNode();
  return res;
}


//==========================================================================
//
//  VViewClipper::RemoveClipNode
//
//==========================================================================
void VViewClipper::RemoveClipNode (VViewClipper::VClipNode *Node) noexcept {
  if (Node->Next) Node->Next->Prev = Node->Prev;
  if (Node->Prev) Node->Prev->Next = Node->Next;
  if (Node == ClipHead) ClipHead = Node->Next;
  if (Node == ClipTail) ClipTail = Node->Prev;
  Node->Next = FreeClipNodes;
  FreeClipNodes = Node;
}


//==========================================================================
//
//  VViewClipper::ClearClipNodes
//
//==========================================================================
void VViewClipper::ClearClipNodes (const TVec &AOrigin, VLevel *ALevel, float aradius) noexcept {
  if (ClipHead) {
    ClipTail->Next = FreeClipNodes;
    FreeClipNodes = ClipHead;
  }
  ClipHead = nullptr;
  ClipTail = nullptr;
  Origin = AOrigin;
  Level = ALevel;
  ClipResetFrustumPlanes();
  ClearClipNodesCalled = true;
  Radius = aradius;
}


//==========================================================================
//
//  VViewClipper::ClipResetFrustumPlanes
//
//==========================================================================
void VViewClipper::ClipResetFrustumPlanes () noexcept {
  Frustum.clear();
}


void VViewClipper::ClipInitFrustumPlanes (const TAVec &viewangles, const TVec &viewforward, const TVec &viewright, const TVec &viewup,
                                          const float fovx, const float fovy) noexcept
{
  if (clip_frustum && !viewright.z && isFiniteF(fovy) && fovy != 0 && isFiniteF(fovx) && fovx != 0) {
    // no view roll, create frustum
    TClipBase cb(fovx, fovy);
    TFrustumParam fp(Origin, viewangles, viewforward, viewright, viewup);
    Frustum.setup(cb, fp, true); // create back plane, no far plane
    /*
    Frustum.planes[TFrustum::Left].invalidate();
    Frustum.planes[TFrustum::Right].invalidate();
    Frustum.planes[TFrustum::Top].invalidate();
    Frustum.planes[TFrustum::Bottom].invalidate();
    Frustum.planes[TFrustum::Back].invalidate();
    Frustum.planes[TFrustum::Forward].invalidate();
    */
    /*
    // sanity check
    const TClipPlane *cp = &Frustum.planes[0];
    for (unsigned i = 0; i < 6; ++i, ++cp) {
      if (!cp->clipflag) continue; // don't need to clip against it
      if (cp->PointOnSide(Origin+viewforward)) GCon->Logf("invalid frustum plane #%u", i); // viewer is in back side or on plane (k8: why check this?)
    }
    */
  } else {
    ClipResetFrustumPlanes();
  }
  //ClipResetFrustumPlanes();
}


//==========================================================================
//
//  VViewClipper::ClipInitFrustumPlanes
//
//==========================================================================
void VViewClipper::ClipInitFrustumPlanes (const TAVec &viewangles, const float fovx, const float fovy) noexcept {
  TVec f, r, u;
  AngleVectors(viewangles, f, r, u);
  ClipInitFrustumPlanes(viewangles, f, r, u, fovx, fovy);
}


//==========================================================================
//
//  VViewClipper::ClipInitFrustumRange
//
//==========================================================================
void VViewClipper::ClipInitFrustumRange (const TAVec &viewangles, const TVec &viewforward,
                                         const TVec &viewright, const TVec &viewup,
                                         const float fovx, const float fovy) noexcept
{
  vassert(ClipIsEmpty());

  ClipInitFrustumPlanes(viewangles, viewforward, viewright, viewup, fovx, fovy);

  //if (viewforward.z > 0.9f || viewforward.z < -0.9f) return; // looking up or down, can see behind
  /*
  if (viewforward.z >= 0.985f || viewforward.z <= -0.985f) {
    // looking up or down, can see behind
    //return;
  }
  */

  ClearClipNodesCalled = false;
  Forward = viewforward;
  if (dbg_clip_dump_added_ranges) GCon->Log("==== ClipInitFrustumRange ===");

  if (!clip_frustum) return;
  if (!clip_frustum_init_range) return;

  TVec Pts[4];
  TVec TransPts[4];
  Pts[0] = TVec(fovx, fovy, 1.0f);
  Pts[1] = TVec(fovx, -fovy, 1.0f);
  Pts[2] = TVec(-fovx, fovy, 1.0f);
  Pts[3] = TVec(-fovx, -fovy, 1.0f);
  TVec clipforward(viewforward.x, viewforward.y, 0.0f);
  //k8: i don't think that we need to normalize it, but...
  clipforward.normaliseInPlace();

  // pseudoangles are not linear, so use real angles here
  const VFloat fwdAngle = viewangles.yaw;
  VFloat d1 = (VFloat)0;
  VFloat d2 = (VFloat)0;

  for (unsigned i = 0; i < 4; ++i) {
    TransPts[i].x = VSUM3(Pts[i].x*viewright.x, Pts[i].y*viewup.x, /*Pts[i].z* */viewforward.x);
    TransPts[i].y = VSUM3(Pts[i].x*viewright.y, Pts[i].y*viewup.y, /*Pts[i].z* */viewforward.y);
    TransPts[i].z = VSUM3(Pts[i].x*viewright.z, Pts[i].y*viewup.z, /*Pts[i].z* */viewforward.z);

    if (TransPts[i].dot(clipforward) <= 0.01f) { // was 0.0f
      //GCon->Logf(NAME_Debug, "  ooops; pitch=%g", viewangles.pitch);
      // player can see behind, use back frustum plane to clip
      return;
    }

    // pseudoangles are not linear
    VFloat d = VVC_AngleMod180(PointToRealAngleZeroOrigin(TransPts[i].x, TransPts[i].y)-fwdAngle);

    if (d1 > d) d1 = d;
    if (d2 < d) d2 = d;
  }

  if (d1 != d2) {
    // this prevents some clipping bugs (i hope) with zdbsp
    d1 -= 2;
    d2 += 2;
    VFloat a1 = fwdAngle+d1;
    VFloat a2 = fwdAngle+d2;
    //a1 = VVC_AngleMod(a1);
    //a2 = VVC_AngleMod(a2);
    auto na1 = AngleToClipperAngle(a1);
    auto na2 = AngleToClipperAngle(a2);

    if (na1 > na2) {
      ClipHead = NewClipNode();
      ClipTail = ClipHead;
      ClipHead->From = na2;
      ClipHead->To = na1;
      ClipHead->Prev = nullptr;
      ClipHead->Next = nullptr;
    } else {
      ClipHead = NewClipNode();
      ClipHead->From = 0;
      ClipHead->To = na1;
      ClipTail = NewClipNode();
      ClipTail->From = na2;
      ClipTail->To = MAX_ANGLE;
      ClipHead->Prev = nullptr;
      ClipHead->Next = ClipTail;
      ClipTail->Prev = ClipHead;
      ClipTail->Next = nullptr;
    }
    #ifdef XXX_CLIPPER_DUMP_ADDED_RANGES
    if (dbg_clip_dump_added_ranges) { GCon->Log("=== FRUSTUM ==="); Dump(); GCon->Log("---"); }
    #endif
  }
}


//==========================================================================
//
//  VViewClipper::ClipInitFrustumRange
//
//==========================================================================
void VViewClipper::ClipInitFrustumRange (const TAVec &viewangles, const float fovx, const float fovy) noexcept {
  TVec f, r, u;
  AngleVectors(viewangles, f, r, u);
  ClipInitFrustumRange(viewangles, f, r, u, fovx, fovy);
}


//==========================================================================
//
//  VViewClipper::ClipToRanges
//
//==========================================================================
void VViewClipper::ClipToRanges (const VViewClipper &Range) noexcept {
  if (&Range == this) return; // just in case

  if (!Range.ClipHead) {
    // no ranges, everything is clipped away
    //DoAddClipRange((VFloat)0, (VFloat)360);
    /*
    // remove free clip nodes
    VClipNode *Node = FreeClipNodes;
    while (Node) {
      VClipNode *Next = Node->Next;
      delete Node;
      Node = Next;
    }
    FreeClipNodes = nullptr;
    // remove used clip nodes
    Node = ClipHead;
    while (Node) {
      VClipNode *Next = Node->Next;
      delete Node;
      Node = Next;
    }
    */
    // remove used clip nodes
    if (ClipHead) {
      ClipTail->Next = FreeClipNodes;
      FreeClipNodes = ClipHead;
    }
    ClipHead = ClipTail = nullptr;
    // add new clip node
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = MIN_ANGLE;
    ClipHead->To = MAX_ANGLE;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
    return;
  }

  // add head and tail ranges
  if (Range.ClipHead->From > MIN_ANGLE) DoAddClipRange(MIN_ANGLE, Range.ClipHead->From);
  if (Range.ClipTail->To < MAX_ANGLE) DoAddClipRange(Range.ClipTail->To, MAX_ANGLE);

  // add middle ranges
  for (VClipNode *N = Range.ClipHead; N->Next; N = N->Next) DoAddClipRange(N->To, N->Next->From);
}


//==========================================================================
//
//  VViewClipper::Dump
//
//==========================================================================
void VViewClipper::Dump () const noexcept {
  GCon->Logf("=== CLIPPER: origin=(%f,%f,%f) ===", Origin.x, Origin.y, Origin.z);
#ifdef VV_CLIPPER_FULL_CHECK
  GCon->Logf(" full: %d", (int)ClipIsFull());
#endif
  GCon->Logf(" empty: %d", (int)ClipIsEmpty());
  for (VClipNode *node = ClipHead; node; node = node->Next) {
#ifdef VAVOOM_CLIPPER_USE_PSEUDO_INT
    GCon->Logf("  node: (0x%08x : 0x%08x)", node->From, node->To);
#else
    GCon->Logf("  node: (%f : %f)", node->From, node->To);
#endif
  }
}


//==========================================================================
//
//  VViewClipper::DoAddClipRange
//
//  returns `true` if clip range was modified
//
//==========================================================================
bool VViewClipper::DoAddClipRange (FromTo From, FromTo To) noexcept {
  #ifndef VAVOOM_CLIPPER_NO_ANGLE_CLAMP
  if (From < MIN_ANGLE) From = MIN_ANGLE; else if (From > MAX_ANGLE) From = MAX_ANGLE;
  if (To < MIN_ANGLE) To = MIN_ANGLE; else if (To > MAX_ANGLE) To = MAX_ANGLE;
  #endif

  if (ClipIsEmpty()) {
    ClipHead = NewClipNode();
    ClipTail = ClipHead;
    ClipHead->From = From;
    ClipHead->To = To;
    ClipHead->Prev = nullptr;
    ClipHead->Next = nullptr;
    return true;
  }

  bool res = false;
  for (VClipNode *Node = ClipHead; Node; Node = Node->Next) {
    if (Node->To < From) continue; // before this range

    if (To < Node->From) {
      // insert a new clip range before the current one
      RNGLOG(" +++ inserting (%f : %f) before (%f : %f)", From, To, Node->From, Node->To);
      VClipNode *N = NewClipNode();
      N->From = From;
      N->To = To;
      N->Prev = Node->Prev;
      N->Next = Node;

      if (Node->Prev) Node->Prev->Next = N; else ClipHead = N;
      Node->Prev = N;

      return true;
    }

    if (Node->From <= From && Node->To >= To) {
      // it contains this range
      RNGLOG(" +++ hidden (%f : %f) with (%f : %f)", From, To, Node->From, Node->To);
      return res;
    }

    if (From < Node->From) {
      // extend start of the current range
      RNGLOG(" +++ using (%f : %f) to extend head of (%f : %f)", From, To, Node->From, Node->To);
      Node->From = From;
      res = true;
    }

    if (To <= Node->To) {
      // end is included, so we are done here
      RNGLOG(" +++ hidden (end) (%f : %f) with (%f : %f)", From, To, Node->From, Node->To);
      return res;
    }

    // merge with following nodes if needed
    while (Node->Next && Node->Next->From <= To) {
      RNGLOG(" +++ merger (%f : %f) eat (%f : %f)", From, To, Node->From, Node->To);
      Node->To = Node->Next->To;
      RemoveClipNode(Node->Next);
      res = true;
    }

    if (To > Node->To) {
      // extend end
      RNGLOG(" +++ using (%f : %f) to extend end of (%f : %f)", From, To, Node->From, Node->To);
      Node->To = To;
      res = true;
    }

    // we are done here
    return res;
  }

  RNGLOG(" +++ appending (%f : %f) to the end (%f : %f)", From, To, ClipTail->From, ClipTail->To);

  // if we are here it means it's a new range at the end
  VClipNode *NewTail = NewClipNode();
  NewTail->From = From;
  NewTail->To = To;
  NewTail->Prev = ClipTail;
  NewTail->Next = nullptr;
  ClipTail->Next = NewTail;
  ClipTail = NewTail;
  return true;
}


//==========================================================================
//
//  VViewClipper::AddClipRange
//
//==========================================================================
bool VViewClipper::AddClipRangeAngle (const FromTo From, const FromTo To) noexcept {
  if (From > To) {
    bool res = DoAddClipRange(MIN_ANGLE, To);
    if (DoAddClipRange(From, MAX_ANGLE)) res = true;
    return res;
  } else {
    return DoAddClipRange(From, To);
  }
}


//==========================================================================
//
//  VViewClipper::DoRemoveClipRange
//
//  NOT TESTED!
//
//==========================================================================
void VViewClipper::DoRemoveClipRange (FromTo From, FromTo To) noexcept {
  #ifndef VAVOOM_CLIPPER_NO_ANGLE_CLAMP
  if (From < MIN_ANGLE) From = MIN_ANGLE; else if (From > MAX_ANGLE) From = MAX_ANGLE;
  if (To < MIN_ANGLE) To = MIN_ANGLE; else if (To > MAX_ANGLE) To = MAX_ANGLE;
  #endif

  if (ClipHead) {
    // check to see if range contains any old ranges
    VClipNode *node = ClipHead;
    while (node != nullptr && node->From < To) {
      if (node->From >= From && node->To <= To) {
        VClipNode *temp = node;
        node = node->Next;
        RemoveClipNode(temp);
      } else {
        node = node->Next;
      }
    }

    // check to see if range overlaps a range (or possibly 2)
    node = ClipHead;
    while (node) {
      if (node->From >= From && node->From <= To) {
        node->From = To;
        break;
      } else if (node->To >= From && node->To <= To) {
        node->To = From;
      } else if (node->From < From && node->To > To) {
        // split node
        VClipNode *temp = NewClipNode();
        temp->From = To;
        temp->To = node->To;
        node->To = From;
        temp->Next = node->Next;
        temp->Prev = node;
        node->Next = temp;
        if (temp->Next) temp->Next->Prev = temp;
        break;
      }
      node = node->Next;
    }
  }
}


//==========================================================================
//
//  VViewClipper::RemoveClipRange
//
//==========================================================================
void VViewClipper::RemoveClipRangeAngle (const FromTo From, const FromTo To) noexcept {
  if (From > To) {
    DoRemoveClipRange(MIN_ANGLE, To);
    DoRemoveClipRange(From, MAX_ANGLE);
  } else {
    DoRemoveClipRange(From, To);
  }
}


//==========================================================================
//
//  VViewClipper::DoIsRangeVisible
//
//==========================================================================
bool VViewClipper::DoIsRangeVisible (const FromTo From, const FromTo To) const noexcept {
  for (const VClipNode *N = ClipHead; N; N = N->Next) {
    if (From >= N->From && To <= N->To) return false;
  }
  return true;
}


//==========================================================================
//
//  VViewClipper::IsRangeVisible
//
//==========================================================================
bool VViewClipper::IsRangeVisibleAngle (const FromTo From, const FromTo To) const noexcept {
  if (From > To) return (DoIsRangeVisible(MIN_ANGLE, To) || DoIsRangeVisible(From, MAX_ANGLE));
  return DoIsRangeVisible(From, To);
}


//==========================================================================
//
//  CreateBBVerts
//
//  `origin` MUST NOT be inside the bbox
//
//==========================================================================
inline static void CreateBBVerts (TVec &v1, TVec &v2, const float bbox[6], const TVec &origin) noexcept {
  if (bbox[BOX3D_MINX] > origin.x) {
    if (bbox[BOX3D_MINY] > origin.y) {
      v1.x = bbox[BOX3D_MAXX];
      v1.y = bbox[BOX3D_MINY];
      v2.x = bbox[BOX3D_MINX];
      v2.y = bbox[BOX3D_MAXY];
    } else if (bbox[BOX3D_MAXY] < origin.y) {
      v1.x = bbox[BOX3D_MINX];
      v1.y = bbox[BOX3D_MINY];
      v2.x = bbox[BOX3D_MAXX];
      v2.y = bbox[BOX3D_MAXY];
    } else {
      v1.x = bbox[BOX3D_MINX];
      v1.y = bbox[BOX3D_MINY];
      v2.x = bbox[BOX3D_MINX];
      v2.y = bbox[BOX3D_MAXY];
    }
  } else if (bbox[BOX3D_MAXX] < origin.x) {
    if (bbox[BOX3D_MINY] > origin.y) {
      v1.x = bbox[BOX3D_MAXX];
      v1.y = bbox[BOX3D_MAXY];
      v2.x = bbox[BOX3D_MINX];
      v2.y = bbox[BOX3D_MINY];
    } else if (bbox[BOX3D_MAXY] < origin.y) {
      v1.x = bbox[BOX3D_MINX];
      v1.y = bbox[BOX3D_MAXY];
      v2.x = bbox[BOX3D_MAXX];
      v2.y = bbox[BOX3D_MINY];
    } else {
      v1.x = bbox[BOX3D_MAXX];
      v1.y = bbox[BOX3D_MAXY];
      v2.x = bbox[BOX3D_MAXX];
      v2.y = bbox[BOX3D_MINY];
    }
  } else {
    if (bbox[BOX3D_MINY] > origin.y) {
      v1.x = bbox[BOX3D_MAXX];
      v1.y = bbox[BOX3D_MINY];
      v2.x = bbox[BOX3D_MINX];
      v2.y = bbox[BOX3D_MINY];
    } else {
      v1.x = bbox[BOX3D_MINX];
      v1.y = bbox[BOX3D_MAXY];
      v2.x = bbox[BOX3D_MAXX];
      v2.y = bbox[BOX3D_MAXY];
    }
  }
  v1.z = v2.z = 0.0f;
}


//==========================================================================
//
//  VViewClipper::CheckSubsectorFrustum
//
//==========================================================================
/*
int VViewClipper::CheckSubsectorFrustum (subsector_t *sub, const unsigned mask) const noexcept {
  if (!sub || !Frustum.isValid() || !mask) return 1;
  float bbox[6];
  Level->GetSubsectorBBox(sub, bbox);

  if (bbox[0] <= Origin.x && bbox[3] >= Origin.x &&
      bbox[1] <= Origin.y && bbox[4] >= Origin.y &&
      bbox[2] <= Origin.z && bbox[5] >= Origin.z)
  {
    // viewer is inside the box
    return 1;
  }

  // check
  return Frustum.checkBoxEx(bbox, clip_frustum_check_mask&mask);
}
*/


//==========================================================================
//
//  VViewClipper::CheckSegFrustum
//
//==========================================================================
/*
bool VViewClipper::CheckSegFrustum (const subsector_t *sub, const seg_t *seg, const unsigned mask) const noexcept {
  if (!clip_frustum_bsp_segs.asBool()) return true;
  //const subsector_t *sub = seg->frontsub;
  if (!seg || !sub || !Frustum.isValid() || !mask) return true;
  const sector_t *sector = sub->sector;
  if (!sector) return true; // just in case
  //FIXME: ignore transparent doors (because their bounding box is wrong)
  if (sub->sector && (sub->sector->SectorFlags&sector_t::SF_IsTransDoor)) return true;
  const line_t *ldef = seg->linedef;
  if (ldef) {
    if ((ldef->flags&ML_ADDITIVE) != 0 || ldef->alpha < 1.0f) return true; // skip translucent walls
  }
  // check quad
  const TVec sv0(seg->v1->x, seg->v1->y, sector->floor.GetPointZ(*seg->v1));
  const TVec sv1(seg->v1->x, seg->v1->y, sector->ceiling.GetPointZ(*seg->v1));
  const TVec sv2(seg->v2->x, seg->v2->y, sector->ceiling.GetPointZ(*seg->v2));
  const TVec sv3(seg->v2->x, seg->v2->y, sector->floor.GetPointZ(*seg->v2));
  return Frustum.checkQuad(sv0, sv1, sv2, sv3, mask);
}
*/


//==========================================================================
//
//  VViewClipper::ClipIsBBoxVisible
//
//==========================================================================
bool VViewClipper::ClipIsBBoxVisible (const float bbox[6]) const noexcept {
  if (!clip_enabled || !clip_bbox) return true;
  if (ClipIsEmpty()) return true; // no clip nodes yet
  if (ClipIsFull()) return false;

  if (bbox[BOX3D_MINX] <= Origin.x && bbox[BOX3D_MAXX] >= Origin.x &&
      bbox[BOX3D_MINY] <= Origin.y && bbox[BOX3D_MAXY] >= Origin.y &&
      bbox[BOX3D_MINZ] <= Origin.z && bbox[BOX3D_MAXZ] >= Origin.z)
  {
    // viewer is inside the box
    return true;
  }

  TVec v1, v2;
  CreateBBVerts(v1, v2, bbox, Origin);
  return IsRangeVisible(v1, v2);
}


//==========================================================================
//
//  VViewClipper::ClipAddLine
//
//==========================================================================
void VViewClipper::ClipAddLine (const TVec v1, const TVec v2) noexcept {
  if (v1.x == v2.x && v1.y == v2.y) return;
  TPlane pl;
  pl.SetPointDirXY(v1, v2-v1); // for boxes, this doesn't even do sqrt
  // viewer is in back side or on plane?
  const int orgside = ClipPointOnSide2(pl, Origin);
  if (orgside) return; // origin is on plane, or on back
  AddClipRange(v2, v1);
}


//==========================================================================
//
//  VViewClipper::ClipAddBBox
//
//==========================================================================
void VViewClipper::ClipAddBBox (const float bbox[6]) noexcept {
  if (bbox[0] >= bbox[0+3] || bbox[1] >= bbox[1+3]) return;
  // if we are inside this bbox, don't add it
  if (bbox[0] <= Origin.x && bbox[0+3] >= Origin.x &&
      bbox[1] <= Origin.y && bbox[1+3] >= Origin.y)
  {
    // viewer is inside the box
    return;
  }
  ClipAddLine(TVec(bbox[0+0], bbox[1+0]), TVec(bbox[0+3], bbox[1+0]));
  ClipAddLine(TVec(bbox[0+0], bbox[1+3]), TVec(bbox[0+3], bbox[1+3]));
  ClipAddLine(TVec(bbox[0+0], bbox[1+0]), TVec(bbox[0+0], bbox[1+3]));
  ClipAddLine(TVec(bbox[0+3], bbox[1+0]), TVec(bbox[0+3], bbox[1+3]));
}


//==========================================================================
//
//  VViewClipper::ClipCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipCheckSubsector (const subsector_t *sub) noexcept {
  if (!clip_enabled) return true;
  if (ClipIsFull()) {
    SBCLOG("  ::: sub #%d: clip is full", (int)(ptrdiff_t)(sub-Level->Subsectors));
    return false;
  }
  #ifdef XXX_CLIPPER_DUMP_ADDED_RANGES
  if (dbg_clip_dump_sub_checks) Dump();
  #endif

  /*
  if (checkBBox) {
    float bbox[6];
    Level->GetSubsectorBBox(sub, bbox);
    if (!ClipIsBBoxVisible(bbox)) return false;
  }
  */

  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    //k8: q: i am not sure here, but why don't check both sides?
    //    a: because differently oriented side is not visible in any case
    const int orgside = ClipPointOnSide2(*seg, Origin);
    if (orgside) {
      if (orgside == 2) {
        // if origin is on the seg, it doesn't matter how we'll count it
        // let's say that it is visible
        SBCLOG("  ::: sub #%d: seg #%u contains origin", (int)(ptrdiff_t)(sub-Level->Subsectors), (unsigned)(ptrdiff_t)(seg-Level->Segs));
        return true;
      }
      continue; // viewer is in back side
    }
    const TVec &v1 = *seg->v1;
    const TVec &v2 = *seg->v2;
    if (IsRangeVisible(v2, v1)) {
      SBCLOG("  ::: sub #%d: visible seg #%u (%f : %f) (vis=%d)", (int)(ptrdiff_t)(sub-Level->Subsectors), (unsigned)(ptrdiff_t)(seg-Level->Segs), PointToClipAngle(v2), PointToClipAngle(v1), IsRangeVisible(v2, v1));
      return true;
    }
    SBCLOG("  ::: sub #%d: invisible seg #%u (%f : %f)", (int)(ptrdiff_t)(sub-Level->Subsectors), (unsigned)(ptrdiff_t)(seg-Level->Segs), PointToClipAngle(v2), PointToClipAngle(v1));
  }
  SBCLOG("  ::: sub #%d: no visible segs", (int)(ptrdiff_t)(sub-Level->Subsectors));
  return false;
}


//==========================================================================
//
//  MirrorCheck
//
//==========================================================================
static inline bool MirrorCheck (const TPlane *Mirror, const TVec &v1, const TVec &v2) noexcept {
  if (Mirror) {
    // clip seg with mirror plane
    //if (Mirror->PointOnSideThreshold(v1) && Mirror->PointOnSideThreshold(v2)) return false;
    if (ClipPointOnSide2(*Mirror, v1) || ClipPointOnSide2(*Mirror, v2)) return false;
    // and clip it while we are here
    //const float dist1 = v1.dot(Mirror->normal)-Mirror->dist;
    //const float dist2 = v2.dot(Mirror->normal)-Mirror->dist;
    // k8: really?
    /*
         if (dist1 > 0.0f && dist2 <= 0.0f) v2 = v1+(v2-v1)*Dist1/(Dist1-Dist2);
    else if (dist2 > 0.0f && dist1 <= 0.0f) v1 = v2+(v1-v2)*Dist2/(Dist2-Dist1);
    */
  }
  return true;
}


//==========================================================================
//
//  VViewClipper::ClipCheckAddSubsector
//
//  this returns `true` if clip was modified
//
//  THIS DOESN'T WORK RIGHT, AND IS NOT USED!
//
//==========================================================================
#if 0
bool VViewClipper::ClipCheckAddSubsector (const subsector_t *sub, const TPlane *Mirror) noexcept {
  if (!clip_enabled) return true;
  if (ClipIsFull()) return false;

  const bool addBackface = clip_add_backface_segs.asBool();
  //const bool slopes1s = clip_skip_slopes_1side.asBool();

  bool res = false;

  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    const line_t *ldef = seg->linedef;

    //k8: q: i am not sure here, but why don't check both sides?
    //    a: because differently oriented side is not visible in any case
    const int orgside = ClipPointOnSide2(*seg, Origin);
    if (orgside) {
      if (orgside == 2) {
        // origin is on plane
        // if this seg faces in the direction different from camera view direction,
        // we can ignore it; otherwise, this subsector should be considered visible
        // to check this, we will move origin back a little, and check the side
        // here, if seg is "in front", that means that it is not interested
        // (because we moved inside another subsector)
        //res = true;
        if (!res) res = (seg->PointOnSide(Origin-Forward*10.0f) != 0);
        continue;
      } else {
        if (!addBackface || !seg->partner) {
          // viewer is in back side; we still need to check visibility to render floors
          if (!res) {
            res = IsRangeVisible(*seg->v1, *seg->v2);
            //if (res && !CheckSegFrustum(sub, seg)) res = false; // out of frustum
          }
          continue;
        }
        // if we have a partner seg, use it, so midtex clipper can do it right
        // without this, fake walls clips out everything
        seg = seg->partner;
      }
    }

    const TVec &v1 = *seg->v1;
    const TVec &v2 = *seg->v2;

    // minisegs should still be checked
    if (!ldef) {
      if (!res) {
        res = IsRangeVisible(v2, v1);
        //if (res && !CheckSegFrustum(sub, seg)) res = false; // out of frustum
      }
      continue;
    }

    /*
    if (slopes1s) {
      // do not clip with slopes, if it has no midtex
      if (ldef->frontsector && (ldef->flags&ML_TWOSIDED) == 0) {
        int fcpic, ffpic;
        TPlane ffplane, fcplane;
        CopyHeight(ldef->frontsector, &ffplane, &fcplane, &ffpic, &fcpic);
        // only apply this to sectors without slopes
        if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return;
      }
    }
    */

    if (!MirrorCheck(Mirror, v1, v2)) continue;

    // for 2-sided line, determine if it can be skipped
    if (seg->backsector && (ldef->flags&ML_TWOSIDED)) {
      const bool solid = (!RepSectors ? IsSegAClosedSomething(Level, &Frustum, seg) : IsSegAClosedSomethingServer(Level, RepSectors, seg));
      if (!solid) {
        if (!res) {
          res = IsRangeVisible(v2, v1);
          //if (res && !CheckSegFrustum(sub, seg)) res = false; // out of frustum
        }
        continue;
      }
    }

    // add to range
    if (!res) {
      res = AddClipRange(v2, v1);
      //if (res && !CheckSegFrustum(sub, seg)) res = false; // out of frustum
    } else {
      AddClipRange(v2, v1);
    }
  }

  return res;
}
#endif


//==========================================================================
//
//  VViewClipper::CheckAddClipSeg
//
//==========================================================================
void VViewClipper::CheckAddClipSeg (const seg_t *seg, const TPlane *Mirror, bool clipAll) noexcept {
  const line_t *ldef = seg->linedef;
  if (!clipAll && !ldef) return; // miniseg

  // viewer is in back side or on plane?
  int orgside = ClipPointOnSide2(*seg, Origin);
  if (orgside == 2) return; // origin is on plane
  // add differently oriented segs too
  if (orgside) {
    if (!clip_add_backface_segs) return;
    // if we have a partner seg, use it, so midtex clipper can do it right
    // without this, fake walls clips out everything
    if (seg->partner) { seg = seg->partner; orgside = 0; }
  }

  if (clip_skip_slopes_1side) {
    // do not clip with slopes, if it has no midtex
    if (ldef->frontsector && (ldef->flags&ML_TWOSIDED) == 0) {
      int fcpic, ffpic;
      TPlane ffplane, fcplane;
      CopyHeight(ldef->frontsector, &ffplane, &fcplane, &ffpic, &fcpic);
      // only apply this to sectors without slopes
      if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return;
    }
  }

  const TVec &v1 = (!orgside ? *seg->v1 : *seg->v2);
  const TVec &v2 = (!orgside ? *seg->v2 : *seg->v1);

  if (!MirrorCheck(Mirror, v1, v2)) return;

  // for 2-sided line, determine if it can be skipped
  if (!clipAll && seg->backsector && (ldef->flags&ML_TWOSIDED)) {
    if (!RepSectors) {
      if (!IsSegAClosedSomething(Level, &Frustum, seg)) return;
    } else {
      if (!IsSegAClosedSomethingServer(Level, RepSectors, seg)) return;
    }
  }

  RNGLOG("added seg #%d (line #%d); range=(%f : %f); vis=%d : (%g,%g)-(%g,%g)",
    (int)(ptrdiff_t)(seg-Level->Segs),
    (int)(ptrdiff_t)(ldef-Level->Lines),
    PointToClipAngle(v2), PointToClipAngle(v1),
    (int)IsRangeVisible(v2, v1), v1.x, v1.y, v2.x, v2.y);

  AddClipRange(v2, v1);
}


//==========================================================================
//
//  VViewClipper::CheckAddPObjClipSeg
//
//==========================================================================
void VViewClipper::CheckAddPObjClipSeg (polyobj_t *pobj, const subsector_t *sub, const seg_t *seg, const TPlane *Mirror, bool /*clipAll*/) noexcept {
  // viewer is in back side or on plane?
  int orgside = ClipPointOnSide2(*seg, Origin);
  if (orgside == 2) return; // origin is on plane

  // add differently oriented segs too
  if (orgside) {
    if (!clip_add_backface_segs) return;
    // if we have a partner seg, use it, so midtex clipper can do it right
    // without this, fake walls clips out everything
    if (seg->partner) { seg = seg->partner; orgside = 0; }
  }

  const TVec &v1 = (!orgside ? *seg->v1 : *seg->v2);
  const TVec &v2 = (!orgside ? *seg->v2 : *seg->v1);

  if (!MirrorCheck(Mirror, v1, v2)) return;

  // treat each line as two-sided
  if (!RepSectors) {
    if (!IsPObjSegAClosedSomething(Level, &Frustum, pobj, sub, seg)) return;
  } else {
    if (!IsPObjSegAClosedSomethingServer(Level, &Frustum, pobj, sub, seg)) return;
  }

  AddClipRange(v2, v1);
}


//==========================================================================
//
//  VViewClipper::ClipAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipAddSubsectorSegs (const subsector_t *sub, const TPlane *Mirror, bool clipAll) noexcept {
  if (!clip_enabled) return;
  if (ClipIsFull()) return;

  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    //if (doPoly && doCheck && !IsGoodSegForPoly(*this, seg)) doPoly = false;
    CheckAddClipSeg(seg, Mirror, clipAll);
  }
}


//==========================================================================
//
//  VViewClipper::ClipAddPObjSegs
//
//==========================================================================
void VViewClipper::ClipAddPObjSegs (const subsector_t *sub, const TPlane *Mirror, bool clipAll) noexcept {
  if (!clip_enabled) return;
  if (ClipIsFull()) return;

  if (!sub->HasPObjs() || !clip_with_polyobj.asBool()) return;

  for (auto &&it : sub->PObjFirst()) {
    polyobj_t *pobj = it.pobj();
    // do not clip with slopes (yet)
    if (pobj->pofloor.isSlope() || pobj->poceiling.isSlope()) continue;
    // do not clip with zero-height polyobjects
    if (pobj->pofloor.maxz >= pobj->poceiling.minz) return;
    // process clip segs
    it.part()->EnsureClipSegs(Level);
    for (auto &&sit : it.part()->SegFirst()) {
      const seg_t *seg = sit.seg();
      CheckAddPObjClipSeg(pobj, sub, seg, Mirror, clipAll);
    }
  }
}


#ifdef CLIENT
//==========================================================================
//
//  VViewClipper::CheckSubsectorLight
//
//  returns:
//    0: the light is outside
//    1: the box is fully inside the light
//   -1: the light intersects the box, or the light is fully inside the box
//
//==========================================================================
int VViewClipper::CheckSubsectorLight (subsector_t *sub) const noexcept {
  if (!sub) return 0;
  float bbox[6];
  Level->GetSubsectorBBox(sub, bbox);

  if (bbox[BOX3D_MINX] >= Origin.x-Radius && bbox[BOX3D_MAXX] <= Origin.x+Radius &&
      bbox[BOX3D_MINY] >= Origin.y-Radius && bbox[BOX3D_MAXY] <= Origin.y+Radius &&
      bbox[BOX3D_MINZ] >= Origin.z-Radius && bbox[BOX3D_MAXZ] <= Origin.z+Radius)
  {
    // the box is fully inside the light
    return 1;
  }

  if (bbox[BOX3D_MINX] <= Origin.x-Radius && bbox[BOX3D_MAXX] >= Origin.x+Radius &&
      bbox[BOX3D_MINY] <= Origin.y-Radius && bbox[BOX3D_MAXY] >= Origin.y+Radius &&
      bbox[BOX3D_MINZ] <= Origin.z-Radius && bbox[BOX3D_MAXZ] >= Origin.z+Radius)
  {
    // the light is fully inside the box
    return -1;
  }

  // if light origin is inside the box, we have an intersection
  if (bbox[BOX3D_MINX] <= Origin.x && bbox[BOX3D_MAXX] >= Origin.x &&
      bbox[BOX3D_MINY] <= Origin.y && bbox[BOX3D_MAXY] >= Origin.y &&
      bbox[BOX3D_MINZ] <= Origin.z && bbox[BOX3D_MAXZ] >= Origin.z)
  {
    // viewer is inside the box
    return -1;
  }

  if (!CheckSphereVsAABB(bbox, Origin, Radius)) return 0; // cannot touch

  #if 1
  // the light is not fully inside the box, but intersects it
  return -1;
  #else
  // check if all box vertices are inside a sphere
  // early exit if sphere is smaller than bbox
  if (Radius < bbox[BOX3D_MAXX]-bbox[BOX3D_MINX]) return -1;
  if (Radius < bbox[BOX3D_MAXY]-bbox[BOX3D_MINY]) return -1;
  if (Radius < bbox[BOX3D_MAXZ]-bbox[BOX3D_MINZ]) return -1;

  const float xradSq = Radius*Radius;

  for (unsigned bidx = 0; bidx < MAX_BBOX3D_CORNERS; ++bidx) {
    const TVec bv(GetBBox3DCorner(bidx, bbox)-Origin);
    if (bv.lengthSquared() > xradSq) return -1; // partially inside, because at least one corner is not in the sphere
  }

  // fully inside
  return 1;
  #endif
}


//==========================================================================
//
//  VViewClipper::ClipLightIsBBoxVisible
//
//==========================================================================
bool VViewClipper::ClipLightIsBBoxVisible (const float bbox[6]) const noexcept {
  if (ClipIsFull()) return false;
  if (ClipIsEmpty()) return true; // no clip nodes yet

  if (bbox[BOX3D_MINX] <= Origin.x && bbox[BOX3D_MAXX] >= Origin.x &&
      bbox[BOX3D_MINY] <= Origin.y && bbox[BOX3D_MAXY] >= Origin.y /*&&
      bbox[BOX3D_MINZ] <= Origin.z && bbox[BOX3D_MAXZ] >= Origin.z*/)
  {
    // viewer is inside the box
    return true;
  }

  if (!CheckSphereVsAABBIgnoreZ(bbox, Origin, Radius)) return false;

  TVec v1, v2;
  CreateBBVerts(v1, v2, bbox, Origin);
  return IsRangeVisible(v1, v2);
}


//==========================================================================
//
//  VViewClipper::ClipLightCheckRegion
//
//==========================================================================
/*
bool VViewClipper::ClipLightCheckRegion (const subregion_t *region, subsector_t *sub, int asShadow) const noexcept {
  if (ClipIsFull()) return false;
  const int slight = CheckSubsectorLight(sub);
  if (!slight) return false;
  if (slight > 0 && ClipIsEmpty()) return true; // no clip nodes yet
  const drawseg_t *ds = region->lines;
  for (auto count = sub->numlines; count--; ++ds) {
    if (!ds->seg) continue; // just in case
    if (slight < 0) {
      if (!ds->seg->SphereTouches(Origin, Radius)) continue;
    }
    // we have to check even "invisible" segs here, 'cause we need them all
    const TVec *v1, *v2;
    const int orgside = ds->seg->PointOnSide2(Origin);
    if (orgside == 2) return true; // origin is on plane, we cannot do anything sane
    if (orgside) {
      if (!asShadow) continue;
      v1 = ds->seg->v2;
      v2 = ds->seg->v1;
    } else {
      v1 = ds->seg->v1;
      v2 = ds->seg->v2;
    }
    if (IsRangeVisible(*v2, *v1)) return true;
  }
  return false;
}
*/


//==========================================================================
//
//  VViewClipper::ClipLightCheckSeg
//
//  this doesn't do raduis and subsector checks: this is done in
//  `CalcLightVisCheckNode()`
//
//==========================================================================
bool VViewClipper::ClipLightCheckSeg (const seg_t *seg, int /*asShadow*/) const noexcept {
  if (ClipIsEmpty()) return true; // no clip nodes yet
  if (!seg->SphereTouches(Origin, Radius)) return false;
  // we have to check even "invisible" segs here, 'cause we need them all
  const TVec *v1, *v2;
  const int orgside = ClipPointOnSide2(*seg, Origin);
  if (orgside == 2) return true; // origin is on plane, we cannot do anything sane
  if (orgside) {
    //if (!asShadow) return false;
    v1 = seg->v2;
    v2 = seg->v1;
  } else {
    v1 = seg->v1;
    v2 = seg->v2;
  }
  return IsRangeVisible(*v2, *v1);
}


//==========================================================================
//
//  VViewClipper::ClipLightCheckSubsector
//
//==========================================================================
bool VViewClipper::ClipLightCheckSubsector (subsector_t *sub, int /*asShadow*/) const noexcept {
  if (ClipIsFull()) return false;
  const int slight = CheckSubsectorLight(sub);
  if (!slight) return false;
  if (slight > 0 && ClipIsEmpty()) return true; // no clip nodes yet, and the box is fully inside the light
  // here we may not touch any seg, but still touch floor/ceiling
  // check for this case (there's no need to check segs in this case)
  if (slight < 0) {
    const sector_t *sector = sub->sector;
    if ((Origin.z > sector->floor.maxz && Origin.z-Radius-sector->floor.maxz < -2.0f) ||
        (Origin.z < sector->ceiling.minz && Origin.z+Radius-sector->ceiling.minz > 2.0f))
    {
      // surface check is done in another function (`UpdateBBoxWithSurface()`)
      return true;
    }
  }
  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    // if the light intersects the box, or fully inside the box, we need to check for touching
    if (slight < 0) {
      if (!seg->SphereTouches(Origin, Radius)) continue;
    }
    // we have to check even "invisible" segs here, 'cause we need them all
    const TVec *v1, *v2;
    const int orgside = ClipPointOnSide2(*seg, Origin);
    if (orgside == 2) return true; // origin is on plane, we cannot do anything sane
    if (orgside) {
      //if (!asShadow) continue;
      v1 = seg->v2;
      v2 = seg->v1;
    } else {
      v1 = seg->v1;
      v2 = seg->v2;
    }
    if (IsRangeVisible(*v2, *v1)) return true;
  }
  return false;
}


//==========================================================================
//
//  VViewClipper::CheckLightAddClipSeg
//
//==========================================================================
void VViewClipper::CheckLightAddClipSeg (const seg_t *seg, const TPlane *Mirror, int asShadow) noexcept {
  // no need to check light radius, it is already done
  const line_t *ldef = seg->linedef;
  if (!ldef) return; // miniseg should not clip
  /*
  if (clip_skip_slopes_1side) {
    // do not clip with slopes, if it has no midtex
    if ((ldef->flags&ML_TWOSIDED) == 0) {
      int fcpic, ffpic;
      TPlane ffplane, fcplane;
      CopyHeight(ldef->frontsector, &ffplane, &fcplane, &ffpic, &fcpic);
      // only apply this to sectors without slopes
      if (ffplane.normal.z != 1.0f || fcplane.normal.z != -1.0f) return;
    }
  }
  */

  if (asShadow == AsShadowMap && (ldef->flags&ML_TWOSIDED)) return; // hack for shadowmaps

  // light has 360 degree FOV, so clip with all walls
  const TVec *v1, *v2;
  const int orgside = ClipPointOnSide2(*seg, Origin);
#if 1
  if (orgside == 2) return; // origin is on plane, we cannot do anything sane
  if (orgside) {
    if (!asShadow) return;
    v1 = seg->v2;
    v2 = seg->v1;
  } else {
    v1 = seg->v1;
    v2 = seg->v2;
  }
#else
  if (orgside > 0) return; // ignore backfacing segs
  v1 = seg->v1;
  v2 = seg->v2;
#endif

  if (!MirrorCheck(Mirror, *v1, *v2)) return;

  // for 2-sided line, determine if it can be skipped
  if (seg->backsector && (ldef->flags&ML_TWOSIDED)) {
    if (!IsSegAClosedSomething(Level, /*&Frustum*/nullptr, seg, &Origin, &Radius)) return;
  }

  AddClipRange(*v2, *v1);
}


//==========================================================================
//
//  VViewClipper::CheckLightAddPObjClipSeg
//
//==========================================================================
void VViewClipper::CheckLightAddPObjClipSeg (polyobj_t *pobj, const subsector_t *sub, const seg_t *seg, const TPlane *Mirror, int asShadow) noexcept {
  //if (asShadow == AsShadowMap && (ldef->flags&ML_TWOSIDED)) return; // hack for shadowmaps

  // light has 360 degree FOV, so clip with all walls
  const TVec *v1, *v2;
  const int orgside = ClipPointOnSide2(*seg, Origin);
  if (orgside == 2) return; // origin is on plane, we cannot do anything sane

#if 1
  if (orgside == 2) return; // origin is on plane, we cannot do anything sane
  if (orgside) {
    if (!asShadow) return;
    v1 = seg->v2;
    v2 = seg->v1;
  } else {
    v1 = seg->v1;
    v2 = seg->v2;
  }
#else
  if (orgside > 0) return; // ignore backfacing segs
  v1 = seg->v1;
  v2 = seg->v2;
#endif

  if (!MirrorCheck(Mirror, *v1, *v2)) return;

  // treat each line as two-sided
  if (!IsPObjSegAClosedSomething(Level, /*&Frustum*/nullptr, pobj, sub, seg, &Origin, &Radius)) return;

  AddClipRange(*v2, *v1);
}


//==========================================================================
//
//  VViewClipper::ClipLightAddSubsectorSegs
//
//==========================================================================
void VViewClipper::ClipLightAddSubsectorSegs (const subsector_t *sub, int asShadow, const TPlane *Mirror) noexcept {
  if (ClipIsFull()) return;

  const seg_t *seg = &Level->Segs[sub->firstline];
  for (int count = sub->numlines; count--; ++seg) {
    CheckLightAddClipSeg(seg, Mirror, asShadow);
  }
}


//==========================================================================
//
//  VViewClipper::ClipLightAddPObjSegs
//
//==========================================================================
void VViewClipper::ClipLightAddPObjSegs (const subsector_t *sub, int asShadow, const TPlane *Mirror) noexcept {
  if (!clip_enabled) return;
  if (ClipIsFull()) return;

  if (!sub->HasPObjs() || !clip_with_polyobj.asBool()) return;

  for (auto &&it : sub->PObjFirst()) {
    polyobj_t *pobj = it.pobj();
    // do not clip with slopes (yet)
    if (pobj->pofloor.isSlope() || pobj->poceiling.isSlope()) continue;
    // do not clip with zero-height polyobjects
    if (pobj->pofloor.maxz >= pobj->poceiling.minz) return;
    // process clip segs
    it.part()->EnsureClipSegs(Level);
    for (auto &&sit : it.part()->SegFirst()) {
      const seg_t *seg = sit.seg();
      CheckLightAddPObjClipSeg(pobj, sub, seg, Mirror, asShadow);
    }
  }
}
#endif
