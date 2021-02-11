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
#include "../server/sv_local.h"

//#define VV_NEW_SLIDE_CHECK

//#define VV_DBG_VERBOSE_TRYMOVE
//#define VV_DBG_VERBOSE_REL_LINE_FC

//#define VV_DBG_VERBOSE_SLIDE


#ifdef VV_DBG_VERBOSE_TRYMOVE
# define TMDbgF(...)  GCon->Logf(NAME_Debug, __VA_ARGS__)
#else
# define TMDbgF(...)  (void)0
#endif


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB gm_smart_z("gm_smart_z", true, "Fix Z position for some things, so they won't fall thru ledge edges?", /*CVAR_Archive|*/CVAR_PreInit);
static VCvarB gm_use_new_slide_code("gm_use_new_slide_code", true, "Use new sliding code (experimental)?", CVAR_Archive);
// this may create stuck monsters on Arch-Vile revival and such, but meh...
// i REALLY HATE hanging corpses
static VCvarB gm_smaller_corpses("gm_smaller_corpses", false, "Make corpses smaller, so they will not hang on ledges?", CVAR_Archive);
static VCvarF gm_corpse_radius_mult("gm_corpse_radius_mult", "0.4", "Corpse radius multiplier for 'smaller corpese' mode.", CVAR_Archive);
#ifdef CLIENT
VCvarB r_interpolate_thing_movement("r_interpolate_thing_movement", true, "Interpolate mobj movement?", CVAR_Archive);
VCvarB r_interpolate_thing_angles_models("r_interpolate_thing_angles_models", true, "Interpolate mobj rotation for 3D models?", CVAR_Archive);
VCvarB r_interpolate_thing_angles_sprites("r_interpolate_thing_angles_sprites", false, "Interpolate mobj rotation for sprites?", CVAR_Archive);
#endif

VCvarB r_limit_blood_spots("r_limit_blood_spots", true, "Limit blood spoit amount in a sector?", CVAR_Archive);
VCvarI r_limit_blood_spots_max("r_limit_blood_spots_max", "96", "Maximum blood spots?", CVAR_Archive);
VCvarI r_limit_blood_spots_leave("r_limit_blood_spots", "64", "Leva no more than this number of blood spots.", CVAR_Archive);


//==========================================================================
//
//  VEntity::GetMoveRadius
//
//  corpses could have smaller radius
//
//  TODO: return real radius in nightmare?
//
//==========================================================================
float VEntity::GetMoveRadius () const noexcept {
  if ((EntityFlags&(EF_Corpse|EF_Missile)) == EF_Corpse && (FlagsEx&EFEX_Monster)) {
    // monster corpse
    if (gm_smaller_corpses.asBool()) {
      const float rmult = gm_corpse_radius_mult.asFloat();
      if (isFiniteF(rmult) && rmult > 0.0f && rmult < 1.0f) return Radius*rmult;
    }
  }
  return Radius;
}


//==========================================================================
//
//  tmtSetupGap
//
//  the base floor / ceiling is from the subsector that contains the point
//  any contacted lines the step closer together will adjust them
//
//==========================================================================
static void tmtSetupGap (tmtrace_t *tmtrace, sector_t *sector, float Height, bool debugDump) {
  SV_FindGapFloorCeiling(sector, tmtrace->End, Height, tmtrace->EFloor, tmtrace->ECeiling, debugDump);
  tmtrace->FloorZ = tmtrace->EFloor.GetPointZClamped(tmtrace->End);
  tmtrace->DropOffZ = tmtrace->FloorZ;
  tmtrace->CeilingZ = tmtrace->ECeiling.GetPointZClamped(tmtrace->End);
}


//==========================================================================
//
//  VEntity::CopyTraceFloor
//
//==========================================================================
void VEntity::CopyTraceFloor (tmtrace_t *tr, bool setz) {
  EFloor = tr->EFloor;
  if (setz) FloorZ = tr->FloorZ;
}


//==========================================================================
//
//  VEntity::CopyTraceCeiling
//
//==========================================================================
void VEntity::CopyTraceCeiling (tmtrace_t *tr, bool setz) {
  ECeiling = tr->ECeiling;
  if (setz) CeilingZ = tr->CeilingZ;
}


//==========================================================================
//
//  VEntity::CopyRegFloor
//
//==========================================================================
void VEntity::CopyRegFloor (sec_region_t *r, bool setz) {
  EFloor = r->efloor;
  FloorZ = EFloor.GetPointZClamped(Origin);
}


//==========================================================================
//
//  VEntity::CopyRegCeiling
//
//==========================================================================
void VEntity::CopyRegCeiling (sec_region_t *r, bool setz) {
  ECeiling = r->eceiling;
  CeilingZ = ECeiling.GetPointZClamped(Origin);
}


// ////////////////////////////////////////////////////////////////////////// //
// searches though the surrounding mapblocks for monsters/players
// distance is in MAPBLOCKUNITS
class VRoughBlockSearchIterator : public VScriptIterator {
private:
  VEntity *Self;
  int Distance;
  VEntity *Ent;
  VEntity **EntPtr;

  int StartX;
  int StartY;
  int Count;
  int CurrentEdge;
  int BlockIndex;
  int FirstStop;
  int SecondStop;
  int ThirdStop;
  int FinalStop;

public:
  VRoughBlockSearchIterator (VEntity *, int, VEntity **);
  virtual bool GetNext () override;
};


//**************************************************************************
//
//  THING POSITION SETTING
//
//**************************************************************************

//=============================================================================
//
//  VEntity::Destroy
//
//=============================================================================
void VEntity::Destroy () {
  UnlinkFromWorld();
  if (XLevel) XLevel->DelSectorList();
  if (TID && Level) RemoveFromTIDList(); // remove from TID list
  TID = 0; // just in case
  Super::Destroy();
}


//=============================================================================
//
//  VEntity::CreateSecNodeList
//
//  phares 3/14/98
//
//  alters/creates the sector_list that shows what sectors the object
//  resides in
//
//=============================================================================
void VEntity::CreateSecNodeList () {
  msecnode_t *Node;

  // first, clear out the existing Thing fields. as each node is
  // added or verified as needed, Thing will be set properly.
  // when finished, delete all nodes where Thing is still nullptr.
  // these represent the sectors the Thing has vacated.
  Node = XLevel->SectorList;
  while (Node) {
    Node->Thing = nullptr;
    Node = Node->TNext;
  }

  // use RenderRadius here, so we can check sectors in renderer instead of going through all objects
  // no, we cannot do this, because touching sector list is used to move objects too
  // we need another list here, if we want to avoid loop in renderer
  //const float rad = max2(RenderRadius, Radius);
  const float rad = GetMoveRadius();

  //++validcount; // used to make sure we only process a line once
  XLevel->IncrementValidCount();

  if (rad > 1.0f) {
    float tmbbox[4];
    Create2DBBox(tmbbox, Origin, rad);
    DeclareMakeBlockMapCoordsBBox2D(tmbbox, xl, yl, xh, yh);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        line_t *ld;
        for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
          // locates all the sectors the object is in by looking at the lines that cross through it.
          // you have already decided that the object is allowed at this location, so don't
          // bother with checking impassable or blocking lines.
          if (tmbbox[BOX2D_RIGHT] <= ld->bbox2d[BOX2D_LEFT] ||
              tmbbox[BOX2D_LEFT] >= ld->bbox2d[BOX2D_RIGHT] ||
              tmbbox[BOX2D_TOP] <= ld->bbox2d[BOX2D_BOTTOM] ||
              tmbbox[BOX2D_BOTTOM] >= ld->bbox2d[BOX2D_TOP])
          {
            continue;
          }

          if (P_BoxOnLineSide(tmbbox, ld) != -1) continue;

          // this line crosses through the object

          // collect the sector(s) from the line and add to the SectorList you're examining.
          // if the Thing ends up being allowed to move to this position, then the sector_list will
          // be attached to the Thing's VEntity at TouchingSectorList.

          XLevel->SectorList = XLevel->AddSecnode(ld->frontsector, this, XLevel->SectorList);

          // don't assume all lines are 2-sided, since some Things like
          // MT_TFOG are allowed regardless of whether their radius
          // takes them beyond an impassable linedef.

          // killough 3/27/98, 4/4/98:
          // use sidedefs instead of 2s flag to determine two-sidedness

          if (ld->backsector && ld->backsector != ld->frontsector) {
            XLevel->SectorList = XLevel->AddSecnode(ld->backsector, this, XLevel->SectorList);
          }
        }
      }
    }
  }

  // add the sector of the (x,y) point to sector_list
  XLevel->SectorList = XLevel->AddSecnode(Sector, this, XLevel->SectorList);

  // now delete any nodes that won't be used
  // these are the ones where Thing is still nullptr
  Node = XLevel->SectorList;
  while (Node) {
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


static TArray<VEntity *> bspotList;


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

  if (BlockMapCell /*&& !(EntityFlags&EF_NoBlockmap)*/) {
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

  /*
  if (IsGoreBloodSpot(GetClass())) {
    GCon->Logf(NAME_Debug, "*** %s:%u: LinkToWorld (%d); nosector=%u", GetClass()->GetName(), GetUniqueId(), (int)IsGoreBloodSpot(GetClass()), (unsigned)(!!(EntityFlags&EF_NoSector)));
  }
  */

  const float rad = GetMoveRadius();

  // link into subsector
  subsector_t *ss = XLevel->PointInSubsector_Buggy(Origin);
  //vassert(ss); // meh, it will segfault on `nullptr` anyway
  SubSector = ss;
  Sector = ss->sector;

  if (properFloorCheck != -666) {
    if (!IsPlayer()) {
      if (properFloorCheck) {
        if (rad < 4 || (EntityFlags&(EF_ColideWithWorld|EF_NoSector|EF_NoBlockmap|EF_Invisible|EF_Missile|EF_ActLikeBridge)) != EF_ColideWithWorld) {
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

    tmtSetupGap(&tmtrace, newsubsec->sector, Height, false);

    // check lines
    XLevel->IncrementValidCount();

    //tmtrace.FloorZ = tmtrace.DropOffZ;

    DeclareMakeBlockMapCoordsBBox2D(tmtrace.BBox, xl, yl, xh, yh);

    //float lastFZ, lastCZ;
    //sec_plane_t *lastFloor = nullptr;
    //sec_plane_t *lastCeiling = nullptr;

    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        line_t *ld;
        for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
          // we don't care about any blocking line info...
          (void)CheckRelLine(tmtrace, ld, true); // ...and we don't want to process any specials
        }
      }
    }

    CopyTraceFloor(&tmtrace);
    CopyTraceCeiling(&tmtrace);

    //if (IsPlayer()) GCon->Logf(NAME_Debug, "LTW: proper! z=%g; floor=(%g,%g,%g); ceil=(%g,%g,%g); floorz=%g", Origin.z, EFloor.GetNormal().x, EFloor.GetNormal().y, EFloor.GetNormal().z, ECeiling.GetNormal().x, ECeiling.GetNormal().y, ECeiling.GetNormal().z, FloorZ);
  } else {
    // simplified check
    TSecPlaneRef floor, ceiling;
    SV_FindGapFloorCeiling(ss->sector, Origin, Height, EFloor, ECeiling);
    FloorZ = EFloor.GetPointZClamped(Origin);
    CeilingZ = ECeiling.GetPointZClamped(Origin);
  }

  // link into sector
  if (!(EntityFlags&EF_NoSector)) {
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
//  VEntity::CheckWater
//
//  this sets `WaterLevel` and `WaterType`
//
//==========================================================================
void VEntity::CheckWater () {
  WaterLevel = 0;
  WaterType = 0;

  TVec point = Origin;
  point.z += 1.0f;
  const int contents = SV_PointContents(Sector, point);
  if (contents > 0) {
    WaterType = contents;
    WaterLevel = 1;
    point.z = Origin.z+Height*0.5f;
    if (SV_PointContents(Sector, point) > 0) {
      WaterLevel = 2;
      if (EntityFlags&EF_IsPlayer) {
        point = Player->ViewOrg;
        if (SV_PointContents(Sector, point) > 0) WaterLevel = 3;
      } else {
        point.z = Origin.z+Height*0.75f;
        if (SV_PointContents(Sector, point) > 0) WaterLevel = 3;
      }
    }
  }
  //return (WaterLevel > 1);
}


//**************************************************************************
//
//  CHECK ABSOLUTE POSITION
//
//**************************************************************************

//==========================================================================
//
//  VEntity::CheckPosition
//
//  This is purely informative, nothing is modified
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
//==========================================================================
bool VEntity::CheckPosition (TVec Pos) {
  //sec_region_t *gap;
  //sec_region_t *reg;
  tmtrace_t cptrace;
  bool good = true;

  memset((void *)&cptrace, 0, sizeof(cptrace));
  cptrace.Pos = Pos;

  const float rad = GetMoveRadius();

  cptrace.BBox[BOX2D_TOP] = Pos.y+rad;
  cptrace.BBox[BOX2D_BOTTOM] = Pos.y-rad;
  cptrace.BBox[BOX2D_RIGHT] = Pos.x+rad;
  cptrace.BBox[BOX2D_LEFT] = Pos.x-rad;

  subsector_t *newsubsec = XLevel->PointInSubsector_Buggy(Pos);

  // The base floor / ceiling is from the subsector that contains the point.
  // Any contacted lines the step closer together will adjust them.
  SV_FindGapFloorCeiling(newsubsec->sector, Pos, Height, cptrace.EFloor, cptrace.ECeiling);
  cptrace.DropOffZ = cptrace.FloorZ = cptrace.EFloor.GetPointZClamped(Pos);
  cptrace.CeilingZ = cptrace.ECeiling.GetPointZClamped(Pos);
  /*
  gap = SV_FindThingGap(newsubsec->sector, Pos, Height);
  reg = gap;
  while (reg->prev && (reg->efloor.splane->flags&SPF_NOBLOCKING) != 0) reg = reg->prev;
  cptrace.CopyRegFloor(reg, &Pos);
  cptrace.DropOffZ = cptrace.FloorZ;
  reg = gap;
  while (reg->next && (reg->eceiling.splane->flags&SPF_NOBLOCKING) != 0) reg = reg->next;
  cptrace.CopyRegCeiling(reg, &Pos);
  */

  //GCon->Logf("%s: CheckPosition (%g,%g,%g : %g); fz=%g; cz=%g", GetClass()->GetName(), Origin.x, Origin.y, Origin.z, Origin.z+Height, cptrace.FloorZ, cptrace.CeilingZ);

  //++validcount;
  XLevel->IncrementValidCount();

  if (EntityFlags&EF_ColideWithThings) {
    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    DeclareMakeBlockMapCoordsBBox2D(cptrace.BBox, xl, yl, xh, yh);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (VBlockThingsIterator It(XLevel, bx, by); It; ++It) {
          if (!CheckThing(cptrace, *It)) {
            //GCon->Logf("%s: collided with thing `%s`", GetClass()->GetName(), (*It)->GetClass()->GetName());
            return false;
          }
        }
      }
    }
  }

  if (EntityFlags&EF_ColideWithWorld) {
    // check lines
    DeclareMakeBlockMapCoordsBBox2D(cptrace.BBox, xl, yl, xh, yh);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        line_t *ld;
        for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
          //good &= CheckLine(cptrace, ld);
          if (!CheckLine(cptrace, ld)) {
            good = false; // no early exit!
            //GCon->Logf("%s: collided with line %d", GetClass()->GetName(), (int)(ptrdiff_t)(ld-&XLevel->Lines[0]));
          }
        }
      }
    }

    if (!good) return false;
  }

  return true;
}


