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


//**************************************************************************
//**
//**  Seg surfaces
//**
//**************************************************************************

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
  sp->texinfo.ColorMap = 0;

  if (sp->texinfo.Tex->Type != TEXTYPE_Null) {
    TVec wv[4];

    const float topz1 = r_ceiling.GetPointZ(*seg->v1);
    const float topz2 = r_ceiling.GetPointZ(*seg->v2);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = topz1;
    wv[1].z = wv[2].z = skyheight;
    wv[3].z = topz2;

    CreateWorldSurfFromWV(sub, seg, sp, wv, 0); // sky texture, no type flags
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
  int peghack = 0;
  unsigned hackflag = 0;

  if (!seg->pobj &&
      ((sub->sector->SectorFlags&sector_t::SF_IsTransDoorTop) || (r_hack_transparent_doors && TTex->Type == TEXTYPE_Null && IsTransDoorHackTop(seg))))
  {
    TTex = GTextureManager(sidedef->MidTexture);
    if (!TTex) TTex = GTextureManager[GTextureManager.DefaultTexture];
    //peghack = ML_DONTPEGTOP;
    hackflag = surface_t::TF_TOPHACK;
    // this doesn't check height, so we cannot set any flags here
    //sub->sector->SectorFlags |= (sector_t::SF_IsTransDoor|sector_t::SF_IsTransDoorTop);
    //GCon->Logf(NAME_Debug, "line #%d, side #%d (%d): tdtex='%s'", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), seg->side, *TTex->Name);
  }

  sp->texinfo.Tex = TTex;
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;

  SetupTextureAxesOffset(seg, &sp->texinfo, TTex, &sidedef->Top);

  if (TTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    //GCon->Logf(NAME_Debug, "2S-TOP: line #%d, side #%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]));

    // see `SetupTwoSidedBotWSurf()` for explanation
    const float topz1 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(max2, ceiling, *seg->v1, seg->frontsector, r_ceiling) : r_ceiling.GetPointZ(*seg->v1));
    const float topz2 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(max2, ceiling, *seg->v2, seg->frontsector, r_ceiling) : r_ceiling.GetPointZ(*seg->v2));
    const float botz1 = r_floor.GetPointZ(*seg->v1);
    const float botz2 = r_floor.GetPointZ(*seg->v2);

    // see `SetupTwoSidedBotWSurf()` for explanation
    const float back_topz1 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(min2, ceiling, *seg->v1, seg->backsector, (*back_ceiling)) : back_ceiling->GetPointZ(*seg->v1));
    const float back_topz2 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(min2, ceiling, *seg->v2, seg->backsector, (*back_ceiling)) : back_ceiling->GetPointZ(*seg->v2));

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

    const float tscale = TextureTScale(TTex)*sidedef->Top.ScaleY;
    if ((linedef->flags&ML_DONTPEGTOP)^peghack) {
      // top of texture at top
      sp->texinfo.toffs = top_TexZ*tscale;
    } else {
      // bottom of texture
      //GCon->Logf(NAME_Debug, "line #%d, side #%d: tz=%g; sch=%d; scy=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), back_ceiling->TexZ, TTex->GetScaledHeight(), sidedef->Top.ScaleY);
      sp->texinfo.toffs = back_ceiling->TexZ*tscale+TTex->Height;
    }
    sp->texinfo.toffs += sidedef->Top.RowOffset*TextureOffsetTScale(TTex);

    // transparent/translucent door
    if (hackflag == surface_t::TF_TOPHACK) {
      sp->texinfo.Alpha = linedef->alpha;
      sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);
    }

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = max2(back_topz1, botz1);
    wv[1].z = top_topz1;
    wv[2].z = top_topz2;
    wv[3].z = max2(back_topz2, botz2);

    bool createSurf = true;

    //FIXME: this is totally wrong with slopes!
    if (hackflag != surface_t::TF_TOPHACK && seg->backsector->heightsec && r_hack_fake_floor_decorations) {
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
        wv[0].z = max2(wv[0].z, hsec->ceiling.GetPointZ(*seg->v1));
        wv[3].z = max2(wv[3].z, hsec->ceiling.GetPointZ(*seg->v2));
        //createSurf = false;
      }
    }

    if (createSurf) {
      //CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_TOP|hackflag);
      CreateWorldSurfFromWVSplit(seg->frontsector, sub, seg, sp, wv, surface_t::TF_TOP|hackflag);
      if (sp->surfs) {
        sp->texinfo.Tex = TTex;
        sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
      }
    }
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
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  sec_plane_t *back_floor = &seg->backsector->floor;
  sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  VTexture *BTex = GTextureManager(sidedef->BottomTexture);
  if (!BTex) BTex = GTextureManager[GTextureManager.DefaultTexture];


  // HACK: sector with height of 1, and only middle masked texture is "transparent door"
  //       also, invert "lower unpegged" flag for this case
  int peghack = 0;
  unsigned hackflag = 0;
  //if (r_hack_transparent_doors && BTex->Type == TEXTYPE_Null) GCon->Logf(NAME_Debug, "line #%d, side #%d: transdoor check=%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), IsTransDoorHackTop(seg));
  if (!seg->pobj &&
      ((sub->sector->SectorFlags&sector_t::SF_IsTransDoorBot) || (r_hack_transparent_doors && BTex->Type == TEXTYPE_Null && IsTransDoorHackBot(seg))))
  {
    BTex = GTextureManager(sidedef->MidTexture);
    if (!BTex) BTex = GTextureManager[GTextureManager.DefaultTexture];
    //peghack = ML_DONTPEGBOTTOM;
    hackflag = surface_t::TF_TOPHACK;
    // this doesn't check height, so we cannot set any flags here
    //sub->sector->SectorFlags |= (sector_t::SF_IsTransDoor|sector_t::SF_IsTransDoorBot);
  }

  sp->texinfo.Tex = BTex;
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;

  SetupTextureAxesOffset(seg, &sp->texinfo, BTex, &sidedef->Bot);

  if (BTex->Type != TEXTYPE_Null) {
    TVec wv[4];

    //GCon->Logf(NAME_Debug, "2S-BOTTOM: line #%d, side #%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]));

    float topz1 = r_ceiling.GetPointZ(*seg->v1);
    float topz2 = r_ceiling.GetPointZ(*seg->v2);

    // some map authors are making floor decorations with height transfer
    // (that is so player won't wobble walking on such floors)
    // so we should use minimum front height here (sigh)
    //FIXME: this is totally wrong, because we may have alot of different
    //       configurations here, and we should check for them all
    //FIXME: also, moving height sector should trigger surface recreation
    float botz1 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(min2, floor, *seg->v1, seg->frontsector, r_floor) : r_floor.GetPointZ(*seg->v1));
    float botz2 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(min2, floor, *seg->v2, seg->frontsector, r_floor) : r_floor.GetPointZ(*seg->v2));
    float top_TexZ = r_ceiling.splane->TexZ;

    // same height fix as above
    float back_botz1 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(max2, floor, *seg->v1, seg->backsector, (*back_floor)) : back_floor->GetPointZ(*seg->v1));
    float back_botz2 = (r_hack_fake_floor_decorations ? GetFixedZWithFake(max2, floor, *seg->v2, seg->backsector, (*back_floor)) : back_floor->GetPointZ(*seg->v2));

    /* k8: boomedit.wad -- i can't make heads or tails of this crap; when it should be rendered, and when it shouldn't? */
    /*     this is total hack to make boomedit.wad underwater look acceptable; but why it is like this? */
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

    if ((linedef->flags&ML_DONTPEGBOTTOM)^peghack) {
      // bottom of texture at bottom
      // top of texture at top
      sp->texinfo.toffs = top_TexZ;
    } else {
      // top of texture at top
      sp->texinfo.toffs = back_floor->TexZ;
    }
    sp->texinfo.toffs *= TextureTScale(BTex)*sidedef->Bot.ScaleY;
    sp->texinfo.toffs += sidedef->Bot.RowOffset*TextureOffsetTScale(BTex);

    // transparent/translucent door
    if (hackflag == surface_t::TF_TOPHACK) {
      sp->texinfo.Alpha = linedef->alpha;
      sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);
    }

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    wv[0].z = botz1;
    wv[1].z = min2(back_botz1, topz1);
    wv[2].z = min2(back_botz2, topz2);
    wv[3].z = botz2;

    /* k8: boomedit.wad -- debug crap */
    /*
    if (seg->frontsector->heightsec && r_hack_fake_floor_decorations) {
      const sector_t *fhsec = seg->frontsector->heightsec;
      const sector_t *bhsec = (seg->backsector ? seg->backsector->heightsec : nullptr);
      int lidx = (int)(ptrdiff_t)(linedef-&Level->Lines[0]);
      if (lidx == 589) {
        GCon->Logf(NAME_Debug, "seg #%d: bsec=%d; fsec=%d; bhsec=%d; fhsec=%d", (int)(ptrdiff_t)(seg-&Level->Segs[0]), (int)(ptrdiff_t)(seg->backsector-&Level->Sectors[0]), (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]), (bhsec ? (int)(ptrdiff_t)(bhsec-&Level->Sectors[0]) : -1), (int)(ptrdiff_t)(fhsec-&Level->Sectors[0]));
        GCon->Logf(NAME_Debug, "linedef #%d: botz=(%g : %g); topz=(%g : %g); back_botz=(%g : %g); fhsecbotz=(%g : %g); fhsectopz=(%g : %g); bhsecbotz=(%g : %g); bhsectopz=(%g : %g)",
          lidx,
          botz1, botz2, topz1, topz2,
          back_botz1, back_botz2,
          fhsec->floor.GetPointZ(*seg->v1), fhsec->floor.GetPointZ(*seg->v2),
          fhsec->ceiling.GetPointZ(*seg->v1), fhsec->ceiling.GetPointZ(*seg->v2),
          (bhsec ? bhsec->floor.GetPointZ(*seg->v1) : -666.999f), (bhsec ? bhsec->floor.GetPointZ(*seg->v2) : -666.999f),
          (bhsec ? bhsec->ceiling.GetPointZ(*seg->v1) : -666.999f), (bhsec ? bhsec->ceiling.GetPointZ(*seg->v2) : -666.999f)
          );
      }
    }
    */

    bool createSurf = true;

    //FIXME: this is totally wrong with slopes!
    if (hackflag != surface_t::TF_TOPHACK && seg->backsector->heightsec && r_hack_fake_floor_decorations) {
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
        wv[1].z = min2(wv[1].z, hsec->floor.GetPointZ(*seg->v1));
        wv[2].z = min2(wv[2].z, hsec->floor.GetPointZ(*seg->v2));
        //createSurf = false;
      }
    }

    if (createSurf) {
      CreateWorldSurfFromWVSplit(seg->frontsector, sub, seg, sp, wv, surface_t::TF_BOTTOM|hackflag);
      if (sp->surfs) {
        sp->texinfo.Tex = BTex;
        sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
      }
    }
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

  SetupTextureAxesOffset(seg, &sp->texinfo, MTex, &sidedef->Mid);

  const sec_plane_t *back_floor = &seg->backsector->floor;
  const sec_plane_t *back_ceiling = &seg->backsector->ceiling;

  sp->texinfo.Tex = MTex;
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;

  if (MTex->Type != TEXTYPE_Null && !(sub->sector->SectorFlags&sector_t::SF_IsTransDoor)) {
    TVec wv[4];

    //GCon->Logf(NAME_Debug, "line #%d, sidenum #%d: midtex=%s (side: %d); transparent=%d; translucent=%d; seethrough=%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), *MTex->Name, seg->side, (int)MTex->isTransparent(), (int)MTex->isTranslucent(), (int)MTex->isSeeThrough());

    const sec_plane_t *bfloor = back_floor;
    const sec_plane_t *bceiling = back_ceiling;

    if (seg->backsector->heightsec && r_hack_fake_floor_decorations) {
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

    const float exbotz = min2(back_botz1, back_botz2);
    const float extopz = max2(back_topz1, back_topz2);

    //GCon->Logf(NAME_Debug, "line #%d, sidenum #%d: midtex=%s (side: %d); back_botz=(%g : %g); back_topz=(%g : %g), exbotz=%g; extopz=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), *MTex->Name, seg->side, back_botz1, back_botz2, back_topz1, back_topz2, exbotz, extopz);

    //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d: seg for line #%d (sector #%d); TexZ=%g:%g (notpeg=%d)", seg->pobj->tag, (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]), seg->pobj->pofloor.TexZ, seg->pobj->poceiling.TexZ, !!(linedef->flags&ML_DONTPEGBOTTOM));

    const float texh = DivByScale(MTex->GetScaledHeight(), sidedef->Mid.ScaleY);
    float z_org; // texture top
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      if (seg->pobj && seg->pobj->Is3D()) {
        z_org = seg->pobj->pofloor.TexZ+texh;
      } else {
        z_org = max2(seg->frontsector->floor.TexZ, seg->backsector->floor.TexZ)+texh;
      }
    } else {
      // top of texture at top
      if (seg->pobj && seg->pobj->Is3D()) {
        z_org = seg->pobj->poceiling.TexZ;
      } else {
        z_org = min2(seg->frontsector->ceiling.TexZ, seg->backsector->ceiling.TexZ);
      }
    }
    FixMidTextureOffsetAndOrigin(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid);

    sp->texinfo.Alpha = linedef->alpha;
    sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

    //bool doDump = ((ptrdiff_t)(linedef-Level->Lines) == 7956);
    enum { doDump = 0 };
    //const bool doDump = !!seg->pobj;
    if (doDump) { GCon->Logf("=== MIDSURF FOR LINE #%d (fs=%d; bs=%d; side=%d) ===", (int)(ptrdiff_t)(linedef-Level->Lines), (int)(ptrdiff_t)(seg->frontsector-Level->Sectors), (int)(ptrdiff_t)(seg->backsector-Level->Sectors), (int)(ptrdiff_t)(sidedef-Level->Sides)); }
    //if (linedef->alpha < 1.0f) GCon->Logf("=== MIDSURF FOR LINE #%d (fs=%d; bs=%d) ===", (int)(ptrdiff_t)(linedef-Level->Lines), (int)(ptrdiff_t)(seg->frontsector-Level->Sectors), (int)(ptrdiff_t)(seg->backsector-Level->Sectors));
    if (doDump) { GCon->Logf("   LINEWRAP=%u; SIDEWRAP=%u; ADDITIVE=%u; Alpha=%g; botpeg=%u; z_org=%g; texh=%g", (linedef->flags&ML_WRAP_MIDTEX), (sidedef->Flags&SDF_WRAPMIDTEX), (linedef->flags&ML_ADDITIVE), linedef->alpha, linedef->flags&ML_DONTPEGBOTTOM, z_org, texh); }
    if (doDump) { GCon->Logf("   tx is '%s'; size=(%d,%d); scale=(%g,%g)", *MTex->Name, MTex->GetWidth(), MTex->GetHeight(), MTex->SScale, MTex->TScale); }

    //k8: HACK! HACK! HACK!
    //    move middle wall backwards a little, so it will be hidden behind up/down surfaces
    //    this is required for sectors with 3d floors, until i wrote a proper texture clipping math
    const bool doOffset = seg->backsector->Has3DFloors();

    // another hack (Doom II MAP31)
    // if we have no 3d floors here, and the front sector can be covered with midtex, cover it
    bool bottomCheck = false;
    if (!doOffset && !seg->frontsector->Has3DFloors() && sidedef->BottomTexture < 1 &&
        seg->frontsector->floor.normal.z == 1.0f && seg->backsector->floor.normal.z == 1.0f &&
        seg->frontsector->floor.minz < seg->backsector->floor.minz)
    {
      //GCon->Logf(NAME_Debug, "BOO!");
      bottomCheck = true;
    }

    for (opening_t *cop = (lastQuadSplit ? GetBaseSectorOpening(seg->frontsector) : GetSectorOpenings(seg->frontsector, true)); cop; cop = cop->next) {
      if (extopz <= cop->bottom || exbotz >= cop->top) {
        if (doDump) { GCon->Log(" SKIP opening"); VLevel::DumpOpening(cop); }
        //continue;
      }
      if (doDump) { GCon->Logf(" ACCEPT opening"); VLevel::DumpOpening(cop); }
      // ok, we are at least partially in this opening

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      const float topz1 = min2(back_topz1, cop->eceiling.GetPointZ(*seg->v1));
      const float topz2 = min2(back_topz2, cop->eceiling.GetPointZ(*seg->v2));
      const float botz1 = max2(back_botz1, cop->efloor.GetPointZ(*seg->v1));
      const float botz2 = max2(back_botz2, cop->efloor.GetPointZ(*seg->v2));

      float midtopz1 = topz1;
      float midtopz2 = topz2;
      float midbotz1 = botz1;
      float midbotz2 = botz2;

      if (doDump) { GCon->Logf(" zorg=(%g,%g); botz=(%g,%g); topz=(%g,%g)", z_org-texh, z_org, midbotz1, midbotz2, midtopz1, midtopz2); }

      if (sidedef->TopTexture > 0) {
        midtopz1 = min2(midtopz1, cop->eceiling.GetPointZ(*seg->v1));
        midtopz2 = min2(midtopz2, cop->eceiling.GetPointZ(*seg->v2));
      }

      if (sidedef->BottomTexture > 0) {
        midbotz1 = max2(midbotz1, cop->efloor.GetPointZ(*seg->v1));
        midbotz2 = max2(midbotz2, cop->efloor.GetPointZ(*seg->v2));
      }

      if (midbotz1 >= midtopz1 || midbotz2 >= midtopz2) continue;

      if (doDump) { GCon->Logf(" zorg=(%g,%g); botz=(%g,%g); topz=(%g,%g); backbotz=(%g,%g); backtopz=(%g,%g)", z_org-texh, z_org, midbotz1, midbotz2, midtopz1, midtopz2, back_botz1, back_botz2, back_topz1, back_topz2); }

      float hgts[4];

      // linedef->flags&ML_CLIP_MIDTEX, sidedef->Flags&SDF_CLIPMIDTEX
      // this clips texture to a floor, otherwise it goes beyound it
      // it seems that all modern OpenGL renderers just ignores clip flag, and
      // renders all midtextures as always clipped.
      if ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX)) {
        hgts[0] = midbotz1;
        hgts[1] = midtopz1;
        hgts[2] = midtopz2;
        hgts[3] = midbotz2;
      } else {
        if (z_org <= max2(midbotz1, midbotz2)) continue;
        if (z_org-texh >= max2(midtopz1, midtopz2)) continue;
        /*
        if (doDump) {
          GCon->Log(" === front regions ===");
          VLevel::dumpSectorRegions(seg->frontsector);
          GCon->Log(" === front openings ===");
          for (opening_t *bop = GetSectorOpenings2(seg->frontsector, true); bop; bop = bop->next) VLevel::DumpOpening(bop);
          GCon->Log(" === back regions ===");
          VLevel::dumpSectorRegions(seg->backsector);
          GCon->Log(" === back openings ===");
          for (opening_t *bop = GetSectorOpenings2(seg->backsector, true); bop; bop = bop->next) VLevel::DumpOpening(bop);
        }
        */
        hgts[0] = max2(midbotz1, z_org-texh);
        hgts[1] = min2(midtopz1, z_org);
        hgts[2] = min2(midtopz2, z_org);
        hgts[3] = max2(midbotz2, z_org-texh);
        // cover bottom texture with this too (because why not?)
        if (bottomCheck && hgts[0] > z_org-texh) {
          //GCon->Logf(NAME_Debug, "BOO! hgts=%g, %g, %g, %g (fz=%g); texh=%g", hgts[0], hgts[1], hgts[2], hgts[3], seg->frontsector->floor.minz, texh);
          hgts[0] = hgts[3] = min2(hgts[0], max2(seg->frontsector->floor.minz, z_org-texh));
        }
      }

      wv[0].z = hgts[0];
      wv[1].z = hgts[1];
      wv[2].z = hgts[2];
      wv[3].z = hgts[3];

      if (doDump) {
        GCon->Logf("  z:(%g,%g,%g,%g)", hgts[0], hgts[1], hgts[2], hgts[3]);
        for (int wc = 0; wc < 4; ++wc) GCon->Logf("  wc #%d: (%g,%g,%g)", wc, wv[wc].x, wv[wc].y, wv[wc].z);
      }

      CreateWorldSurfFromWVSplit(seg->frontsector, sub, seg, sp, wv, surface_t::TF_MIDDLE, doOffset);
    }
  } else {
    // empty midtexture
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
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


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedMidWSurf3DPObj
//
//  create normal midtexture surface for 3d polyobject
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedMidWSurf3DPObj (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  if (!MTex) MTex = GTextureManager[GTextureManager.DefaultTexture];

  SetupTextureAxesOffset(seg, &sp->texinfo, MTex, &sidedef->Mid);

  sp->texinfo.Tex = MTex;
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;

  const polyobj_t *po = seg->pobj;

  if (MTex->Type != TEXTYPE_Null && !(sub->sector->SectorFlags&sector_t::SF_IsTransDoor)) {
    TVec wv[4];

    //GCon->Logf(NAME_Debug, "line #%d, sidenum #%d: midtex=%s (side: %d); transparent=%d; translucent=%d; seethrough=%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), *MTex->Name, seg->side, (int)MTex->isTransparent(), (int)MTex->isTranslucent(), (int)MTex->isSeeThrough());
    //GCon->Logf(NAME_Debug, "line #%d, sidenum #%d: midtex=%s (side: %d); back_botz=(%g : %g); back_topz=(%g : %g), exbotz=%g; extopz=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), *MTex->Name, seg->side, back_botz1, back_botz2, back_topz1, back_topz2, exbotz, extopz);

    //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d: seg for line #%d (sector #%d); TexZ=%g:%g (notpeg=%d)", seg->pobj->tag, (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]), seg->pobj->pofloor.TexZ, seg->pobj->poceiling.TexZ, !!(linedef->flags&ML_DONTPEGBOTTOM));

    const float texh = DivByScale(MTex->GetScaledHeight(), sidedef->Mid.ScaleY);
    float z_org; // texture top
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      if (seg->pobj && seg->pobj->Is3D()) {
        z_org = seg->pobj->pofloor.TexZ+texh;
      } else {
        z_org = max2(seg->frontsector->floor.TexZ, seg->backsector->floor.TexZ)+texh;
      }
    } else {
      // top of texture at top
      if (seg->pobj && seg->pobj->Is3D()) {
        z_org = seg->pobj->poceiling.TexZ;
      } else {
        z_org = min2(seg->frontsector->ceiling.TexZ, seg->backsector->ceiling.TexZ);
      }
    }
    FixMidTextureOffsetAndOrigin(z_org, linedef, sidedef, &sp->texinfo, MTex, &sidedef->Mid);

    sp->texinfo.Alpha = linedef->alpha;
    sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    const float topz1 = po->poceiling.GetPointZ(*seg->v1);
    const float topz2 = po->poceiling.GetPointZ(*seg->v2);
    const float botz1 = po->pofloor.GetPointZ(*seg->v1);
    const float botz2 = po->pofloor.GetPointZ(*seg->v2);

    if (botz1 >= topz1 || botz2 >= topz2) {
    } else {
      bool doit = true;
      float hgts[4];

      // linedef->flags&ML_CLIP_MIDTEX, sidedef->Flags&SDF_CLIPMIDTEX
      // this clips texture to a floor, otherwise it goes beyound it
      // it seems that all modern OpenGL renderers just ignores clip flag, and
      // renders all midtextures as always clipped.
      if ((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX)) {
        hgts[0] = botz1;
        hgts[1] = topz1;
        hgts[2] = topz2;
        hgts[3] = botz2;
      } else {
        if (z_org <= max2(botz1, botz2)) {
          doit = false;
        } else if (z_org-texh >= max2(topz1, topz2)) {
          doit = false;
        } else {
          hgts[0] = max2(botz1, z_org-texh);
          hgts[1] = min2(topz1, z_org);
          hgts[2] = min2(topz2, z_org);
          hgts[3] = max2(botz2, z_org-texh);
        }
      }

      if (doit) {
        wv[0].z = hgts[0];
        wv[1].z = hgts[1];
        wv[2].z = hgts[2];
        wv[3].z = hgts[3];

        CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE, /*doOffset*/false);
      }
    }
  } else {
    // empty midtexture
    sp->texinfo.Alpha = 1.1f;
    sp->texinfo.Additive = false;
  }

  sp->frontTopDist = po->poceiling.dist;
  sp->frontBotDist = po->pofloor.dist;
  sp->backTopDist = po->poceiling.dist;
  sp->backBotDist = po->pofloor.dist;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
  sp->ResetFixTJunk();
  SetupFakeDistances(seg, sp);
}
