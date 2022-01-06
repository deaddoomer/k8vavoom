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


//**************************************************************************
//**
//**  Seg surfaces
//**
//**************************************************************************

//==========================================================================
//
//  GetMinFloorZWithFake
//
//  get max floor height for given floor and fake floor
//
//==========================================================================
// i haet shitpp templates!
#define GetFixedZWithFake(func_,plname_,v_,sec_,r_plane_)  \
  ((sec_) && (sec_)->heightsec ? func_((r_plane_).GetPointZ(v_), (sec_)->heightsec->plname_.GetPointZ(v_)) : (r_plane_).GetPointZ(v_))


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedSkyWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  sp->texinfo.Tex = GTextureManager[skyflatnum];
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColorMap = CM_Default;

  if (sp->texinfo.Tex->Type != TEXTYPE_Null && !seg->pobj) {
    TVec quad[4];

    const float topz1 = r_ceiling.GetPointZ(*seg->v1);
    const float topz2 = r_ceiling.GetPointZ(*seg->v2);

    quad[0].x = quad[1].x = seg->v1->x;
    quad[0].y = quad[1].y = seg->v1->y;
    quad[2].x = quad[3].x = seg->v2->x;
    quad[2].y = quad[3].y = seg->v2->y;

    quad[0].z = topz1;
    quad[1].z = quad[2].z = skyheight;
    quad[3].z = topz2;

    CreateWorldSurfFromWV(sub, seg, sp, quad, 0); // sky texture, no type flags
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
  sp->frontFakeFloorDist = 0.0f;
  sp->frontFakeCeilDist = 0.0f;
  sp->backFakeFloorDist = 0.0f;
  sp->backFakeCeilDist = 0.0f;
  sp->ResetFixTJunk();
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedTopWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedTopWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  if (seg->pobj && seg->pobj->Is3D()) return SetupTwoSidedTopWSurf3DPObj(sub, seg, sp, r_floor, r_ceiling);

  FreeWSurfs(sp->surfs);
  vassert(sub->sector);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  VTexture *TTex = GTextureManager(sidedef->TopTexture);
  if (!TTex) TTex = GTextureManager[GTextureManager.DefaultTexture];
  if (r_ceiling.splane->SkyBox != back_ceiling->SkyBox &&
      R_IsStrictlySkyFlatPlane(r_ceiling.splane) && R_IsStrictlySkyFlatPlane(back_ceiling))
  {
    TTex = GTextureManager[skyflatnum];
  }

  // HACK: sector with height of 1, and only middle masked texture is "transparent door"
  //       also, invert "upper unpegged" flag for this case
  unsigned hackflag = 0;

  if (!seg->pobj &&
      ((sub->sector->SectorFlags&sector_t::SF_IsTransDoorTop) || (r_hack_transparent_doors && TTex->Type == TEXTYPE_Null && IsTransDoorHackTop(seg))))
  {
    TTex = GTextureManager(sidedef->MidTexture);
    if (!TTex) TTex = GTextureManager[GTextureManager.DefaultTexture];
    hackflag = surface_t::TF_TOPHACK;
    // this doesn't check height, so we cannot set any flags here
    //sub->sector->SectorFlags |= (sector_t::SF_IsTransDoor|sector_t::SF_IsTransDoorTop);
    //GCon->Logf(NAME_Debug, "line #%d, side #%d (%d): tdtex='%s'", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), seg->side, *TTex->Name);
  }

  if (TTex->Type != TEXTYPE_Null && !seg->pobj) {
    TVec quad[4];

    // see `SetupTwoSidedBotWSurf()` for explanation
    const float topz1 = GetFixedZWithFake(max2, ceiling, *seg->v1, seg->frontsector, r_ceiling);
    const float topz2 = GetFixedZWithFake(max2, ceiling, *seg->v2, seg->frontsector, r_ceiling);
    const float botz1 = r_floor.GetPointZ(*seg->v1);
    const float botz2 = r_floor.GetPointZ(*seg->v2);

    float back_topz1 = back_ceiling->GetPointZ(*seg->v1);
    float back_topz2 = back_ceiling->GetPointZ(*seg->v2);

    {
      const sector_t *hsec = seg->backsector->heightsec;
      if (hsec && !hsec->IsUnderwater()) {
        //k8: why did i used `min2` here?
        back_topz1 = min2(back_topz1, hsec->ceiling.GetPointZ(*seg->v1));
        back_topz2 = min2(back_topz2, hsec->ceiling.GetPointZ(*seg->v2));
      }
    }

    // hack to allow height changes in outdoor areas
    float top_topz1 = topz1;
    float top_topz2 = topz2;
    float top_TexZ = r_ceiling.splane->TexZ;
    if (r_ceiling.splane->SkyBox == back_ceiling->SkyBox &&
        R_IsStrictlySkyFlatPlane(r_ceiling.splane) && R_IsStrictlySkyFlatPlane(back_ceiling))
    {
      top_topz1 = back_topz1;
      top_topz2 = back_topz2;
      top_TexZ = back_ceiling->TexZ;
    }

    /*
      The upper texture is pegged to the lowest ceiling. That is, the bottom of
      the upper texture will be at the lowest ceiling and drawn upwards.

      If the "upper unpegged" flag is set on the linedef, the upper texture
      will begin at the higher ceiling and be drawn downwards.
     */
    // Doom "up" is positive `z`
    // texture origin is left bottom corner (don't even ask me why)
    float zOrg; // texture bottom
    if (linedef->flags&ML_DONTPEGTOP) {
      // top of texture at the higher ceiling
      // it is ok to use seg frontsector ceiling here, because if it is
      // not higher than the back sector ceiling, the upper texture is invisible
      const float texh = TTex->GetScaledHeightF()/sidedef->Top.ScaleY;
      zOrg = top_TexZ-texh;
    } else {
      // bottom of texture at the lower ceiling
      zOrg = back_ceiling->TexZ;
    }
    SetupTextureAxesOffsetTop(seg, &sp->texinfo, TTex, &sidedef->Top, zOrg);

    // transparent/translucent door
    if (hackflag == surface_t::TF_TOPHACK) {
      sp->texinfo.Alpha = linedef->alpha;
      sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);
    }

    quad[0].x = quad[1].x = seg->v1->x;
    quad[0].y = quad[1].y = seg->v1->y;
    quad[2].x = quad[3].x = seg->v2->x;
    quad[2].y = quad[3].y = seg->v2->y;

    quad[0].z = min2(top_topz1, max2(back_topz1, botz1));  // was without outer min
    quad[1].z = top_topz1;
    quad[2].z = top_topz2;
    quad[3].z = min2(top_topz2, max2(back_topz2, botz2));  // was without outer min

    bool createSurf = true;

    //FIXME: this is totally wrong with slopes!
    if (hackflag != surface_t::TF_TOPHACK && seg->backsector->heightsec && !seg->backsector->heightsec->IsUnderwater()) {
      // do not create outer top texture surface if our fake ceiling is higher than the surrounding ceiling
      // otherwise, make sure that it is not lower than the fake ceiling (simc)
      const sector_t *bsec = seg->backsector;
      const sector_t *fsec = seg->frontsector;
      const sector_t *hsec = bsec->heightsec;
      if (hsec->ceiling.minz >= fsec->ceiling.minz) {
        //GCon->Logf(NAME_Debug, "BSH: %d (%d) -- SKIP", (int)(ptrdiff_t)(bsec-&Level->Sectors[0]), (int)(ptrdiff_t)(hsec-&Level->Sectors[0]));
        createSurf = false;
      } else if (hsec->ceiling.minz >= bsec->ceiling.minz) {
        //GCon->Logf(NAME_Debug, "BSH: %d (%d) -- FIX", (int)(ptrdiff_t)(bsec-&Level->Sectors[0]), (int)(ptrdiff_t)(hsec-&Level->Sectors[0]));
        quad[0].z = max2(quad[0].z, hsec->ceiling.GetPointZ(*seg->v1));
        quad[3].z = max2(quad[3].z, hsec->ceiling.GetPointZ(*seg->v2));
        //createSurf = false;
      }
    }

    if (createSurf) {
      //CreateWorldSurfFromWV(sub, seg, sp, quad, surface_t::TF_TOP|hackflag);
      CreateWorldSurfFromWVSplit(sub, seg, sp, quad, surface_t::TF_TOP|hackflag);
      if (sp->surfs) {
        sp->texinfo.Tex = TTex;
        sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
      }
    }
  } else {
    SetupTextureAxesOffsetDummy(&sp->texinfo, TTex);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->backBotDist = back_floor->dist;
  sp->TextureOffset = sidedef->Top.TextureOffset;
  sp->RowOffset = sidedef->Top.RowOffset;
  sp->ResetFixTJunk();
  SetupFakeDistances(seg, sp);
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedBotWSurf
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedBotWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  if (seg->pobj && seg->pobj->Is3D()) return SetupTwoSidedBotWSurf3DPObj(sub, seg, sp, r_floor, r_ceiling);
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  VTexture *BTex = GTextureManager(sidedef->BottomTexture);
  if (!BTex) BTex = GTextureManager[GTextureManager.DefaultTexture];

  // HACK: sector with height of 1, and only middle masked texture is "transparent door"
  //       also, invert "lower unpegged" flag for this case
  unsigned hackflag = 0;
  //if (r_hack_transparent_doors && BTex->Type == TEXTYPE_Null) GCon->Logf(NAME_Debug, "line #%d, side #%d: transdoor check=%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), IsTransDoorHackTop(seg));
  if (!seg->pobj &&
      ((sub->sector->SectorFlags&sector_t::SF_IsTransDoorBot) || (r_hack_transparent_doors && BTex->Type == TEXTYPE_Null && IsTransDoorHackBot(seg))))
  {
    BTex = GTextureManager(sidedef->MidTexture);
    if (!BTex) BTex = GTextureManager[GTextureManager.DefaultTexture];
    hackflag = surface_t::TF_TOPHACK;
    // this doesn't check height, so we cannot set any flags here
    //sub->sector->SectorFlags |= (sector_t::SF_IsTransDoor|sector_t::SF_IsTransDoorBot);
  }

  if (BTex->Type != TEXTYPE_Null && !seg->pobj) {
    TVec quad[4];

    //GCon->Logf(NAME_Debug, "2S-BOTTOM: line #%d, side #%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]));

    float topz1 = r_ceiling.GetPointZ(*seg->v1);
    float topz2 = r_ceiling.GetPointZ(*seg->v2);

    // some map authors are making floor decorations with height transfer
    // (that is so player won't wobble walking on such floors)
    // so we should use minimum front height here (sigh)
    //FIXME: moving height sector should trigger surface recreation
    float botz1 = r_floor.GetPointZ(*seg->v1);
    float botz2 = r_floor.GetPointZ(*seg->v2);
    {
      const sector_t *hsec = seg->frontsector->heightsec;
      if (hsec && !hsec->IsUnderwater()) {
        botz1 = min2(botz1, hsec->floor.GetPointZ(*seg->v1));
        botz2 = min2(botz2, hsec->floor.GetPointZ(*seg->v2));
      }
    }
    float top_TexZ = r_ceiling.splane->TexZ;

    float back_botz1 = back_floor->GetPointZ(*seg->v1);
    float back_botz2 = back_floor->GetPointZ(*seg->v2);

    // same height fix as above
    {
      const sector_t *hsec = seg->backsector->heightsec;
      if (hsec && !hsec->IsUnderwater()) {
        //const sector_t *fsec = seg->frontsector;
        //if (hsec->floor.minz > fsec->floor.minz) bfloor = &hsec->floor;
        //if (hsec->ceiling.minz < fsec->ceiling.minz) bceiling = &hsec->ceiling;
        //k8: why did i used `max2` here?
        back_botz1 = max2(back_botz1, hsec->floor.GetPointZ(*seg->v1));
        back_botz2 = max2(back_botz2, hsec->floor.GetPointZ(*seg->v2));
      }
    }

    /* k8: boomedit.wad -- i can't make heads or tails of this crap; when it should be rendered, and when it shouldn't? */
    /*     this is total hack to make boomedit.wad underwater look acceptable; but why it is done like this? */
    {
      const sector_t *fhsec = seg->frontsector->heightsec;
      const sector_t *bhsec = seg->backsector->heightsec;
      if (fhsec && bhsec && ((fhsec->SectorFlags|bhsec->SectorFlags)&(sector_t::SF_TransferSource|sector_t::SF_FakeBoomMask)) == sector_t::SF_TransferSource) {
        if (fhsec == bhsec || // same sector (boomedit.wad stairs)
            (fhsec->floor.dist == bhsec->floor.dist && fhsec->floor.normal == bhsec->floor.normal)) // or same floor height (boomedit.wad lift)
        {
          back_botz1 = seg->backsector->floor.GetPointZ(*seg->v1);
          back_botz2 = seg->backsector->floor.GetPointZ(*seg->v2);
        }
        else if (fhsec->floor.dist < bhsec->floor.dist) {
          // no, i don't know either, but this fixes sector 96 of boomedit.wad from the top
          botz1 = fhsec->floor.GetPointZ(*seg->v1);
          botz2 = fhsec->floor.GetPointZ(*seg->v2);
        }
        else if (fhsec->floor.dist > bhsec->floor.dist) {
          // no, i don't know either, but this fixes sector 96 of boomedit.wad from the bottom after the teleport
          back_botz1 = seg->backsector->floor.GetPointZ(*seg->v1);
          back_botz2 = seg->backsector->floor.GetPointZ(*seg->v2);
        }
      }
    }

    // hack to allow height changes in outdoor areas
    if (R_IsStrictlySkyFlatPlane(r_ceiling.splane) && R_IsStrictlySkyFlatPlane(back_ceiling)) {
      topz1 = back_ceiling->GetPointZ(*seg->v1);
      topz2 = back_ceiling->GetPointZ(*seg->v2);
      top_TexZ = back_ceiling->TexZ;
    }

    /*
      If a linedef is two sided, acting as a "bridge" between two sectors -
      such as a window or a step - the upper texture is pegged to the lowest
      ceiling, and the lower texture is pegged to the highest floor. That is,
      the bottom of the upper texture will be at the lowest ceiling and drawn
      upwards, and the top of the lower texture will be at the highest floor
      and drawn downwards. This is appropriate for textures that "belong" to
      the surface behind them, such as lifts or platforms.

      If the "upper unpegged" flag is set on the linedef, the upper texture
      will begin at the higher ceiling and be drawn downwards.

      If the "lower unpegged" flag is set on the linedef, the lower texture
      will be drawn as if it began at the higher ceiling and continued
      downwards. So if the higher ceiling is at 96 and the higher floor is at
      64, a lower unpegged texture will start 32 pixels from its top edge and
      draw downwards from the higher floor.

      Both forms of unpegging cause textures to be drawn the same way as if
      they were on a one-sided wall -- relative to the ceiling -- but with the
      middle section "cut out". Unpegging is thus appropriate for textures that
      "belong" to the surface in front of them, such as windows or recessed
      switches.
     */
    // Doom "up" is positive `z`
    // texture origin is left bottom corner (don't even ask me why)
    float zOrg; // texture bottom
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // the lower texture will be drawn as if it began at the higher ceiling and continued downwards
      zOrg = top_TexZ;
    } else {
      // the top of the lower texture will be at the highest floor and drawn downwards
      // it is ok to use seg backsector floor here, because if it is
      // not lower than the front sector floor, the lower texture is invisible
      zOrg = back_floor->TexZ;
    }
    const float texh = BTex->GetScaledHeightF()/sidedef->Bot.ScaleY;
    zOrg -= texh; // convert to texture bottom
    SetupTextureAxesOffsetBot(seg, &sp->texinfo, BTex, &sidedef->Bot, zOrg);

    // transparent/translucent door
    if (hackflag == surface_t::TF_TOPHACK) {
      sp->texinfo.Alpha = linedef->alpha;
      sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);
    }

    quad[0].x = quad[1].x = seg->v1->x;
    quad[0].y = quad[1].y = seg->v1->y;
    quad[2].x = quad[3].x = seg->v2->x;
    quad[2].y = quad[3].y = seg->v2->y;

    quad[0].z = botz1;
    quad[1].z = max2(botz1, min2(back_botz1, topz1)); // was without outer max
    quad[2].z = max2(botz2, min2(back_botz2, topz2)); // was without outer max
    quad[3].z = botz2;

    bool createSurf = true;

    //FIXME: this is totally wrong with slopes!
    if (hackflag != surface_t::TF_TOPHACK && seg->backsector->heightsec && !seg->backsector->heightsec->IsUnderwater()) {
      // do not create outer bottom texture surface if our fake floor is lower than the surrounding floor
      // otherwise, make sure that it is not higher than the fake floor (simc)
      const sector_t *bsec = seg->backsector;
      const sector_t *fsec = seg->frontsector;
      const sector_t *hsec = bsec->heightsec;
      if (hsec->floor.minz <= fsec->floor.minz) {
        //GCon->Logf(NAME_Debug, "BSH: %d (%d) -- SKIP", (int)(ptrdiff_t)(bsec-&Level->Sectors[0]), (int)(ptrdiff_t)(hsec-&Level->Sectors[0]));
        createSurf = false;
      } else if (hsec->floor.minz <= bsec->floor.minz) {
        //GCon->Logf(NAME_Debug, "BSH: %d (%d) -- FIX", (int)(ptrdiff_t)(bsec-&Level->Sectors[0]), (int)(ptrdiff_t)(hsec-&Level->Sectors[0]));
        quad[1].z = min2(quad[1].z, hsec->floor.GetPointZ(*seg->v1));
        quad[2].z = min2(quad[2].z, hsec->floor.GetPointZ(*seg->v2));
        //createSurf = false;
      }
    }

    if (createSurf) {
      //CreateWorldSurfFromWV(sub, seg, sp, quad, surface_t::TF_BOTTOM|hackflag);
      CreateWorldSurfFromWVSplit(sub, seg, sp, quad, surface_t::TF_BOTTOM|hackflag);
      if (sp->surfs) {
        sp->texinfo.Tex = BTex;
        sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
      }
    }
  } else {
    SetupTextureAxesOffsetDummy(&sp->texinfo, BTex);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backBotDist = back_floor->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->TextureOffset = sidedef->Bot.TextureOffset;
  sp->RowOffset = sidedef->Bot.RowOffset;
  sp->ResetFixTJunk();
  SetupFakeDistances(seg, sp);
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidWSurf
//
//  create normal midtexture surface
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  if (seg->pobj && seg->pobj->Is3D()) return SetupTwoSidedMidWSurf3DPObj(sub, seg, sp, r_floor, r_ceiling);

  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

  const sec_plane_t *back_floor = &seg->backsector->floor;
  const sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  if (MTex->Type != TEXTYPE_Null && !(sub->sector->SectorFlags&sector_t::SF_IsTransDoor)) {
    TVec quad[4];

    //GCon->Logf(NAME_Debug, "line #%d, sidenum #%d: midtex=%s (side: %d); transparent=%d; translucent=%d; seethrough=%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), *MTex->Name, seg->side, (int)MTex->isTransparent(), (int)MTex->isTranslucent(), (int)MTex->isSeeThrough());

    #if 0
    if ((int)(ptrdiff_t)(linedef-&Level->Lines[0]) == 4834) {
      GCon->Logf(NAME_Debug, "2S-TOP: line #%d, side #%d; RowOffset=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), sidedef->Mid.RowOffset);
      //sidedef->Mid.RowOffset = 0.0f;
      //sidedef->Mid.ScaleX = 1.0f;
      //sidedef->Mid.ScaleY = 1.0f;
    }
    #endif

    const sec_plane_t *bfloor = back_floor;
    const sec_plane_t *bceiling = back_ceiling;

    if (seg->backsector->heightsec && !seg->backsector->heightsec->IsUnderwater()) {
      const sector_t *bsec = seg->backsector;
      const sector_t *fsec = seg->frontsector;
      const sector_t *hsec = bsec->heightsec;
      if (hsec->floor.minz > fsec->floor.minz) bfloor = &hsec->floor;
      if (hsec->ceiling.minz < fsec->ceiling.minz) bceiling = &hsec->ceiling;
    }

    const float back_topz1 = bceiling->GetPointZ(*seg->v1);
    const float back_topz2 = bceiling->GetPointZ(*seg->v2);
    const float back_botz1 = bfloor->GetPointZ(*seg->v1);
    const float back_botz2 = bfloor->GetPointZ(*seg->v2);

    //GCon->Logf(NAME_Debug, "line #%d, sidenum #%d: midtex=%s (side: %d); back_botz=(%g : %g); back_topz=(%g : %g), exbotz=%g; extopz=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), *MTex->Name, seg->side, back_botz1, back_botz2, back_topz1, back_topz2, exbotz, extopz);

    //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d: seg for line #%d (sector #%d); TexZ=%g:%g (notpeg=%d)", seg->pobj->tag, (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]), seg->pobj->pofloor.TexZ, seg->pobj->poceiling.TexZ, !!(linedef->flags&ML_DONTPEGBOTTOM));

    /*
      If a linedef is one sided (a solid wall), the texture is "pegged" to the
      top of the wall. That is to say, the top of the texture is at the
      ceiling. The texture continues downward to the floor.

      If the "lower unpegged" flag is set on the linedef, the texture is
      instead "pegged" to the bottom of the wall. That is, the bottom of the
      texture is located at the floor. The texture is drawn upwards from here.
      This is commonly used in the jambs in doors; because Doom's engine treats
      each door as a "rising ceiling", a doorjamb pegged to the top of the wall
      would "rise up" with the door.

      The alignment of the texture can be adjusted using the sidedef X and Y
      alignment controls. This is applied after the logic controlling pegging.
     */

    // Doom "up" is positive `z`
    // texture origin is left bottom corner (don't even ask me why)

    vassert(!seg->pobj || !seg->pobj->Is3D());
    const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;
    //GCon->Logf(NAME_Debug, "line #%d: side=%d; %s: texh=%g; backh=%g; fronth=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), seg->side, *MTex->Name, texh, back_topz1-back_botz1, seg->frontsector->ceiling.GetPointZ(*seg->v1)-seg->frontsector->floor.GetPointZ(*seg->v1));

    float zOrg; // texture bottom
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      zOrg = max2(r_floor.splane->TexZ, seg->backsector->floor.TexZ);
    } else {
      // top of texture at top
      zOrg = min2(r_ceiling.splane->TexZ, seg->backsector->ceiling.TexZ)-texh;
      /*
      GCon->Logf(NAME_Debug, "line #%d: side=%d; %s: texh=%g; backh=%g; fronth=%g; zOrg=%g (%g); backcz=%g (%g:%g), frontcz=%g (%g:%g) (%g:%g)", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), seg->side, *MTex->Name, texh, back_topz1-back_botz1, seg->frontsector->ceiling.GetPointZ(*seg->v1)-seg->frontsector->floor.GetPointZ(*seg->v1), zOrg, zOrg+texh,
        seg->backsector->ceiling.TexZ, back_topz1, back_topz2, r_ceiling.splane->TexZ, seg->frontsector->ceiling.GetPointZ(*seg->v1), seg->frontsector->ceiling.GetPointZ(*seg->v2),
        r_ceiling.splane->GetPointZ(*seg->v1), r_ceiling.splane->GetPointZ(*seg->v2));
      */
    }
    SetupTextureAxesOffsetMid2S(seg, &sp->texinfo, MTex, &sidedef->Mid, zOrg);

    sp->texinfo.Alpha = linedef->alpha;
    sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

    const sector_t *fsec = seg->frontsector;

    do {
      quad[0].x = quad[1].x = seg->v1->x;
      quad[0].y = quad[1].y = seg->v1->y;
      quad[2].x = quad[3].x = seg->v2->x;
      quad[2].y = quad[3].y = seg->v2->y;

      const float topz1 = min2(back_topz1, fsec->ceiling.GetPointZ(*seg->v1));
      const float topz2 = min2(back_topz2, fsec->ceiling.GetPointZ(*seg->v2));
      // those `min2()` fixes some slopes
      const float botz1 = min2(topz1, max2(back_botz1, fsec->floor.GetPointZ(*seg->v1)));
      const float botz2 = min2(topz2, max2(back_botz2, fsec->floor.GetPointZ(*seg->v2)));

      float midtopz1 = topz1;
      float midtopz2 = topz2;
      float midbotz1 = botz1;
      float midbotz2 = botz2;

      // cover top part if it has no texture
      if (!GTextureManager.IsEmptyTexture(sidedef->TopTexture)) {
        midtopz1 = min2(midtopz1, fsec->ceiling.GetPointZ(*seg->v1));
        midtopz2 = min2(midtopz2, fsec->ceiling.GetPointZ(*seg->v2));
      }

      // cover bottom part if it has no texture
      if (!GTextureManager.IsEmptyTexture(sidedef->BottomTexture)) {
        midbotz1 = max2(midbotz1, fsec->floor.GetPointZ(*seg->v1));
        midbotz2 = max2(midbotz2, fsec->floor.GetPointZ(*seg->v2));
      }

      if (midbotz1 >= midtopz1 || midbotz2 >= midtopz2) break;

      //GCon->Logf(NAME_Debug, "line #%d: hgts: (%g,%g) (%g,%g) (%g,%g) texh=%g (ts=%g; ots=%g; ys=%g)", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), midbotz1, midtopz1, midtopz2, midbotz2, zOrg, zOrg+texh, texh, TextureTScale(MTex), TextureOffsetTScale(MTex), sidedef->Mid.ScaleY);

      float hgts[4];

      // linedef->flags&ML_CLIP_MIDTEX, sidedef->Flags&SDF_CLIPMIDTEX
      // this clips texture to a floor, otherwise it goes beyound it
      // it seems that all modern OpenGL renderers just ignores clip flag, and
      // renders all midtextures as always clipped.

      // calculate z coordinates of the quad
      if ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX)) {
        // if the texture is wrapped, the quad occupied the whole empty space
        hgts[0] = midbotz1;
        hgts[1] = midtopz1;
        hgts[2] = midtopz2;
        hgts[3] = midbotz2;
      } else {
        // the texture is not wrapped, so top of the texture should start at the top
        //GCon->Logf(NAME_Debug, "line #%d: side=%d; %s: texh=%g; backh=%g; fronth=%g; midzh=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), seg->side, *MTex->Name, texh, back_topz1-back_botz1, topz1-botz1, midtopz1-midbotz1);
        const float tz0 = zOrg;
        const float tz1 = zOrg+texh;
        if (tz0 >= max2(midtopz1, midtopz2)) break;
        if (tz1 <= max2(midbotz1, midbotz2)) break;
        hgts[0] = max2(midbotz1, tz0);
        hgts[1] = min2(midtopz1, tz1);
        hgts[2] = min2(midtopz2, tz1);
        hgts[3] = max2(midbotz2, tz0);
      }

      // just in case
      quad[QUAD_V1_BOTTOM].z = min2(hgts[1], hgts[0]);
      quad[QUAD_V1_TOP].z = hgts[1];
      quad[QUAD_V2_TOP].z = hgts[2];
      quad[QUAD_V2_BOTTOM].z = min2(hgts[2], hgts[3]);

      //CreateWorldSurfFromWV(sub, seg, sp, quad, surface_t::TF_MIDDLE);
      CreateWorldSurfFromWVSplit(sub, seg, sp, quad, surface_t::TF_MIDDLE);
    } while (0);
  } else {
    // empty midtexture
    SetupTextureAxesOffsetDummy(&sp->texinfo, MTex);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = back_ceiling->dist;
  sp->backBotDist = back_floor->dist;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
  sp->ResetFixTJunk();
  SetupFakeDistances(seg, sp);
}