//==========================================================================
//
//  VEntity::CheckThing
//
//  returns `false` when blocked
//
//==========================================================================
bool VEntity::CheckThing (tmtrace_t &cptrace, VEntity *Other) {
  // don't clip against self
  if (Other == this) return true;
  //if (OwnerSUId && Other && Other->ServerUId == OwnerSUId) return true;

  // can't hit thing
  if (!(Other->EntityFlags&EF_ColideWithThings)) return true;
  if (!(Other->EntityFlags&EF_Solid)) return true;
  if (Other->EntityFlags&EF_Corpse) return true; //k8: skip corpses

  const float rad = GetMoveRadius();
  const float otherrad = Other->GetMoveRadius();

  const float blockdist = otherrad+rad;

  if (fabsf(Other->Origin.x-cptrace.Pos.x) >= blockdist ||
      fabsf(Other->Origin.y-cptrace.Pos.y) >= blockdist)
  {
    // didn't hit it
    return true;
  }

  /*
  if ((EntityFlags&(EF_PassMobj|EF_Missile)) || (Other->EntityFlags&EF_ActLikeBridge)) {
    // prevent some objects from overlapping
    if (EntityFlags&Other->EntityFlags&EF_DontOverlap) return false;
    // check if a mobj passed over/under another object
    if (cptrace.Pos.z >= Other->Origin.z+GetBlockingHeightFor(Other)) return true;
    if (cptrace.Pos.z+Height <= Other->Origin.z) return true; // under thing
  }
  */

  if ((EntityFlags&EF_Missile) ||
      (((EntityFlags&EF_PassMobj)|(Other->EntityFlags&EF_ActLikeBridge)) && !Level->GetNoPassOver()))
  {
    // prevent some objects from overlapping
    if ((EntityFlags&Other->EntityFlags)&EF_DontOverlap) return false;
    // check if a mobj passed over/under another object
    if (cptrace.End.z >= Other->Origin.z+GetBlockingHeightFor(Other)) return true; // overhead
    if (cptrace.End.z+Height <= Other->Origin.z) return true; // underneath
  }

  if (!eventTouch(Other, /*noPickups*/true)) return false;

  return true;
}


//==========================================================================
//
//  VEntity::CheckLine
//
//  Adjusts cptrace.FloorZ and cptrace.CeilingZ as lines are contacted
//
//==========================================================================
bool VEntity::CheckLine (tmtrace_t &cptrace, line_t *ld) {
  if (cptrace.BBox[BOX2D_RIGHT] <= ld->bbox2d[BOX2D_LEFT] ||
      cptrace.BBox[BOX2D_LEFT] >= ld->bbox2d[BOX2D_RIGHT] ||
      cptrace.BBox[BOX2D_TOP] <= ld->bbox2d[BOX2D_BOTTOM] ||
      cptrace.BBox[BOX2D_BOTTOM] >= ld->bbox2d[BOX2D_TOP])
  {
    return true;
  }

  if (P_BoxOnLineSide(&cptrace.BBox[0], ld) != -1) return true;

  // a line has been hit
  if (!ld->backsector) return false; // one sided line

  if (!(ld->flags&ML_RAILING)) {
    if (ld->flags&ML_BLOCKEVERYTHING) return false; // explicitly blocking everything
    if ((EntityFlags&VEntity::EF_Missile) && (ld->flags&ML_BLOCKPROJECTILE)) return false; // blocks projectile
    if ((EntityFlags&VEntity::EF_CheckLineBlocking) && (ld->flags&ML_BLOCKING)) return false; // explicitly blocking everything
    if ((EntityFlags&VEntity::EF_CheckLineBlockMonsters) && (ld->flags&ML_BLOCKMONSTERS)) return false; // block monsters only
    if ((EntityFlags&VEntity::EF_IsPlayer) && (ld->flags&ML_BLOCKPLAYERS)) return false; // block players only
    if ((EntityFlags&VEntity::EF_Float) && (ld->flags&ML_BLOCK_FLOATERS)) return false; // block floaters only
  }

  // set openrange, opentop, openbottom
  //TVec hit_point = cptrace.Pos-(DotProduct(cptrace.Pos, ld->normal)-ld->dist)*ld->normal;
  TVec hit_point = cptrace.Pos-(ld->PointDistance(cptrace.Pos)*ld->normal);
  opening_t *open = SV_LineOpenings(ld, hit_point, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
  open = SV_FindOpening(open, cptrace.Pos.z, cptrace.Pos.z+Height);

  if (open) {
    // adjust floor / ceiling heights
    if (!(open->eceiling.splane->flags&SPF_NOBLOCKING) && open->top < cptrace.CeilingZ) {
      cptrace.CopyOpenCeiling(open);
    }

    if (!(open->efloor.splane->flags&SPF_NOBLOCKING) && open->bottom > cptrace.FloorZ) {
      cptrace.CopyOpenFloor(open);
    }

    if (open->lowfloor < cptrace.DropOffZ) cptrace.DropOffZ = open->lowfloor;

    if (ld->flags&ML_RAILING) cptrace.FloorZ += 32;
  } else {
    cptrace.CeilingZ = cptrace.FloorZ;
  }

  return true;
}


//**************************************************************************
//
//  MOVEMENT CLIPPING
//
//**************************************************************************

//==========================================================================
//
//  VEntity::CheckRelPosition
//
// This is purely informative, nothing is modified
// (except things picked up).
//
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//  VEntity *BlockingMobj = pointer to thing that blocked position (nullptr if not
//   blocked, or blocked by a line).
//
//==========================================================================
bool VEntity::CheckRelPosition (tmtrace_t &tmtrace, TVec Pos, bool noPickups, bool ignoreMonsters, bool ignorePlayers) {
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "*** CheckRelPosition: pos=(%g,%g,%g)", Pos.x, Pos.y, Pos.z);
  memset((void *)&tmtrace, 0, sizeof(tmtrace));

  tmtrace.End = Pos;

  const float rad = GetMoveRadius();

  tmtrace.BBox[BOX2D_TOP] = Pos.y+rad;
  tmtrace.BBox[BOX2D_BOTTOM] = Pos.y-rad;
  tmtrace.BBox[BOX2D_RIGHT] = Pos.x+rad;
  tmtrace.BBox[BOX2D_LEFT] = Pos.x-rad;

  subsector_t *newsubsec = XLevel->PointInSubsector_Buggy(Pos);
  tmtrace.CeilingLine = nullptr;

  tmtSetupGap(&tmtrace, newsubsec->sector, Height, false);

  XLevel->IncrementValidCount();
  tmtrace.SpecHit.resetNoDtor(); // was `Clear()`

  tmtrace.BlockingMobj = nullptr;
  tmtrace.StepThing = nullptr;
  VEntity *thingblocker = nullptr;

  // check things first, possibly picking things up.
  // the bounding box is extended by MAXRADIUS
  // because mobj_ts are grouped into mapblocks
  // based on their origin point, and can overlap
  // into adjacent blocks by up to MAXRADIUS units.
  if (EntityFlags&EF_ColideWithThings) {
    DeclareMakeBlockMapCoordsBBox2D(tmtrace.BBox, xl, yl, xh, yh);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (VBlockThingsIterator It(XLevel, bx, by); It; ++It) {
          if (ignoreMonsters || ignorePlayers) {
            VEntity *ent = *It;
            if (ignorePlayers && ent->IsPlayer()) continue;
            if (ignoreMonsters && (ent->IsMissile() || ent->IsMonster())) continue;
          }
          if (!CheckRelThing(tmtrace, *It, noPickups)) {
            // continue checking for other things in to see if we hit something
            if (!tmtrace.BlockingMobj || Level->GetNoPassOver()) {
              // slammed into something
              return false;
            } else if (!tmtrace.BlockingMobj->Player &&
                       !(EntityFlags&(VEntity::EF_Float|VEntity::EF_Missile)) &&
                       tmtrace.BlockingMobj->Origin.z+tmtrace.BlockingMobj->Height-tmtrace.End.z <= MaxStepHeight)
            {
              if (!thingblocker || tmtrace.BlockingMobj->Origin.z > thingblocker->Origin.z) thingblocker = tmtrace.BlockingMobj;
              tmtrace.BlockingMobj = nullptr;
            } else if (Player && tmtrace.End.z+Height-tmtrace.BlockingMobj->Origin.z <= MaxStepHeight) {
              if (thingblocker) {
                // something to step up on, set it as the blocker so that we don't step up
                return false;
              }
              // nothing is blocking, but this object potentially could
              // if there is something else to step on
              tmtrace.BlockingMobj = nullptr;
            } else {
              // blocking
              return false;
            }
          }
        }
      }
    }
  }

  // check lines
  XLevel->IncrementValidCount();

  float thingdropoffz = tmtrace.FloorZ;
  tmtrace.FloorZ = tmtrace.DropOffZ;
  tmtrace.BlockingMobj = nullptr;

  if (EntityFlags&EF_ColideWithWorld) {
    DeclareMakeBlockMapCoordsBBox2D(tmtrace.BBox, xl, yl, xh, yh);
    bool good = true;
    line_t *fuckhit = nullptr;
    float lastFrac = 1e7f;
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        line_t *ld;
        for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
          //good &= CheckRelLine(tmtrace, ld);
          // if we don't want pickups, don't activate specials
          if (!CheckRelLine(tmtrace, ld, noPickups)) {
            good = false;
            // find the fractional intercept point along the trace line
            const float den = DotProduct(ld->normal, tmtrace.End-Pos);
            if (den == 0) {
              fuckhit = ld;
              lastFrac = 0;
            } else {
              const float num = ld->dist-DotProduct(Pos, ld->normal);
              const float frac = num/den;
              if (fabsf(frac) < lastFrac) {
                fuckhit = ld;
                lastFrac = fabsf(frac);
              }
            }
          }
        }
      }
    }

    if (!good) {
      if (fuckhit) {
        if (!tmtrace.BlockingLine) tmtrace.BlockingLine = fuckhit;
        if (!tmtrace.AnyBlockingLine) tmtrace.AnyBlockingLine = fuckhit;
      }
      return false;
    }

    if (tmtrace.CeilingZ-tmtrace.FloorZ < Height) {
      if (fuckhit) {
        if (!tmtrace.CeilingLine && !tmtrace.FloorLine && !tmtrace.BlockingLine) {
          // this can happen when you're crouching, for example
          // `fuckhit` is not set in that case too
          GCon->Logf(NAME_Warning, "CheckRelPosition for `%s` is height-blocked, but no block line determined!", GetClass()->GetName());
          tmtrace.BlockingLine = fuckhit;
        }
        if (!tmtrace.AnyBlockingLine) tmtrace.AnyBlockingLine = fuckhit;
      }
      return false;
    }
  }

  if (tmtrace.StepThing) tmtrace.DropOffZ = thingdropoffz;

  tmtrace.BlockingMobj = thingblocker;
  if (tmtrace.BlockingMobj) return false;

  return true;
}


