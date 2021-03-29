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
#include "r_surf_utils.cpp"

//#define VV_DEBUG_MIDEXTRA


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_3dfloor_clip_both_sides("r_3dfloor_clip_both_sides", false, "Clip 3d floors with both sectors?", CVAR_Archive);


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

  // it seems that `segsidedef` offset is in effect, but scaling is not
  #ifdef VV_SURFCTOR_3D_USE_SEGSIDEDEF_SCALE
  const float scale2Y = segsidedef->Mid.ScaleY;
  #else
  // it seems that `segsidedef` offset is in effect, but scaling is not
  const float scale2Y = 1.0f;
  #endif

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

  // apply offsets from seg side
  SetupTextureAxesOffsetEx(seg, &sp->texinfo, MTex, &sidedef->Mid, &segsidedef->Mid);

  //const float texh = DivByScale(MTex->GetScaledHeight(), texsideparm->Mid.ScaleY);
  const float texh = DivByScale2(MTex->GetScaledHeight(), sidedef->Mid.ScaleY, scale2Y);
  //const float texhsc = MTex->GetHeight();
  float z_org; // texture top

  // (reg->regflags&sec_region_t::RF_SaneRegion) // vavoom 3d floor
  if (linedef->flags&ML_DONTPEGBOTTOM) {
    // bottom of texture at bottom
    z_org = reg->efloor.splane->TexZ+texh;
  } else if (linedef->flags&ML_DONTPEGTOP) {
    // top of texture at top of top region (???)
    z_org = seg->frontsub->sector->ceiling.TexZ;
    //z_org = reg->eceiling.splane->TexZ;
  } else {
    // top of texture at top
    z_org = reg->eceiling.splane->TexZ;
  }

  // apply offsets from both sides
  FixMidTextureOffsetAndOriginEx(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid, &segsidedef->Mid);
  //FixMidTextureOffsetAndOrigin(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid);

  sp->texinfo.Tex = MTex;
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;

  sp->texinfo.Alpha = (reg->efloor.splane->Alpha < 1.0f ? reg->efloor.splane->Alpha : 1.1f);
  sp->texinfo.Additive = !!(reg->efloor.splane->flags&SPF_ADDITIVE);

  // hack: 3d floor with sky texture seems to be transparent in renderer
  if (MTex->Type != TEXTYPE_Null && sidedef->MidTexture.id != skyflatnum) {
    TVec quad[4];

    quad[0].x = quad[1].x = seg->v1->x;
    quad[0].y = quad[1].y = seg->v1->y;
    quad[2].x = quad[3].x = seg->v2->x;
    quad[2].y = quad[3].y = seg->v2->y;

    const float topz1 = reg->eceiling.GetPointZ(*seg->v1);
    const float topz2 = reg->eceiling.GetPointZ(*seg->v2);

    const float botz1 = reg->efloor.GetPointZ(*seg->v1);
    const float botz2 = reg->efloor.GetPointZ(*seg->v2);

    //const bool wrapped = !!((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
    // side 3d floor midtex should always be wrapped
    /*
    // 3d midtextures are always wrapped
    // check texture limits
    if (!wrapped) {
      if (max2(topz1, topz2) <= z_org-texhsc) continue;
      if (min2(botz1, botz2) >= z_org) continue;
    }
    */

    if (botz1 < topz1 && botz2 < topz2 &&
        ((!r_floor.PointOnSide(TVec(seg->v1->x, seg->v1->y, topz1)) || !r_floor.PointOnSide(TVec(seg->v2->x, seg->v2->y, topz2))) ||
         (!r_ceiling.PointOnSide(TVec(seg->v1->x, seg->v1->y, botz1)) || !r_ceiling.PointOnSide(TVec(seg->v2->x, seg->v2->y, botz2)))))
    {
      quad[0].z = botz1;
      quad[1].z = topz1;
      quad[2].z = topz2;
      quad[3].z = botz2;

      // clip with floor and ceiling
      SplitQuadWithPlane(quad, r_ceiling, quad, nullptr); // leave bottom part
      SplitQuadWithPlane(quad, r_floor, nullptr, quad); // leave top part

      if (isValidNormalQuad(quad)) {
        // clip with other 3d floors
        // clip only with the following regions, to avoid holes
        // if we have a back sector, split with it too
        bool found = false;
        for (sec_region_t *sr = seg->frontsector->eregions->next; sr; sr = sr->next) if (sr == reg) { found = true; break; }
        sector_t *bsec = (found ? seg->backsector : seg->frontsector);
        if (seg->frontsector == seg->backsector) bsec = nullptr;
        //bsec = nullptr;
        CreateWorldSurfFromWVSplitFromReg(reg->next, bsec, sub, seg, sp, quad, surface_t::TF_MIDDLE);
      }
    }

    if (sp->surfs && (sp->texinfo.Alpha < 1.0f || sp->texinfo.Additive || MTex->isTranslucent())) {
      for (surface_t *sf = sp->surfs; sf; sf = sf->next) sf->drawflags |= surface_t::DF_NO_FACE_CULL;
    }
  } else {
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
