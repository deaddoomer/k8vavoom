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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#include "gamedefs.h"
#include "r_local.h"


//==========================================================================
//
//  VPortal::VPortal
//
//==========================================================================
VPortal::VPortal (VRenderLevelShared *ARLev)
  : RLev(ARLev)
  , stackedSector(false)
{
  Level = RLev->PortalLevel+1;
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
bool VPortal::NeedsDepthBuffer () const {
  return true;
}


//==========================================================================
//
//  VPortal::IsSky
//
//==========================================================================
bool VPortal::IsSky () const {
  return false;
}


//==========================================================================
//
//  VPortal::MatchSky
//
//==========================================================================
bool VPortal::MatchSky (VSky *) const {
  return false;
}


//==========================================================================
//
//  VPortal::MatchSkyBox
//
//==========================================================================
bool VPortal::MatchSkyBox (VEntity *) const {
  return false;
}


//==========================================================================
//
//  VPortal::MatchMirror
//
//==========================================================================
bool VPortal::MatchMirror (TPlane *) const {
  return false;
}


//==========================================================================
//
//  VPortal::Draw
//
//==========================================================================
void VPortal::Draw (bool UseStencil) {
  guard(VPortal::Draw);

  if (!Drawer->StartPortal(this, UseStencil)) {
    // all portal polygons are clipped away
    //GCon->Logf("portal is clipped away");
    return;
  }

  // save renderer settings
  TVec SavedViewOrg = vieworg;
  TAVec SavedViewAngles = viewangles;
  TVec SavedViewForward = viewforward;
  TVec SavedViewRight = viewright;
  TVec SavedViewUp = viewup;
  VEntity *SavedViewEnt = RLev->ViewEnt;
  int SavedExtraLight = RLev->ExtraLight;
  int SavedFixedLight = RLev->FixedLight;
  vuint8 *SavedBspVis = RLev->BspVis;
  auto savedTraspFirst = RLev->traspFirst;
  auto savedTraspUsed = RLev->traspUsed;
  bool SavedMirrorClip = MirrorClip;
  TClipPlane SavedClip = view_clipplanes[4];
  //TClipPlane *SavedClipLink = view_clipplanes[3].next;

  VRenderLevelShared::PPMark pmark;
  VRenderLevelShared::MarkPortalPool(&pmark);

  bool restoreVis = false;
  if (NeedsDepthBuffer()) {
    // set up BSP visibility table and translated sprites
    // this has to be done only for portals that do rendering of view

    // notify allocator about minimal node size
    VRenderLevelShared::SetMinPoolNodeSize(RLev->VisSize);
    // allocate new bsp vis
    RLev->BspVis = VRenderLevelShared::AllocPortalPool(RLev->VisSize);
    if (RLev->VisSize) memset(RLev->BspVis, 0, RLev->VisSize);
    //fprintf(stderr, "BSPVIS: size=%d\n", RLev->VisSize);

    // allocate new transsprites list
    RLev->traspFirst = RLev->traspUsed;
    restoreVis = true;
  }

  DrawContents();

  // restore render settings
  vieworg = SavedViewOrg;
  viewangles = SavedViewAngles;
  viewforward = SavedViewForward;
  viewright = SavedViewRight;
  viewup = SavedViewUp;
  RLev->ViewEnt = SavedViewEnt;
  RLev->ExtraLight = SavedExtraLight;
  RLev->FixedLight = SavedFixedLight;
  if (restoreVis) {
    RLev->BspVis = SavedBspVis;
    if (RLev->VisSize) memcpy(RLev->BspVisThing, RLev->BspVis, RLev->VisSize);
    RLev->traspFirst = savedTraspFirst;
    RLev->traspUsed = savedTraspUsed;
  }
  MirrorClip = SavedMirrorClip;
  view_clipplanes[4] = SavedClip;
  //view_clipplanes[3].next = SavedClipLink;
  RLev->TransformFrustum();
  Drawer->SetupViewOrg();

  Drawer->EndPortal(this, UseStencil);

  // restore ppol
  VRenderLevelShared::RestorePortalPool(&pmark);

  unguard;
}


//==========================================================================
//
//  VPortal::SetUpRanges
//
//==========================================================================
void VPortal::SetUpRanges (VViewClipper &Range, bool Revert, bool SetFrustum) {
  guard(VPortal::SetUpRanges);
  Range.ClearClipNodes(vieworg, RLev->Level);
  if (SetFrustum) {
    Range.ClipInitFrustrumRange(viewangles, viewforward, viewright, viewup, refdef.fovx, refdef.fovy);
    //GCon->Logf("SURFS: %d", Surfs.Num());
    //return;
  }
  for (int i = 0; i < Surfs.Num(); ++i) {
    if (Surfs[i]->plane->normal.z == 0) {
      // wall
      seg_t *Seg = (seg_t *)Surfs[i]->plane;
      check(Seg >= RLev->Level->Segs);
      check(Seg < RLev->Level->Segs+RLev->Level->NumSegs);
      float a1 = Range.PointToClipAngle(*Seg->v2);
      float a2 = Range.PointToClipAngle(*Seg->v1);
      if (Revert) {
        Range.AddClipRange(a2, a1);
      } else {
        Range.AddClipRange(a1, a2);
      }
    } else {
      // subsector
      for (int j = 0; j < Surfs[i]->count; ++j) {
        TVec v1, v2;
        if ((Surfs[i]->plane->normal.z < 0) != Revert) {
          v1 = Surfs[i]->verts[j < Surfs[i]->count-1 ? j+1 : 0];
          v2 = Surfs[i]->verts[j];
        } else {
          v1 = Surfs[i]->verts[j];
          v2 = Surfs[i]->verts[j < Surfs[i]->count-1 ? j+1 : 0];
        }
        TVec Dir = v2-v1;
        Dir.z = 0;
        if (Dir.x > -0.01f && Dir.x < 0.01f && Dir.y > -0.01f && Dir.y < 0.01f) continue; // too short
        TPlane P;
        P.SetPointDirXY(v1, Dir);
        if ((DotProduct(vieworg, P.normal)-P.dist < 0.01f) != Revert) continue; // view origin is on the back side
        float a1 = Range.PointToClipAngle(v2);
        float a2 = Range.PointToClipAngle(v1);
        Range.AddClipRange(a1, a2);
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  VSkyPortal::NeedsDepthBuffer
//
//==========================================================================
bool VSkyPortal::NeedsDepthBuffer () const {
  return false;
}


//==========================================================================
//
//  VSkyPortal::IsSky
//
//==========================================================================
bool VSkyPortal::IsSky () const {
  return true;
}


//==========================================================================
//
//  VSkyPortal::MatchSky
//
//==========================================================================
bool VSkyPortal::MatchSky (VSky *ASky) const {
  return (Level == RLev->PortalLevel+1 && Sky == ASky);
}


//==========================================================================
//
//  VSkyPortal::DrawContents
//
//==========================================================================
void VSkyPortal::DrawContents () {
  guard(VSkyPortal::DrawContents);
  vieworg = TVec(0, 0, 0);
  RLev->TransformFrustum();
  Drawer->SetupViewOrg();

  Sky->Draw(RLev->ColourMap);

  Drawer->WorldDrawing();
  unguard;
}


//==========================================================================
//
//  VSkyBoxPortal::IsSky
//
//==========================================================================
bool VSkyBoxPortal::IsSky () const {
  return true;
}


//==========================================================================
//
//  VSkyBoxPortal::MatchSkyBox
//
//==========================================================================
bool VSkyBoxPortal::MatchSkyBox (VEntity *AEnt) const {
  return (Level == RLev->PortalLevel+1 && Viewport == AEnt);
}


//==========================================================================
//
//  VSkyBoxPortal::DrawContents
//
//==========================================================================
void VSkyBoxPortal::DrawContents () {
  guard(VSkyBoxPortal::DrawContents);
  // set view origin to be sky view origin
  RLev->ViewEnt = Viewport;
  vieworg = Viewport->Origin;
  viewangles.yaw += Viewport->Angles.yaw;
  AngleVectors(viewangles, viewforward, viewright, viewup);

  // no light flashes in the sky
  RLev->ExtraLight = 0;
  if (RLev->ColourMap == CM_Default) RLev->FixedLight = 0;

  // reuse FixedModel flag to prevent recursion
  Viewport->EntityFlags |= VEntity::EF_FixedModel;

  RLev->RenderScene(&refdef, nullptr);

  Viewport->EntityFlags &= ~VEntity::EF_FixedModel;
  unguard;
}


//==========================================================================
//
//  VSectorStackPortal::MatchSkyBox
//
//==========================================================================
bool VSectorStackPortal::MatchSkyBox (VEntity *AEnt) const {
  return (Level == RLev->PortalLevel+1 && Viewport == AEnt);
}


//==========================================================================
//
//  VSectorStackPortal::DrawContents
//
//==========================================================================
void VSectorStackPortal::DrawContents () {
  guard(VSectorStackPortal::DrawContents);
  VViewClipper Range;
  VPortal::SetUpRanges(Range, false, true); //k8: after moving viewport?

  RLev->ViewEnt = Viewport;
  VEntity *Mate = Viewport->eventSkyBoxGetMate();

  //GCon->Logf("rendering portal contents; offset=(%f,%f)", Viewport->Origin.x-Mate->Origin.x, Viewport->Origin.y-Mate->Origin.y);

  vieworg.x = vieworg.x+Viewport->Origin.x-Mate->Origin.x;
  vieworg.y = vieworg.y+Viewport->Origin.y-Mate->Origin.y;

  //VPortal::SetUpRanges(Range, false, true); //k8: after moving viewport?

  // reuse FixedModel flag to prevent recursion
  Viewport->EntityFlags |= VEntity::EF_FixedModel;

  //glClear(GL_COLOR_BUFFER_BIT);
  //glClearDepth(0.0f);
  //glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
  //glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);


  RLev->RenderScene(&refdef, &Range);

  Viewport->EntityFlags &= ~VEntity::EF_FixedModel;
  unguard;
}


//==========================================================================
//
//  VMirrorPortal::MatchMirror
//
//==========================================================================
bool VMirrorPortal::MatchMirror (TPlane *APlane) const {
  return (Level == RLev->PortalLevel+1 && Plane->normal == APlane->normal && Plane->dist == APlane->dist);
}


//==========================================================================
//
//  VMirrorPortal::DrawContents
//
//==========================================================================
void VMirrorPortal::DrawContents () {
  guard(VMirrorPortal::DrawContents);
  RLev->ViewEnt = nullptr;

  ++RLev->MirrorLevel;
  MirrorFlip = RLev->MirrorLevel&1;
  MirrorClip = true;

  float Dist = DotProduct(vieworg, Plane->normal)-Plane->dist;
  vieworg -= 2*Dist*Plane->normal;

  Dist = DotProduct(viewforward, Plane->normal);
  viewforward -= 2*Dist*Plane->normal;
  Dist = DotProduct(viewright, Plane->normal);
  viewright -= 2*Dist*Plane->normal;
  Dist = DotProduct(viewup, Plane->normal);
  viewup -= 2*Dist*Plane->normal;
  VectorsAngles(viewforward, (MirrorFlip ? -viewright : viewright), viewup, viewangles);

  VViewClipper Range;
  SetUpRanges(Range, true, false);

  view_clipplanes[4].normal = Plane->normal;
  view_clipplanes[4].dist = Plane->dist;
  //view_clipplanes[3].next = &view_clipplanes[4];
  //view_clipplanes[4].next = nullptr;
  view_clipplanes[4].clipflag = 0x10U;

  int *pindex = RLev->FrustumIndexes[4];
  for (int j = 0; j < 3; ++j) {
    if (view_clipplanes[4].normal[j] < 0) {
      pindex[j] = j;
      pindex[j+3] = j+3;
    } else {
      pindex[j] = j+3;
      pindex[j+3] = j;
    }
  }

  RLev->RenderScene(&refdef, &Range);

  --RLev->MirrorLevel;
  MirrorFlip = RLev->MirrorLevel&1;
  unguard;
}