//==========================================================================
//
//  VEntity::CheckRelThing
//
//  returns `false` when blocked
//
//==========================================================================
bool VEntity::CheckRelThing (tmtrace_t &tmtrace, VEntity *Other, bool noPickups) {
  // don't clip against self
  if (Other == this) return true;
  //if (OwnerSUId && Other && Other->ServerUId == OwnerSUId) return true;

  // can't hit thing
  if (!(Other->EntityFlags&EF_ColideWithThings)) return true;

  const float rad = GetMoveRadius();
  const float otherrad = Other->GetMoveRadius();

  const float blockdist = otherrad+rad;

  if (fabsf(Other->Origin.x-tmtrace.End.x) >= blockdist ||
      fabsf(Other->Origin.y-tmtrace.End.y) >= blockdist)
  {
    // didn't hit it
    return true;
  }

  // can't walk on corpses (but can touch them)
  const bool isOtherCorpse = !!(Other->EntityFlags&EF_Corpse);

  // /*!(Level->LevelInfoFlags2&VLevelInfo::LIF2_CompatNoPassOver) && !compat_nopassover*/!Level->GetNoPassOver()

  if (!isOtherCorpse) {
    //tmtrace.BlockingMobj = Other;

    // check bridges
    if (!Level->GetNoPassOver() &&
        !(EntityFlags&(EF_Float|EF_Missile|EF_NoGravity)) &&
        (Other->EntityFlags&(EF_Solid|EF_ActLikeBridge)) == (EF_Solid|EF_ActLikeBridge))
    {
      // allow actors to walk on other actors as well as floors
      if (fabsf(Other->Origin.x-tmtrace.End.x) < otherrad ||
          fabsf(Other->Origin.y-tmtrace.End.y) < otherrad)
      {
        const float ohgt = GetBlockingHeightFor(Other);
        if (Other->Origin.z+ohgt >= tmtrace.FloorZ &&
            Other->Origin.z+ohgt <= tmtrace.End.z+MaxStepHeight)
        {
          //tmtrace.BlockingMobj = Other;
          tmtrace.StepThing = Other;
          tmtrace.FloorZ = Other->Origin.z+ohgt;
        }
      }
    }
  }

  //if (!(tmtrace.Thing->EntityFlags & VEntity::EF_NoPassMobj) || Actor(Other).bSpecial)
  if ((EntityFlags&EF_Missile) ||
      (((EntityFlags&EF_PassMobj)|(Other->EntityFlags&EF_ActLikeBridge)) && !Level->GetNoPassOver()))
  {
    // prevent some objects from overlapping
    if (!isOtherCorpse && ((EntityFlags&Other->EntityFlags)&EF_DontOverlap)) {
      tmtrace.BlockingMobj = Other;
      return false;
    }
    // check if a mobj passed over/under another object
    if (tmtrace.End.z >= Other->Origin.z+GetBlockingHeightFor(Other)) return true; // overhead
    if (tmtrace.End.z+Height <= Other->Origin.z) return true; // underneath
  }

  if (!eventTouch(Other, noPickups)) {
    // just in case
    tmtrace.BlockingMobj = Other;
    return false;
  }

  return true;
}


//==========================================================================
//
//  VEntity::CheckRelLine
//
//  Adjusts tmtrace.FloorZ and tmtrace.CeilingZ as lines are contacted
//
//  returns `false` if blocked
//
//==========================================================================
bool VEntity::CheckRelLine (tmtrace_t &tmtrace, line_t *ld, bool skipSpecials) {
  if (GGameInfo->NetMode == NM_Client) skipSpecials = true;

  //if (IsPlayer()) GCon->Logf(NAME_Debug, "  trying line: %d", (int)(ptrdiff_t)(ld-&XLevel->Lines[0]));
  // check line bounding box for early out
  if (tmtrace.BBox[BOX2D_RIGHT] <= ld->bbox2d[BOX2D_LEFT] ||
      tmtrace.BBox[BOX2D_LEFT] >= ld->bbox2d[BOX2D_RIGHT] ||
      tmtrace.BBox[BOX2D_TOP] <= ld->bbox2d[BOX2D_BOTTOM] ||
      tmtrace.BBox[BOX2D_BOTTOM] >= ld->bbox2d[BOX2D_TOP])
  {
    return true;
  }

  if (P_BoxOnLineSide(&tmtrace.BBox[0], ld) != -1) return true;

  // a line has been hit

  // The moving thing's destination position will cross the given line.
  // If this should not be allowed, return false.
  // If the line is special, keep track of it to process later if the move is proven ok.
  // NOTE: specials are NOT sorted by order, so two special lines that are only 8 pixels apart
  //       could be crossed in either order.

  // k8: this code is fuckin' mess. why some lines are processed immediately, and
  //     other lines are pushed to be processed later? what the fuck is going on here?!
  //     it seems that the original intent was to immediately process blocking lines,
  //     but push non-blocking lines. wtf?!

  if (!ld->backsector) {
    // one sided line
    if (!skipSpecials) BlockedByLine(ld);
    // mark the line as blocking line
    tmtrace.BlockingLine = ld;
    return false;
  }

  if (!(ld->flags&ML_RAILING)) {
    if (ld->flags&ML_BLOCKEVERYTHING) {
      // explicitly blocking everything
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_Missile) && (ld->flags&ML_BLOCKPROJECTILE)) {
      // blocks projectile
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_CheckLineBlocking) && (ld->flags&ML_BLOCKING)) {
      // explicitly blocking everything
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_CheckLineBlockMonsters) && (ld->flags&ML_BLOCKMONSTERS)) {
      // block monsters only
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_IsPlayer) && (ld->flags&ML_BLOCKPLAYERS)) {
      // block players only
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    }

    if ((EntityFlags&VEntity::EF_Float) && (ld->flags&ML_BLOCK_FLOATERS)) {
      // block floaters only
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    }
  }

  // set openrange, opentop, openbottom
  const float hgt = (Height > 0 ? Height : 0.0f);
  //TVec hit_point = tmtrace.End-(DotProduct(tmtrace.End, ld->normal)-ld->dist)*ld->normal;
  TVec hit_point = tmtrace.End-(ld->PointDistance(tmtrace.End)*ld->normal);
  opening_t *open = SV_LineOpenings(ld, hit_point, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex

  #ifdef VV_DBG_VERBOSE_REL_LINE_FC
  if (IsPlayer()) {
    GCon->Logf(NAME_Debug, "  checking line: %d; sz=%g; ez=%g; hgt=%g; traceFZ=%g; traceCZ=%g", (int)(ptrdiff_t)(ld-&XLevel->Lines[0]), tmtrace.End.z, tmtrace.End.z+hgt, hgt, tmtrace.FloorZ, tmtrace.CeilingZ);
    for (opening_t *op = open; op; op = op->next) {
      GCon->Logf(NAME_Debug, "   %p: bot=%g; top=%g; range=%g; lowfloor=%g; highceil=%g", op, op->bottom, op->top, op->range, op->lowfloor, op->highceiling);
    }
  }
  #endif

  open = SV_FindRelOpening(open, tmtrace.End.z, tmtrace.End.z+hgt);
  #ifdef VV_DBG_VERBOSE_REL_LINE_FC
  if (IsPlayer()) GCon->Logf(NAME_Debug, "  open=%p", open);
  #endif

  if (open) {
    // adjust floor / ceiling heights
    // use epsilon to avoid getting stuck on some slope configurations
    if (!(open->eceiling.splane->flags&SPF_NOBLOCKING)) {
      bool replaceIt;
      if (open->eceiling.GetNormalZ() != -1.0f) {
        // slope
        replaceIt = (tmtrace.CeilingZ-open->top > 0.1f);
      } else {
        replaceIt = (tmtrace.CeilingZ > open->top);
      }
      if (/*open->top < tmtrace.CeilingZ*/replaceIt) {
        if (!skipSpecials || open->top /*+hgt*/ >= Origin.z+hgt) {
          #ifdef VV_DBG_VERBOSE_REL_LINE_FC
          if (IsPlayer()) GCon->Logf(NAME_Debug, "    copy ceiling; hgt=%g; z+hgt=%g; top=%g; curcz-top=%g", hgt, Origin.z+hgt, open->top, tmtrace.CeilingZ-open->top);
          #endif
          tmtrace.CopyOpenCeiling(open);
          tmtrace.CeilingLine = ld;
        }
      }
    }

    if (!(open->efloor.splane->flags&SPF_NOBLOCKING)) {
      bool replaceIt;
      if (open->efloor.GetNormalZ() != 1.0f) {
        // slope
        replaceIt = (open->bottom-tmtrace.FloorZ > 0.1f);
      } else {
        replaceIt = (open->bottom > tmtrace.FloorZ);
      }
      //const bool slope = (open->efloor.GetNormalZ() != 1.0f);
      //const float diffz = open->bottom-tmtrace.FloorZ;
      if (/*open->bottom > tmtrace.FloorZ*/replaceIt) {
        if (!skipSpecials || open->bottom <= Origin.z) {
          #ifdef VV_DBG_VERBOSE_REL_LINE_FC
          if (IsPlayer()) GCon->Logf(NAME_Debug, "    copy floor");
          #endif
          tmtrace.CopyOpenFloor(open);
          tmtrace.FloorLine = ld;
        }
      }
    }

    if (open->lowfloor < tmtrace.DropOffZ) tmtrace.DropOffZ = open->lowfloor;

    if (ld->flags&ML_RAILING) tmtrace.FloorZ += 32;
  } else {
    tmtrace.CeilingZ = tmtrace.FloorZ;
    //k8: oops; it seems that we have to return `false` here
    //    but only if this is not a special line, otherwise monsters cannot open doors
    if (!ld->special) {
      //GCon->Logf(NAME_Debug, "BLK: %s (hgt=%g); line=%d", GetClass()->GetName(), hgt, (int)(ptrdiff_t)(ld-&XLevel->Lines[0]));
      if (!skipSpecials) BlockedByLine(ld);
      tmtrace.BlockingLine = ld;
      return false;
    } else {
      // this is special line, don't block movement (but remember this line as blocking!)
      tmtrace.BlockingLine = ld;
    }
  }

  // if contacted a special line, add it to the list
  if (!skipSpecials && ld->special) tmtrace.SpecHit.Append(ld);

  return true;
}


//==========================================================================
//
//  VEntity::BlockedByLine
//
//==========================================================================
void VEntity::BlockedByLine (line_t *ld) {
  if (EntityFlags&EF_Blasted) eventBlastedHitLine();
  if (ld->special) eventCheckForPushSpecial(ld, 0);
}


//==========================================================================
//
//  VEntity::GetDrawOrigin
//
//==========================================================================
TVec VEntity::GetDrawOrigin () {
  TVec sprorigin = Origin+GetDrawDelta();
  sprorigin.z -= FloorClip;
  // do floating bob here, so the dlight will not move
#ifdef CLIENT
  // perform bobbing
  if (FlagsEx&EFEX_FloatBob) {
    //float FloatBobPhase; // in seconds; <0 means "choose at random"; should be converted to ticks; amp is [0..63]
    //float FloatBobStrength;
    if (FloatBobPhase < 0) FloatBobPhase = Random()*256.0f/35.0f; // just in case
    const float amp = FloatBobStrength*8.0f;
    const float phase = fmodf(FloatBobPhase*35.0, 64.0f);
    const float angle = phase*360.0f/64.0f;
    #if 0
    float zofs = msin(angle)*amp;
    if (Sector) {
           if (Origin.z+zofs < FloorZ && Origin.z >= FloorZ) zofs = Origin.z-FloorZ;
      else if (Origin.z+zofs+Height > CeilingZ && Origin.z+Height <= CeilingZ) zofs = CeilingZ-Height-Origin.z;
    }
    res.z += zofs;
    #else
    sprorigin.z += msin(angle)*amp;
    #endif
  }
#endif
  return sprorigin;
}


