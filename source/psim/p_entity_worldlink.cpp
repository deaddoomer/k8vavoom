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
//**  VEntity collision, physics and related methods.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "p_entity.h"
#include "p_world.h"

/*
  3d polyobjects, will sector-link to themselves all things that are touching
  even a little. this is used in 3d pobj movement code.

  also, if mobj origin is inside a 3d pobj, mobj Sector will be 3d pobj inner
  sector. in all cases, mobj BaseSector will be the "real" sector.

  of course, floor and ceiling z will (at least should) be correctly set.
 */

// ////////////////////////////////////////////////////////////////////////// //
static VCvarB gm_smart_z("gm_smart_z", true, "Fix Z position for some things, so they won't fall thru ledge edges?", /*CVAR_Archive|*/CVAR_PreInit|CVAR_NoShadow);

VCvarB r_limit_blood_spots("r_limit_blood_spots", true, "Limit blood spoit amount in a sector?", CVAR_Archive|CVAR_NoShadow);
VCvarI r_limit_blood_spots_max("r_limit_blood_spots_max", "96", "Maximum blood spots?", CVAR_Archive|CVAR_NoShadow);
VCvarI r_limit_blood_spots_leave("r_limit_blood_spots", "64", "Leave no more than this number of blood spots.", CVAR_Archive|CVAR_NoShadow);


//FIXME: sorry for this static
// this is used to add 3d pobj sectors to list without scanning it second time
static TArrayNC<sector_t *> linkAdditionalSectors;
static TArrayNC<VEntity *> bspotList;


//==========================================================================
//
//  IsGoreBloodSpot
//
//==========================================================================
static inline bool IsGoreBloodSpot (VClass *cls) {
  const char *name = cls->GetName();
  if (name[0] == 'K' &&
      name[1] == '8' &&
      name[2] == 'G' &&
      name[3] == 'o' &&
      name[4] == 'r' &&
      name[5] == 'e' &&
      name[6] == '_')
  {
    return
      VStr::startsWith(name, "K8Gore_BloodSpot") ||
      VStr::startsWith(name, "K8Gore_CeilBloodSpot") ||
      VStr::startsWith(name, "K8Gore_MinuscleBloodSpot") ||
      VStr::startsWith(name, "K8Gore_GrowingBloodPool");
  }
  return false;
}


//=============================================================================
//
//  VEntity::CreateSecNodeList
//
//  phares 3/14/98
//
//  alters/creates the sector_list that shows what
//  sectors the object resides in
//
//  also, changes current object Sector if the object is above on 3d pobj
//
//=============================================================================
void VEntity::CreateSecNodeList () {
  // first, clear out the existing Thing fields. as each node is
  // added or verified as needed, Thing will be set properly.
  // when finished, delete all nodes where Thing is still nullptr.
  // these represent the sectors the Thing has vacated.
  for (msecnode_t *Node = XLevel->SectorList; Node; Node = Node->TNext) Node->Thing = nullptr;

  // use RenderRadius here, so we can check sectors in renderer instead of going through all objects
  // no, we cannot do this, because touching sector list is used to move objects too
  // we need another list here, if we want to avoid loop in renderer
  //const float rad = max2(GetRenderRadius(), GetMoveRadius());
  const float rad = GetMoveRadius();

  XLevel->IncrementValidCount(); // used to make sure we only process a line once

  sector_t *sec = Sector;
  //polyobj_t *currpo = (sec->ownpobj ? sec->ownpobj : nullptr);

  if (rad >= 1.0f) {
    float tmbbox[4];
    Create2DBBox(tmbbox, Origin, rad);
    DeclareMakeBlockMapCoordsBBox2D(tmbbox, xl, yl, xh, yh);

    //GCon->Logf(NAME_Debug, "CreateSecNodeList: %s(%u): checking lines...", GetClass()->GetName(), GetUniqueId());

    // check lines
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        // ignore polyobject lines (we already did 3d pobj scan in `LinkToWorld()`)
        for (auto &&it : XLevel->allBlockLines(bx, by, VLevel::BLINES_LINES)) {
          line_t *ld = it.line();
          if (ld->pobj()) continue; // just in case
          // ignore original polyobject sectors
          if (!ld->frontsector || ld->frontsector->isOriginalPObj()) continue;

          // locates all the sectors the object is in by looking at the lines that cross through it.
          // you have already decided that the object is allowed at this location, so don't
          // bother with checking impassable or blocking lines.
          if (!ld->Box2DHit(tmbbox)) continue;

          // this line crosses through the object

          // collect the sector(s) from the line and add to the SectorList you're examining.
          // if the Thing ends up being allowed to move to this position, then the sector_list will
          // be attached to the Thing's VEntity at TouchingSectorList.

          if (ld->frontsector != sec) XLevel->SectorList = XLevel->AddSecnode(ld->frontsector, this, XLevel->SectorList);

          // don't assume all lines are 2-sided, since some Things like
          // MT_TFOG are allowed regardless of whether their radius
          // takes them beyond an impassable linedef.

          // killough 3/27/98, 4/4/98:
          // use sidedefs instead of 2s flag to determine two-sidedness

          if (ld->backsector && ld->backsector != ld->frontsector && ld->backsector != sec) {
            XLevel->SectorList = XLevel->AddSecnode(ld->backsector, this, XLevel->SectorList);
          }
        }
      }
    }
  }

  // add 3d pobj sectors
  for (auto &&s : linkAdditionalSectors) XLevel->SectorList = XLevel->AddSecnode(s, this, XLevel->SectorList);

  // add base sector
  if (sec != BaseSector) XLevel->SectorList = XLevel->AddSecnode(BaseSector, this, XLevel->SectorList);

  // add the sector of the (x,y) point to sector_list
  XLevel->SectorList = XLevel->AddSecnode(sec, this, XLevel->SectorList);

  // now delete any nodes that won't be used
  // these are the ones where Thing is still nullptr
  for (msecnode_t *Node = XLevel->SectorList; Node; ) {
    if (Node->Thing == nullptr) {
      if (Node == XLevel->SectorList) XLevel->SectorList = Node->TNext;
      Node = XLevel->DelSecnode(Node);
    } else {
      Node = Node->TNext;
    }
  }
}


