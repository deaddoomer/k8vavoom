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

//#define VV_DEBUG_MIDEXTRA


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_3dfloor_clip_both_sides("r_3dfloor_clip_both_sides", false, "Clip 3d floors with both sectors?", CVAR_Archive|CVAR_NoShadow);


//**************************************************************************
//**
//**  Seg surfaces -- 3d floors (extra surfaces)
//**
//**************************************************************************


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidExtraWSurf
//
//  create 3d-floors midtexture (side) surfaces
//  this creates surfaces not in 3d-floor sector, but in neighbouring one
//
//  do not create side surfaces if they aren't in openings
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidExtraWSurf (sec_region_t *reg, subsector_t *sub, seg_t *seg, segpart_t *sp,
                                                     TSecPlaneRef r_floor, TSecPlaneRef r_ceiling)
{
  FreeWSurfs(sp->surfs);

  const line_t *linedef = reg->extraline;
  /*const*/ side_t *sidedef = &Level->Sides[linedef->sidenum[0]];
  /*const*/ side_t *segsidedef = seg->sidedef;
  //const side_t *texsideparm = (segsidedef ? segsidedef : sidedef);

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

  /*
  if (sidedef->Mid.ScaleY != 1) GCon->Logf(NAME_Debug, "extra: line #%d (%d), side #%d: midscale=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), sidedef->Mid.ScaleY);
  if (segsidedef->Mid.ScaleY != 1) GCon->Logf(NAME_Debug, "seg: line #%d (%d), side #%d: midscale=%g", (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(segsidedef-&Level->Sides[0]), segsidedef->Mid.ScaleY);
  if ((int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]) == 49) {
    //sidedef->Mid.ScaleY = segsidedef->Mid.ScaleY = 1;
    segsidedef->Mid.ScaleY = 1;
    //segsidedef->Mid.RowOffset = 0;
    //sidedef->Mid.RowOffset = 0;
  }
  */
  /*
   1. solid 3d floors should be cut only by other solids (including other sector)
   2. swimmable (water) 3d floors should be cut by all solids (including other sector), and
      by all swimmable (including other sector)
   */

  #ifdef VV_DEBUG_MIDEXTRA
  //bool doDump = false;
  enum { doDump = 0 };
  //bool doDump = (sidedef->MidTexture.id == 1713);
  //const bool doDump = (seg->linedef-&Level->Lines[0] == 33);
  //const bool doDump = true;

  /*
  if (seg->frontsector-&Level->Sectors[0] == 70) {
    doDump = true;
    GCon->Logf("::: SECTOR #70 (back #%d) EF: texture='%s'", (seg->backsector ? (int)(ptrdiff_t)(seg->backsector-&Level->Sectors[0]) : -1), *MTex->Name);
    GCon->Log(" === front regions ===");
    VLevel::dumpSectorRegions(seg->frontsector);
  }

  if (seg->backsector && seg->backsector-&Level->Sectors[0] == 70) {
    doDump = true;
    GCon->Logf("::: BACK-SECTOR #70 (front #%d) EF: texture='%s'", (seg->frontsector ? (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]) : -1), *MTex->Name);
    GCon->Log(" === front regions ===");
    VLevel::dumpSectorRegions(seg->frontsector);
  }
  */

  if (doDump) GCon->Logf(NAME_Debug, "*** line #%d (extra line #%d); seg side #%d ***", (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(linedef-&Level->Lines[0]), seg->side);
  if (doDump) {
    GCon->Log(NAME_Debug, "=== REGIONS:FRONT ===");
    VLevel::dumpSectorRegions(seg->frontsector);
    GCon->Log(NAME_Debug, "=== REGIONS:BACK ===");
    VLevel::dumpSectorRegions(seg->backsector);
  }
  #endif

  sp->texinfo.Alpha = (reg->efloor.splane->Alpha < 1.0f ? reg->efloor.splane->Alpha : 1.1f);
  sp->texinfo.Additive = !!(reg->efloor.splane->flags&SPF_ADDITIVE);

  // hack: 3d floor with sky texture seems to be transparent in renderer
  // "only visual" regions has no walls, they are used to render floors and ceilings when the camera is inside a region
  if (MTex->Type != TEXTYPE_Null && sidedef->MidTexture.id != skyflatnum && (reg->regflags&sec_region_t::RF_OnlyVisual) == 0) {
    TVec quad[4];

    const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;
    float zOrg; // texture bottom
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      zOrg = reg->efloor.splane->TexZ; //+(MTex->GetScaledHeightF()*sidedef->Mid.ScaleY);
    } else if (linedef->flags&ML_DONTPEGTOP) {
      // top of texture at top of top region (???)
      zOrg = seg->frontsub->sector->ceiling.TexZ-texh;
      //zOrg = reg->eceiling.splane->TexZ;
    } else {
      // top of texture at top
      zOrg = reg->eceiling.splane->TexZ-texh;
    }
    SetupTextureAxesOffsetMidES(seg, &sp->texinfo, MTex, &sidedef->Mid, zOrg, &segsidedef->Mid);

    const bool isSolid = !(reg->regflags&sec_region_t::RF_NonSolid);

    do {
      // if non-solid region is inside a sector (i.e. both subsectors belong to the same sector), don't create a wall
      if (!isSolid && seg->frontsector && seg->backsector && seg->frontsector == seg->backsector) {
        break;
      }

      quad[0].x = quad[1].x = seg->v1->x;
      quad[0].y = quad[1].y = seg->v1->y;
      quad[2].x = quad[3].x = seg->v2->x;
      quad[2].y = quad[3].y = seg->v2->y;

      const float topz1 = reg->eceiling.GetPointZ(*seg->v1);
      const float topz2 = reg->eceiling.GetPointZ(*seg->v2);

      // those `min2()` fixes some slopes
      const float botz1 = min2(topz1, reg->efloor.GetPointZ(*seg->v1));
      const float botz2 = min2(topz2, reg->efloor.GetPointZ(*seg->v2));

      //const bool wrapped = !!((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
      // side 3d floor midtex should always be wrapped
      /*
      // 3d midtextures are always wrapped
      // check texture limits
      if (!wrapped) {
        if (max2(topz1, topz2) <= zOrg-texhsc) continue;
        if (min2(botz1, botz2) >= zOrg) continue;
      }
      */

      if (botz1 <= topz1 && botz2 <= topz2 /*&&
          ((!r_floor.PointOnSide(TVec(seg->v1->x, seg->v1->y, topz1)) || !r_floor.PointOnSide(TVec(seg->v2->x, seg->v2->y, topz2))) ||
           (!r_ceiling.PointOnSide(TVec(seg->v1->x, seg->v1->y, botz1)) || !r_ceiling.PointOnSide(TVec(seg->v2->x, seg->v2->y, botz2))))*/)
      {
        quad[0].z = botz1;
        quad[1].z = topz1;
        quad[2].z = topz2;
        quad[3].z = botz2;

        if (isValidQuad(quad)) {
          // clip with passed floor and ceiling (just in case)
          const bool valid0 = SplitQuadWithPlane(quad, r_ceiling, quad, nullptr); // leave bottom part
          const bool valid1 = (valid0 && SplitQuadWithPlane(quad, r_floor, nullptr, quad)); // leave top part

          if (valid0 && valid1 && isValidQuad(quad)) {
            const unsigned typeFlags = surface_t::TF_MIDDLE;

            sec_region_t *backregs = (sub == seg->frontsub ? (seg->backsector ? seg->backsector->eregions : nullptr) : (seg->frontsector ? seg->frontsector->eregions : nullptr));
            if (backregs == sub->sector->eregions) backregs = nullptr; // just in case

            CreateWorldSurfFromWVSplitFromReg(sub->sector->eregions, backregs, sub, seg, sp, quad, typeFlags, isSolid/*onlySolid*/, reg/*ignorereg*/);
            #if 0
            if (isSolid) {
              // solid region; clip only with the following regions
              CreateWorldSurfFromWVSplitFromReg(reg->next, backregs, sub, seg, sp, quad, typeFlags, true/*onlySolid*/, reg/*ignorereg*/);
            } else {
              // non-solid region: clip with all subsector regions
              CreateWorldSurfFromWVSplitFromReg(sub->sector->eregions, backregs, sub, seg, sp, quad, typeFlags, false/*onlySolid*/, reg/*ignorereg*/);
            }
            #endif
          }
        }
      }
    } while (0);

    if (sp->surfs && (sp->texinfo.Additive || sp->texinfo.Alpha < 1.0f || MTex->isTranslucent())) {
      for (surface_t *sf = sp->surfs; sf; sf = sf->next) sf->drawflags |= surface_t::DF_NO_FACE_CULL;
    }
  } else {
    vassert(!sp->surfs);
    SetupTextureAxesOffsetDummy(&sp->texinfo, MTex, false);
  }

  if (!sp->surfs) {
    if (sidedef->MidTexture.id != skyflatnum) {
      sp->texinfo.Alpha = 1.1f;
      sp->texinfo.Additive = false;
    } else {
      sp->texinfo.Alpha = 0.0f;
      sp->texinfo.Additive = true;
    }
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = reg->eceiling.splane->dist;
  sp->backBotDist = reg->efloor.splane->dist;
  /*
  sp->TextureOffset = texsideparm->Mid.TextureOffset;
  sp->RowOffset = texsideparm->Mid.RowOffset;
  */
  sp->TextureOffset = sidedef->Mid.TextureOffset+segsidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset+segsidedef->Mid.RowOffset;
  sp->ResetFixTJunk();
  SetupFakeDistances(seg, sp);
}