//==========================================================================
//
//  VEntity::GetDrawDelta
//
//==========================================================================
TVec VEntity::GetDrawDelta () {
#ifdef CLIENT
  // movement interpolation
  if (r_interpolate_thing_movement && (MoveFlags&MVF_JustMoved)) {
    const float ctt = XLevel->Time-LastMoveTime;
    //GCon->Logf(NAME_Debug, "%s:%u: ctt=%g; delta=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), ctt, (Origin-LastMoveOrigin).x, (Origin-LastMoveOrigin).y, (Origin-LastMoveOrigin).z);
    if (ctt >= 0.0f && ctt < LastMoveDuration && LastMoveDuration > 0.0f) {
      TVec delta = Origin-LastMoveOrigin;
      if (!delta.isZero()) {
        delta *= ctt/LastMoveDuration;
        //GCon->Logf(NAME_Debug, "%s:%u:   ...realdelta=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), delta.x, delta.y, delta.z);
        return (LastMoveOrigin+delta)-Origin;
      } else {
        // reset if angles are equal
        if (LastMoveAngles.yaw == Angles.yaw) MoveFlags &= ~VEntity::MVF_JustMoved;
      }
    } else {
      // unconditional reset
      MoveFlags &= ~VEntity::MVF_JustMoved;
    }
  }
#endif
  return TVec(0.0f, 0.0f, 0.0f);
}


//==========================================================================
//
//  VEntity::GetInterpolatedDrawAngles
//
//==========================================================================
TAVec VEntity::GetInterpolatedDrawAngles () {
#ifdef CLIENT
  // movement interpolation
  if (MoveFlags&MVF_JustMoved) {
    const float ctt = XLevel->Time-LastMoveTime;
    if (ctt >= 0.0f && ctt < LastMoveDuration && LastMoveDuration > 0.0f) {
      float deltaYaw = AngleDiff(AngleMod(LastMoveAngles.yaw), AngleMod(Angles.yaw));
      if (deltaYaw) {
        deltaYaw *= ctt/LastMoveDuration;
        return TAVec(Angles.pitch, AngleMod(LastMoveAngles.yaw+deltaYaw), Angles.roll);
      } else {
        // reset if origins are equal
        if ((Origin-LastMoveOrigin).isZero()) MoveFlags &= ~VEntity::MVF_JustMoved;
      }
    } else {
      // unconditional reset
      MoveFlags &= ~VEntity::MVF_JustMoved;
    }
  }
#endif
  return Angles;
}


//==========================================================================
//
//  VEntity::CheckBlockingMobj
//
//  returns `false` if blocked
//
//==========================================================================
bool VEntity::CheckBlockingMobj (VEntity *blockmobj) {
  VEntity *bmee = (blockmobj ? blockmobj->CheckOnmobj() : nullptr);
  VEntity *myee = CheckOnmobj();
  return
    (!blockmobj || !bmee || (bmee && bmee != this)) &&
    (!myee || (myee && myee != blockmobj));
}


//==========================================================================
//
//  VEntity::TryMove
//
//  attempt to move to a new position, crossing special lines.
//  returns `false` if move is blocked.
//
//==========================================================================
bool VEntity::TryMove (tmtrace_t &tmtrace, TVec newPos, bool AllowDropOff, bool checkOnly, bool noPickups) {
  bool check;
  TVec oldorg(0, 0, 0);
  line_t *ld;
  sector_t *OldSec = Sector;

  if (IsGoingToDie() || !Sector) {
    memset((void *)&tmtrace, 0, sizeof(tmtrace));
    return false; // just in case, dead object is immovable
  }

  const bool isClient = (GGameInfo->NetMode == NM_Client);

  bool skipEffects = (checkOnly || noPickups);

  check = CheckRelPosition(tmtrace, newPos, skipEffects);
  tmtrace.TraceFlags &= ~tmtrace_t::TF_FloatOk;
  //if (IsPlayer()) GCon->Logf(NAME_Debug, "trying to move from (%g,%g,%g) to (%g,%g,%g); check=%d", Origin.x, Origin.y, Origin.z, newPos.x, newPos.y, newPos.z, (int)check);
  TMDbgF("%s: trying to move from (%g,%g,%g) to (%g,%g,%g); check=%d", GetClass()->GetName(), Origin.x, Origin.y, Origin.z, newPos.x, newPos.y, newPos.z, (int)check);

  if (isClient) skipEffects = true;

  if (!check) {
    // cannot fit into destination point
    VEntity *O = tmtrace.BlockingMobj;
    TMDbgF("%s:   HIT %s", GetClass()->GetName(), (O ? O->GetClass()->GetName() : "<none>"));
    if (!O) {
      // can't step up or doesn't fit
      PushLine(tmtrace, skipEffects);
      return false;
    }

    const float ohgt = GetBlockingHeightFor(O);
    if (!(EntityFlags&EF_IsPlayer) ||
        (O->EntityFlags&EF_IsPlayer) ||
        O->Origin.z+ohgt-Origin.z > MaxStepHeight ||
        O->CeilingZ-(O->Origin.z+ohgt) < Height ||
        tmtrace.CeilingZ-(O->Origin.z+ohgt) < Height)
    {
      // can't step up or doesn't fit
      PushLine(tmtrace, skipEffects);
      return false;
    }

    if (!(EntityFlags&EF_PassMobj) || Level->GetNoPassOver()) {
      // can't go over
      return false;
    }
  }

  if (EntityFlags&EF_ColideWithWorld) {
    if (tmtrace.CeilingZ-tmtrace.FloorZ < Height) {
      // doesn't fit
      PushLine(tmtrace, skipEffects);
      TMDbgF("%s:   DOESN'T FIT(0)!", GetClass()->GetName());
      return false;
    }

    tmtrace.TraceFlags |= tmtrace_t::TF_FloatOk;

    if (tmtrace.CeilingZ-Origin.z < Height && !(EntityFlags&EF_Fly) && !(EntityFlags&EF_IgnoreCeilingStep)) {
      // mobj must lower itself to fit
      PushLine(tmtrace, skipEffects);
      TMDbgF("%s:   DOESN'T FIT(1)! ZBox=(%g,%g); ceilz=%g", GetClass()->GetName(), Origin.z, Origin.z+Height, tmtrace.CeilingZ);
      return false;
    }

    if (EntityFlags&EF_Fly) {
      // when flying, slide up or down blocking lines until the actor is not blocked
      if (Origin.z+Height > tmtrace.CeilingZ) {
        // if sliding down, make sure we don't have another object below
        if (/*(!tmtrace.BlockingMobj || !tmtrace.BlockingMobj->CheckOnmobj() ||
             (tmtrace.BlockingMobj->CheckOnmobj() && tmtrace.BlockingMobj->CheckOnmobj() != this)) &&
            (!CheckOnmobj() || (CheckOnmobj() && CheckOnmobj() != tmtrace.BlockingMobj))*/
            CheckBlockingMobj(tmtrace.BlockingMobj))
        {
          if (!checkOnly && !isClient) Velocity.z = -8.0f*35.0f;
        }
        PushLine(tmtrace, skipEffects);
        return false;
      } else if (Origin.z < tmtrace.FloorZ && tmtrace.FloorZ-tmtrace.DropOffZ > MaxStepHeight) {
        // check to make sure there's nothing in the way for the step up
        if (/*(!tmtrace.BlockingMobj || !tmtrace.BlockingMobj->CheckOnmobj() ||
             (tmtrace.BlockingMobj->CheckOnmobj() && tmtrace.BlockingMobj->CheckOnmobj() != this)) &&
            (!CheckOnmobj() || (CheckOnmobj() && CheckOnmobj() != tmtrace.BlockingMobj))*/
            CheckBlockingMobj(tmtrace.BlockingMobj))
        {
          if (!checkOnly && !isClient) Velocity.z = 8.0f*35.0f;
        }
        PushLine(tmtrace, skipEffects);
        return false;
      }
    }

    if (!(EntityFlags&EF_IgnoreFloorStep)) {
      if (tmtrace.FloorZ-Origin.z > MaxStepHeight) {
        // too big a step up
        if ((EntityFlags&EF_CanJump) && Health > 0) {
          // check to make sure there's nothing in the way for the step up
          if (!Velocity.z || tmtrace.FloorZ-Origin.z > 48.0f ||
              (tmtrace.BlockingMobj && tmtrace.BlockingMobj->CheckOnmobj()) ||
              TestMobjZ(TVec(newPos.x, newPos.y, tmtrace.FloorZ)))
          {
            PushLine(tmtrace, skipEffects);
            TMDbgF("%s:   FLOORSTEP(0)!", GetClass()->GetName());
            return false;
          }
        } else {
          PushLine(tmtrace, skipEffects);
          TMDbgF("%s:   FLOORSTEP(1)!", GetClass()->GetName());
          return false;
        }
      }

      if ((EntityFlags&EF_Missile) && !(EntityFlags&EF_StepMissile) && tmtrace.FloorZ > Origin.z) {
        PushLine(tmtrace, skipEffects);
        TMDbgF("%s:   FLOORX(0)! z=%g; fl=%g; spechits=%d", GetClass()->GetName(), Origin.z, tmtrace.FloorZ, tmtrace.SpecHit.length());
        return false;
      }

      if (Origin.z < tmtrace.FloorZ) {
        if (EntityFlags&EF_StepMissile) {
          Origin.z = tmtrace.FloorZ;
          // if moving down, cancel vertical component of velocity
          if (Velocity.z < 0) Velocity.z = 0.0f;
        }
        // check to make sure there's nothing in the way for the step up
        if (TestMobjZ(TVec(newPos.x, newPos.y, tmtrace.FloorZ))) {
          PushLine(tmtrace, skipEffects);
          TMDbgF("%s:   OBJZ(0)!", GetClass()->GetName());
          return false;
        }
      }
    }

    // killough 3/15/98: Allow certain objects to drop off
    if ((!AllowDropOff && !(EntityFlags&EF_DropOff) &&
        !(EntityFlags&EF_Float) && !(EntityFlags&EF_Missile)) ||
        (EntityFlags&EF_NoDropOff))
    {
      if (!(EntityFlags&EF_AvoidingDropoff)) {
        float floorz = tmtrace.FloorZ;
        // [RH] If the thing is standing on something, use its current z as the floorz.
        // this is so that it does not walk off of things onto a drop off.
        if (EntityFlags&EF_OnMobj) floorz = max2(Origin.z, tmtrace.FloorZ);

        if ((floorz-tmtrace.DropOffZ > MaxDropoffHeight) && !(EntityFlags&EF_Blasted)) {
          // can't move over a dropoff unless it's been blasted
          TMDbgF("%s:   DROPOFF(0)!", GetClass()->GetName());
          return false;
        }
      } else {
        // special logic to move a monster off a dropoff
        // this intentionally does not check for standing on things
        // k8: ...and due to this, the monster can stuck into another monster. what a great idea!
        if (FloorZ-tmtrace.FloorZ > MaxDropoffHeight || DropOffZ-tmtrace.DropOffZ > MaxDropoffHeight) {
          TMDbgF("%s:   DROPOFF(1)!", GetClass()->GetName());
          return false;
        }
        // check to make sure there's nothing in the way
        if (!CheckBlockingMobj(tmtrace.BlockingMobj)) {
          return false;
        }
      }
    }

    if ((EntityFlags&EF_CantLeaveFloorpic) &&
        (tmtrace.EFloor.splane->pic != EFloor.splane->pic || tmtrace.FloorZ != Origin.z))
    {
      // must stay within a sector of a certain floor type
      TMDbgF("%s:   SECTORSTAY(0)!", GetClass()->GetName());
      return false;
    }
  }

  bool OldAboveFakeFloor = false;
  bool OldAboveFakeCeiling = false;
  if (Sector->heightsec) {
    float EyeZ = (Player ? Player->ViewOrg.z : Origin.z+Height*0.5f);
    OldAboveFakeFloor = (EyeZ > Sector->heightsec->floor.GetPointZClamped(Origin));
    OldAboveFakeCeiling = (EyeZ > Sector->heightsec->ceiling.GetPointZClamped(Origin));
  }

  if (checkOnly) return true;

  TMDbgF("%s:   ***OK-TO-MOVE", GetClass()->GetName());

  // the move is ok, so link the thing into its new position
  UnlinkFromWorld();

  oldorg = Origin;
  Origin = newPos;

  LinkToWorld();

  FloorZ = tmtrace.FloorZ;
  CeilingZ = tmtrace.CeilingZ;
  DropOffZ = tmtrace.DropOffZ;
  CopyTraceFloor(&tmtrace, false);
  CopyTraceCeiling(&tmtrace, false);

  if (EntityFlags&EF_FloorClip) {
    eventHandleFloorclip();
  } else {
    FloorClip = 0.0f;
  }

  if (!noPickups && !isClient) {
    // if any special lines were hit, do the effect
    if (EntityFlags&EF_ColideWithWorld) {
      while (tmtrace.SpecHit.Num() > 0) {
        // see if the line was crossed
        ld = tmtrace.SpecHit[tmtrace.SpecHit.Num()-1];
        tmtrace.SpecHit.SetNum(tmtrace.SpecHit.Num()-1, false);
        // some moron once placed the entity *EXACTLY* on the fuckin' linedef! what a brilliant idea!
        // this will *NEVER* be fixed, it is a genuine map bug (it's impossible to fix it with our freestep engine anyway)
        // let's try use "front inclusive" here; it won't solve all cases, but *may* solve the one above
        const int oldside = ld->PointOnSideFri(oldorg);
        const int side = ld->PointOnSideFri(Origin);
        if (side != oldside) {
          if (ld->special) eventCrossSpecialLine(ld, oldside);
        }
      }
    }

    // do additional check here to avoid calling progs
    if ((OldSec->heightsec && Sector->heightsec && Sector->ActionList) ||
        (OldSec != Sector && (OldSec->ActionList || Sector->ActionList)))
    {
      eventCheckForSectorActions(OldSec, OldAboveFakeFloor, OldAboveFakeCeiling);
    }
  }

  return true;
}