//==========================================================================
//
//  VEntity::UnlinkFromWorld
//
//  unlinks a thing from block map and sectors. on each position change,
//  BLOCKMAP and other lookups maintaining lists ot things inside these
//  structures need to be updated.
//
//==========================================================================
void VEntity::UnlinkFromWorld () {
  //!MoveFlags &= ~MVF_JustMoved;
  if (!SubSector) return;

  if (Sector) LastSector = Sector;

  if (!(EntityFlags&EF_NoSector)) {
    // invisible things don't need to be in sector list
    // unlink from subsector
    if (SNext) SNext->SPrev = SPrev;
    if (SPrev) SPrev->SNext = SNext; else Sector->ThingList = SNext;
    SNext = nullptr;
    SPrev = nullptr;

    // phares 3/14/98
    //
    // Save the sector list pointed to by TouchingSectorList. In
    // LinkToWorld, we'll keep any nodes that represent sectors the Thing
    // still touches. We'll add new ones then, and delete any nodes for
    // sectors the Thing has vacated. Then we'll put it back into
    // TouchingSectorList. It's done this way to avoid a lot of
    // deleting/creating for nodes, when most of the time you just get
    // back what you deleted anyway.
    //
    // If this Thing is being removed entirely, then the calling routine
    // will clear out the nodes in sector_list.
    //
    XLevel->DelSectorList();
    XLevel->SectorList = TouchingSectorList;
    TouchingSectorList = nullptr; // to be restored by LinkToWorld
  }

  if (BlockMapCell) {
    // unlink from block map
    if (BlockMapNext) BlockMapNext->BlockMapPrev = BlockMapPrev;
    if (BlockMapPrev) {
      BlockMapPrev->BlockMapNext = BlockMapNext;
    } else {
      // we are the first entity in blockmap cell
      BlockMapCell -= 1; // real cell number
      // do some sanity checks
      vassert(XLevel->BlockLinks[BlockMapCell] == this);
      // fix list head
      XLevel->BlockLinks[BlockMapCell] = BlockMapNext;
    }
    BlockMapCell = 0;
  }

  SubSector = nullptr;
  Sector = nullptr;
  BaseSubSector = nullptr;
  BaseSector = nullptr;
  PolyObj = nullptr;
}


