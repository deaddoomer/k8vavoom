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
#include "r_local.h"

//#define VV_EXPERIMENTAL_LMAP_FILTER
//#define VV_DEBUG_BMAP_TRACER

// for some reason Janis tried to nudge lightmap texture point a little
// with this, we'll get spurious "invalid lightmap points", and it causes
// black (unlit) surface parts. i removed this calculation, and those
// lighting bugs are gone. of course, the lighting at geometry edges is
// still somewhat wrong, but it is way better than totally missed light.
// also, this creates cheap ambient-occlusion-like effect. ;-)
static VCvarB r_lmap_stfix_enabled("r_lmap_stfix_enabled", true, "Enable lightmap \"inside wall\" fixing?", CVAR_Archive);
static VCvarF r_lmap_stfix_step("r_lmap_stfix_step", "2", "Lightmap \"inside wall\" texel step", CVAR_Archive);

static VCvarB r_lmap_texture_check_static("r_lmap_texture_check_static", true, "Check textures of two-sided lines?", CVAR_Archive);
static VCvarB r_lmap_texture_check_dynamic("r_lmap_texture_check_dynamic", true, "Check textures of two-sided lines?", CVAR_Archive);
static VCvarI r_lmap_texture_check_radius_dynamic("r_lmap_texture_check_radius_dynamic", "300", "Disable texture check for dynamic lights with radius lower than this.", CVAR_Archive);