//==========================================================================
//
//  VEntity::PushLine
//
//==========================================================================
void VEntity::PushLine (const tmtrace_t &tmtrace, bool checkOnly) {
  if (checkOnly || GGameInfo->NetMode == NM_Client) return;
  if (EntityFlags&EF_ColideWithWorld) {
    if (EntityFlags&EF_Blasted) eventBlastedHitLine();
    int NumSpecHitTemp = tmtrace.SpecHit.Num();
    while (NumSpecHitTemp > 0) {
      --NumSpecHitTemp;
      // see if the line was crossed
      line_t *ld = tmtrace.SpecHit[NumSpecHitTemp];
      int side = ld->PointOnSide(Origin);
      eventCheckForPushSpecial(ld, side);
    }
  }
}


//**************************************************************************
//
//  SLIDE MOVE
//
//  Allows the player to slide along any angled walls.
//
//**************************************************************************

//==========================================================================
//
//  VEntity::ClipVelocity
//
//  Slide off of the impacting object
//
//==========================================================================
TVec VEntity::ClipVelocity (const TVec &in, const TVec &normal, float overbounce) {
  return in-normal*(DotProduct(in, normal)*overbounce);
}


//==========================================================================
//
//  VEntity::SlidePathTraverse
//
//==========================================================================
void VEntity::SlidePathTraverse (float &BestSlideFrac, line_t *&BestSlideLine, float x, float y, float StepVelScale) {
  TVec SlideOrg(x, y, Origin.z);
  TVec SlideDir = Velocity*StepVelScale;
  if (gm_use_new_slide_code.asBool()) {
    const float rad = GetMoveRadius();
    // new slide code
    DeclareMakeBlockMapCoords(Origin.x, Origin.y, rad, xl, yl, xh, yh);
    XLevel->IncrementValidCount();
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        line_t *li;
        for (VBlockLinesIterator It(XLevel, bx, by, &li); It.GetNext(); ) {
          bool IsBlocked = false;
          if (!(li->flags&ML_TWOSIDED) || !li->backsector) {
            if (li->PointOnSide(Origin)) continue; // don't hit the back side
            IsBlocked = true;
          } else if (li->flags&(ML_BLOCKING|ML_BLOCKEVERYTHING)) {
            IsBlocked = true;
          } else if ((EntityFlags&EF_IsPlayer) && (li->flags&ML_BLOCKPLAYERS)) {
            IsBlocked = true;
          } else if ((EntityFlags&EF_CheckLineBlockMonsters) && (li->flags&ML_BLOCKMONSTERS)) {
            IsBlocked = true;
          }

          VLevel::CD_HitType hitType;
          int hitplanenum = -1;
          TVec contactPoint;
          const float fdist = VLevel::SweepLinedefAABB(li, SlideOrg, SlideOrg+SlideDir, TVec(-rad, -rad, 0), TVec(rad, rad, Height), nullptr, &contactPoint, &hitType, &hitplanenum);
          // ignore cap planes
          if (hitplanenum < 0 || hitplanenum > 1 || fdist < 0.0f || fdist >= 1.0f) continue;

          if (!IsBlocked) {
            const TVec hpoint = contactPoint; //SlideOrg+fdist*SlideDir;
            // set openrange, opentop, openbottom
            opening_t *open = SV_LineOpenings(li, hpoint, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
            open = SV_FindOpening(open, Origin.z, Origin.z+Height);
            if (open && open->range >= Height && // fits
                open->top-Origin.z >= Height && // mobj is not too high
                open->bottom-Origin.z <= MaxStepHeight) // not too big a step up
            {
              // this line doesn't block movement
              if (Origin.z < open->bottom) {
                // check to make sure there's nothing in the way for the step up
                TVec CheckOrg = Origin;
                CheckOrg.z = open->bottom;
                if (!TestMobjZ(CheckOrg)) continue;
              } else {
                continue;
              }
            }
          }

          if (fdist < BestSlideFrac) {
            #ifdef VV_DBG_VERBOSE_SLIDE
            if (IsPlayer()) {
              TVec cvel = ClipVelocity(SlideDir, li->normal, 1.0f);
              GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: NEW line #%d; norm=(%g,%g); sldir=(%g,%g); cvel=(%g,%g); ht=%d", GetClass()->GetName(), (int)(ptrdiff_t)(li-&XLevel->Lines[0]), li->normal.x, li->normal.y, SlideDir.x, SlideDir.y, cvel.x, cvel.y, (int)hitType);
            }
            #endif
            BestSlideFrac = fdist;
            BestSlideLine = li;
          }
        }
      }
    }
  } else {
    // old slide code
    intercept_t *in;
    for (VPathTraverse It(this, &in, x, y, x+SlideDir.x, y+SlideDir.y, PT_ADDLINES); It.GetNext(); ) {
      if (!(in->Flags&intercept_t::IF_IsALine)) Host_Error("PTR_SlideTraverse: not a line?");

      line_t *li = in->line;

      bool IsBlocked = false;
      if (!(li->flags&ML_TWOSIDED) || !li->backsector) {
        if (li->PointOnSide(Origin)) continue; // don't hit the back side
        IsBlocked = true;
      } else if (li->flags&(ML_BLOCKING|ML_BLOCKEVERYTHING)) {
        IsBlocked = true;
      } else if ((EntityFlags&EF_IsPlayer) && (li->flags&ML_BLOCKPLAYERS)) {
        IsBlocked = true;
      } else if ((EntityFlags&EF_CheckLineBlockMonsters) && (li->flags&ML_BLOCKMONSTERS)) {
        IsBlocked = true;
      }

      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlidePathTraverse: best=%g; frac=%g; line #%d; flags=0x%08x; blocked=%d", GetClass()->GetName(), BestSlideFrac, in->frac, (int)(ptrdiff_t)(li-&XLevel->Lines[0]), li->flags, (int)IsBlocked);
      #endif

      if (!IsBlocked) {
        // set openrange, opentop, openbottom
        TVec hpoint = SlideOrg+in->frac*SlideDir;
        opening_t *open = SV_LineOpenings(li, hpoint, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
        open = SV_FindOpening(open, Origin.z, Origin.z+Height);

        if (open && open->range >= Height && // fits
            open->top-Origin.z >= Height && // mobj is not too high
            open->bottom-Origin.z <= MaxStepHeight) // not too big a step up
        {
          // this line doesn't block movement
          if (Origin.z < open->bottom) {
            // check to make sure there's nothing in the way for the step up
            TVec CheckOrg = Origin;
            CheckOrg.z = open->bottom;
            if (!TestMobjZ(CheckOrg)) continue;
          } else {
            continue;
          }
        }
      }

      // the line blocks movement, see if it is closer than best so far
      if (in->frac < BestSlideFrac) {
        BestSlideFrac = in->frac;
        BestSlideLine = li;
      }

      break;  // stop
    }
  }
}


//==========================================================================
//
//  VEntity::SlideMove
//
//  The momx / momy move is bad, so try to slide along a wall.
//  Find the first line hit, move flush to it, and slide along it.
//  This is a kludgy mess.
//
//  `noPickups` means "don't activate specials" too.
//
//  k8: TODO: switch to beveled BSP!
//
//==========================================================================
void VEntity::SlideMove (float StepVelScale, bool noPickups) {
  float leadx, leady;
  float trailx, traily;
  int hitcount = 0;
  tmtrace_t tmtrace;
  memset((void *)&tmtrace, 0, sizeof(tmtrace)); // valgrind: AnyBlockingLine

  float XMove = Velocity.x*StepVelScale;
  float YMove = Velocity.y*StepVelScale;
  if (XMove == 0.0f && YMove == 0.0f) return; // just in case

  #ifdef VV_DBG_VERBOSE_SLIDE
  if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: === SlideMove; move=(%g,%g) ===", GetClass()->GetName(), XMove, YMove);
  #endif

  const float rad = GetMoveRadius();

  float prevVelX = XMove;
  float prevVelY = YMove;

  do {
    if (++hitcount == 3) {
      // don't loop forever
      const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true) : false);
      if (!movedY) TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true, noPickups);
      return;
    }

    if (XMove != 0.0f) prevVelX = XMove;
    if (YMove != 0.0f) prevVelY = YMove;

    // trace along the three leading corners
    if (/*XMove*/prevVelX > 0.0f) {
      leadx = Origin.x+rad;
      trailx = Origin.x-rad;
    } else {
      leadx = Origin.x-rad;
      trailx = Origin.x+rad;
    }

    if (/*Velocity.y*/prevVelY > 0.0f) {
      leady = Origin.y+rad;
      traily = Origin.y-rad;
    } else {
      leady = Origin.y-rad;
      traily = Origin.y+rad;
    }

    float BestSlideFrac = 1.00001f;
    line_t *BestSlideLine = nullptr;

    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; fracleft=%g; move=(%g,%g) (%g,%g) (%d:%d)", GetClass()->GetName(), hitcount, BestSlideFrac, XMove, YMove, prevVelX, prevVelY, (XMove == 0.0f), (YMove == 0.0f));
    #endif

    if (gm_use_new_slide_code.asBool()) {
      SlidePathTraverse(BestSlideFrac, BestSlideLine, Origin.x, Origin.y, StepVelScale);
      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d; pos=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), leadx, leady);
      #endif
    } else {
      // old slide code
      SlidePathTraverse(BestSlideFrac, BestSlideLine, leadx, leady, StepVelScale);
      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d; pos=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), leadx, leady);
      #endif
      if (BestSlideFrac != 0.0f) SlidePathTraverse(BestSlideFrac, BestSlideLine, trailx, leady, StepVelScale);
      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d; pos=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), trailx, leady);
      #endif
      if (BestSlideFrac != 0.0f) SlidePathTraverse(BestSlideFrac, BestSlideLine, leadx, traily, StepVelScale);
      #ifdef VV_DBG_VERBOSE_SLIDE
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d; pos=(%g,%g)", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), leadx, traily);
      #endif
    }

    // move up to the wall
    if (BestSlideFrac == 1.00001f) {
      // the move must have hit the middle, so stairstep
      const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true) : false);
      #ifdef VV_DBG_VERBOSE_SLIDE
      bool movedX = false;
      if (!movedY) movedX = (XMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true) : false);
      if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; stairstep! (no line found!); movedx=%d; movedy=%d", GetClass()->GetName(), hitcount, (int)movedX, (int)movedY);
      #else
      if (!movedY) TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true);
      #endif
      return;
    }

    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; frac=%g; line #%d", GetClass()->GetName(), hitcount, BestSlideFrac, (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]));
    #endif

    // fudge a bit to make sure it doesn't hit
    const float origSlide = BestSlideFrac; // we'll need it later
    BestSlideFrac -= 0.03125f;
    if (BestSlideFrac > 0.0f) {
      const float newx = XMove*BestSlideFrac;
      const float newy = YMove*BestSlideFrac;

      if (!TryMove(tmtrace, TVec(Origin.x+newx, Origin.y+newy, Origin.z), true)) {
        const bool movedY = (YMove != 0.0f ? TryMove(tmtrace, TVec(Origin.x, Origin.y+YMove, Origin.z), true) : false);
        if (!movedY) TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y, Origin.z), true);
        return;
      }
    }

    // now continue along the wall
    // first calculate remainder
    BestSlideFrac = 1.0f-origSlide;
    #ifdef VV_DBG_VERBOSE_SLIDE
    if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: SlideMove: hitcount=%d; fracleft=%g", GetClass()->GetName(), hitcount, BestSlideFrac);
    #endif

    if (BestSlideFrac > 1.0f) BestSlideFrac = 1.0f;
    if (BestSlideFrac <= 0.0f) return;

    // clip the moves
    // k8: don't multiply z, 'cause it makes jumping against a wall unpredictably hard
    Velocity.x *= BestSlideFrac;
    Velocity.y *= BestSlideFrac;
    Velocity = ClipVelocity(Velocity, BestSlideLine->normal, 1.0f);
    //Velocity = ClipVelocity(Velocity*BestSlideFrac, BestSlideLine->normal, 1.0f);
    XMove = Velocity.x*StepVelScale;
    YMove = Velocity.y*StepVelScale;
  } while (!TryMove(tmtrace, TVec(Origin.x+XMove, Origin.y+YMove, Origin.z), true));
}


