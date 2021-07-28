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
#include "p_levelinfo.h"
#include "p_world.h"
#include "p_player.h"


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
//  in:
//   a position to be checked (doesn't need to be related to the mobj_t->x,y)
//
//==========================================================================
bool VEntity::CheckPosition (TVec Pos) {
  if (!(EntityFlags&(EF_ColideWithThings|EF_ColideWithWorld))) return true;

  tmtrace_t cptrace;
  //bool good = true;

  memset((void *)&cptrace, 0, sizeof(cptrace));
  cptrace.End = Pos;

  const float rad = GetMoveRadius();

  cptrace.BBox[BOX2D_TOP] = Pos.y+rad;
  cptrace.BBox[BOX2D_BOTTOM] = Pos.y-rad;
  cptrace.BBox[BOX2D_RIGHT] = Pos.x+rad;
  cptrace.BBox[BOX2D_LEFT] = Pos.x-rad;

  subsector_t *newsubsec = XLevel->PointInSubsector_Buggy(Pos);

  // The base floor / ceiling is from the subsector that contains the point.
  // Any contacted lines the step closer together will adjust them.
  XLevel->FindGapFloorCeiling(newsubsec->sector, Pos, Height, cptrace.EFloor, cptrace.ECeiling);
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

  XLevel->IncrementValidCount();

  if (EntityFlags&EF_ColideWithThings) {
    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    DeclareMakeBlockMapCoordsBBox2DMaxRadius(cptrace.BBox, xl, yl, xh, yh);
    for (int bx = xl; bx <= xh; ++bx) {
      for (int by = yl; by <= yh; ++by) {
        for (auto &&it : XLevel->allBlockThings(bx, by)) {
          if (!CheckThing(cptrace, it.entity())) {
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
        for (auto &&it : XLevel->allBlockLines(bx, by)) {
          line_t *ld = it.line();
          //good &= CheckLine(cptrace, ld);
          if (!CheckLine(cptrace, ld)) {
            //good = false; // no early exit!
            return false; // ok to early exit
            //GCon->Logf("%s(%u): collided with line %d", GetClass()->GetName(), GetUniqueId(), (int)(ptrdiff_t)(ld-&XLevel->Lines[0]));
          }
        }
      }
    }

    //bool inpobj = false;
    // check if we can stand inside some polyobject
    // there is no need to check it if our position is already invalid
    if (/*good &&*/ XLevel->Has3DPolyObjects()) {
      // no need for new validcount
      const float z1 = cptrace.End.z+max2(0.0f, Height);
      for (int bx = xl; bx <= xh; ++bx) {
        for (int by = yl; by <= yh; ++by) {
          polyobj_t *po;
          for (VBlockPObjIterator It(XLevel, bx, by, &po); It.GetNext(); ) {
            if (po->pofloor.minz >= po->poceiling.maxz) continue;
            if (!Are2DBBoxesOverlap(po->bbox2d, cptrace.BBox)) continue;
            if (!XLevel->IsBBox2DTouchingSector(po->GetSector(), cptrace.BBox)) continue;
            if (!Copy3DPObjFloorCeiling(cptrace, po, cptrace.End.z, z1)) { return false; /*inpobj = true;*/ }
          }
        }
      }
    }

    //if (!good || inpobj) return false;
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

  if (fabsf(Other->Origin.x-cptrace.End.x) >= blockdist ||
      fabsf(Other->Origin.y-cptrace.End.y) >= blockdist)
  {
    // didn't hit it
    return true;
  }

  /*
  if ((EntityFlags&(EF_PassMobj|EF_Missile)) || (Other->EntityFlags&EF_ActLikeBridge)) {
    // prevent some objects from overlapping
    if (EntityFlags&Other->EntityFlags&EF_DontOverlap) return false;
    // check if a mobj passed over/under another object
    if (cptrace.End.z >= Other->Origin.z+GetBlockingHeightFor(Other)) return true;
    if (cptrace.End.z+Height <= Other->Origin.z) return true; // under thing
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
//  returns `false` if blocked
//
//==========================================================================
bool VEntity::CheckLine (tmtrace_t &cptrace, line_t *ld) {
  if (!ld->Box2DHit(cptrace.BBox)) return true; // no intersection

  // a line has been hit
  if (!ld->backsector) return false; // one sided line

  // check polyobject
  polyobj_t *po = ld->pobj();
  if (po) {
    if (po->validcount == validcount) return true; // already checked and stuck, ignore it
    if (!IsBlockingLine(ld)) return true;
    //TODO: normal polyobjects?
    if (!po->Is3D()) return true;
    if (po->pofloor.minz < po->poceiling.maxz) return true; // paper-thin or invalid polyobject
    const float z1 = cptrace.End.z+max2(0.0f, Height);
    if (!Copy3DPObjFloorCeiling(cptrace, po, cptrace.End.z, z1)) {
      po->validcount = validcount; // do not check if we are inside of it, because we definitely are
      return false; // stuck inside
    }
    // check top texture, if it blocking
    if (ld->flags&ML_CLIP_MIDTEX) {
      const TVec hitPoint = cptrace.End;
      const float hgt = max2(0.0f, Height);
      // check if we are above the pobj (at least partially)
      const float ptopz = po->poceiling.minz;
      if (hitPoint.z >= ptopz || hitPoint.z+hgt > ptopz) {
        // side doesn't matter, as it is guaranteed that both sides has the texture with the same height
        const side_t *tside = &XLevel->Sides[ld->sidenum[0]];
        VTexture *ttex = GTextureManager(tside->TopTexture);
        if (!ttex) ttex = GTextureManager[GTextureManager.DefaultTexture];
        const float texh = ttex->GetScaledHeightF()/tside->Top.ScaleY;
        if (texh > 0.0f) {
          if ((hitPoint.z >= ptopz && hitPoint.z < ptopz+texh) ||
              (hitPoint.z+hgt >= ptopz && hitPoint.z+hgt < ptopz+texh))
          {
            // blocked by top texture
            po->validcount = validcount; // do not check if we are inside of it, because we definitely are
            return false;
          }
        }
      }
    }

  }

  if (!IsBlockingLine(ld)) return true;
  /*
  if (!(ld->flags&ML_RAILING)) {
    if (ld->flags&ML_BLOCKEVERYTHING) return false; // explicitly blocking everything
    if ((EntityFlags&VEntity::EF_Missile) && (ld->flags&ML_BLOCKPROJECTILE)) return false; // blocks projectile
    if ((EntityFlags&VEntity::EF_CheckLineBlocking) && (ld->flags&ML_BLOCKING)) return false; // explicitly blocking everything
    if ((EntityFlags&VEntity::EF_CheckLineBlockMonsters) && (ld->flags&ML_BLOCKMONSTERS)) return false; // block monsters only
    if ((EntityFlags&VEntity::EF_IsPlayer) && (ld->flags&ML_BLOCKPLAYERS)) return false; // block players only
    if ((EntityFlags&VEntity::EF_Float) && (ld->flags&ML_BLOCK_FLOATERS)) return false; // block floaters only
  }
  */

  // set openrange, opentop, openbottom
  //TVec hit_point = cptrace.End-(DotProduct(cptrace.End, ld->normal)-ld->dist)*ld->normal;
  TVec hit_point = cptrace.End-(ld->PointDistance(cptrace.End)*ld->normal);
  opening_t *open = XLevel->LineOpenings(ld, hit_point, SPF_NOBLOCKING, !(EntityFlags&EF_Missile)/*do3dmidtex*/); // missiles ignores 3dmidtex
  open = VLevel::FindOpening(open, cptrace.End.z, cptrace.End.z+Height);

  if (open) {
    // adjust floor / ceiling heights
    /*
    if (!(open->eceiling.splane->flags&SPF_NOBLOCKING) && open->top < cptrace.CeilingZ) {
      cptrace.CopyOpenCeiling(open);
    }
    */
    if (!(open->eceiling.splane->flags&SPF_NOBLOCKING)) {
      bool replaceIt;
      if (open->eceiling.GetNormalZ() != -1.0f) {
        // slope
        replaceIt = (cptrace.CeilingZ-open->top > 0.1f);
      } else {
        replaceIt = (cptrace.CeilingZ > open->top || (open->top == cptrace.CeilingZ && cptrace.ECeiling.isSlope()));
      }
      if (replaceIt) {
        cptrace.CopyOpenCeiling(open);
      }
    }

    /*
    if (!(open->efloor.splane->flags&SPF_NOBLOCKING) && open->bottom > cptrace.FloorZ) {
      cptrace.CopyOpenFloor(open);
    }
    */
    if (!(open->efloor.splane->flags&SPF_NOBLOCKING)) {
      bool replaceIt;
      if (open->efloor.GetNormalZ() != 1.0f) {
        // slope
        replaceIt = (open->bottom-cptrace.FloorZ > 0.1f);
      } else {
        replaceIt = (open->bottom > cptrace.FloorZ || (open->bottom == cptrace.FloorZ && cptrace.EFloor.isSlope()));
      }
      if (replaceIt) {
        cptrace.CopyOpenFloor(open);
      }
    }

    if (open->lowfloor < cptrace.DropOffZ) cptrace.DropOffZ = open->lowfloor;

    if (ld->flags&ML_RAILING) cptrace.FloorZ += 32;
  } else {
    cptrace.CeilingZ = cptrace.FloorZ;
  }

  return true;
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
  const int contents = XLevel->PointContents(Sector, point);
  if (contents > 0) {
    WaterType = contents;
    WaterLevel = 1;
    point.z = Origin.z+Height*0.5f;
    if (XLevel->PointContents(Sector, point) > 0) {
      WaterLevel = 2;
      if (EntityFlags&EF_IsPlayer) {
        point = Player->ViewOrg;
        if (XLevel->PointContents(Sector, point) > 0) WaterLevel = 3;
      } else {
        point.z = Origin.z+Height*0.75f;
        if (XLevel->PointContents(Sector, point) > 0) WaterLevel = 3;
      }
    }
  }
  //return (WaterLevel > 1);
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
      for (auto &&it : XLevel->allBlockLines(bx, by)) {
        line_t *line = it.line();
        // ignore polyobject lines (for now)
        if (line->pobj()) continue;
        // ingore polyobject sectors (for now)
        if (!line->frontsector || line->frontsector->isOriginalPObj()) continue;
        if (!line->backsector || line->frontsector == line->backsector) continue; // ignore one-sided linedefs and selfrefs
        // linedef must be contacted
        if (line->Box2DHit(tmbbox)) {
          // new logic for 3D Floors
          /*
          sec_region_t *FrontReg = SV_FindThingGap(line->frontsector, Origin, Height);
          sec_region_t *BackReg = SV_FindThingGap(line->backsector, Origin, Height);
          float front = FrontReg->efloor.GetPointZClamped(Origin);
          float back = BackReg->efloor.GetPointZClamped(Origin);
          */
          TSecPlaneRef ffloor, fceiling;
          TSecPlaneRef bfloor, bceiling;
          XLevel->FindGapFloorCeiling(line->frontsector, Origin, Height, ffloor, fceiling);
          XLevel->FindGapFloorCeiling(line->backsector, Origin, Height, bfloor, bceiling);
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
int VEntity::FindDropOffLine (TArray<VDropOffLineInfo> *list, TVec /*pos*/) {
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
      for (auto &&it : XLevel->allBlockLines(bx, by)) {
        line_t *line = it.line();
        // ignore polyobject lines (for now)
        if (line->pobj()) continue;
        // ingore polyobject sectors (for now)
        if (!line->frontsector || line->frontsector->isOriginalPObj()) continue;
        if (!line->backsector || line->frontsector == line->backsector) continue; // ignore one-sided linedefs and selfrefs
        // linedef must be contacted
        if (line->Box2DHit(tmbbox)) {
          // new logic for 3D Floors
          /*
          sec_region_t *FrontReg = SV_FindThingGap(line->frontsector, Origin, Height);
          sec_region_t *BackReg = SV_FindThingGap(line->backsector, Origin, Height);
          float front = FrontReg->efloor.GetPointZClamped(Origin);
          float back = BackReg->efloor.GetPointZClamped(Origin);
          */
          TSecPlaneRef ffloor, fceiling;
          TSecPlaneRef bfloor, bceiling;
          XLevel->FindGapFloorCeiling(line->frontsector, Origin, Height, ffloor, fceiling);
          XLevel->FindGapFloorCeiling(line->backsector, Origin, Height, bfloor, bceiling);
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



//==========================================================================
//
//  Script natives
//
//==========================================================================
IMPLEMENT_FUNCTION(VEntity, CheckPosition) {
  TVec pos;
  vobjGetParamSelf(pos);
  RET_BOOL(Self->CheckPosition(pos));
}

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