//==========================================================================
//
//  VEntity::LinkToWorld
//
//  Links a thing into both a block and a subsector based on it's x y.
//  Sets thing->subsector properly
//  pass -666 to force proper check (sorry for this hack)
//
//==========================================================================
void VEntity::LinkToWorld (int properFloorCheck) {
  if (SubSector) UnlinkFromWorld();

  const float rad = GetMoveRadius();

  // link into subsector
  subsector_t *ss = XLevel->PointInSubsector_Buggy(Origin);
  //vassert(ss); // meh, it will segfault on `nullptr` anyway
  BaseSubSector = SubSector = ss;
  BaseSector = Sector = ss->sector;
  PolyObj = nullptr;

  if (properFloorCheck != -666) {
    if (!IsPlayer()) {
      if (properFloorCheck) {
        if (rad < 4.0f ||
            (EntityFlags&(EF_ColideWithWorld|EF_NoSector|EF_NoBlockmap|EF_Invisible|EF_Missile|EF_ActLikeBridge)) != EF_ColideWithWorld)
        {
          properFloorCheck = false;
        }
      }
    } else {
      properFloorCheck = true;
    }

    if (!gm_smart_z) properFloorCheck = false;
  }

  if (properFloorCheck) {
    //FIXME: this is copypasta from `CheckRelPos()`; factor it out
    tmtrace_t tmtrace;
    memset((void *)&tmtrace, 0, sizeof(tmtrace));
    subsector_t *newsubsec = ss;
    const TVec Pos = Origin;

    tmtrace.End = Pos;

    tmtrace.BBox[BOX2D_TOP] = Pos.y+rad;
    tmtrace.BBox[BOX2D_BOTTOM] = Pos.y-rad;
    tmtrace.BBox[BOX2D_RIGHT] = Pos.x+rad;
    tmtrace.BBox[BOX2D_LEFT] = Pos.x-rad;

    tmtrace.setupGap(XLevel, newsubsec->sector, Height);

    // check lines
    XLevel->IncrementValidCount();
    DeclareMakeBlockMapCoordsBBox2D(tmtrace.BBox, xl, yl, xh, yh);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        // ignore polyobjects here, because we already know our polyobject
        for (auto &&it : XLevel->allBlockLines(bx, by, VLevel::BLINES_LINES)) {
          line_t *ld = it.line();
          if (ld->pobj()) continue; // just in case
          // ingore polyobject sectors (for now)
          if (ld->frontsector && ld->frontsector->isOriginalPObj()) continue;
          // we don't care about any blocking line info...
          (void)CheckRelLine(tmtrace, ld, true); // ...and we don't want to process any specials
        }
      }
    }

    CopyTraceFloor(&tmtrace);
    CopyTraceCeiling(&tmtrace);
  } else {
    // simplified check
    TSecPlaneRef floor, ceiling;
    XLevel->FindGapFloorCeiling(ss->sector, Origin, Height, EFloor, ECeiling);
    FloorZ = EFloor.GetPointZClamped(Origin);
    CeilingZ = ECeiling.GetPointZClamped(Origin);
  }

  const bool needSectorList = !(EntityFlags&EF_NoSector);

  // just in case, always clear it
  linkAdditionalSectors.resetNoDtor();

  // check polyobjects, and remember their sectors (to be added to sector list later)
  // we have to do it here, because we need a right `Sector`
  if (XLevel->Has3DPolyObjects()) {
    XLevel->IncrementValidCount(); // used to make sure we only process a pobj once
    float tmbbox[4];
    Create2DBBox(tmbbox, Origin, max2(1.0f, rad));
    DeclareMakeBlockMapCoordsBBox2D(tmbbox, xl, yl, xh, yh);
    polyobj_t *spo = nullptr;
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        polyobj_t *po;
        for (VBlockPObjIterator It(XLevel, bx, by, &po); It.GetNext(); ) {
          if (po->pofloor.minz >= po->poceiling.maxz) continue;
          if (!Are2DBBoxesOverlap(po->bbox2d, tmbbox)) continue;
          if (!XLevel->IsBBox2DTouchingSector(po->GetSector(), tmbbox)) continue;
          if (needSectorList) linkAdditionalSectors.append(po->GetSector());
          (void)Copy3DPObjFloorCeiling(po, EFloor, FloorZ, DropOffZ, ECeiling, CeilingZ, spo, Origin.z, Origin.z+max2(0.0f, Height));
        }
      }
    }
    //GCon->Logf(NAME_Debug, "***LinkToWorld:%s(%u): SPO #%d; z0=%f; z1=%f; FloorZ=%f (DropOffZ=%f); CeilingZ=%f", GetClass()->GetName(), GetUniqueId(), (spo ? spo->tag : 0), Origin.z, Origin.z+max2(0.0f, Height), FloorZ, DropOffZ, CeilingZ);
    if (spo) {
      // subsector info required only for bots
      Sector = spo->GetSector();
      SubSector = Sector->subsectors;
      PolyObj = spo;
    }
  }

  // link into sector
  if (needSectorList) {
    // invisible things don't go into the sector links
    VEntity **Link = &Sector->ThingList;
    SPrev = nullptr;
    SNext = *Link;
    if (*Link) (*Link)->SPrev = this;
    *Link = this;
    // phares 3/16/98
    //
    // If sector_list isn't nullptr, it has a collection of sector
    // nodes that were just removed from this Thing.
    //
    // Collect the sectors the object will live in by looking at
    // the existing sector_list and adding new nodes and deleting
    // obsolete ones.
    //
    // When a node is deleted, its sector links (the links starting
    // at sector_t->touching_thinglist) are broken. When a node is
    // added, new sector links are created.
    CreateSecNodeList();
    TouchingSectorList = XLevel->SectorList; // attach to thing
    XLevel->SectorList = nullptr; // clear for next time
  } else {
    XLevel->DelSectorList();
  }

  // link into blockmap
  if (!(EntityFlags&EF_NoBlockmap)) {
    vassert(BlockMapCell == 0);
    // inert things don't need to be in blockmap
    int blockx = MapBlock(Origin.x-XLevel->BlockMapOrgX);
    int blocky = MapBlock(Origin.y-XLevel->BlockMapOrgY);

    if (blockx >= 0 && blockx < XLevel->BlockMapWidth &&
        blocky >= 0 && blocky < XLevel->BlockMapHeight)
    {
      BlockMapCell = ((unsigned)blocky*(unsigned)XLevel->BlockMapWidth+(unsigned)blockx);
      VEntity **link = &XLevel->BlockLinks[BlockMapCell];
      BlockMapPrev = nullptr;
      BlockMapNext = *link;
      if (*link) (*link)->BlockMapPrev = this;
      *link = this;
      BlockMapCell += 1;
    } else {
      // thing is off the map
      BlockMapNext = BlockMapPrev = nullptr;
    }
  }

  // limit blood spots
  // blood spots are never moved, so this won't be called often
  if (r_limit_blood_spots.asBool() && Sector && Sector != LastSector && Sector->TouchingThingList && IsGoreBloodSpot(GetClass())) {
    //GCon->Logf(NAME_Debug, "counting blood spots... (%s)", GetClass()->GetName());
    int spotsMax = r_limit_blood_spots_max.asInt();
    int spotsLeave = r_limit_blood_spots_leave.asInt();
    if (spotsMax > 0) {
      if (spotsLeave < 1 || spotsLeave > spotsMax) spotsLeave = spotsMax;
      bspotList.resetNoDtor();
      const int visCount = XLevel->nextVisitedCount();
      for (msecnode_t *n = Sector->TouchingThingList; n; n = n->SNext) {
        if (n->Thing != this && n->Visited != visCount && !n->Thing->IsGoingToDie()) {
          // unprocessed thing found, mark thing as processed
          n->Visited = visCount;
          // process it
          //TVec oldOrg = n->Thing->Origin;
          if (IsGoreBloodSpot(n->Thing->GetClass())) {
            const float sqDist = (n->Thing->Origin-Origin).length2DSquared();
            if (sqDist <= 96.0f*96.0f) {
              bspotList.append(n->Thing);
            }
          }
        }
      }
      // check number?
      if (bspotList.length() >= spotsMax) {
        // too much, reduce
        vassert(spotsLeave <= spotsMax);
        //GCon->Logf(NAME_Debug, "found %d blood spots, max is %d, leaving only %d", bspotList.length(), spotsMax, spotsLeave);
        for (int f = spotsLeave; f < bspotList.length(); ++f) bspotList[f]->DestroyThinker();
      }
    }
  }
}