//==========================================================================
//
//  VEntity::SlideMove
//
//  this is used to move chase camera
//
//==========================================================================
TVec VEntity::SlideMoveCamera (TVec org, TVec end, float radius) {
  TVec velo = end-org;
  if (!velo.isValid() || velo.isZero()) return org;

  if (radius < 2) {
    // just trace
    linetrace_t ltr;
    if (XLevel->TraceLine(ltr, org, end, 0/*SPF_NOBLOCKSIGHT*/)) return end; // no hit
    // hit something, move slightly forward
    TVec mdelta = ltr.LineEnd-org;
    //const float wantdist = velo.length();
    const float movedist = mdelta.length();
    if (movedist > 2.0f) {
      //GCon->Logf("*** hit! (%g,%g,%g)", ltr.HitPlaneNormal.x, ltr.HitPlaneNormal.y, ltr.HitPlaneNormal.z);
      if (ltr.HitPlaneNormal.z) {
        // floor
        //GCon->Logf("floor hit! (%g,%g,%g)", ltr.HitPlaneNormal.x, ltr.HitPlaneNormal.y, ltr.HitPlaneNormal.z);
        ltr.LineEnd += ltr.HitPlaneNormal*2;
      } else {
        ltr.LineEnd -= mdelta.normalised()*2;
      }
    }
    return ltr.LineEnd;
  }

  // split move in multiple steps if moving too fast
  const float xmove = velo.x;
  const float ymove = velo.y;

  int Steps = 1;
  float XStep = fabsf(xmove);
  float YStep = fabsf(ymove);
  float MaxStep = radius-1.0f;

  if (XStep > MaxStep || YStep > MaxStep) {
    if (XStep > YStep) {
      Steps = int(XStep/MaxStep)+1;
    } else {
      Steps = int(YStep/MaxStep)+1;
    }
  }

  const float StepXMove = xmove/float(Steps);
  const float StepYMove = ymove/float(Steps);
  const float StepZMove = velo.z/float(Steps);

  //GCon->Logf("*** *** Steps=%d; move=(%g,%g,%g); onestep=(%g,%g,%g)", Steps, xmove, ymove, velo.z, StepXMove, StepYMove, StepZMove);
  tmtrace_t tmtrace;
  for (int step = 0; step < Steps; ++step) {
    float ptryx = org.x+StepXMove;
    float ptryy = org.y+StepYMove;
    float ptryz = org.z+StepZMove;

    TVec newPos = TVec(ptryx, ptryy, ptryz);
    bool check = CheckRelPosition(tmtrace, newPos, true);
    if (check) {
      org = newPos;
      continue;
    }

    // blocked move; trace back until we got a good position
    // this sux, but we'll do it only once per frame, so...
    float len = (newPos-org).length();
    TVec dir = (newPos-org).normalised(); // to newpos
    //GCon->Logf("*** len=%g; dir=(%g,%g,%g)", len, dir.x, dir.y, dir.z);
    float curr = 1.0f;
    while (curr <= len) {
      if (!CheckRelPosition(tmtrace, org+dir*curr, true)) {
        curr -= 1.0f;
        break;
      }
      curr += 1.0f;
    }
    if (curr > len) curr = len;
    //GCon->Logf("   final len=%g", curr);
    org += dir*curr;
    break;
  }

  // clamp to floor/ceiling
  CheckRelPosition(tmtrace, org, true);
  if (org.z < tmtrace.FloorZ+radius) org.z = tmtrace.FloorZ+radius;
  if (org.z > tmtrace.CeilingZ-radius) org.z = tmtrace.CeilingZ-radius;
  return org;
}


//**************************************************************************
//
//  BOUNCING
//
//  Bounce missile against walls
//
//**************************************************************************

//============================================================================
//
//  VEntity::BounceWall
//
//============================================================================
void VEntity::BounceWall (float DeltaTime, const line_t *blockline, float overbounce, float bouncefactor) {
  const line_t *BestSlideLine = nullptr; //blockline;

  if (!BestSlideLine || BestSlideLine->PointOnSide(Origin)) {
    BestSlideLine = nullptr;
    TVec SlideOrg = Origin;
    const float rad = max2(1.0f, GetMoveRadius());
    SlideOrg.x += (Velocity.x > 0.0f ? rad : -rad);
    SlideOrg.y += (Velocity.y > 0.0f ? rad : -rad);
    TVec SlideDir = Velocity*DeltaTime;
    intercept_t *in;
    float BestSlideFrac = 99999.0f;

    //GCon->Logf(NAME_Debug, "%s:%u:xxx: vel=(%g,%g,%g); dt=%g; slide=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f, DeltaTime, SlideDir.x, SlideDir.y, SlideDir.z);
    for (VPathTraverse It(this, &in, SlideOrg.x, SlideOrg.y, SlideOrg.x+SlideDir.x, SlideOrg.y+SlideDir.y, PT_ADDLINES); It.GetNext(); ) {
      if (!(in->Flags&intercept_t::IF_IsALine)) Host_Error("PTR_BounceTraverse: not a line?");
      line_t *li = in->line;
      //if (in->frac > 1.0f) continue;

      if (!(li->flags&ML_BLOCKEVERYTHING)) {
        if (li->flags&ML_TWOSIDED) {
          // set openrange, opentop, openbottom
          TVec hpoint = SlideOrg+in->frac*SlideDir;
          opening_t *open = SV_LineOpenings(li, hpoint, SPF_NOBLOCKING, true); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
          open = SV_FindOpening(open, Origin.z, Origin.z+Height);

          if (open && open->range >= Height && // fits
              Origin.z+Height <= open->top &&
              Origin.z >= open->bottom) // mobj is not too high
          {
            continue; // this line doesn't block movement
          }
        } else {
          if (li->PointOnSide(Origin)) continue; // don't hit the back side
        }
      }

      if (!BestSlideLine || BestSlideFrac < in->frac) {
        BestSlideFrac = in->frac;
        BestSlideLine = li;
      }
      //break;
    }
    if (!BestSlideLine) {
      //GCon->Logf(NAME_Debug, "%s:%u:999: cannot find slide line! vel=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f);
      BestSlideLine = blockline;
    }
  }

  if (BestSlideLine) {
    /*
    TAVec delta_ang;
    TAVec lineang;
    TVec delta(0, 0, 0);

    // convert BestSlideLine normal to an angle
    lineang = VectorAngles(BestSlideLine->normal);
    if (BestSlideLine->PointOnSide(Origin)) lineang.yaw += 180.0f;

    // convert the line angle back to a vector, so that
    // we can use it to calculate the delta against the Velocity vector
    delta = AngleVector(lineang);
    delta = (delta*2.0f)-Velocity;

    if (delta.length2DSquared() < 1.0f) {
      GCon->Logf(NAME_Debug, "%s:%u: deltalen=%g", GetClass()->GetName(), GetUniqueId(), delta.length2D());
    }

    // finally get the delta angle to use
    delta_ang = VectorAngles(delta);

    Velocity.x = (Velocity.x*bouncefactor)*cos(delta_ang.yaw);
    Velocity.y = (Velocity.y*bouncefactor)*sin(delta_ang.yaw);
    #if 0
    const float vz = Velocity.z;
    Velocity = ClipVelocity(Velocity, BestSlideLine->normal, overbounce);
    Velocity.z = vz; // just in case
    #endif
    */

    float lineangle = VectorAngleYaw(BestSlideLine->normal);
    if (BestSlideLine->PointOnSide(Origin)) lineangle += 180.0f;
    lineangle = AngleMod(lineangle);

    float moveangle = AngleMod(VectorAngleYaw(Velocity));
    float deltaangle = AngleMod((lineangle*2.0f)-moveangle);
    Angles.yaw = /*AngleMod*/(deltaangle);

    float movelen = Velocity.length2D()*bouncefactor;

    #if 0
    float bbox3d[6];
    Create3DBBox(bbox3d, Origin, rad+0.1f, Height);
    float bbox2d[4];
    Create2DBBox(bbox2d, Origin, rad+0.1f);
    GCon->Logf(NAME_Debug, "%s:%u:BOXSIDE: %d : %d", GetClass()->GetName(), GetUniqueId(), BestSlideLine->checkBoxEx(bbox3d), BoxOnLineSide2D(bbox2d, *BestSlideLine->v1, *BestSlideLine->v2));
    #endif

    //TODO: unstuck from the wall?
    #if 0
    float bbox[4];
    Create2DBBox(bbox, Origin, max2(1.0f, rad+0.25f));
    if (BoxOnLineSide2D(bbox, *BestSlideLine->v1, *BestSlideLine->v2) == -1) {
      GCon->Logf(NAME_Debug, "%s:%u:666: STUCK! lineangle=%g; moveangle=%g; deltaangle=%g; movelen=%g; vel.xy=(%g,%g,%g); side=%d", GetClass()->GetName(), GetUniqueId(), lineangle, moveangle, deltaangle, movelen, Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f, BestSlideLine->PointOnSide(Origin));
      UnlinkFromWorld();
      TVec mvdir = AngleVectorYaw(deltaangle);
      Origin += mvdir;
      LinkToWorld();
    } else {
      //GCon->Logf(NAME_Debug, "BOXSIDE: %d", BoxOnLineSide2D(bbox, *BestSlideLine->v1, *BestSlideLine->v2));
    }
    #endif

    //GCon->Logf(NAME_Debug, "%s:%u:000: linedef=%d; lineangle=%g; moveangle=%g; deltaangle=%g; movelen=%g; vel.xy=(%g,%g,%g); side=%d", GetClass()->GetName(), GetUniqueId(), (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), lineangle, moveangle, deltaangle, movelen, Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f, BestSlideLine->PointOnSide(Origin));

    if (movelen < 35.0f) movelen = 70.0f;
    TVec newvel = AngleVectorYaw(deltaangle)*movelen;
    Velocity.x = newvel.x;
    Velocity.y = newvel.y;

    //GCon->Logf(NAME_Debug, "%s:%u:001: oldangle=%g; newangle=%g; newvel.xy=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), moveangle, deltaangle, Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f);

    #if 1
    const float vz = Velocity.z;
    Velocity = ClipVelocity(Velocity, BestSlideLine->normal, overbounce);
    Velocity.z = vz; // just in case
    #endif

    // roughly smaller than lowest fixed point 16.16 (it is more like 0.0000152587890625)
    /*
    if (fabsf(Velocity.x) < 0.000016) Velocity.x = 0;
    if (fabsf(Velocity.y) < 0.000016) Velocity.y = 0;
    */
  } else {
    //GCon->Logf(NAME_Debug, "%s:%u:999: cannot find slide line! vel=(%g,%g,%g); sdir=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f, SlideDir.x, SlideDir.y, SlideDir.z);
    //GCon->Logf(NAME_Debug, "%s:%u:999: cannot find slide line! vel=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f);
  }
}



//**************************************************************************
//
//  TEST ON MOBJ
//
//**************************************************************************

