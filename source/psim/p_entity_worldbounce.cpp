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
//**
//**  VEntity collision, physics and related methods.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "p_entity.h"
#include "p_world.h"

//#define VV_DEBUG_BOUNCE


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
  #ifdef VV_DEBUG_BOUNCE
  GCon->Logf(NAME_Debug, "=======: BOUNCE %s:%u: START; line=%d; overbounce=%f; bouncefactor=%f", GetClass()->GetName(), GetUniqueId(), (blockline ? (intptr_t)(blockline-&XLevel->Lines[0]) : -666), overbounce, bouncefactor);
  #endif

  if (!BestSlideLine || BestSlideLine->PointOnSide(Origin)) {
    BestSlideLine = nullptr;
    TVec SlideOrg = Origin;
    const float rad = max2(1.0f, GetMoveRadius());
    SlideOrg.x += (Velocity.x > 0.0f ? rad : -rad);
    SlideOrg.y += (Velocity.y > 0.0f ? rad : -rad);
    TVec SlideDir = Velocity*DeltaTime;
    intercept_t in;
    float BestSlideFrac = 99999.0f;

    //FIXME: use code from `SlidePathTraverse()` here!
    #ifdef VV_DEBUG_BOUNCE
    GCon->Logf(NAME_Debug, "%s:%u:xxx: vel=(%g,%g,%g); dt=%g; slide=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f, DeltaTime, SlideDir.x, SlideDir.y, SlideDir.z);
    #endif
    //for (VPathTraverse It(this, &in, SlideOrg.x, SlideOrg.y, SlideOrg.x+SlideDir.x, SlideOrg.y+SlideDir.y, PT_ADDLINES); It.GetNext(); )
    for (VPathTraverse It(this, &in, SlideOrg, SlideOrg+TVec(SlideDir.x, SlideDir.y, 0.0f), PT_ADDLINES|PT_NOOPENS|PT_RAILINGS); It.GetNext(); )
    {
      if (!(in.Flags&intercept_t::IF_IsALine)) { continue; /*Host_Error("PTR_BounceTraverse: not a line?");*/ }
      line_t *li = in.line;
      //if (in.frac > 1.0f) continue;

      if (!(li->flags&ML_BLOCKEVERYTHING)) {
        if (li->flags&ML_TWOSIDED) {
          // set openrange, opentop, openbottom
          TVec hpoint = SlideOrg+in.frac*SlideDir;
          opening_t *open = XLevel->LineOpenings(li, hpoint, SPF_NOBLOCKING, true/*do3dmidtex*/); //!(EntityFlags&EF_Missile)); // missiles ignores 3dmidtex
          open = VLevel::FindOpening(open, Origin.z, Origin.z+Height);

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

      if (!BestSlideLine || BestSlideFrac < in.frac) {
        BestSlideFrac = in.frac;
        BestSlideLine = li;
      }
      //break;
    }
    if (!BestSlideLine) {
      #ifdef VV_DEBUG_BOUNCE
      GCon->Logf(NAME_Debug, "%s:%u:999: cannot find slide line! vel=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f);
      #endif
      BestSlideLine = blockline;
    }
  }

  #ifdef VV_DEBUG_BOUNCE
  GCon->Logf(NAME_Debug, "=======: BOUNCE %s:%u: START; best-line=%d; overbounce=%f; bouncefactor=%f", GetClass()->GetName(), GetUniqueId(), (BestSlideLine ? (intptr_t)(BestSlideLine-&XLevel->Lines[0]) : -666), overbounce, bouncefactor);
  #endif

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

    float lineangle = VectorAngleYaw(BestSlideLine->ndir);
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
    GCon->Logf(NAME_Debug, "%s:%u:BOXSIDE: %d : %d", GetClass()->GetName(), GetUniqueId(), BestSlideLine->checkBox3DEx(bbox3d), BoxOnLineSide2D(bbox2d, *BestSlideLine->v1, *BestSlideLine->v2));
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

    #ifdef VV_DEBUG_BOUNCE
    GCon->Logf(NAME_Debug, "%s:%u:000: linedef=%d; lineangle=%g; moveangle=%g; deltaangle=%g; movelen=%g; vel.xy=(%g,%g,%g); side=%d", GetClass()->GetName(), GetUniqueId(), (int)(ptrdiff_t)(BestSlideLine-&XLevel->Lines[0]), lineangle, moveangle, deltaangle, movelen, Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f, BestSlideLine->PointOnSide(Origin));
    #endif

    if (movelen < 35.0f) movelen = 70.0f;
    const TVec newvel = AngleVectorYaw(deltaangle)*movelen;
    Velocity.x = newvel.x;
    Velocity.y = newvel.y;

    #ifdef VV_DEBUG_BOUNCE
    GCon->Logf(NAME_Debug, "%s:%u:001: oldangle=%g; newangle=%g; newvel.xy=(%g,%g,%g)", GetClass()->GetName(), GetUniqueId(), moveangle, deltaangle, Velocity.x/35.0f, Velocity.y/35.0f, Velocity.z/35.0f);
    #endif

    #if 0
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



//==========================================================================
//
//  Script natives
//
//==========================================================================
//native final void BounceWall (float DeltaTime, const line_t *blockline, float overbounce, float bouncefactor);
IMPLEMENT_FUNCTION(VEntity, BounceWall) {
  line_t *blkline;
  float deltaTime, overbounce, bouncefactor;
  vobjGetParamSelf(deltaTime, blkline, overbounce, bouncefactor);
  Self->BounceWall(deltaTime, blkline, overbounce, bouncefactor);
}
