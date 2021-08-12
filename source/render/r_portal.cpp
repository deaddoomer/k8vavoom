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
//**
//**  Portals.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../psim/p_entity.h"
#include "r_local.h"

//#define VV_OLD_SKYPORTAL_CODE


extern VCvarB gl_dbg_wireframe;
extern VCvarB clip_frustum;

static VCvarB r_dbg_mirror_use_frustum("r_dbg_mirror_use_frustum", false, "Use frustum clipping for mirror portals? (DEBUG)", CVAR_Archive);
static VCvarB r_dbg_mirror_no_cliprange("r_dbg_mirror_no_cliprange", false, "Do not build narrowed cliprange for mirror portals? (DEBUG)", CVAR_Archive);

static VCvarB r_dbg_stack_use_frustum("r_dbg_stack_use_frustum", false, "Use frustum clipping for stack portals? (DEBUG)", CVAR_Archive);
static VCvarB r_dbg_stack_no_cliprange("r_dbg_stack_no_cliprange", false, "Do not build narrowed cliprange for stack portals? (DEBUG)", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// "autosave" struct to avoid some pasta
struct AutoSavedView {
  VRenderLevelShared *RLev;
  TVec SavedViewOrg;
  TAVec SavedViewAngles;
  TVec SavedViewForward;
  TVec SavedViewRight;
  TVec SavedViewUp;
  VEntity *SavedViewEnt;
  VPortal *SavedPortal;
  int SavedExtraLight;
  int SavedFixedLight;

  TClipPlane SavedClip;
  unsigned planeCount;
  bool SavedMirrorClip;
  TPlane SavedMirrorPlane;

  bool shadowsDisabled;
  bool oldFrustumClip;

  AutoSavedView () = delete;
  AutoSavedView (const AutoSavedView &) = delete;
  AutoSavedView &operator = (const AutoSavedView &) = delete;

  inline AutoSavedView (VRenderLevelShared *ARLev) noexcept {
    vassert(ARLev);
    vassert(Drawer);
    RLev = ARLev;

    SavedViewOrg = Drawer->vieworg;
    SavedViewAngles = Drawer->viewangles;
    SavedViewForward = Drawer->viewforward;
    SavedViewRight = Drawer->viewright;
    SavedViewUp = Drawer->viewup;
    SavedMirrorClip = Drawer->MirrorClip;
    SavedMirrorPlane = Drawer->MirrorPlane;
    SavedViewEnt = RLev->ViewEnt;
    SavedPortal = RLev->CurrPortal;
    SavedExtraLight = RLev->ExtraLight;
    SavedFixedLight = RLev->FixedLight;

    SavedClip = Drawer->viewfrustum.planes[TFrustum::Forward]; // save far/mirror plane
    planeCount = Drawer->viewfrustum.planeCount;

    shadowsDisabled = RLev->forceDisableShadows;
    oldFrustumClip = clip_frustum.asBool();
  }

  inline ~AutoSavedView () noexcept {
    // restore render settings
    Drawer->vieworg = SavedViewOrg;
    Drawer->viewangles = SavedViewAngles;
    Drawer->viewforward = SavedViewForward;
    Drawer->viewright = SavedViewRight;
    Drawer->viewup = SavedViewUp;
    Drawer->MirrorClip = SavedMirrorClip;
    Drawer->MirrorPlane = SavedMirrorPlane;

    // restore original frustum
    RLev->ViewEnt = SavedViewEnt;
    RLev->CurrPortal = SavedPortal;
    RLev->CallTransformFrustum();
    RLev->ExtraLight = SavedExtraLight;
    RLev->FixedLight = SavedFixedLight;
    Drawer->viewfrustum.planes[TFrustum::Forward] = SavedClip; // restore far/mirror plane
    Drawer->viewfrustum.planeCount = planeCount;

    RLev->forceDisableShadows = shadowsDisabled;
    clip_frustum = oldFrustumClip;

    // resetup view origin
    Drawer->SetupViewOrg();
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct AutoSavedBspVis {
  VRenderLevelShared *RLev;
  unsigned *SavedBspVis;
  unsigned *SavedBspVisSector;
  unsigned SavedBspVisFrame;
  VRenderLevelShared::PPMark SavedBspVisPMark;

  AutoSavedBspVis () = delete;
  AutoSavedBspVis (const AutoSavedView &) = delete;
  AutoSavedBspVis &operator = (const AutoSavedView &) = delete;

  inline AutoSavedBspVis (VRenderLevelShared *ARLev) noexcept {
    vassert(ARLev);
    RLev = ARLev;
    SavedBspVis = ARLev->BspVisData;
    SavedBspVisSector = ARLev->BspVisSectorData;
    SavedBspVisFrame = ARLev->BspVisFrame;
    VRenderLevelShared::MarkPortalPool(&SavedBspVisPMark);
    // notify allocator about minimal node size
    VRenderLevelShared::SetMinPoolNodeSize((ARLev->Level->NumSubsectors+ARLev->Level->NumSectors+2)*sizeof(unsigned));
    // allocate new bsp vis
    ARLev->BspVisData = (unsigned *)VRenderLevelShared::AllocPortalPool((ARLev->Level->NumSubsectors+ARLev->Level->NumSectors+2)*sizeof(unsigned));
    ARLev->BspVisSectorData = ARLev->BspVisData+ARLev->Level->NumSubsectors+1;
    memset(ARLev->BspVisData, 0, (ARLev->Level->NumSubsectors+1)*sizeof(ARLev->BspVisData[0]));
    memset(ARLev->BspVisSectorData, 0, (ARLev->Level->NumSectors+1)*sizeof(ARLev->BspVisSectorData[0]));
    ARLev->PushDrawLists();
  }

  inline ~AutoSavedBspVis () noexcept {
    if (RLev) {
      RLev->BspVisData = SavedBspVis;
      RLev->BspVisSectorData = SavedBspVisSector;
      RLev->BspVisFrame = SavedBspVisFrame;
      VRenderLevelShared::RestorePortalPool(&SavedBspVisPMark);
      RLev->PopDrawLists();
      RLev = nullptr;
    }
  }
};



//**************************************************************************
//
// VPortal
//
//**************************************************************************

//==========================================================================
//
//  VPortal::VPortal
//
//==========================================================================
VPortal::VPortal (VRenderLevelShared *ARLev) noexcept
  : RLev(ARLev)
  , needBBox(true)
{
  Level = RLev->PortalLevel+1;
  memset((void *)bbox3d, 0, sizeof(float)*6);
}


//==========================================================================
//
//  VPortal::~VPortal
//
//==========================================================================
VPortal::~VPortal () {
}


//==========================================================================
//
//  VPortal::NeedsDepthBuffer
//
//==========================================================================
bool VPortal::NeedsDepthBuffer () const noexcept {
  return true;
}


//==========================================================================
//
//  VPortal::IsSky
//
//==========================================================================
bool VPortal::IsSky () const noexcept {
  return false;
}


//==========================================================================
//
//  VPortal::IsSkyBox
//
//==========================================================================
bool VPortal::IsSkyBox () const noexcept {
  return false;
}


//==========================================================================
//
//  VPortal::IsMirror
//
//==========================================================================
bool VPortal::IsMirror () const noexcept {
  return false;
}


//==========================================================================
//
//  VPortal::IsStack
//
//==========================================================================
bool VPortal::IsStack () const noexcept {
  return false;
}


//==========================================================================
//
//  VPortal::MatchSky
//
//==========================================================================
bool VPortal::MatchSky (VSky *) const noexcept {
  return false;
}


//==========================================================================
//
//  VPortal::MatchSkyBox
//
//==========================================================================
bool VPortal::MatchSkyBox (VEntity *) const noexcept {
  return false;
}


//==========================================================================
//
//  VPortal::MatchMirror
//
//==========================================================================
bool VPortal::MatchMirror (const TPlane *) const noexcept {
  return false;
}


//==========================================================================
//
//  VPortal::AppendSurface
//
//  this also updates bbox
//
//==========================================================================
void VPortal::AppendSurface (surface_t *surf) noexcept {
  if (!surf || surf->count < 3) return; // just in case
  // first surface?
  if (Surfs.length() == 0) {
    bbox3d[BOX3D_MINX] = bbox3d[BOX3D_MINY] = bbox3d[BOX3D_MINZ] = +FLT_MAX;
    bbox3d[BOX3D_MAXX] = bbox3d[BOX3D_MAXY] = bbox3d[BOX3D_MAXZ] = -FLT_MAX;
  }
  Surfs.Append(surf);
  // update bbox
  if (needBBox) {
    const SurfVertex *sv = surf->verts;
    for (int f = surf->count; f--; ++sv) {
      bbox3d[BOX3D_MINX] = min2(bbox3d[BOX3D_MINX], sv->x);
      bbox3d[BOX3D_MINY] = min2(bbox3d[BOX3D_MINY], sv->y);
      bbox3d[BOX3D_MINZ] = min2(bbox3d[BOX3D_MINZ], sv->z);
      bbox3d[BOX3D_MAXX] = max2(bbox3d[BOX3D_MAXX], sv->x);
      bbox3d[BOX3D_MAXY] = max2(bbox3d[BOX3D_MAXY], sv->y);
      bbox3d[BOX3D_MAXZ] = max2(bbox3d[BOX3D_MAXZ], sv->z);
    }
  }
}


//==========================================================================
//
//  VPortal::Draw
//
//==========================================================================
void VPortal::Draw (bool UseStencil) {
  if (gl_dbg_wireframe) return;

  if (!Drawer->StartPortal(this, UseStencil)) {
    // all portal polygons are clipped away
    //GCon->Logf("portal is clipped away (stencil:%d)", (int)UseStencil);
    return;
  }
  //GCon->Logf("doing portal (stencil:%d)", (int)UseStencil);

  {
    // save renderer settings
    AutoSavedView guard(RLev/*, !IsSky()*/);
    RLev->CurrPortal = this; // will be restored by the guard
    RLev->forceDisableShadows = true; // will be restored by the guard
    clip_frustum = false; // anyway
    DrawContents();
  }

  Drawer->EndPortal(this, UseStencil);
}


//==========================================================================
//
//  VPortal::SetupRanges
//
//==========================================================================
void VPortal::SetupRanges (const refdef_t &refdef, VViewClipper &Range, bool Revert, bool SetFrustum) {
  Range.ClearClipNodes(Drawer->vieworg, RLev->Level);
  if (SetFrustum) {
    Range.ClipInitFrustumRange(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup, refdef.fovx, refdef.fovy);
    //GCon->Logf("SURFS: %d", Surfs.length());
    //return;
  }
  for (auto &&surf : Surfs) {
    if (surf->GetNormalZ() == 0.0f) {
      // wall
      seg_t *Seg = surf->seg;
      vassert(Seg);
      vassert(Seg >= RLev->Level->Segs);
      vassert(Seg < RLev->Level->Segs+RLev->Level->NumSegs);
      if (Revert) {
        Range.AddClipRange(*Seg->v1, *Seg->v2);
      } else {
        Range.AddClipRange(*Seg->v2, *Seg->v1);
      }
    } else {
      //k8: do we need to do this?
      #if 1
      //if (Revert) continue; //???
      // floor/ceiling
      for (int j = 0; j < surf->count; ++j) {
        TVec v1, v2;
        if ((surf->GetNormalZ() < 0.0f) != Revert) {
          v1 = surf->verts[j < surf->count-1 ? j+1 : 0].vec();
          v2 = surf->verts[j].vec();
        } else {
          v1 = surf->verts[j].vec();
          v2 = surf->verts[j < surf->count-1 ? j+1 : 0].vec();
        }
        TVec Dir = v2-v1;
        Dir.z = 0;
        if (fabsf(Dir.x) < 0.01f && fabsf(Dir.y) < 0.01f) continue; // too short
        TPlane P;
        P.SetPointDirXY(v1, Dir);
        if ((P.PointDistance(Drawer->vieworg) < 0.01f) != Revert) continue; // view origin is on the back side
        Range.AddClipRange(v2, v1);
      }
      #endif
    }
  }
}



//**************************************************************************
//
// VSkyPortal
//
//**************************************************************************

//==========================================================================
//
//  VSkyPortal::NeedsDepthBuffer
//
//==========================================================================
bool VSkyPortal::NeedsDepthBuffer () const noexcept {
  return false;
}


//==========================================================================
//
//  VSkyPortal::IsSky
//
//==========================================================================
bool VSkyPortal::IsSky () const noexcept {
  return true;
}


//==========================================================================
//
//  VSkyPortal::MatchSky
//
//==========================================================================
bool VSkyPortal::MatchSky (VSky *ASky) const noexcept {
  return (Level == RLev->PortalLevel+1 && Sky == ASky);
}


//==========================================================================
//
//  VSkyPortal::DrawContents
//
//  TODO: use scissor for secondary skies
//
//==========================================================================
void VSkyPortal::DrawContents () {
  //GCon->Logf(NAME_Debug, "VSkyPortal::DrawContents!");
  Drawer->vieworg = TVec(0, 0, 0);
  RLev->TransformFrustum();
  Drawer->SetupViewOrg();

  Sky->Draw(RLev->ColorMap);

  // this should not be the case (render lists should be empty)
  #ifdef VV_OLD_SKYPORTAL_CODE
  Drawer->DrawLightmapWorld();
  #endif
}



//**************************************************************************
//
// VSkyBoxPortal
//
//**************************************************************************

//==========================================================================
//
//  VSkyBoxPortal::IsSky
//
//==========================================================================
bool VSkyBoxPortal::IsSky () const noexcept {
  return true;
}


//==========================================================================
//
//  VSkyBoxPortal::IsSkyBox
//
//==========================================================================
bool VSkyBoxPortal::IsSkyBox () const noexcept {
  return true;
}


//==========================================================================
//
//  VSkyBoxPortal::MatchSkyBox
//
//==========================================================================
bool VSkyBoxPortal::MatchSkyBox (VEntity *AEnt) const noexcept {
  return (Level == RLev->PortalLevel+1 && Viewport == AEnt);
}


//==========================================================================
//
//  VSkyBoxPortal::DrawContents
//
//  TODO: use scissor for secondary skies
//
//==========================================================================
void VSkyBoxPortal::DrawContents () {
  if (gl_dbg_wireframe) return;

  // set view origin to be sky view origin
  RLev->ViewEnt = Viewport;
  Drawer->vieworg = Viewport->Origin;
  Drawer->viewangles.yaw += Viewport->Angles.yaw;
  AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);

  const int oldExtraLight = RLev->ExtraLight;
  const int oldFixedLight = RLev->FixedLight;
  const int oldFullbrightThings = RLev->FullbrightThings;

  // no light flashes in the sky
  RLev->ExtraLight = 0;
  //if (RLev->ColorMap == CM_Default) RLev->FixedLight = 0;
  if (!RLev->ColorMapFixedLight) RLev->FixedLight = 0;
  RLev->FullbrightThings = 0; //FIXME

  {
    // prevent recursion
    VEntity::AutoPortalDirty guard(Viewport);
    AutoSavedBspVis bspvisguard(RLev);
    refdef_t rd = RLev->refdef;
    RLev->CurrPortal = this;
    RLev->RenderScene(&rd, nullptr);
  }

  RLev->ExtraLight = oldExtraLight;
  RLev->FixedLight = oldFixedLight;
  RLev->FullbrightThings = oldFullbrightThings;
}



//**************************************************************************
//
// VSectorStackPortal
//
//**************************************************************************

//==========================================================================
//
//  VSectorStackPortal::IsStack
//
//==========================================================================
bool VSectorStackPortal::IsStack () const noexcept {
  return true;
}


//==========================================================================
//
//  VSectorStackPortal::MatchSkyBox
//
//==========================================================================
bool VSectorStackPortal::MatchSkyBox (VEntity *AEnt) const noexcept {
  return (Level == RLev->PortalLevel+1 && Viewport == AEnt);
}


//==========================================================================
//
//  VSectorStackPortal::DrawContents
//
//==========================================================================
void VSectorStackPortal::DrawContents () {
  VViewClipper Range;
  refdef_t rd = RLev->refdef;
  // this range is used to clip away everything that is not "filled", hence "no revert" here
  // frustum doesn't matter, so don't bother initialising it too
  if (r_dbg_stack_no_cliprange.asBool()) {
    Range.ClearClipNodes(Drawer->vieworg, RLev->Level);
    if (r_dbg_stack_use_frustum.asBool()) {
      Range.ClipInitFrustumRange(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup, rd.fovx, rd.fovy);
    }
  } else {
    SetupRanges(rd, Range, false/*revert*/, r_dbg_stack_use_frustum.asBool()/*setfrustum*/);
  }

  RLev->ViewEnt = Viewport;
  VEntity *Mate = Viewport->GetSkyBoxMate();

  //GCon->Logf(NAME_Debug, "rendering portal contents; offset=(%f,%f)", Viewport->Origin.x-Mate->Origin.x, Viewport->Origin.y-Mate->Origin.y);

  Drawer->vieworg.x = Drawer->vieworg.x+Viewport->Origin.x-Mate->Origin.x;
  Drawer->vieworg.y = Drawer->vieworg.y+Viewport->Origin.y-Mate->Origin.y;

  // prevent recursion
  VEntity::AutoPortalDirty guard(Viewport);
  AutoSavedBspVis bspvisguard(RLev);
  RLev->CurrPortal = this;
  RLev->RenderScene(&rd, &Range);
}



//**************************************************************************
//
// VMirrorPortal
//
//**************************************************************************

//==========================================================================
//
//  VMirrorPortal::IsMirror
//
//==========================================================================
bool VMirrorPortal::IsMirror () const noexcept {
  return true;
}


//==========================================================================
//
//  VMirrorPortal::MatchMirror
//
//==========================================================================
bool VMirrorPortal::MatchMirror (const TPlane *APlane) const noexcept {
  return (Level == RLev->PortalLevel+1 && Plane.dist == APlane->dist && Plane.normal == APlane->normal);
}


//==========================================================================
//
//  VMirrorPortal::DrawContents
//
//==========================================================================
void VMirrorPortal::DrawContents () {
  RLev->ViewEnt = nullptr;

  ++RLev->MirrorLevel;
  Drawer->MirrorFlip = RLev->MirrorLevel&1;
  Drawer->MirrorClip = true;
  Drawer->MirrorPlane = Plane;

  float Dist = Plane.PointDistance(Drawer->vieworg);
  Drawer->vieworg -= 2.0f*Dist*Plane.normal;

  Dist = Drawer->viewforward.dot(Plane.normal);
  Drawer->viewforward -= 2.0f*Dist*Plane.normal;
  Dist = Drawer->viewright.dot(Plane.normal);
  Drawer->viewright -= 2.0f*Dist*Plane.normal;
  Dist = Drawer->viewup.dot(Plane.normal);
  Drawer->viewup -= 2.0f*Dist*Plane.normal;

  // k8: i added this, but i don't know if it is required
  Drawer->viewforward.normaliseInPlace();
  Drawer->viewright.normaliseInPlace();
  Drawer->viewup.normaliseInPlace();

  VectorsAngles(Drawer->viewforward, (Drawer->MirrorFlip ? -Drawer->viewright : Drawer->viewright), Drawer->viewup, Drawer->viewangles);

  refdef_t rd = RLev->refdef;
  VViewClipper Range;
  if (r_dbg_mirror_no_cliprange.asBool()) {
    Range.ClearClipNodes(Drawer->vieworg, RLev->Level);
    if (r_dbg_mirror_use_frustum.asBool()) {
      Range.ClipInitFrustumRange(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup, rd.fovx, rd.fovy);
    }
  } else {
    SetupRanges(rd, Range, true/*revert*/, r_dbg_mirror_use_frustum.asBool()/*setfrustum*/);
  }

  {
    AutoSavedBspVis bspvisguard(RLev);
    RLev->RenderScene(&rd, &Range);
  }

  --RLev->MirrorLevel;
  Drawer->MirrorFlip = RLev->MirrorLevel&1;
}