//=============================================================================
//
//  TestMobjZ
//
//  Checks if the new Z position is legal
//  returns blocking thing
//
//=============================================================================
VEntity *VEntity::TestMobjZ (const TVec &TryOrg) {
  // can't hit things, or not solid?
  if ((EntityFlags&(EF_ColideWithThings|EF_Solid)) != (EF_ColideWithThings|EF_Solid)) return nullptr;

  const float rad = GetMoveRadius();

  // the bounding box is extended by MAXRADIUS because mobj_ts are grouped
  // into mapblocks based on their origin point, and can overlap into adjacent
  // blocks by up to MAXRADIUS units
  //FIXME
  DeclareMakeBlockMapCoords(TryOrg.x, TryOrg.y, (rad+MAXRADIUS), xl, yl, xh, yh);
  // xl->xh, yl->yh determine the mapblock set to search
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      for (VBlockThingsIterator Other(XLevel, bx, by); Other; ++Other) {
        if (*Other == this) continue; // don't clip against self
        //if (OwnerSUId && Other->ServerUId == OwnerSUId) continue;
        //k8: can't hit corpse
        if ((Other->EntityFlags&(EF_ColideWithThings|EF_Solid|EF_Corpse)) != (EF_ColideWithThings|EF_Solid)) continue; // can't hit things, or not solid
        const float ohgt = GetBlockingHeightFor(*Other);
        if (TryOrg.z > Other->Origin.z+ohgt) continue; // over thing
        if (TryOrg.z+Height < Other->Origin.z) continue; // under thing
        const float blockdist = Other->GetMoveRadius()+rad;
        if (fabsf(Other->Origin.x-TryOrg.x) >= blockdist ||
            fabsf(Other->Origin.y-TryOrg.y) >= blockdist)
        {
          // didn't hit thing
          continue;
        }
        return *Other;
      }
    }
  }

  return nullptr;
}


//=============================================================================
//
//  VEntity::FakeZMovement
//
//  Fake the zmovement so that we can check if a move is legal
//
//=============================================================================
TVec VEntity::FakeZMovement () {
  TVec Ret = TVec(0, 0, 0);
  eventCalcFakeZMovement(Ret, host_frametime);
  // clip movement
  if (Ret.z <= FloorZ) Ret.z = FloorZ; // hit the floor
  if (Ret.z+Height > CeilingZ) Ret.z = CeilingZ-Height; // hit the ceiling
  return Ret;
}


//=============================================================================
//
//  VEntity::CheckOnmobj
//
//  Checks if an object is above another object
//
//=============================================================================
VEntity *VEntity::CheckOnmobj () {
  // can't hit things, or not solid? (check it here to save one VM invocation)
  if ((EntityFlags&(EF_ColideWithThings|EF_Solid)) != (EF_ColideWithThings|EF_Solid)) return nullptr;
  return TestMobjZ(FakeZMovement());
}


//==========================================================================
//
//  VEntity::CheckSides
//
//  This routine checks for Lost Souls trying to be spawned    // phares
//  across 1-sided lines, impassible lines, or "monsters can't //   |
//  cross" lines. Draw an imaginary line between the PE        //   V
//  and the new Lost Soul spawn spot. If that line crosses
//  a 'blocking' line, then disallow the spawn. Only search
//  lines in the blocks of the blockmap where the bounding box
//  of the trajectory line resides. Then check bounding box
//  of the trajectory vs. the bounding box of each blocking
//  line to see if the trajectory and the blocking line cross.
//  Then check the PE and LS to see if they're on different
//  sides of the blocking line. If so, return true, otherwise
//  false.
//
//==========================================================================
bool VEntity::CheckSides (TVec lsPos) {
  // here is the bounding box of the trajectory
  float tmbbox[4];
  tmbbox[BOX2D_LEFT] = min2(Origin.x, lsPos.x);
  tmbbox[BOX2D_RIGHT] = max2(Origin.x, lsPos.x);
  tmbbox[BOX2D_TOP] = max2(Origin.y, lsPos.y);
  tmbbox[BOX2D_BOTTOM] = min2(Origin.y, lsPos.y);

  // determine which blocks to look in for blocking lines
  DeclareMakeBlockMapCoordsBBox2D(tmbbox, xl, yl, xh, yh);

  //k8: is this right?
  int projblk = (EntityFlags&VEntity::EF_Missile ? ML_BLOCKPROJECTILE : 0);

  // xl->xh, yl->yh determine the mapblock set to search
  //++validcount; // prevents checking same line twice
  XLevel->IncrementValidCount();
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      line_t *ld;
      for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
        // Checks to see if a PE->LS trajectory line crosses a blocking
        // line. Returns false if it does.
        //
        // tmbbox holds the bounding box of the trajectory. If that box
        // does not touch the bounding box of the line in question,
        // then the trajectory is not blocked. If the PE is on one side
        // of the line and the LS is on the other side, then the
        // trajectory is blocked.
        //
        // Currently this assumes an infinite line, which is not quite
        // correct. A more correct solution would be to check for an
        // intersection of the trajectory and the line, but that takes
        // longer and probably really isn't worth the effort.

        if (ld->flags&(ML_BLOCKING|ML_BLOCKMONSTERS|ML_BLOCKEVERYTHING|projblk)) {
          if (tmbbox[BOX2D_LEFT] <= ld->bbox2d[BOX2D_RIGHT] &&
              tmbbox[BOX2D_RIGHT] >= ld->bbox2d[BOX2D_LEFT] &&
              tmbbox[BOX2D_TOP] >= ld->bbox2d[BOX2D_BOTTOM] &&
              tmbbox[BOX2D_BOTTOM] <= ld->bbox2d[BOX2D_TOP])
          {
            if (ld->PointOnSide(Origin) != ld->PointOnSide(lsPos)) return true; // line blocks trajectory
          }
        }

        // line doesn't block trajectory
      }
    }
  }

  return false;
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
      line_t *ld;
      for (VBlockLinesIterator It(XLevel, bx, by, &ld); It.GetNext(); ) {
        if (ld->frontsector == ld->backsector) continue; // skip two-sided lines inside a single sector

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
        float distance = 0;
        {
          const double v1x = dll_x;
          const double v1y = dll_y;
          const double v1dx = dll_dx;
          const double v1dy = dll_dy;
          const double v2x = dlv_x;
          const double v2y = dlv_y;
          const double v2dx = dlv_dx;
          const double v2dy = dlv_dy;

          const double den = v1dy*v2dx - v1dx*v2dy;

          if (den == 0) {
            // parallel
            distance = 0;
          } else {
            const double num = (v1x-v2x)*v1dy+(v2y-v1y)*v1dx;
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


//=============================================================================
//
//  CheckDropOff
//
//  killough 11/98:
//
//  Monsters try to move away from tall dropoffs.
//
//  In Doom, they were never allowed to hang over dropoffs, and would remain
//  stuck if involuntarily forced over one. This logic, combined with P_TryMove,
//  allows monsters to free themselves without making them tend to hang over
//  dropoffs.
//
//=============================================================================
void VEntity::CheckDropOff (float &DeltaX, float &DeltaY, float baseSpeed) {
  // try to move away from a dropoff
  DeltaX = DeltaY = 0;

  const float rad = GetMoveRadius();
  float tmbbox[4];
  Create2DBBox(tmbbox, Origin, rad);
  DeclareMakeBlockMapCoordsBBox2D(tmbbox, xl, yl, xh, yh);

  // check lines
  //++validcount;
  XLevel->IncrementValidCount();
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      line_t *line;
      for (VBlockLinesIterator It(XLevel, bx, by, &line); It.GetNext(); ) {
        if (!line->backsector || line->frontsector == line->backsector) continue; // ignore one-sided linedefs and selfrefs
        // linedef must be contacted
        if (tmbbox[BOX2D_RIGHT] > line->bbox2d[BOX2D_LEFT] &&
            tmbbox[BOX2D_LEFT] < line->bbox2d[BOX2D_RIGHT] &&
            tmbbox[BOX2D_TOP] > line->bbox2d[BOX2D_BOTTOM] &&
            tmbbox[BOX2D_BOTTOM] < line->bbox2d[BOX2D_TOP] &&
            P_BoxOnLineSide(tmbbox, line) == -1)
        {
          // new logic for 3D Floors
          /*
          sec_region_t *FrontReg = SV_FindThingGap(line->frontsector, Origin, Height);
          sec_region_t *BackReg = SV_FindThingGap(line->backsector, Origin, Height);
          float front = FrontReg->efloor.GetPointZClamped(Origin);
          float back = BackReg->efloor.GetPointZClamped(Origin);
          */
          TSecPlaneRef ffloor, fceiling;
          TSecPlaneRef bfloor, bceiling;
          SV_FindGapFloorCeiling(line->frontsector, Origin, Height, ffloor, fceiling);
          SV_FindGapFloorCeiling(line->backsector, Origin, Height, bfloor, bceiling);
          const float front = ffloor.GetPointZClamped(Origin);
          const float back = bfloor.GetPointZClamped(Origin);

          // the monster must contact one of the two floors, and the other must be a tall dropoff
          TVec Dir;
          if (back == Origin.z && front < Origin.z-MaxDropoffHeight) {
            // front side dropoff
            Dir = line->normal;
          } else if (front == Origin.z && back < Origin.z-MaxDropoffHeight) {
            // back side dropoff
            Dir = -line->normal;
          } else {
            continue;
          }
          // move away from dropoff at a standard speed
          // multiple contacted linedefs are cumulative (e.g. hanging over corner)
          DeltaX -= Dir.x*baseSpeed;
          DeltaY -= Dir.y*baseSpeed;
        }
      }
    }
  }
}


//=============================================================================
//
//  FindDropOffLines
//
//  find dropoff lines (the same as `CheckDropOff()` is using)
//
//=============================================================================
int VEntity::FindDropOffLine (TArray<VDropOffLineInfo> *list, TVec pos) {
  int res = 0;

  const float rad = GetMoveRadius();
  float tmbbox[4];
  Create2DBBox(tmbbox, Origin, rad);
  DeclareMakeBlockMapCoordsBBox2D(tmbbox, xl, yl, xh, yh);

  // check lines
  //++validcount;
  XLevel->IncrementValidCount();
  for (int bx = xl; bx <= xh; ++bx) {
    for (int by = yl; by <= yh; ++by) {
      line_t *line;
      for (VBlockLinesIterator It(XLevel, bx, by, &line); It.GetNext(); ) {
        if (!line->backsector) continue; // ignore one-sided linedefs
        // linedef must be contacted
        if (tmbbox[BOX2D_RIGHT] > line->bbox2d[BOX2D_LEFT] &&
            tmbbox[BOX2D_LEFT] < line->bbox2d[BOX2D_RIGHT] &&
            tmbbox[BOX2D_TOP] > line->bbox2d[BOX2D_BOTTOM] &&
            tmbbox[BOX2D_BOTTOM] < line->bbox2d[BOX2D_TOP] &&
            P_BoxOnLineSide(tmbbox, line) == -1)
        {
          // new logic for 3D Floors
          /*
          sec_region_t *FrontReg = SV_FindThingGap(line->frontsector, Origin, Height);
          sec_region_t *BackReg = SV_FindThingGap(line->backsector, Origin, Height);
          float front = FrontReg->efloor.GetPointZClamped(Origin);
          float back = BackReg->efloor.GetPointZClamped(Origin);
          */
          TSecPlaneRef ffloor, fceiling;
          TSecPlaneRef bfloor, bceiling;
          SV_FindGapFloorCeiling(line->frontsector, Origin, Height, ffloor, fceiling);
          SV_FindGapFloorCeiling(line->backsector, Origin, Height, bfloor, bceiling);
          const float front = ffloor.GetPointZClamped(Origin);
          const float back = bfloor.GetPointZClamped(Origin);

          // the monster must contact one of the two floors, and the other must be a tall dropoff
          int side;
          if (back == Origin.z && front < Origin.z-MaxDropoffHeight) {
            // front side dropoff
            side = 0;
          } else if (front == Origin.z && back < Origin.z-MaxDropoffHeight) {
            // back side dropoff
            side = 1;
          } else {
            continue;
          }

          ++res;
          if (list) {
            VDropOffLineInfo *la = &list->alloc();
            la->line = line;
            la->side = side;
          }
        }
      }
    }
  }

  return res;
}


//=============================================================================
//
//  VEntity::UpdateVelocity
//
//  called from entity `Physics()`
//
//=============================================================================
void VEntity::UpdateVelocity (float DeltaTime, bool allowSlopeFriction) {
  if (!Sector) return; // just in case

  /*
  if (Origin.z <= FloorZ && !Velocity && !bCountKill && !bIsPlayer) {
    // no gravity for non-moving things on ground to prevent static objects from sliding on slopes
    return;
  }
  */

  const float fnormz = EFloor.GetNormalZ();

  // apply slope friction
  if (allowSlopeFriction && Origin.z <= FloorZ && fnormz != 1.0f && (EntityFlags&(EF_Fly|EF_Missile)) == 0 &&
      fabsf(Sector->floor.maxz-Sector->floor.minz) > MaxStepHeight)
  {
    if (IsPlayer() && Player && Player->IsNoclipActive()) {
      // do nothing
    } else if (true /*fnormz <= 0.7f*/) {
      TVec Vel = EFloor.GetNormal();
      const float dot = DotProduct(Velocity, Vel);
      if (dot < 0.0f) {
        /*
        if (IsPlayer()) {
          GCon->Logf(NAME_Debug, "%s: slopeZ=%g; dot=%g; Velocity=(%g,%g,%g); Vel=(%g,%g,%g); Vel*dot=(%g,%g,%g)",
            GetClass()->GetName(), fnormz, dot, Velocity.x, Velocity.y, Velocity.z, Vel.x, Vel.y, Vel.z, Vel.x*dot, Vel.y*dot, Vel.z*dot);
        }
        */
        //TVec Vel = dot*EFloor.spGetNormal();
        //if (bIsPlayer) printdebug("%C: Velocity=%s; Vel=%s; dot=%s; Vel*dot=%s (%s); dt=%s", self, Velocity.xy, Vel, dot, Vel.xy*dot, Vel.xy*(dot*DeltaTime), DeltaTime);
        //Vel *= dot*35.0f*DeltaTime;
        if (fnormz <= 0.7f) {
          Vel *= dot*1.75f; // k8: i pulled this out of my ass
        } else {
          Vel *= dot*DeltaTime;
        }
        Vel.z = 0;
        //print("mht=%s; hgt=%s", MaxStepHeight, Sector.floor.maxz-Sector.floor.minz);
        /*
        print("vv: Vel=%s; dot=%s; norm=%s", Vel, dot, EFloor.spGetNormal());
        print("  : z=%s; fminz=%s; fmaxz=%s", Origin.z, Sector.floor.minz, Sector.floor.maxz);
        print("  : velocity: %s -- %s", Velocity, Velocity-Vel.xy);
        */
        Velocity -= Vel;
      }
    }
  }

  if (EntityFlags&EF_NoGravity) return;

  const float dz = Origin.z-FloorZ;

  // don't add gravity if standing on slope with normal.z > 0.7 (aprox 45 degrees)
  if (dz > 0.6f || fnormz <= 0.7f) {
    //if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: *** dfz=%g; normz=%g", GetClass()->GetName(), dz, EFloor.GetNormalZ());
    if (WaterLevel < 2) {
      Velocity.z -= Gravity*Level->Gravity*Sector->Gravity*DeltaTime;
    } else if (!IsPlayer() || Health <= 0) {
      // water gravity
      Velocity.z -= Gravity*Level->Gravity*Sector->Gravity/10.0f*DeltaTime;
      float startvelz = Velocity.z;
      float sinkspeed = -WaterSinkSpeed/(IsRealCorpse() ? 3.0f : 1.0f);
      if (Velocity.z < sinkspeed) {
        Velocity.z = (startvelz < sinkspeed ? startvelz : sinkspeed);
      } else {
        Velocity.z = startvelz+(Velocity.z-startvelz)*WaterSinkFactor;
      }
    } else if (dz > 0.0f && Velocity.z == 0.0f) {
      // snap to the floor
      Velocity.z = dz;
      //if (IsPlayer()) GCon->Logf(NAME_Debug, "%s: *** SNAP! dfz=%g; normz=%g", GetClass()->GetName(), dz, EFloor.GetNormalZ());
    }
  }
}


//==========================================================================
//
//  VRoughBlockSearchIterator
//
//==========================================================================
VRoughBlockSearchIterator::VRoughBlockSearchIterator (VEntity *ASelf, int ADistance, VEntity **AEntPtr)
  : Self(ASelf)
  , Distance(ADistance)
  , Ent(nullptr)
  , EntPtr(AEntPtr)
  , Count(1)
  , CurrentEdge(-1)
{
  StartX = MapBlock(Self->Origin.x-Self->XLevel->BlockMapOrgX);
  StartY = MapBlock(Self->Origin.y-Self->XLevel->BlockMapOrgY);

  // start with current block
  if (StartX >= 0 && StartX < Self->XLevel->BlockMapWidth &&
      StartY >= 0 && StartY < Self->XLevel->BlockMapHeight)
  {
    Ent = Self->XLevel->BlockLinks[StartY*Self->XLevel->BlockMapWidth+StartX];
  }
}


//==========================================================================
//
//  VRoughBlockSearchIterator::GetNext
//
//==========================================================================
bool VRoughBlockSearchIterator::GetNext () {
  int BlockX;
  int BlockY;

  for (;;) {
    while (Ent && Ent->IsGoingToDie()) Ent = Ent->BlockMapNext;

    if (Ent) {
      *EntPtr = Ent;
      Ent = Ent->BlockMapNext;
      return true;
    }

    switch (CurrentEdge) {
      case 0:
        // trace the first block section (along the top)
        if (BlockIndex <= FirstStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          ++BlockIndex;
        } else {
          CurrentEdge = 1;
          --BlockIndex;
        }
        break;
      case 1:
        // trace the second block section (right edge)
        if (BlockIndex <= SecondStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          BlockIndex += Self->XLevel->BlockMapWidth;
        } else {
          CurrentEdge = 2;
          BlockIndex -= Self->XLevel->BlockMapWidth;
        }
        break;
      case 2:
        // trace the third block section (bottom edge)
        if (BlockIndex >= ThirdStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          --BlockIndex;
        } else {
          CurrentEdge = 3;
          ++BlockIndex;
        }
        break;
      case 3:
        // trace the final block section (left edge)
        if (BlockIndex > FinalStop) {
          Ent = Self->XLevel->BlockLinks[BlockIndex];
          BlockIndex -= Self->XLevel->BlockMapWidth;
        } else {
          CurrentEdge = -1;
        }
        break;
      default:
        if (Count > Distance) return false; // we are done
        BlockX = StartX-Count;
        BlockY = StartY-Count;

        if (BlockY < 0) {
          BlockY = 0;
        } else if (BlockY >= Self->XLevel->BlockMapHeight) {
          BlockY = Self->XLevel->BlockMapHeight-1;
        }
        if (BlockX < 0) {
          BlockX = 0;
        } else if (BlockX >= Self->XLevel->BlockMapWidth) {
          BlockX = Self->XLevel->BlockMapWidth-1;
        }
        BlockIndex = BlockY*Self->XLevel->BlockMapWidth+BlockX;
        FirstStop = StartX+Count;
        if (FirstStop < 0) { ++Count; break; }
        if (FirstStop >= Self->XLevel->BlockMapWidth) FirstStop = Self->XLevel->BlockMapWidth-1;
        SecondStop = StartY+Count;
        if (SecondStop < 0) { ++Count; break; }
        if (SecondStop >= Self->XLevel->BlockMapHeight) SecondStop = Self->XLevel->BlockMapHeight-1;
        ThirdStop = SecondStop*Self->XLevel->BlockMapWidth+BlockX;
        SecondStop = SecondStop*Self->XLevel->BlockMapWidth+FirstStop;
        FirstStop += BlockY*Self->XLevel->BlockMapWidth;
        FinalStop = BlockIndex;
        ++Count;
        CurrentEdge = 0;
        break;
    }
  }

  return false;
}


//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, CheckWater) {
  vobjGetParamSelf();
  Self->CheckWater();
}

