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
#include "../gamedefs.h"
#include "r_local.h"


//==========================================================================
//
//  VRenderLevelShared::CopyPlaneIfValid
//
//==========================================================================
bool VRenderLevelShared::CopyPlaneIfValid (sec_plane_t *dest, const sec_plane_t *source, const sec_plane_t *opp) {
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

  if (copy) {
    *(TPlane *)dest = *(TPlane *)source;
    //dest->Alpha = source->Alpha; // doesn't matter
  }

  return copy;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateFakeFlats
//
//  killough 3/7/98: Hack floor/ceiling heights for deep water etc.
//
//  If player's view height is underneath fake floor, lower the
//  drawn ceiling to be just under the floor height, and replace
//  the drawn floor and ceiling textures, and light level, with
//  the control sector's.
//
//  Similar for ceiling, only reflected.
//
//  killough 4/11/98, 4/13/98: fix bugs, add 'back' parameter
//
//  k8: this whole thing is a fuckin' mess. it should be rewritten.
//  we can simply create fakes, and let the renderer do the rest (i think).
//
//==========================================================================
void VRenderLevelShared::UpdateFakeFlats (sector_t *sector) {
  if (!r_viewleaf) return; // just in case

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; //k8:just in case

  // sector_t::SF_ClipFakePlanes: improved texture control
  //   the real floor and ceiling will be drawn with the real sector's flats
  //   the fake floor and ceiling (from either side) will be drawn with the control sector's flats
  //   the real floor and ceiling will be drawn even when in the middle part, allowing lifts into and out of deep water to render correctly (not possible in Boom)
  //   this flag does not work properly with sloped floors, and, if flag 2 is not set, with sloped ceilings either

  const sector_t *hs = sector->heightsec;
  sector_t *viewhs = r_viewleaf->sector->heightsec;
  /*debug
  ff->floorplane = hs->floor;
  ff->ceilplane = hs->ceiling;
  return;
  */

  #if 1
  /*
  bool underwater = / *r_fakingunderwater ||* /
    //(viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg));
    (hs && Drawer->vieworg.z <= hs->floor.GetPointZClamped(Drawer->vieworg));
  */
  //bool underwater = (viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg));
  bool underwater = (hs && Drawer->vieworg.z < hs->floor.GetPointZClamped(Drawer->vieworg)+1.0f);
  bool underwaterView = (viewhs && Drawer->vieworg.z < viewhs->floor.GetPointZClamped(Drawer->vieworg)+1.0f);
  bool diffTex = !!(hs && hs->SectorFlags&sector_t::SF_ClipFakePlanes);

  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;

  //ff->SetSkipFloor(false);
  //ff->SetSkipCeiling(false);

  /*
  if (!underwater && diffTex && (hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
    ff->floorplane = hs->floor;
    ff->floorplane.pic = GTextureManager.DefaultTexture;
    return;
  }
  */
  /*
    ff->ceilplane.normal = -hs->floor.normal;
    ff->ceilplane.dist = -hs->floor.dist;
    ff->ceilplane.pic = GTextureManager.DefaultTexture;
    return;
  */
  //if (!underwater && diffTex) ff->floorplane = hs->floor;
  //return;

  //GCon->Logf("sector=%d; hs=%d", (int)(ptrdiff_t)(sector-&Level->Sectors[0]), (int)(ptrdiff_t)(hs-&Level->Sectors[0]));

  // replace floor and ceiling height with control sector's heights
  if (diffTex && !(hs->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
    if (CopyPlaneIfValid(&ff->floorplane, &hs->floor, &sector->ceiling)) {
      ff->floorplane.pic = hs->floor.pic;
      //GCon->Logf("opic=%d; fpic=%d", sector->floor.pic.id, hs->floor.pic.id);
    } else if (hs && (hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
      if (underwater) {
        //GCon->Logf("viewhs=%s", (viewhs ? "tan" : "ona"));
        //tempsec->ColorMap = hs->ColorMap;
        ff->params.Fade = hs->params.Fade;
        if (!(hs->SectorFlags&sector_t::SF_NoFakeLight)) {
          ff->params.lightlevel = hs->params.lightlevel;
          ff->params.LightColor = hs->params.LightColor;
          /*
          if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight(hs);
          if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight(hs);
          */
          //ff->floorplane = (viewhs ? viewhs->floor : sector->floor);
        }
        ff->ceilplane = hs->floor;
        ff->ceilplane.FlipInPlace();
        //ff->ceilplane.normal = -hs->floor.normal;
        //ff->ceilplane.dist = -hs->floor.dist;
        //ff->ceilplane.pic = GTextureManager.DefaultTexture;
        //ff->ceilplane.pic = hs->floor.pic;
      } else {
        ff->floorplane = hs->floor;
        //ff->floorplane.pic = hs->floor.pic;
        //ff->floorplane.pic = GTextureManager.DefaultTexture;
      }
      return;
    }
  } else {
    if (hs && !(hs->SectorFlags&sector_t::SF_FakeCeilingOnly)) {
      //ff->floorplane.normal = hs->floor.normal;
      //ff->floorplane.dist = hs->floor.dist;
      //GCon->Logf("  000");
      if (!underwater) *(TPlane *)&ff->floorplane = *(TPlane *)&hs->floor;
      //*(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
      //CopyPlaneIfValid(&ff->floorplane, &hs->floor, &sector->ceiling);
    }
  }


  if (hs && !(hs->SectorFlags&sector_t::SF_FakeFloorOnly)) {
    if (diffTex) {
      if (CopyPlaneIfValid(&ff->ceilplane, &hs->ceiling, &sector->floor)) {
        ff->ceilplane.pic = hs->ceiling.pic;
      }
    } else {
      //ff->ceilplane.normal = hs->ceiling.normal;
      //ff->ceilplane.dist = hs->ceiling.dist;
      //GCon->Logf("  001");
      *(TPlane *)&ff->ceilplane = *(TPlane *)&hs->ceiling;
    }
  }

  //float refflorz = hs->floor.GetPointZClamped(viewx, viewy);
  float refceilz = (hs ? hs->ceiling.GetPointZClamped(Drawer->vieworg) : 0); // k8: was `nullptr` -- wtf?!
  //float orgflorz = sector->floor.GetPointZClamped(viewx, viewy);
  float orgceilz = sector->ceiling.GetPointZClamped(Drawer->vieworg);

  if (underwater /*||(viewhs && Drawer->vieworg.z <= viewhs->floor.GetPointZClamped(Drawer->vieworg))*/) {
    //!ff->floorplane.normal = sector->floor.normal;
    //!ff->floorplane.dist = sector->floor.dist;
    //!ff->ceilplane.normal = -hs->floor.normal;
    //!ff->ceilplane.dist = -hs->floor.dist/* - -hs->floor.normal.z*/;
    *(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
    *(TPlane *)&ff->ceilplane = *(TPlane *)&hs->ceiling;
    //ff->ColorMap = hs->ColorMap;
    if (underwaterView) ff->params.Fade = hs->params.Fade;
  }

  // killough 11/98: prevent sudden light changes from non-water sectors:
  if ((underwater /*&& !back*/) || underwaterView) {
    // head-below-floor hack
    ff->floorplane.pic = (diffTex ? sector->floor.pic : hs->floor.pic);
    ff->floorplane.CopyOffsetsFrom(hs->floor);
    //ff->floorplane = hs->floor;
    //*(TPlane *)&ff->floorplane = *(TPlane *)&sector->floor;
    //ff->floorplane.dist -= 42;
    //ff->floorplane.dist += 9;

    ff->ceilplane.normal = -hs->floor.normal;
    ff->ceilplane.dist = -hs->floor.dist/* - -hs->floor.normal.z*/;
    //ff->ceilplane.pic = GTextureManager.DefaultTexture;
    //GCon->Logf("!!!");
    if (hs->ceiling.pic == skyflatnum) {
      ff->floorplane.normal = -ff->ceilplane.normal;
      ff->floorplane.dist = -ff->ceilplane.dist/* - ff->ceilplane.normal.z*/;
      ff->ceilplane.pic = ff->floorplane.pic;
      ff->ceilplane.CopyOffsetsFrom(ff->floorplane);
    } else {
      ff->ceilplane.pic = (diffTex ? sector->floor.pic : hs->ceiling.pic);
      ff->ceilplane.CopyOffsetsFrom(hs->ceiling);
    }

    // k8: why underwaterView? because of kdizd bugs
    //     this seems to be totally wrong, though
    if (!(hs->SectorFlags&sector_t::SF_NoFakeLight) && /*underwaterView*/viewhs) {
      ff->params.lightlevel = hs->params.lightlevel;
      ff->params.LightColor = hs->params.LightColor;
      /*
      if (floorlightlevel != nullptr) *floorlightlevel = GetFloorLight(hs);
      if (ceilinglightlevel != nullptr) *ceilinglightlevel = GetFloorLight(hs);
      */
    }
  } else if (((hs && Drawer->vieworg.z > hs->ceiling.GetPointZClamped(Drawer->vieworg)) || //k8: dunno, it was `floor` there, and it seems to be a typo
              (viewhs && Drawer->vieworg.z > viewhs->ceiling.GetPointZClamped(Drawer->vieworg))) &&
             orgceilz > refceilz && !(hs->SectorFlags&sector_t::SF_FakeFloorOnly))
  {
    // above-ceiling hack
    ff->ceilplane.normal = hs->ceiling.normal;
    ff->ceilplane.dist = hs->ceiling.dist;
    ff->floorplane.normal = -hs->ceiling.normal;
    ff->floorplane.dist = -hs->ceiling.dist;
    ff->params.Fade = hs->params.Fade;
    //ff->params.ColorMap = hs->params.ColorMap;

    ff->ceilplane.pic = diffTex ? sector->ceiling.pic : hs->ceiling.pic;
    ff->floorplane.pic = hs->ceiling.pic;
    ff->floorplane.CopyOffsetsFrom(hs->ceiling);
    ff->ceilplane.CopyOffsetsFrom(hs->ceiling);

    if (hs->floor.pic != skyflatnum) {
      ff->ceilplane.normal = sector->ceiling.normal;
      ff->ceilplane.dist = sector->ceiling.dist;
      ff->floorplane.pic = hs->floor.pic;
      ff->floorplane.CopyOffsetsFrom(hs->floor);
    }

    // k8: why underwaterView? because of kdizd bugs
    //     this seems to be totally wrong, though
    if (!(hs->SectorFlags&sector_t::SF_NoFakeLight) && viewhs) {
      ff->params.lightlevel = hs->params.lightlevel;
      ff->params.LightColor = hs->params.LightColor;
    }
  } else {
    if (diffTex) {
      ff->floorplane = hs->floor;
      ff->ceilplane = hs->floor;
      if (Drawer->vieworg.z < hs->floor.GetPointZClamped(Drawer->vieworg)) {
        // fake floor is actually a ceiling now
        ff->floorplane.FlipInPlace();
      }
      if (Drawer->vieworg.z > hs->ceiling.GetPointZClamped(Drawer->vieworg)) {
        // fake ceiling is actually a floor now
        ff->ceilplane.FlipInPlace();
      }
    }
  }
  #endif

  /*
  ff->floorplane.Alpha = 0.0f;
  ff->ceilplane.Alpha = 0.0f;
  */
}


//==========================================================================
//
//  VRenderLevelShared::UpdateDeepWater
//
//==========================================================================
void VRenderLevelShared::UpdateDeepWater (sector_t *sector) {
  if (!sector) return; // just in case
  const sector_t *ds = sector->deepref;

  if (!ds) return; // just in case

  // replace sector being drawn with a copy to be hacked
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; //k8:just in case
  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;

  ff->floorplane.normal = ds->floor.normal;
  ff->floorplane.dist = ds->floor.dist;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateFloodBug
//
//  emulate floodfill bug
//
//==========================================================================
void VRenderLevelShared::UpdateFloodBug (sector_t *sector) {
  if (!sector) return; // just in case
  fakefloor_t *ff = sector->fakefloors;
  if (!ff) return; // just in case
  //GCon->Logf("UpdateFloodBug: sector #%d (bridge: %s)", (int)(ptrdiff_t)(sector-&Level->Sectors[0]), (sector->SectorFlags&sector_t::SF_HangingBridge ? "tan" : "ona"));
  if (sector->SectorFlags&sector_t::SF_HangingBridge) {
    sector_t *sursec = sector->othersecFloor;
    ff->floorplane = sursec->floor;
    // ceiling must be current sector's floor, flipped
    ff->ceilplane = sector->floor;
    ff->ceilplane.FlipInPlace();
    ff->params = sursec->params;
    //GCon->Logf("  floor: (%g,%g,%g : %g)", ff->floorplane.normal.x, ff->floorplane.normal.y, ff->floorplane.normal.z, ff->floorplane.dist);
    return;
  }
  // replace sector being drawn with a copy to be hacked
  ff->floorplane = sector->floor;
  ff->ceilplane = sector->ceiling;
  ff->params = sector->params;
  // floor
  if (sector->othersecFloor && sector->floor.minz < sector->othersecFloor->floor.minz) {
    ff->floorplane = sector->othersecFloor->floor;
    ff->params = sector->othersecFloor->params;
    ff->floorplane.LightSourceSector = (int)(ptrdiff_t)(sector->othersecFloor-Level->Sectors);
  }
  if (sector->othersecCeiling && sector->ceiling.minz > sector->othersecCeiling->ceiling.minz) {
    ff->ceilplane = sector->othersecCeiling->ceiling;
    ff->params = sector->othersecCeiling->params;
    ff->ceilplane.LightSourceSector = (int)(ptrdiff_t)(sector->othersecCeiling-Level->Sectors);
  }
}