VCvarI r_lmap_recalc_timeout("r_lmap_recalc_timeout", "20", "Do not use more than this number of milliseconds for static lightmap updates (0 means 'no limit').", CVAR_Archive);
VCvarB r_lmap_recalc_static("r_lmap_recalc_static", true, "Recalc static lightmaps when map geometry changed?", CVAR_Archive);
VCvarB r_lmap_recalc_moved_static("r_lmap_recalc_moved_static", true, "Recalc static lightmaps when static light source moved?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
extern VCvarB r_dynamic_clip;
extern VCvarB r_glow_flat;
extern VCvarB r_draw_queue_warnings;

vuint32 gf_dynlights_processed = 0;
vuint32 gf_dynlights_traced = 0;
int ldr_extrasamples_override = -1;

enum { Filter4X = false };

// ////////////////////////////////////////////////////////////////////////// //
static VCvarI r_lmap_filtering("r_lmap_filtering", "3", "Static lightmap filtering (0: none; 1: simple; 2: simple++; 3: extra).", CVAR_Archive);
static VCvarB r_lmap_lowfilter("r_lmap_lowfilter", false, "Filter lightmaps without extra samples?", CVAR_Archive);
static VCvarI r_lmap_atlas_limit("r_lmap_atlas_limit", "14", "Nuke lightmap cache if it reached this number of atlases.", CVAR_Archive);

VCvarB r_lmap_bsp_trace_static("r_lmap_bsp_trace_static", false, "Trace static lightmaps with BSP tree instead of blockmap?", CVAR_Archive);
VCvarB r_lmap_bsp_trace_dynamic("r_lmap_bsp_trace_dynamic", false, "Trace dynamic lightmaps with BSP tree instead of blockmap?", CVAR_Archive);

extern VCvarB dbg_adv_light_notrace_mark;


// ////////////////////////////////////////////////////////////////////////// //
enum { GridSize = VRenderLevelLightmap::LMapTraceInfo::GridSize };
enum { MaxSurfPoints = VRenderLevelLightmap::LMapTraceInfo::MaxSurfPoints };

static_assert((Filter4X ? (MaxSurfPoints >= GridSize*GridSize*16) : (MaxSurfPoints >= GridSize*GridSize*4)), "invalid grid size");

struct LightmapTracer {
  vuint32 blocklightsr[GridSize*GridSize];
  vuint32 blocklightsg[GridSize*GridSize];
  vuint32 blocklightsb[GridSize*GridSize];

  #ifdef VV_EXPERIMENTAL_LMAP_FILTER
  vuint32 blocklightsrNew[GridSize*GridSize];
  vuint32 blocklightsgNew[GridSize*GridSize];
  vuint32 blocklightsbNew[GridSize*GridSize];
  #endif

  // this is for static lightmaps
  // *4 for extra filtering
  vuint8 lightmapHit[MaxSurfPoints];
  float lightmapMono[MaxSurfPoints];
  float lightmapr[MaxSurfPoints];
  float lightmapg[MaxSurfPoints];
  float lightmapb[MaxSurfPoints];
  // set in lightmap merge code
  bool hasOverbright; // has overbright component?
  bool isColored; // is lightmap colored?
};


int light_mem = 0;
static LightmapTracer lmtracer;


//==========================================================================
//
//  getSurfLightLevelInt
//
//==========================================================================
static inline int getSurfLightLevelInt (const surface_t *surf) {
  if (!surf) return 0;
  if (r_glow_flat && !surf->seg && surf->subsector) {
    const sector_t *sec = surf->subsector->sector;
    //FIXME: check actual view height here
    if (sec && !sec->heightsec) {
      if (sec->floor.pic && surf->GetNormalZ() > 0.0f) {
        VTexture *gtex = GTextureManager(sec->floor.pic);
        if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return 255;
      }
      if (sec->ceiling.pic && surf->GetNormalZ() < 0.0f) {
        VTexture *gtex = GTextureManager(sec->ceiling.pic);
        if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return 255;
      }
    }
  }
  return (surf->Light>>24)&0xff;
}


//==========================================================================
//
//  fixSurfLightLevel
//
//==========================================================================
static inline vuint32 fixSurfLightLevel (const surface_t *surf) {
  if (!surf) return 0;
  if (r_glow_flat && !surf->seg && surf->subsector) {
    const sector_t *sec = surf->subsector->sector;
    //FIXME: check actual view height here
    if (sec && !sec->heightsec) {
      if (sec->floor.pic && surf->GetNormalZ() > 0.0f) {
        VTexture *gtex = GTextureManager(sec->floor.pic);
        if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return (surf->Light&0xffffffu)|0xff000000u;
      }
      if (sec->ceiling.pic && surf->GetNormalZ() < 0.0f) {
        VTexture *gtex = GTextureManager(sec->ceiling.pic);
        if (gtex && gtex->Type != TEXTYPE_Null && gtex->IsGlowFullbright()) return (surf->Light&0xffffffu)|0xff000000u;
      }
    }
  }
  //return (surf->Light&0xffffffu)|(((vuint32)((surf->Light>>24)&0xff))<<24);
  return surf->Light;
}


//==========================================================================
//
//  VRenderLevelLightmap::IsStaticLightmapTimeLimitExpired
//
//  returns `true` if expired
//
//==========================================================================
bool VRenderLevelLightmap::IsStaticLightmapTimeLimitExpired () {
  // if we're creating the world, there is no time limit
  if (inWorldCreation) return false;
  // possibly done with the current frame?
  if (lastLMapStaticRecalcFrame == currDLightFrame && lmapStaticRecalcTimeLeft == 0) return true;
  // either not done, or a new frame
  // new frame?
  if (lastLMapStaticRecalcFrame != currDLightFrame) {
    const int msec = r_lmap_recalc_timeout.asInt();
    if (msec <= 0) {
      lmapStaticRecalcTimeLeft = -1;
      return false;
    }
    lastLMapStaticRecalcFrame = currDLightFrame;
    lmapStaticRecalcTimeLeft = (double)msec/1000.0;
    return false;
  }
  // check current frame timeout
  if (lmapStaticRecalcTimeLeft < 0) return false;
  return (lmapStaticRecalcTimeLeft == 0);
}


//==========================================================================
//
//  VRenderLevelLightmap::CastStaticRay
//
//  Returns the distance between the points, or 0 if blocked
//
//  `p1` is light origin
//
//==========================================================================
bool VRenderLevelLightmap::CastStaticRay (float *dist, const subsector_t *srcsubsector, const TVec &p1, const subsector_t *destsubsector, const TVec &p2, const float squaredist, const bool allowTextureCheck) {
  const TVec delta = p2-p1;
  const float t = delta.dot(delta);
  if (t >= squaredist) {
    // too far away
    if (dist) *dist = 0.0f;
    return false;
  }
  if (allowTextureCheck) {
    if (t <= 2.0f*2.0f) {
      // at light point
      if (dist) *dist = 1.0f;
      return true;
    }
  } else {
    if (t <= 0.0f) {
      // at light point
      if (dist) *dist = 1.0f;
      return true;
    }
  }

  if (!r_lmap_bsp_trace_static) {
    if (!Level->CastLightRay((allowTextureCheck && r_lmap_texture_check_static.asBool()), destsubsector, p2, p1, srcsubsector)) {
      // ray was blocked
      if (dist) *dist = 0.0f;
      return false;
    }
  } else {
    linetrace_t Trace;
    if (!Level->TraceLine(Trace, p1, p2, SPF_NOBLOCKSIGHT)) {
      if (dist) *dist = 0.0f;
      return false;
    }
  }

  if (dist) *dist = sqrtf(t); // 1.0f/fastInvSqrtf(t); k8: not much faster
  return true;
}


//==========================================================================
//
//  VRenderLevelLightmap::CalcMinMaxs
//
//==========================================================================
void VRenderLevelLightmap::CalcMinMaxs (LMapTraceInfo &lmi, const surface_t *surf) {
  TVec smins(+FLT_MAX, +FLT_MAX, +FLT_MAX);
  TVec smaxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  const SurfVertex *v = &surf->verts[0];
  for (int i = surf->count; i--; ++v) {
    if (smins.x > v->x) smins.x = v->x;
    if (smins.y > v->y) smins.y = v->y;
    if (smins.z > v->z) smins.z = v->z;
    if (smaxs.x < v->x) smaxs.x = v->x;
    if (smaxs.y < v->y) smaxs.y = v->y;
    if (smaxs.z < v->z) smaxs.z = v->z;
  }
  smins.x = clampval(smins.x, -32768.0f, 32768.0f);
  smins.y = clampval(smins.y, -32768.0f, 32768.0f);
  smins.z = clampval(smins.z, -32768.0f, 32768.0f);
  smaxs.x = clampval(smaxs.x, -32768.0f, 32768.0f);
  smaxs.y = clampval(smaxs.y, -32768.0f, 32768.0f);
  smaxs.z = clampval(smaxs.z, -32768.0f, 32768.0f);
  lmi.smins = smins;
  lmi.smaxs = smaxs;
}


//==========================================================================
//
//  VRenderLevelLightmap::CalcFaceVectors
//
//  fills in texorg, worldtotex, and textoworld
//
//==========================================================================
bool VRenderLevelLightmap::CalcFaceVectors (LMapTraceInfo &lmi, const surface_t *surf) {
  const texinfo_t *tex = surf->texinfo;

  lmi.worldtotex[0] = tex->saxisLM;
  lmi.worldtotex[1] = tex->taxisLM;

  // calculate a normal to the texture axis
  // points can be moved along this without changing their S/T
  TVec texnormal(
    tex->taxisLM.y*tex->saxisLM.z-tex->taxisLM.z*tex->saxisLM.y,
    tex->taxisLM.z*tex->saxisLM.x-tex->taxisLM.x*tex->saxisLM.z,
    tex->taxisLM.x*tex->saxisLM.y-tex->taxisLM.y*tex->saxisLM.x);
  texnormal.normaliseInPlace();
  if (!isFiniteF(texnormal.x)) return false; // no need to check other coords

  // flip it towards plane normal
  float distscale = texnormal.dot(surf->GetNormal());
  if (!distscale) Host_Error("Texture axis perpendicular to face");
  if (distscale < 0.0f) {
    distscale = -distscale;
    texnormal = -texnormal;
  }

  // distscale is the ratio of the distance along the texture normal to
  // the distance along the plane normal
  distscale = 1.0f/distscale;
  if (!isFiniteF(distscale)) return false;

  for (unsigned i = 0; i < 2; ++i) {
    const float len = 1.0f/lmi.worldtotex[i].length();
    if (!isFiniteF(len)) return false; // just in case
    const float dist = lmi.worldtotex[i].dot(surf->GetNormal())*distscale;
    lmi.textoworld[i] = lmi.worldtotex[i]-texnormal*dist;
    lmi.textoworld[i] = lmi.textoworld[i]*len*len;
  }

  // calculate texorg on the texture plane
  /*
  for (int i = 0; i < 3; ++i) {
    //lmi.texorg[i] = -tex->soffs*lmi.textoworld[0][i]-tex->toffs*lmi.textoworld[1][i];
    lmi.texorg[i] = 0;
  }

  // project back to the face plane
  const float dist = (lmi.texorg.dot(surf->GetNormal())-surf->GetDist()-1.0f)*distscale;
  lmi.texorg = lmi.texorg-texnormal*dist;
  */

  // project back to the face plane
  // one "unit" in front of surface
  const float dist = (-surf->GetDist()-1.0f)*distscale;
  lmi.texorg = lmi.texorg-texnormal*dist;

  return true;
}


#define CP_FIX_UT(uort_)  do { \
  if (u##uort_ > mid##uort_) { \
    u##uort_ -= r_lmap_stfix_step.asFloat(); \
    if (u##uort_ < mid##uort_) u##uort_ = mid##uort_; \
  } else { \
    u##uort_ += r_lmap_stfix_step.asFloat(); \
    if (u##uort_ > mid##uort_) u##uort_ = mid##uort_; \
  } \
} while (0) \


//==========================================================================
//
//  VRenderLevelLightmap::CalcPoints
//
//  for each texture aligned grid point, back project onto the plane
//  to get the world xyz value of the sample point
//
//  for dynlights, set `lowres` to `true`
//  setting `lowres` skips point visibility determination, because it is
//  done in `AddDynamicLights()`.
//
//==========================================================================
void VRenderLevelLightmap::CalcPoints (LMapTraceInfo &lmi, const surface_t *surf, bool lowres) {
  int w, h;
  float step;
  float starts, startt;
  linetrace_t Trace;

  bool doExtra = (r_lmap_filtering > 2);
  if (ldr_extrasamples_override >= 0) doExtra = (ldr_extrasamples_override > 0);
  if (!lowres && doExtra) {
    // extra filtering
    if (Filter4X) {
      w = ((surf->extents[0]>>4)+1)*4;
      h = ((surf->extents[1]>>4)+1)*4;
      starts = surf->texturemins[0]-16;
      startt = surf->texturemins[1]-16;
      step = 4;
    } else {
      w = ((surf->extents[0]>>4)+1)*2;
      h = ((surf->extents[1]>>4)+1)*2;
      starts = surf->texturemins[0]-8;
      startt = surf->texturemins[1]-8;
      step = 8;
    }
  } else {
    w = (surf->extents[0]>>4)+1;
    h = (surf->extents[1]>>4)+1;
    starts = surf->texturemins[0];
    startt = surf->texturemins[1];
    step = 16;
  }
  lmi.didExtra = doExtra;

  bool dotrace = !lowres;
  if (!surf->subsector) dotrace = false; // just in case
  if (dotrace && !r_lmap_stfix_enabled) dotrace = false;

  lmi.numsurfpt = w*h;
  bool doPointCheck = false;
  // *4 for extra filtering
  if (lmi.numsurfpt > MaxSurfPoints) {
    GCon->Logf(NAME_Warning, "too many points in lightmap tracer");
    lmi.numsurfpt = MaxSurfPoints;
    doPointCheck = true;
  }

  TVec *spt = lmi.surfpt;

  if (dotrace) {
    // fill in surforg
    // the points are biased towards the center of the surface
    // to help avoid edge cases just inside walls
    const float mids = surf->texturemins[0]+surf->extents[0]*0.5f;
    const float midt = surf->texturemins[1]+surf->extents[1]*0.5f;
    const TVec facemid = lmi.texorg+lmi.textoworld[0]*mids+lmi.textoworld[1]*midt;
    const float stfixStep = r_lmap_stfix_step.asFloat();
    const subsector_t *facesubsec = Level->PointInSubsector(facemid);

    for (int t = 0; t < h; ++t) {
      for (int s = 0; s < w; ++s, ++spt) {
        if (doPointCheck && (int)(ptrdiff_t)(spt-lmi.surfpt) >= MaxSurfPoints) return;
        float us = starts+s*step;
        float ut = startt+t*step;

        // if a line can be traced from surf to facemid, the point is good
        // k8: i don't really understand what this code is trying to do
        for (int i = 0; i < 6; ++i) {
          // calculate texture point
          *spt = lmi.calcTexPoint(us, ut);
          //!if (Level->TraceLine(Trace, facemid, *spt, SPF_NOBLOCKSIGHT)) break; // got it
          if (CastStaticRay(nullptr, facesubsec, facemid, surf->subsector, *spt, 999999.0f, false)) { // do not check textures
            //found = true;
            // move the point 1 unit above the surface (this seems to be done in `CalcFaceVectors()`)
            //const TVec pp = surf->plane.Project(*spt)+surf->plane.normal;
            //*spt = pp;
            break;
          }

          // move surf 4 pixels towards the center (was 8)
          if (i&1) {
            CP_FIX_UT(s);
          } else {
            CP_FIX_UT(t);
          }

          const TVec fms = facemid-(*spt);
          *spt += stfixStep*fms.normalise();
        }
        // just in case
        /*
        if (!found) {
          us = starts+s*step;
          ut = startt+t*step;
          *spt = lmi.calcTexPoint(us, ut);
        }
        */
      }
    }
  } else {
    for (int t = 0; t < h; ++t) {
      for (int s = 0; s < w; ++s, ++spt) {
        if (doPointCheck && (int)(ptrdiff_t)(spt-lmi.surfpt) >= MaxSurfPoints) return;
        const float us = starts+s*step;
        const float ut = startt+t*step;
        *spt = lmi.calcTexPoint(us, ut);
      }
    }
  }
}


//==========================================================================
//
//  FilterLightmap
//
//==========================================================================
template<typename T> void FilterLightmap (T *lmap, const int wdt, const int hgt) {
  if (!r_lmap_lowfilter) return;
  if (!lmap || (wdt < 2 && hgt < 2)) return;
  static T *lmnew = nullptr;
  static int lmnewSize = 0;
  if (wdt*hgt > lmnewSize) {
    lmnewSize = wdt*hgt;
    lmnew = (T *)Z_Realloc(lmnew, lmnewSize*sizeof(T));
  }
  for (int y = 0; y < hgt; ++y) {
    for (int x = 0; x < wdt; ++x) {
      int count = 0;
      float sum = 0;
      for (int dy = -1; dy < 2; ++dy) {
        const int sy = y+dy;
        if (sy < 0 || sy >= hgt) continue;
        T *row = lmap+(sy*wdt);
        for (int dx = -1; dx < 2; ++dx) {
          const int sx = x+dx;
          if (sx < 0 || sx >= wdt) continue;
          if ((dx|dy) == 0) continue;
          ++count;
          sum += row[sx];
        }
      }
      if (!count) continue;
      sum /= (float)count;
      float v = lmap[y*wdt+x];
      sum = (sum+v)*0.5f;
      lmnew[y*wdt+x] = sum;
    }
  }
  if (lmnewSize) memcpy(lmap, lmnew, lmnewSize*sizeof(T));
}


//==========================================================================
//
//  VRenderLevelLightmap::SingleLightFace
//
//  light face with static light
//
//==========================================================================
void VRenderLevelLightmap::SingleLightFace (LMapTraceInfo &lmi, light_t *light, surface_t *surf) {
  if (surf->count < 3) return; // wtf?!
  if (!light->active || light->radius < 2) return;

  // check bounding box
  if (light->origin.x+light->radius < lmi.smins.x ||
      light->origin.x-light->radius > lmi.smaxs.x ||
      light->origin.y+light->radius < lmi.smins.y ||
      light->origin.y-light->radius > lmi.smaxs.y ||
      light->origin.z+light->radius < lmi.smins.z ||
      light->origin.z-light->radius > lmi.smaxs.z)
  {
    return;
  }

  //float orgdist = light->origin.dot(surf->GetNormal())-surf->GetDist();
  const float orgdist = surf->PointDistance(light->origin);
  // don't bother with lights behind the surface, or too far away
  if (orgdist <= -0.1f || orgdist >= light->radius) return;

  TVec lorg = light->origin;

  // drop lights inside sectors without height
  if (light->leafnum >= 0 && light->leafnum < Level->NumSubsectors) {
    const sector_t *sec = Level->Subsectors[light->leafnum].sector;
    if (!CheckValidLightPosRough(lorg, sec)) return;
  }

  lmi.setupSpotlight(light->coneDirection, light->coneAngle);

  // calc points only when surface may be lit by a light
  if (!lmi.pointsCalced) {
    if (!CalcFaceVectors(lmi, surf)) {
      GCon->Logf(NAME_Warning, "cannot calculate lightmap vectors");
      lmi.numsurfpt = 0;
      memset(lmtracer.lightmapMono, 0, sizeof(lmtracer.lightmapMono));
      memset(lmtracer.lightmapr, 0, sizeof(lmtracer.lightmapr));
      memset(lmtracer.lightmapg, 0, sizeof(lmtracer.lightmapg));
      memset(lmtracer.lightmapb, 0, sizeof(lmtracer.lightmapb));
      return;
    }

    CalcPoints(lmi, surf, false);
    lmi.pointsCalced = true;
    memset(lmtracer.lightmapMono, 0, lmi.numsurfpt*sizeof(lmtracer.lightmapMono[0]));
    memset(lmtracer.lightmapr, 0, lmi.numsurfpt*sizeof(lmtracer.lightmapr[0]));
    memset(lmtracer.lightmapg, 0, lmi.numsurfpt*sizeof(lmtracer.lightmapg[0]));
    memset(lmtracer.lightmapb, 0, lmi.numsurfpt*sizeof(lmtracer.lightmapb[0]));
  }

  // check it for real
  subsector_t *srcsubsector = Level->PointInSubsector(lorg);
  const TVec *spt = lmi.surfpt;
  const float squaredist = light->radius*light->radius;
  const float rmul = ((light->color>>16)&255)/255.0f;
  const float gmul = ((light->color>>8)&255)/255.0f;
  const float bmul = (light->color&255)/255.0f;

  int w = (surf->extents[0]>>4)+1;
  int h = (surf->extents[1]>>4)+1;

  bool doMidFilter = (!lmi.didExtra && r_lmap_filtering > 0);
  if (doMidFilter && lmi.numsurfpt) memset(lmtracer.lightmapHit, 0, /*w*h*/lmi.numsurfpt);

  bool wasAnyHit = false;
  const TVec lnormal = surf->GetNormal();
  //const TVec lorg = light->origin;

  const bool doCastRay = !(light->flags&(dlight_t::NoShadow|dlight_t::NoGeoClip));

  float attn = 1.0f;
  for (int c = 0; c < lmi.numsurfpt; ++c, ++spt) {
    // check spotlight cone
    if (lmi.spotLight) {
      if (((*spt)-lorg).length2DSquared() > 2.0f*2.0f) {
        attn = spt->calcSpotlightAttMult(lorg, lmi.coneDir, lmi.coneAngle);
        if (attn == 0.0f) continue;
      } else {
        attn = 1.0f;
      }
    }

    float raydist;
    if (doCastRay) {
      if (!CastStaticRay(&raydist, srcsubsector, lorg+lnormal, surf->subsector, (*spt)+lnormal, squaredist, true)) { // allow texture check
        // light ray is blocked
        continue;
      }
    } else {
      raydist = (*spt-lorg).lengthSquared();
      if (raydist >= squaredist) continue; // too far
      raydist = (raydist > 1.0f ? sqrtf(raydist) : 1.0f);
    }

    TVec incoming = lorg-(*spt);
    if (!incoming.isZero()) {
      incoming.normaliseInPlace();
      if (!incoming.isValid()) {
        lmtracer.lightmapMono[c] += 255.0f;
        lmtracer.lightmapr[c] += 255.0f;
        lmtracer.isColored = true;
        lmi.light_hit = true;
        continue;
      }
    }

    float angle = incoming.dot(lnormal);
    angle = 0.5f+0.5f*angle;

    float add = (light->radius-raydist)*angle*attn;
    if (add <= 0.0f) continue;
    // without this, lights with huge radius will overbright everything
    if (add > 255.0f) add = 255.0f;

    if (doMidFilter) { wasAnyHit = true; lmtracer.lightmapHit[c] = 1; }

    lmtracer.lightmapMono[c] += add;
    lmtracer.lightmapr[c] += add*rmul;
    lmtracer.lightmapg[c] += add*gmul;
    lmtracer.lightmapb[c] += add*bmul;
    // ignore really tiny lights
    if (lmtracer.lightmapMono[c] > 1) {
      lmi.light_hit = true;
      if (light->color != 0xffffffff) lmtracer.isColored = true;
    }
  }

  if (doCastRay && doMidFilter && wasAnyHit) {
    //GCon->Logf("w=%d; h=%d; num=%d; cnt=%d", w, h, w*h, lmi.numsurfpt);
   again:
    const vuint8 *lht = lmtracer.lightmapHit;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x, ++lht) {
        const int laddr = y*w+x;
        if (laddr >= lmi.numsurfpt) goto doneall;
        if (*lht) continue;
        // check if any neighbour point was hit
        // if not, don't bother to do better tracing
        bool doit = false;
        for (int dy = -1; dy < 2; ++dy) {
          const int sy = y+dy;
          if (sy < 0 || sy >= h) continue;
          const vuint8 *row = lmtracer.lightmapHit+(sy*w);
          for (int dx = -1; dx < 2; ++dx) {
            if ((dx|dy) == 0) continue;
            const int sx = x+dx;
            if (sx < 0 || sx >= w) continue;
            if (row[sx]) { doit = true; goto done; }
          }
        }
       done:
        if (doit) {
          TVec pt = lmi.surfpt[laddr];
          float raydist = 0.0f;
          for (int dy = -1; dy < 2; ++dy) {
            for (int dx = -1; dx < 2; ++dx) {
              for (int dz = -1; dz < 2; ++dz) {
                if ((dx|dy|dz) == 0) continue;
                if (CastStaticRay(&raydist, srcsubsector, lorg+lnormal, surf->subsector, pt+TVec(4*dx, 4*dy, 4*dz), squaredist, true)) goto donetrace; // allow texture check
              }
            }
          }
          continue;
         donetrace:
          //GCon->Logf("x=%d; y=%d; w=%d; h=%d; raydist=%g", x, y, w, h, raydist);

          TVec incoming = lorg-pt;
          if (!incoming.isZero()) {
            incoming.normaliseInPlace();
            if (!incoming.isValid()) continue;
          }

          float angle = incoming.dot(lnormal);
          angle = 0.5f+0.5f*angle;

          float add = (light->radius-raydist)*angle*0.75f;
          if (add <= 0.0f) continue;
          // without this, lights with huge radius will overbright everything
          if (add > 255.0f) add = 255.0f;

          lmtracer.lightmapMono[laddr] += add;
          lmtracer.lightmapr[laddr] += add*rmul;
          lmtracer.lightmapg[laddr] += add*gmul;
          lmtracer.lightmapb[laddr] += add*bmul;
          // ignore really tiny lights
          if (lmtracer.lightmapMono[laddr] > 1) {
            lmi.light_hit = true;
            if (light->color != 0xffffffff) lmtracer.isColored = true;
          }
          lmtracer.lightmapHit[laddr] = 1;
          if (r_lmap_filtering == 2) goto again;
        }
      }
    }
    doneall: (void)0;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
#define FILTER_LMAP_EXTRA(lmv_)  do { \
  if (Filter4X) { \
    total = 0; \
    for (int dy = 0; dy < 4; ++dy) { \
      for (int dx = 0; dx < 4; ++dx) { \
        total += lmv_[(t*4+dy)*w*4+s*4+dx]; \
      } \
    } \
    total *= 1.0f/16.0f; \
  } else { \
    total = lmv_[t*w*4+s*2]+ \
            lmv_[t*2*w*2+s*2+1]+ \
            lmv_[(t*2+1)*w*2+s*2]+ \
            lmv_[(t*2+1)*w*2+s*2+1]; \
    total *= 0.25f; \
  } \
} while (0)


//==========================================================================
//
//  VRenderLevelLightmap::CanFaceBeStaticallyLit
//
//==========================================================================
bool VRenderLevelLightmap::CanFaceBeStaticallyLit (surface_t *surf) {
  if (!surf || surf->count < 3 || !surf->subsector) return false;
  // check if we have any touching static lights
  const int snum = (int)(ptrdiff_t)(surf->subsector-&Level->Subsectors[0]);
  if (snum < 0 || snum >= SubStaticLights.length()) return false; // just in case
  SubStaticLigtInfo *sli = SubStaticLights.ptr()+snum;
  return (sli->touchedStatic.length() > 0);
}


//==========================================================================
//
//  VRenderLevelLightmap::LightFaceTimeCheckedFreeCaches
//
//  returns `false` if the face should be relit, but relight
//  time limit is expired
//
//==========================================================================
void VRenderLevelLightmap::LightFaceTimeCheckedFreeCaches (surface_t *surf) {
  if (!surf) return;

  if (!CanFaceBeStaticallyLit(surf)) {
    surf->drawflags &= ~surface_t::DF_CALC_LMAP;
    if (surf->CacheSurf) FreeSurfCache(surf->CacheSurf);
    surf->FreeLightmaps();
    return;
  }

  if (IsStaticLightmapTimeLimitExpired()) return;

  if (surf->CacheSurf) {
    FreeSurfCache(surf->CacheSurf);
    vassert(!surf->CacheSurf);
  }

  LightFace(surf);
}


//==========================================================================
//
//  VRenderLevelLightmap::LightFace
//
//==========================================================================
void VRenderLevelLightmap::LightFace (surface_t *surf) {
  if (!surf) return;

  surf->drawflags &= ~surface_t::DF_CALC_LMAP;

  if (!CanFaceBeStaticallyLit(surf)) {
    surf->FreeLightmaps(); // just in case
    return;
  }

  const bool accountTime = (lmapStaticRecalcTimeLeft > 0);
  double stt = (accountTime ? -Sys_Time() : 0.0);

  LMapTraceInfo lmi;
  //lmi.points_calculated = false;
  vassert(!lmi.pointsCalced);

  lmi.light_hit = false;
  lmtracer.isColored = false;

  CalcMinMaxs(lmi, surf);

  // cast all static lights
  if (r_static_lights) {
    #if 0
    light_t *stl = Lights.ptr();
    for (int i = Lights.length(); i--; ++stl) SingleLightFace(lmi, stl, surf);
    #else
    const int snum = (int)(ptrdiff_t)(surf->subsector-&Level->Subsectors[0]);
    if (snum >= 0 && snum < SubStaticLights.length()) {
      SubStaticLigtInfo *sli = SubStaticLights.ptr()+snum;
      if (sli->touchedStatic.length()) {
        // for non-shadowing lights, collect really visible surfaces
        for (auto it : sli->touchedStatic.first()) {
          light_t *stl = &Lights[it.getKey()];
          if (stl->radius <= 2.0f) continue;
          //FIXME: make this faster!
          const bool doCastRay = !(stl->flags&dlight_t::NoShadow);
          if (!doCastRay && !surf->subsector->isAnyPObj()) {
            // check if this surface is visible
            if ((stl->flags&dlight_t::NoGeoClip) == 0) {
              if (stl->litSurfacesValidFrame != updateWorldFrame) {
                //GCon->Logf(NAME_Debug, "updating static light #%d (frm=%u)", it.getKey(), updateWorldFrame);
                stl->litSurfacesValidFrame = updateWorldFrame;
                // `CurrLightPos` and `CurrLightRadius` should be set
                CurrLightPos = stl->origin;
                CurrLightRadius = stl->radius;
                CurrLightNoGeoClip = false;
                stl->litSurfaces.reset();
                CollectRegLightSurfaces(stl->litSurfaces);
                //GCon->Logf(NAME_Debug, "updated static light #%d (frm=%u); %d surfaces found", it.getKey(), updateWorldFrame, stl->litSurfaces.length());
              }
              if (!stl->litSurfaces.has(surf)) continue;
            }
          }
          SingleLightFace(lmi, stl, surf);
        }
      }
    }
    #endif
  }

  if (!lmi.light_hit) {
    // no light hit it, no need to have lightmaps
    surf->FreeLightmaps();
    if (accountTime) {
      stt += Sys_Time();
      if ((lmapStaticRecalcTimeLeft -= stt) <= 0) lmapStaticRecalcTimeLeft = 0;
    }
    return;
  }

  const int w = (surf->extents[0]>>4)+1;
  const int h = (surf->extents[1]>>4)+1;
  vassert(w > 0);
  vassert(h > 0);

  // if the surface already has a static lightmap, we will reuse it,
  // otherwise we must allocate a new one
  if (lmtracer.isColored) {
    // need colored lightmap
    int sz = w*h*(int)sizeof(surf->lightmap_rgb[0]);
    surf->ReserveRGBLightmap(sz);

    if (!lmi.didExtra) {
      if (w*h <= MaxSurfPoints) {
        FilterLightmap(lmtracer.lightmapr, w, h);
        FilterLightmap(lmtracer.lightmapg, w, h);
        FilterLightmap(lmtracer.lightmapb, w, h);
      } else {
        GCon->Logf(NAME_Warning, "skipped filter for lightmap of size %dx%d", w, h);
      }
    }

    //HACK!
    if (w*h > MaxSurfPoints) lmi.didExtra = false;

    int i = 0;
    for (int t = 0; t < h; ++t) {
      for (int s = 0; s < w; ++s, ++i) {
        if (i > MaxSurfPoints) i = MaxSurfPoints-1;
        float total;
        if (lmi.didExtra) {
          // filtered sample
          FILTER_LMAP_EXTRA(lmtracer.lightmapr);
        } else {
          total = lmtracer.lightmapr[i];
        }
        surf->lightmap_rgb[i].r = clampToByte((int)total);

        if (lmi.didExtra) {
          // filtered sample
          FILTER_LMAP_EXTRA(lmtracer.lightmapg);
        } else {
          total = lmtracer.lightmapg[i];
        }
        surf->lightmap_rgb[i].g = clampToByte((int)total);

        if (lmi.didExtra) {
          // filtered sample
          FILTER_LMAP_EXTRA(lmtracer.lightmapb);
        } else {
          total = lmtracer.lightmapb[i];
        }
        surf->lightmap_rgb[i].b = clampToByte((int)total);
      }
    }
  } else {
    // free rgb lightmap, because our light is monochrome
    surf->FreeRGBLightmap();
  }

  {
    // monochrome lightmap
    int sz = w*h*(int)sizeof(surf->lightmap[0]);
    surf->ReserveMonoLightmap(sz);

    if (!lmi.didExtra) {
      if (w*h <= MaxSurfPoints) {
        FilterLightmap(lmtracer.lightmapMono, w, h);
      } else {
        GCon->Logf(NAME_Warning, "skipped filter for lightmap of size %dx%d", w, h);
      }
    }

    //HACK!
    if (w*h > MaxSurfPoints) lmi.didExtra = false;

    int i = 0;
    for (int t = 0; t < h; ++t) {
      for (int s = 0; s < w; ++s, ++i) {
        if (i > MaxSurfPoints) i = MaxSurfPoints-1;
        float total;
        if (lmi.didExtra) {
          // filtered sample
          FILTER_LMAP_EXTRA(lmtracer.lightmapMono);
        } else {
          total = lmtracer.lightmapMono[i];
        }
        surf->lightmap[i] = clampToByte((int)total);
      }
    }
  }

  if (accountTime) {
    stt += Sys_Time();
    if ((lmapStaticRecalcTimeLeft -= stt) <= 0) lmapStaticRecalcTimeLeft = 0;
  }
}


//**************************************************************************
//**
//**  DYNAMIC LIGHTS
//**
//**************************************************************************


/*
NOTES:
we should do several things to speed it up:
first, cache all dynlights, so we'd be able to reuse info from previous tracing
second, store all dynlights affecting a surface, and calculated traceinfo for them

by storing traceinfo, we can reuse it when light radius changed, instead of
tracing a light again and again.

actually, what we are interested in is not a light per se, but light origin.
if we have a light with the same origin, we can reuse it's traceinfo (and possibly
extend it if new light has bigger radius).

thus, we can go with "light cachemap" instead, and store all relevant info there.
also, keep info in cache for several seconds, as player is likely to move around
the light. do cachemap housekeeping once in 2-3 seconds, for example. it doesn't
really matter if we'll accumulate alot of lights there.

also, with proper cache implementation, we can drop "static lights" at all.
just trace and cache "static lights" at level start, and mark 'em as "persistent".
this way, when level geometry changed, we can re-trace static lights too.
*/


//==========================================================================
//
//  VRenderLevelLightmap::AddDynamicLights
//
//  dlight frame already checked
//
//==========================================================================
void VRenderLevelLightmap::AddDynamicLights (surface_t *surf) {
  if (surf->count < 3) return; // wtf?!

  //float mids = 0, midt = 0;
  //TVec facemid = TVec(0,0,0);
  LMapTraceInfo lmi;
  vassert(!lmi.pointsCalced);

  int smax = (surf->extents[0]>>4)+1;
  int tmax = (surf->extents[1]>>4)+1;
  if (smax > GridSize) smax = GridSize;
  if (tmax > GridSize) tmax = GridSize;

  const texinfo_t *tex = surf->texinfo;

  /*
  const float starts = surf->texturemins[0];
  const float startt = surf->texturemins[1];
  const float step = 16;
  */

  const bool doCheckTrace = (r_dynamic_clip && IsShadowsEnabled());
  const bool useBSPTrace = r_lmap_bsp_trace_dynamic.asBool();
  const bool texCheck = r_lmap_texture_check_dynamic.asBool();
  const float texCheckMinRadius = r_lmap_texture_check_radius_dynamic.asInt(); // float, to avoid converions later
  linetrace_t Trace;

  for (unsigned lnum = 0; lnum < MAX_DLIGHTS; ++lnum) {
    if (!(surf->dlightbits&(1U<<lnum))) continue; // not lit by this light

    const dlight_t &dl = DLights[lnum];
    //if (dl.type == DLTYPE_Subtractive) GCon->Logf("***SUBTRACTIVE LIGHT!");
    if (dl.type&DLTYPE_Subtractive) continue;

    // for speed
    const int xnfo = dlinfo[lnum].needTrace;
    if (!xnfo) continue;

    const TVec dorg = dl.origin;
    float rad = dl.radius;
    float dist = surf->PointDistance(dorg);
    // don't bother with lights behind the surface, or too far away
    if (dist <= -0.1f || dist >= rad) continue; // was with `r_dynamic_clip` check; but there is no reason to not check this
    if (dist < 0.0f) dist = 0.0f; // clamp it

    rad -= dist;
    float minlight = dl.minlight;
    if (rad < minlight) continue;
    minlight = rad-minlight;

    TVec impact = dorg-surf->GetNormal()*dist;
    //const TVec surfOffs = surf->GetNormal()*4.0f; // don't land exactly on a surface

    //TODO: we can use clipper to check if destination subsector is occluded
    bool needProperTrace = (doCheckTrace && xnfo > 0 && (dl.flags&(dlight_t::NoShadow|dlight_t::NoGeoClip)) == 0);

    ++gf_dynlights_processed;
    if (needProperTrace) ++gf_dynlights_traced;

    vuint32 dlcolor = (!needProperTrace && dbg_adv_light_notrace_mark ? 0xffff00ffU : dl.color);

    const float rmul = (dlcolor>>16)&255;
    const float gmul = (dlcolor>>8)&255;
    const float bmul = dlcolor&255;

    TVec local;
    local.x = impact.dot(tex->saxisLM)/*+tex->soffs*/;
    local.y = impact.dot(tex->taxisLM)/*+tex->toffs*/;
    local.z = 0;

    local.x -= /*starts*/surf->texturemins[0];
    local.y -= /*startt*/surf->texturemins[1];

    lmi.setupSpotlight(dl.coneDirection, dl.coneAngle);

    if (!lmi.pointsCalced && (needProperTrace || lmi.spotLight)) {
      if (!CalcFaceVectors(lmi, surf)) return;
      CalcPoints(lmi, surf, true);
      lmi.pointsCalced = true;
    }

    //TVec spt(0.0f, 0.0f, 0.0f);
    subsector_t *surfsubsector = surf->subsector;
    float attn = 1.0f;

    const TVec *spt = lmi.surfpt;
    for (int t = 0; t < tmax; ++t) {
      int td = (int)local.y-t*16;
      if (td < 0) td = -td;
      for (int s = 0; s < smax; ++s, ++spt) {
        int sd = (int)local.x-s*16;
        if (sd < 0) sd = -sd;
        const float ptdist = (sd > td ? sd+(td>>1) : td+(sd>>1));
        if (ptdist < minlight) {
          // check spotlight cone
          if (lmi.spotLight) {
            //spt = lmi.calcTexPoint(starts+s*step, startt+t*step);
            if (((*spt)-dorg).length2DSquared() > 2.0f*2.0f) {
              attn = spt->calcSpotlightAttMult(dorg, lmi.coneDir, lmi.coneAngle);
              if (attn == 0.0f) continue;
            } else {
              attn = 1.0f;
            }
          }
          float add = (rad-ptdist)*attn;
          if (add <= 0.0f) continue;
          // without this, lights with huge radius will overbright everything
          if (add > 255.0f) add = 255.0f;
          // do more dynlight clipping
          if (needProperTrace) {
            //if (!lmi.spotLight) spt = lmi.calcTexPoint(starts+s*step, startt+t*step);
            if (((*spt)-dorg).length2DSquared() > 2.0f*2.0f) {
              const TVec &p2 = *spt;
              //const TVec p2 = (*spt)+surfOffs;
              if (!useBSPTrace) {
                if (!Level->CastLightRay((texCheck && dl.radius > texCheckMinRadius), &Level->Subsectors[dlinfo[lnum].leafnum], dorg, p2, surfsubsector)) {
                  #ifdef VV_DEBUG_BMAP_TRACER
                  if (!Level->TraceLine(Trace, dorg, p2, SPF_NOBLOCKSIGHT)) continue;
                  GCon->Logf(NAME_Debug, "TRACEvsTRACE: org=(%g,%g,%g); dest=(%g,%g,%g); bmap=BLOCK; bsp=NON-BLOCK", dorg.x, dorg.y, dorg.z, p2.x, p2.y, p2.z);
                  #endif
                  continue;
                }
                #ifdef VV_DEBUG_BMAP_TRACER
                else {
                  if (!Level->TraceLine(Trace, dorg, p2, SPF_NOBLOCKSIGHT)) {
                    GCon->Logf(NAME_Debug, "TRACEvsTRACE: org=(%g,%g,%g); dest=(%g,%g,%g); bmap=NON-BLOCK; bsp=BLOCK", dorg.x, dorg.y, dorg.z, p2.x, p2.y, p2.z);
                    continue;
                  }
                }
                #endif
              } else {
                if (!Level->TraceLine(Trace, dorg, p2, SPF_NOBLOCKSIGHT)) continue; // ray was blocked
              }
            }
          }
          int i = t*smax+s;
          lmtracer.blocklightsr[i] += rmul*add;
          lmtracer.blocklightsg[i] += gmul*add;
          lmtracer.blocklightsb[i] += bmul*add;
          if (dlcolor != 0xffffffff) lmtracer.isColored = true;
        }
      }
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateSurfacesLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateSurfacesLMaps (const TVec &org, float radius, surface_t *surf) {
  for (; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    if (!surf->SphereTouches(org, radius)) continue;
    if (!invalidateRelight) {
      if (surf->lightmap || surf->lightmap_rgb) {
        surf->drawflags |= surface_t::DF_CALC_LMAP;
      }
    } else {
      surf->drawflags |= surface_t::DF_CALC_LMAP;
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateLineLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateLineLMaps (const TVec &org, float radius, drawseg_t *dseg) {
  if (!dseg) return; // just in case
  const seg_t *seg = dseg->seg;
  if (!seg) return; // just in case

  if (!seg->linedef) return; // miniseg
  if (!seg->SphereTouches(org, radius)) return;

  if (dseg->mid) InvalidateSurfacesLMaps(org, radius, dseg->mid->surfs);
  if (seg->backsector) {
    // two sided line
    if (dseg->top) InvalidateSurfacesLMaps(org, radius, dseg->top->surfs);
    // no lightmaps on sky anyway
    //if (dseg->topsky) InvalidateSurfacesLMaps(org, radius, dseg->topsky->surfs);
    if (dseg->bot) InvalidateSurfacesLMaps(org, radius, dseg->bot->surfs);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      InvalidateSurfacesLMaps(org, radius, sp->surfs);
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateSubsectorLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateSubsectorLMaps (const TVec &org, float radius, int num) {
  subsector_t *sub = &Level->Subsectors[num];
  if (sub->isAnyPObj()) return;

  // polyobj
  if (sub->HasPObjs()) {
    // this is excessive invalidation for polyobj, but meh...
    for (auto &&it : sub->PObjFirst()) {
      for (auto &&sit : it.pobj()->SegFirst()) {
        const seg_t *seg = sit.seg();
        if (seg->linedef && seg->drawsegs) InvalidateLineLMaps(org, radius, seg->drawsegs);
      }
      //FIXME: make this faster!
      if (it.pobj()->Is3D()) {
        for (subsector_t *ss = it.pobj()->posector->subsectors; ss; ss = ss->seclink) {
          for (subregion_t *subregion = ss->regions; subregion; subregion = subregion->next) {
            if (subregion->realfloor) InvalidateSurfacesLMaps(org, radius, subregion->realfloor->surfs);
            if (subregion->realceil) InvalidateSurfacesLMaps(org, radius, subregion->realceil->surfs);
            if (subregion->fakefloor) InvalidateSurfacesLMaps(org, radius, subregion->fakefloor->surfs);
            if (subregion->fakeceil) InvalidateSurfacesLMaps(org, radius, subregion->fakeceil->surfs);
          }
        }
      }
    }
  }

  //TODO: invalidate only relevant segs
  if (sub->numlines > 0) {
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int j = sub->numlines; j--; ++seg) {
      if (!seg->linedef || !seg->drawsegs) continue; // miniseg has no drawsegs/segparts
      InvalidateLineLMaps(org, radius, seg->drawsegs);
    }
  }

  for (subregion_t *subregion = sub->regions; subregion; subregion = subregion->next) {
    if (subregion->realfloor) InvalidateSurfacesLMaps(org, radius, subregion->realfloor->surfs);
    if (subregion->realceil) InvalidateSurfacesLMaps(org, radius, subregion->realceil->surfs);
    if (subregion->fakefloor) InvalidateSurfacesLMaps(org, radius, subregion->fakefloor->surfs);
    if (subregion->fakeceil) InvalidateSurfacesLMaps(org, radius, subregion->fakeceil->surfs);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateBSPNodeLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateBSPNodeLMaps (const TVec &org, float radius, int bspnum, const float *bbox, bool noGeoClip) {
 tailcall:
#ifdef VV_CLIPPER_FULL_CHECK
  if (LightClip.ClipIsFull()) return;
#endif

  if (!LightClip.ClipLightIsBBoxVisible(bbox)) return;
  if (!CheckSphereVsAABBIgnoreZ(bbox, org, radius)) return;

  // found a subsector?
  if (BSPIDX_IS_NON_LEAF(bspnum)) {
    const node_t *bsp = &Level->Nodes[bspnum];
    // decide which side the light is on
    const float dist = bsp->PointDistance(org);
    if (dist >= radius) {
      // light is completely on the front side
      //return InvalidateBSPNodeLMaps(org, radius, bsp->children[0], bsp->bbox[0]);
      bspnum = bsp->children[0];
      bbox = bsp->bbox[0];
      goto tailcall;
    } else if (dist <= -radius) {
      // light is completely on the back side
      //return InvalidateBSPNodeLMaps(org, radius, bsp->children[1], bsp->bbox[1]);
      bspnum = bsp->children[1];
      bbox = bsp->bbox[1];
      goto tailcall;
    } else {
      unsigned side = (unsigned)(dist <= 0.0f);
      // recursively divide front space
      InvalidateBSPNodeLMaps(org, radius, bsp->children[side], bsp->bbox[side], noGeoClip);
      // possibly divide back space
      side ^= 1;
      //return InvalidateBSPNodeLMaps(org, radius, bsp->children[side], bsp->bbox[side]);
      bspnum = bsp->children[side];
      bbox = bsp->bbox[side];
      goto tailcall;
    }
  } else {
    subsector_t *sub = &Level->Subsectors[BSPIDX_LEAF_SUBSECTOR(bspnum)];
    if (!LightClip.ClipLightCheckSubsector(sub, false)) return;
    InvalidateSubsectorLMaps(org, radius, BSPIDX_LEAF_SUBSECTOR(bspnum));
    if (!noGeoClip) LightClip.ClipLightAddSubsectorSegs(sub, false);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateStaticLightmaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidateLightLMaps (const TVec &org, float radius, bool noGeoClip) {
  if (Level->NumSubsectors < 2) {
    if (Level->NumSubsectors == 1) return InvalidateSubsectorLMaps(org, radius, 0);
  } else {
    const float bbox[6] = { -999999.0f, -999999.0f, -999999.0f, +999999.0f, +999999.0f, +999999.0f };
    LightClip.ClearClipNodes(org, Level, radius);
    InvalidateBSPNodeLMaps(org, radius, Level->NumNodes-1, bbox, noGeoClip);
  }
}


/*
TODO:
keep list of static lights that can affect each wall and flat, and trigger
static lightmap recalc when the corresponding surface invalidates.
this will allow us to recalculate static lightmaps when the door opened, for
example.
*/

//==========================================================================
//
//  VRenderLevelLightmap::InvalidateStaticLightmaps
//
//  FIXME:POBJ:
//
//==========================================================================
void VRenderLevelLightmap::InvalidateStaticLightmaps (const TVec &org, float radius, bool relight, bool noGeoClip) {
  //FIXME: make this faster!
  if (radius < 2.0f) return;
  invalidateRelight = relight;
#if 0
  float bbox[6];
  subsector_t *sub = Level->Subsectors;
  for (int count = Level->NumSubsectors; count--; ++sub) {
    if (sub->isOriginalPObj()) continue;
    Level->GetSubsectorBBox(sub, bbox);
    if (!CheckSphereVsAABBIgnoreZ(bbox, org, radius)) continue;
    //GCon->Logf("invalidating subsector %d", (int)(ptrdiff_t)(sub-Level->Subsectors));
    InvalidateSubsectorLMaps(org, radius, (int)(ptrdiff_t)(sub-Level->Subsectors));
  }
#else
  InvalidateLightLMaps(org, radius, noGeoClip);
#endif
}


//==========================================================================
//
//  xblight
//
//==========================================================================
static VVA_ALWAYS_INLINE int xblight (const int add) noexcept {
  enum {
    minlight = 256,
    maxlight = 0xff00,
  };
  const int t = 255*256-add;
  return (t < minlight ? minlight : t > maxlight ? maxlight : t);
}


//==========================================================================
//
//  VRenderLevelLightmap::BuildLightMap
//
//  combine and scale multiple lightmaps into the 8.8 format in blocklights
//
//==========================================================================
void VRenderLevelLightmap::BuildLightMap (surface_t *surf) {
  if (surf->count < 3) {
    surf->drawflags &= ~surface_t::DF_CALC_LMAP;
    return;
  }

  if (surf->drawflags&surface_t::DF_CALC_LMAP) {
    //GCon->Logf("%p: Need to calculate static lightmap for subsector %p!", surf, surf->subsector);
    if (!IsStaticLightmapTimeLimitExpired() || !CanFaceBeStaticallyLit(surf)) LightFace(surf);
  }

  lmtracer.isColored = false;
  lmtracer.hasOverbright = false;
  int smax = (surf->extents[0]>>4)+1;
  int tmax = (surf->extents[1]>>4)+1;
  vassert(smax > 0);
  vassert(tmax > 0);
  if (smax > GridSize) smax = GridSize;
  if (tmax > GridSize) tmax = GridSize;
  int size = smax*tmax;
  const vuint8 *lightmap = surf->lightmap;
  const rgb_t *lightmap_rgb = surf->lightmap_rgb;

  // clear to ambient
  /*
  int t = getSurfLightLevelInt(surf);
  t <<= 8;
  int tR = ((surf->Light>>16)&255)*t/255;
  int tG = ((surf->Light>>8)&255)*t/255;
  int tB = (surf->Light&255)*t/255;
  if (tR != tG || tR != tB) lmtracer.isColored = true;

  for (int i = 0; i < size; ++i) {
    lmtracer.blocklightsr[i] = tR;
    lmtracer.blocklightsg[i] = tG;
    lmtracer.blocklightsb[i] = tB;
    lmtracer.blockaddlightsr[i] = lmtracer.blockaddlightsg[i] = lmtracer.blockaddlightsb[i] = 0;
  }
  */

  memset(lmtracer.blocklightsr, 0, sizeof(lmtracer.blocklightsr[0])*(unsigned)size);
  memset(lmtracer.blocklightsg, 0, sizeof(lmtracer.blocklightsg[0])*(unsigned)size);
  memset(lmtracer.blocklightsb, 0, sizeof(lmtracer.blocklightsb[0])*(unsigned)size);

  // sum lightmaps
  if (lightmap_rgb) {
    if (!lightmap) Sys_Error("RGB lightmap without uncolored lightmap");
    lmtracer.isColored = true;
    for (int i = 0; i < size; ++i) {
      //blocklights[i] += lightmap[i]<<8;
      lmtracer.blocklightsr[i] += lightmap_rgb[i].r<<8;
      lmtracer.blocklightsg[i] += lightmap_rgb[i].g<<8;
      lmtracer.blocklightsb[i] += lightmap_rgb[i].b<<8;
      /*if (!overbright)*/ {
        if (lmtracer.blocklightsr[i] > 0xffff) lmtracer.blocklightsr[i] = 0xffff;
        if (lmtracer.blocklightsg[i] > 0xffff) lmtracer.blocklightsg[i] = 0xffff;
        if (lmtracer.blocklightsb[i] > 0xffff) lmtracer.blocklightsb[i] = 0xffff;
      }
    }
  } else if (lightmap) {
    for (int i = 0; i < size; ++i) {
      const int t = lightmap[i]<<8;
      //blocklights[i] += t;
      lmtracer.blocklightsr[i] += t;
      lmtracer.blocklightsg[i] += t;
      lmtracer.blocklightsb[i] += t;
      /*if (!overbright)*/ {
        if (lmtracer.blocklightsr[i] > 0xffff) lmtracer.blocklightsr[i] = 0xffff;
        if (lmtracer.blocklightsg[i] > 0xffff) lmtracer.blocklightsg[i] = 0xffff;
        if (lmtracer.blocklightsb[i] > 0xffff) lmtracer.blocklightsb[i] = 0xffff;
      }
    }
  }

  // add all the dynamic lights
  if (surf->dlightframe == currDLightFrame) AddDynamicLights(surf);

  // bound, invert, and shift
  for (unsigned i = 0; i < (unsigned)size; ++i) {
    lmtracer.blocklightsr[i] = xblight((int)lmtracer.blocklightsr[i]);
    lmtracer.blocklightsg[i] = xblight((int)lmtracer.blocklightsg[i]);
    lmtracer.blocklightsb[i] = xblight((int)lmtracer.blocklightsb[i]);
  }


  #ifdef VV_EXPERIMENTAL_LMAP_FILTER
  enum {
    minlight = 256,
    maxlight = 0xff00,
  };

  #define DO_ONE_LMFILTER(lmc_)  do { \
    int v = \
      lmc_[pos-1]+lmc_[pos]+lmc_[pos+1]+ \
      lmc_[pos-1-smax]+lmc_[pos-smax]+lmc_[pos+1-smax]+ \
      lmc_[pos-1+smax]+lmc_[pos+smax]+lmc_[pos+1+smax]; \
    v /= 9; \
    if (v < minlight) v = minlight; else if (v > maxlight) v = maxlight; \
    lmc_ ## New[pos] = v; \
  } while (0)

  if (smax > 2 && tmax > 2) {
    for (int j = 1; j < tmax-1; ++j) {
      unsigned pos = j*smax+1;
      lmtracer.blocklightsrNew[pos-1] = lmtracer.blocklightsr[pos-1];
      lmtracer.blocklightsgNew[pos-1] = lmtracer.blocklightsg[pos-1];
      lmtracer.blocklightsbNew[pos-1] = lmtracer.blocklightsb[pos-1];
      for (int i = 1; i < smax-1; ++i, ++pos) {
        DO_ONE_LMFILTER(lmtracer.blocklightsr);
        DO_ONE_LMFILTER(lmtracer.blocklightsg);
        DO_ONE_LMFILTER(lmtracer.blocklightsb);
      }
      lmtracer.blocklightsrNew[pos] = lmtracer.blocklightsr[pos];
      lmtracer.blocklightsgNew[pos] = lmtracer.blocklightsg[pos];
      lmtracer.blocklightsbNew[pos] = lmtracer.blocklightsb[pos];
    }
    const unsigned sz = (unsigned)(smax*(tmax-2));
    if (sz) {
      memcpy(lmtracer.blocklightsr+smax, lmtracer.blocklightsrNew+smax, sz*sizeof(lmtracer.blocklightsr[0]));
      memcpy(lmtracer.blocklightsg+smax, lmtracer.blocklightsgNew+smax, sz*sizeof(lmtracer.blocklightsg[0]));
      memcpy(lmtracer.blocklightsb+smax, lmtracer.blocklightsbNew+smax, sz*sizeof(lmtracer.blocklightsb[0]));
    }
  }
  #endif
}


//==========================================================================
//
//  VRenderLevelLightmap::FlushCaches
//
//==========================================================================
void VRenderLevelLightmap::FlushCaches () {
  lmcache.resetAllBlocks();
  lmcache.reset();
  nukeLightmapsOnNextFrame = false;
  advanceCacheFrame(); // reset all chains
}


//==========================================================================
//
//  VRenderLevelLightmap::NukeLightmapCache
//
//==========================================================================
void VRenderLevelLightmap::NukeLightmapCache () {
  //GCon->Logf(NAME_Warning, "nuking lightmap atlases");
  // nuke all lightmap caches
  FlushCaches();
}


//==========================================================================
//
//  VRenderLevelLightmap::FreeSurfCache
//
//==========================================================================
void VRenderLevelLightmap::FreeSurfCache (surfcache_t *&block) {
  if (block) {
    lmcache.freeBlock((VLMapCache::Item *)block, true);
    block = nullptr;
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::BuildSurfaceLightmap
//
//  returns `false` if cannot allocate lightmap block
//
//==========================================================================
bool VRenderLevelLightmap::BuildSurfaceLightmap (surface_t *surface) {
  // see if the cache holds appropriate data
  //surfcache_t *cache = surface->CacheSurf;
  VLMapCache::Item *cache = (VLMapCache::Item *)surface->CacheSurf;

  const vuint32 srflight = fixSurfLightLevel(surface);

  if (cache) {
    if (cache->lastframe == lmcache.cacheframecount) {
      if (r_draw_queue_warnings) GCon->Log(NAME_Warning, "duplicate surface caching (for lighting)");
      return true;
    }
    if (!(surface->drawflags&surface_t::DF_CALC_LMAP) || IsStaticLightmapTimeLimitExpired()) {
      if (!cache->dlight && surface->dlightframe != currDLightFrame && cache->Light == srflight) {
        chainLightmap(cache);
        //GCon->Logf(NAME_Debug, "unchanged lightmap %p for surface %p", cache, surface);
        return true;
      }
    }
  }

  // determine shape of surface
  int smax = (surface->extents[0]>>4)+1;
  int tmax = (surface->extents[1]>>4)+1;
  vassert(smax > 0);
  vassert(tmax > 0);
  if (smax > GridSize) smax = GridSize;
  if (tmax > GridSize) tmax = GridSize;

  // allocate memory if needed
  // if a texture just animated, don't reallocate it
  if (!cache) {
    cache = lmcache.allocBlock(smax, tmax);
    // in rare case of surface cache overflow, just skip the light
    if (!cache) return false; // alas
    surface->CacheSurf = cache;
    cache->owner = (VLMapCache::Item **)&surface->CacheSurf;
    cache->surf = surface;
    //GCon->Logf(NAME_Debug, "new lightmap %p for surface %p (bnum=%u)", cache, surface, cache->blocknum);
  } else {
    //GCon->Logf(NAME_Debug, "old lightmap %p for surface %p (bnum=%u)", cache, surface, cache->blocknum);
    vassert(surface->CacheSurf == cache);
    vassert(cache->surf == surface);
  }

  cache->dlight = (surface->dlightframe == currDLightFrame);
  cache->Light = srflight;

  // calculate the lightings
  BuildLightMap(surface);

  vassert(cache->t >= 0);
  vassert(cache->s >= 0);

  const vuint32 bnum = cache->atlasid;

  // normal lightmap
  rgba_t *lbp = &light_block[bnum][cache->t*BLOCK_WIDTH+cache->s];
  unsigned blpos = 0;
  for (int y = 0; y < tmax; ++y, lbp += BLOCK_WIDTH) {
    rgba_t *dlbp = lbp;
    for (int x = 0; x < smax; ++x, ++dlbp, ++blpos) {
      dlbp->r = 255-clampToByte(lmtracer.blocklightsr[blpos]>>8);
      dlbp->g = 255-clampToByte(lmtracer.blocklightsg[blpos]>>8);
      dlbp->b = 255-clampToByte(lmtracer.blocklightsb[blpos]>>8);
      dlbp->a = 255;
    }
  }
  chainLightmap(cache);
  block_dirty[bnum].addDirty(cache->s, cache->t, smax, tmax);

  return true;
}


//==========================================================================
//
//  VRenderLevelLightmap::ProcessCachedSurfaces
//
//  this is called after surface queues built, so lightmap
//  renderer can calculate new lightmaps
//
//==========================================================================
void VRenderLevelLightmap::ProcessCachedSurfaces () {
  if (LMSurfList.length() == 0) return; // nothing to do here

  if (nukeLightmapsOnNextFrame) {
    if (dbg_show_lightmap_cache_messages) GCon->Log(NAME_Debug, "LIGHTMAP: *** previous frame requested lightmaps nuking");
    FlushCaches();
  }

  // first pass, try to perform normal allocation
  bool success = true;
  for (auto &&sfc : LMSurfList) {
    if (!BuildSurfaceLightmap(sfc)) {
      success = false;
      break;
    }
  }
  if (success) {
    // all surfaces succesfully lightmapped
    // if we used more lightmaps than allowed, nuke lightmap cache on the next frame
    const int lim = r_lmap_atlas_limit.asInt();
    if (lim > 0 && lmcache.getAtlasCount() > lim) nukeLightmapsOnNextFrame = true;
    return;
  }

  // second pass, nuke all lightmap caches, and do it all again
  GCon->Log(NAME_Warning, "LIGHTMAP: *** out of surface cache blocks, retrying");
  FlushCaches();
  for (auto &&sfc : LMSurfList) {
    if (!BuildSurfaceLightmap(sfc)) {
      // render this surface as non-lightmapped: it is better than completely losing it
      QueueSimpleSurf(sfc);
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::QueueLMapSurface
//
//==========================================================================
bool VRenderLevelLightmap::QueueLMapSurface (surface_t *surf) {
  // HACK: return `true` for invalid surfaces, so they won't be queued as normal ones
  //if (!SurfPrepareForRender(surf)) return true; // should be done by the caller
  if (!surf->IsPlVisible()) return true;
  vassert(surf->count >= 3);
  // remember this surface, it will be processed later
  LMSurfList.append(surf);
  return true;
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateStaticLightmapsSurfaces
//
//==========================================================================
void VRenderLevelLightmap::InvalidateStaticLightmapsSurfaces (surface_t *surf) {
  for (; surf; surf = surf->next) {
    if (surf->count < 3) continue; // just in case
    // mark only surfaces with already built lightmaps (it is faster this way)
    /*if (surf->lightmap || surf->lightmap_rgb)*/ {
      surf->drawflags |= surface_t::DF_CALC_LMAP;
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateStaticLightmapsLine
//
//==========================================================================
void VRenderLevelLightmap::InvalidateStaticLightmapsLine (drawseg_t *dseg) {
  if (!dseg) return; // just in case
  const seg_t *seg = dseg->seg;
  if (!seg) return; // just in case

  if (!seg->linedef) return; // miniseg

  if (dseg->mid) InvalidateStaticLightmapsSurfaces(dseg->mid->surfs);
  if (seg->backsector) {
    // two sided line
    if (dseg->top) InvalidateStaticLightmapsSurfaces(dseg->top->surfs);
    // no lightmaps on sky anyway
    //if (dseg->topsky) InvalidateSurfacesLMaps(org, radius, dseg->topsky->surfs);
    if (dseg->bot) InvalidateStaticLightmapsSurfaces(dseg->bot->surfs);
    for (segpart_t *sp = dseg->extra; sp; sp = sp->next) {
      InvalidateStaticLightmapsSurfaces(sp->surfs);
    }
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateStaticLightmapsSubsector
//
//==========================================================================
void VRenderLevelLightmap::InvalidateStaticLightmapsSubsector (subsector_t *sub) {
  if (sub->isOriginalPObj()) return;

  // polyobj
  if (sub->HasPObjs()) {
    // this is excessive invalidation for polyobj, but meh...
    for (auto &&it : sub->PObjFirst()) {
      for (auto &&sit : it.pobj()->SegFirst()) {
        const seg_t *seg = sit.seg();
        if (seg->linedef && seg->drawsegs) InvalidateStaticLightmapsLine(seg->drawsegs);
      }
      //FIXME: make this faster!
      if (it.pobj()->Is3D()) {
        for (subsector_t *ss = it.pobj()->posector->subsectors; ss; ss = ss->seclink) {
          for (subregion_t *subregion = ss->regions; subregion; subregion = subregion->next) {
            if (subregion->realfloor) InvalidateStaticLightmapsSurfaces(subregion->realfloor->surfs);
            if (subregion->realceil) InvalidateStaticLightmapsSurfaces(subregion->realceil->surfs);
            if (subregion->fakefloor) InvalidateStaticLightmapsSurfaces(subregion->fakefloor->surfs);
            if (subregion->fakeceil) InvalidateStaticLightmapsSurfaces(subregion->fakeceil->surfs);
          }
        }
      }
    }
  }

  //TODO: invalidate only relevant segs
  if (sub->numlines > 0) {
    const seg_t *seg = &Level->Segs[sub->firstline];
    for (int j = sub->numlines; j--; ++seg) {
      if (!seg->linedef || !seg->drawsegs) continue; // miniseg has no drawsegs/segparts
      InvalidateStaticLightmapsLine(seg->drawsegs);
    }
  }

  for (subregion_t *subregion = sub->regions; subregion; subregion = subregion->next) {
    if (subregion->realfloor) InvalidateStaticLightmapsSurfaces(subregion->realfloor->surfs);
    if (subregion->realceil) InvalidateStaticLightmapsSurfaces(subregion->realceil->surfs);
    if (subregion->fakefloor) InvalidateStaticLightmapsSurfaces(subregion->fakefloor->surfs);
    if (subregion->fakeceil) InvalidateStaticLightmapsSurfaces(subregion->fakeceil->surfs);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidateStaticLightmapsSubs
//
//==========================================================================
void VRenderLevelLightmap::InvalidateStaticLightmapsSubs (subsector_t *sub) {
  if (!sub) return;

  const int snum = (int)(ptrdiff_t)(sub-&Level->Subsectors[0]);
  SubStaticLigtInfo *si = &SubStaticLights[snum];
  if (si->invalidateFrame == currDLightFrame) return; // already processed

  // mark subsector to avoid endless recursion
  si->invalidateFrame = currDLightFrame;

  // invalidate subsector lightmaps
  InvalidateStaticLightmapsSubsector(sub);

  // recursively process all touching static lights
  for (auto it : si->touchedStatic.first()) {
    const int stidx = it.getKey();
    if (stidx < 0 || stidx >= Lights.length()) continue; // just in case
    light_t *lt = &Lights.ptr()[stidx];
    if (!lt->active || lt->radius < 2) return;
    // check if it is already processed
    if (lt->invalidateFrame == currDLightFrame) continue;
    // mark it
    lt->invalidateFrame = currDLightFrame;
    // and recurse
    for (auto &&ltsub : lt->touchedSubs) InvalidateStaticLightmapsSubs(ltsub);
  }
}


//==========================================================================
//
//  VRenderLevelLightmap::InvalidatePObjLMaps
//
//==========================================================================
void VRenderLevelLightmap::InvalidatePObjLMaps (polyobj_t *po) {
  if (!po) return; // just in case
  // invalidate 3d pobj flats
  if (po->Is3D()) {
    for (subsector_t *ss = po->posector->subsectors; ss; ss = ss->seclink) {
      for (subregion_t *subregion = ss->regions; subregion; subregion = subregion->next) {
        if (subregion->realfloor) InvalidateStaticLightmapsSurfaces(subregion->realfloor->surfs);
        if (subregion->realceil) InvalidateStaticLightmapsSurfaces(subregion->realceil->surfs);
        if (subregion->fakefloor) InvalidateStaticLightmapsSurfaces(subregion->fakefloor->surfs);
        if (subregion->fakeceil) InvalidateStaticLightmapsSurfaces(subregion->fakeceil->surfs);
      }
    }
  }
  // invalidate pobj segs
  for (auto &&sit : po->SegFirst()) {
    const seg_t *seg = sit.seg();
    if (seg->linedef && seg->drawsegs) InvalidateStaticLightmapsLine(seg->drawsegs);
  }
  // no need to invalidate surrounding subsectors, pobj world linking will do it for us
}