IMPLEMENT_FUNCTION(VEntity, CheckDropOff) {
  float *DeltaX;
  float *DeltaY;
  VOptParamFloat baseSpeed(32.0f);
  vobjGetParamSelf(DeltaX, DeltaY, baseSpeed);
  Self->CheckDropOff(*DeltaX, *DeltaY, baseSpeed);
}

// native final int FindDropOffLine (ref array!VDropOffLineInfo list, TVec pos);
IMPLEMENT_FUNCTION(VEntity, FindDropOffLine) {
  TArray<VDropOffLineInfo> *list;
  TVec pos;
  vobjGetParamSelf(list, pos);
  RET_INT(Self->FindDropOffLine(list, pos));
}

IMPLEMENT_FUNCTION(VEntity, CheckPosition) {
  TVec pos;
  vobjGetParamSelf(pos);
  RET_BOOL(Self->CheckPosition(pos));
}

// native final bool CheckRelPosition (out tmtrace_t tmtrace, TVec Pos, optional bool noPickups/*=false*/, optional bool ignoreMonsters, optional bool ignorePlayers);
IMPLEMENT_FUNCTION(VEntity, CheckRelPosition) {
  tmtrace_t tmp;
  tmtrace_t *tmtrace = nullptr;
  TVec Pos;
  VOptParamBool noPickups(false);
  VOptParamBool ignoreMonsters(false);
  VOptParamBool ignorePlayers(false);
  vobjGetParamSelf(tmtrace, Pos, noPickups, ignoreMonsters, ignorePlayers);
  if (!tmtrace) tmtrace = &tmp;
  RET_BOOL(Self->CheckRelPosition(*tmtrace, Pos, noPickups, ignoreMonsters, ignorePlayers));
}

IMPLEMENT_FUNCTION(VEntity, CheckSides) {
  TVec lsPos;
  vobjGetParamSelf(lsPos);
  RET_BOOL(Self->CheckSides(lsPos));
}

IMPLEMENT_FUNCTION(VEntity, FixMapthingPos) {
  vobjGetParamSelf();
  RET_BOOL(Self->FixMapthingPos());
}

IMPLEMENT_FUNCTION(VEntity, TryMove) {
  tmtrace_t tmtrace;
  TVec Pos;
  bool AllowDropOff;
  VOptParamBool checkOnly(false);
  vobjGetParamSelf(Pos, AllowDropOff, checkOnly);
  RET_BOOL(Self->TryMove(tmtrace, Pos, AllowDropOff, checkOnly));
}

IMPLEMENT_FUNCTION(VEntity, TryMoveEx) {
  tmtrace_t tmp;
  tmtrace_t *tmtrace = nullptr;
  TVec Pos;
  bool AllowDropOff;
  VOptParamBool checkOnly(false);
  vobjGetParamSelf(tmtrace, Pos, AllowDropOff, checkOnly);
  if (!tmtrace) tmtrace = &tmp;
  RET_BOOL(Self->TryMove(*tmtrace, Pos, AllowDropOff, checkOnly));
}

IMPLEMENT_FUNCTION(VEntity, TestMobjZ) {
  vobjGetParamSelf();
  RET_BOOL(!Self->TestMobjZ(Self->Origin));
}

IMPLEMENT_FUNCTION(VEntity, SlideMove) {
  float StepVelScale;
  VOptParamBool noPickups(false);
  vobjGetParamSelf(StepVelScale, noPickups);
  Self->SlideMove(StepVelScale, noPickups);
}

//native final void BounceWall (float DeltaTime, const line_t *blockline, float overbounce, float bouncefactor);
IMPLEMENT_FUNCTION(VEntity, BounceWall) {
  line_t *blkline;
  float deltaTime, overbounce, bouncefactor;
  vobjGetParamSelf(deltaTime, blkline, overbounce, bouncefactor);
  Self->BounceWall(deltaTime, blkline, overbounce, bouncefactor);
}

IMPLEMENT_FUNCTION(VEntity, CheckOnmobj) {
  vobjGetParamSelf();
  RET_REF(Self->CheckOnmobj());
}

IMPLEMENT_FUNCTION(VEntity, LinkToWorld) {
  VOptParamInt properFloorCheck(0);
  vobjGetParamSelf(properFloorCheck);
  Self->LinkToWorld(properFloorCheck);
}

IMPLEMENT_FUNCTION(VEntity, UnlinkFromWorld) {
  vobjGetParamSelf();
  Self->UnlinkFromWorld();
}

IMPLEMENT_FUNCTION(VEntity, CanSee) {
  VEntity *Other;
  VOptParamBool disableBetterSight(false);
  vobjGetParamSelf(Other, disableBetterSight);
  //if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanSee(Other, disableBetterSight));
}

IMPLEMENT_FUNCTION(VEntity, CanSeeAdv) {
  VEntity *Other;
  vobjGetParamSelf(Other);
  //if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanSee(Other, false, true));
}

IMPLEMENT_FUNCTION(VEntity, CanShoot) {
  VEntity *Other;
  vobjGetParamSelf(Other);
  //if (!Self) { VObject::VMDumpCallStack(); Sys_Error("empty `self`!"); }
  RET_BOOL(Self->CanShoot(Other));
}

IMPLEMENT_FUNCTION(VEntity, RoughBlockSearch) {
  VEntity **EntPtr;
  int Distance;
  vobjGetParamSelf(EntPtr, Distance);
  RET_PTR(new VRoughBlockSearchIterator(Self, Distance, EntPtr));
}


// native void UpdateVelocity (float DeltaTime, bool allowSlopeFriction);
IMPLEMENT_FUNCTION(VEntity, UpdateVelocity) {
  float DeltaTime;
  bool allowSlopeFriction;
  vobjGetParamSelf(DeltaTime, allowSlopeFriction);
  Self->UpdateVelocity(DeltaTime, allowSlopeFriction);
}


// native final float GetBlockingHeightFor (Entity other);
IMPLEMENT_FUNCTION(VEntity, GetBlockingHeightFor) {
  VEntity *other;
  vobjGetParamSelf(other);
  RET_FLOAT(other ? Self->GetBlockingHeightFor(other) : 0.0f);
}