//==========================================================================
//
//  VEntity::FixMapthingPos
//
//  if the thing is exactly on a line, move it into the sector
//  slightly in order to resolve clipping issues in the renderer
//
//  code adapted from GZDoom
//
//==========================================================================
bool VEntity::FixMapthingPos () {
  sector_t *sec = XLevel->PointInSubsector_Buggy(Origin)->sector; // here buggy original should be used!
  bool res = false;
  const float rad = GetMoveRadius();
  float tmbbox[4];
  Create2DBBox(tmbbox, Origin, rad);
  DeclareMakeBlockMapCoordsBBox2D(tmbbox, xl, yl, xh, yh);

  // xl->xh, yl->yh determine the mapblock set to search
  //++validcount; // prevents checking same line twice
  XLevel->IncrementValidCount();
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      for (auto &&it : XLevel->allBlockLines(bx, by)) {
        line_t *ld = it.line();
        if (ld->frontsector == ld->backsector) continue; // skip two-sided lines inside a single sector

        // ignore polyobject lines (for now)
        if (ld->pobj()) continue;

        // skip two-sided lines without any height difference on either side
        if (ld->frontsector && ld->backsector) {
          if (ld->frontsector->floor.minz == ld->backsector->floor.minz &&
              ld->frontsector->floor.maxz == ld->backsector->floor.maxz &&
              ld->frontsector->ceiling.minz == ld->backsector->ceiling.minz &&
              ld->frontsector->ceiling.maxz == ld->backsector->ceiling.maxz)
          {
            continue;
          }
        }

        // check line bounding box for early out
        if (tmbbox[BOX2D_RIGHT] <= ld->bbox2d[BOX2D_LEFT] ||
            tmbbox[BOX2D_LEFT] >= ld->bbox2d[BOX2D_RIGHT] ||
            tmbbox[BOX2D_TOP] <= ld->bbox2d[BOX2D_BOTTOM] ||
            tmbbox[BOX2D_BOTTOM] >= ld->bbox2d[BOX2D_TOP])
        {
          continue;
        }

        // get the exact distance to the line
        float linelen = ld->dir.length();
        if (linelen < 0.0001f) continue; // just in case

        //divline_t dll, dlv;
        //P_MakeDivline(ld, &dll);
        float dll_x = ld->v1->x;
        float dll_y = ld->v1->y;
        float dll_dx = ld->dir.x;
        float dll_dy = ld->dir.y;

        float dlv_x = Origin.x;
        float dlv_y = Origin.y;
        float dlv_dx = dll_dy/linelen;
        float dlv_dy = -dll_dx/linelen;

        //double distance = fabs(P_InterceptVector(&dlv, &dll));
        float distance = 0.0f;
        {
          const float v1x = dll_x;
          const float v1y = dll_y;
          const float v1dx = dll_dx;
          const float v1dy = dll_dy;
          const float v2x = dlv_x;
          const float v2y = dlv_y;
          const float v2dx = dlv_dx;
          const float v2dy = dlv_dy;

          const float den = v1dy*v2dx-v1dx*v2dy;

          if (den == 0.0f) {
            // parallel
            distance = 0.0f;
          } else {
            const float num = (v1x-v2x)*v1dy+(v2y-v1y)*v1dx;
            distance = num/den;
          }
        }

        if (distance < rad) {
          /*
          float angle = matan(ld->dir.y, ld->dir.x);
          angle += (ld->backsector && ld->backsector == sec ? 90 : -90);
          // get the distance we have to move the object away from the wall
          distance = rad-distance;
          UnlinkFromWorld();
          Origin += AngleVectorYaw(angle)*distance;
          LinkToWorld();
          */

          //k8: we already have a normal to a wall, let's use it instead
          TVec movedir = ld->normal;
          if (ld->backsector && ld->backsector == sec) movedir = -movedir;
          // get the distance we have to move the object away from the wall
          distance = rad-distance;
          UnlinkFromWorld();
          Origin += movedir*distance;
          LinkToWorld();

          res = true;
        }
      }
    }
  }

  return res;
}



//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, LinkToWorld) {
  VOptParamInt properFloorCheck(0);
  vobjGetParamSelf(properFloorCheck);
  Self->LinkToWorld(properFloorCheck);
}

IMPLEMENT_FUNCTION(VEntity, UnlinkFromWorld) {
  vobjGetParamSelf();
  Self->UnlinkFromWorld();
}

IMPLEMENT_FUNCTION(VEntity, FixMapthingPos) {
  vobjGetParamSelf();
  RET_BOOL(Self->FixMapthingPos());
}
