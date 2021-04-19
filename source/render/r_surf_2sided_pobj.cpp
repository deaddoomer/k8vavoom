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


//**************************************************************************
//**
//**  Seg surfaces
//**
//**************************************************************************

//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedTopWSurf3DPObj
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedTopWSurf3DPObj (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);
  vassert(sub->sector);

  polyobj_t *po = seg->pobj;
  const side_t *sidedef = seg->sidedef;

  VTexture *TTex = GTextureManager(sidedef->TopTexture);
  if (!TTex) TTex = GTextureManager[GTextureManager.DefaultTexture];

  if (TTex->Type != TEXTYPE_Null) {
    TVec quad[4];

    float zOrg = po->poceiling.TexZ; // texture bottom
    SetupTextureAxesOffset(seg, &sp->texinfo, TTex, &sidedef->Top, zOrg);

    const float texh = TTex->GetScaledHeight()/sidedef->Top.ScaleY;

    quad[0].x = quad[1].x = seg->v1->x;
    quad[0].y = quad[1].y = seg->v1->y;
    quad[2].x = quad[3].x = seg->v2->x;
    quad[2].y = quad[3].y = seg->v2->y;

    quad[QUAD_V1_BOTTOM].z = zOrg;
    quad[QUAD_V1_TOP].z = zOrg+texh;
    quad[QUAD_V2_TOP].z = zOrg+texh;
    quad[QUAD_V2_BOTTOM].z = zOrg;

    CreateWorldSurfFromWV(sub, seg, sp, quad, surface_t::TF_3DPOBJ|surface_t::TF_TOP);
    if (sp->surfs) {
      sp->texinfo.Tex = TTex;
      sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
    }
  } else {
    SetupTextureAxesOffsetDummy(&sp->texinfo, TTex);
  }

  sp->frontTopDist = po->poceiling.dist;
  sp->frontBotDist = po->pofloor.dist;
  sp->backTopDist = po->poceiling.dist;
  sp->backBotDist = po->pofloor.dist;
  sp->TextureOffset = sidedef->Top.TextureOffset;
  sp->RowOffset = sidedef->Top.RowOffset;
  sp->ResetFixTJunk();
  SetupFakeDistances(seg, sp);
}


//==========================================================================
//
//  VRenderLevelShared::SetupTwoSidedBotWSurf3DPObj
//
//==========================================================================
void VRenderLevelShared::SetupTwoSidedBotWSurf3DPObj (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const side_t *sidedef = seg->sidedef;
  polyobj_t *po = seg->pobj;

  VTexture *BTex = GTextureManager(sidedef->BottomTexture);
  if (!BTex) BTex = GTextureManager[GTextureManager.DefaultTexture];

  if (BTex->Type != TEXTYPE_Null) {
    TVec quad[4];

    float zOrg = po->pofloor.TexZ; // texture bottom
    const float texh = BTex->GetScaledHeight()/sidedef->Bot.ScaleY;
    zOrg -= texh; // convert to texture bottom
    SetupTextureAxesOffset(seg, &sp->texinfo, BTex, &sidedef->Bot, zOrg);

    quad[0].x = quad[1].x = seg->v1->x;
    quad[0].y = quad[1].y = seg->v1->y;
    quad[2].x = quad[3].x = seg->v2->x;
    quad[2].y = quad[3].y = seg->v2->y;

    quad[QUAD_V1_BOTTOM].z = zOrg;
    quad[QUAD_V1_TOP].z = zOrg+texh;
    quad[QUAD_V2_TOP].z = zOrg+texh;
    quad[QUAD_V2_BOTTOM].z = zOrg;

    CreateWorldSurfFromWV(sub, seg, sp, quad, surface_t::TF_3DPOBJ|surface_t::TF_BOTTOM);
    if (sp->surfs) {
      sp->texinfo.Tex = BTex;
      sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
    }
  } else {
    SetupTextureAxesOffsetDummy(&sp->texinfo, BTex);
  }

  SetupTextureAxesOffsetDummy(&sp->texinfo, BTex);

  sp->frontTopDist = po->poceiling.dist;
  sp->frontBotDist = po->pofloor.dist;
  sp->backTopDist = po->poceiling.dist;
  sp->backBotDist = po->pofloor.dist;
  sp->TextureOffset = sidedef->Bot.TextureOffset;
  sp->RowOffset = sidedef->Bot.RowOffset;
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

  const polyobj_t *po = seg->pobj;

  if (MTex->Type != TEXTYPE_Null && !(sub->sector->SectorFlags&sector_t::SF_IsTransDoor)) {
    TVec quad[4];

    //GCon->Logf(NAME_Debug, "line #%d, sidenum #%d: midtex=%s (side: %d); transparent=%d; translucent=%d; seethrough=%d", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), *MTex->Name, seg->side, (int)MTex->isTransparent(), (int)MTex->isTranslucent(), (int)MTex->isSeeThrough());
    //GCon->Logf(NAME_Debug, "line #%d, sidenum #%d: midtex=%s (side: %d); back_botz=(%g : %g); back_topz=(%g : %g), exbotz=%g; extopz=%g", (int)(ptrdiff_t)(linedef-&Level->Lines[0]), (int)(ptrdiff_t)(sidedef-&Level->Sides[0]), *MTex->Name, seg->side, back_botz1, back_botz2, back_topz1, back_topz2, exbotz, extopz);

    //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d: seg for line #%d (sector #%d); TexZ=%g:%g (notpeg=%d)", seg->pobj->tag, (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]), seg->pobj->pofloor.TexZ, seg->pobj->poceiling.TexZ, !!(linedef->flags&ML_DONTPEGBOTTOM));

    const float texh = MTex->GetScaledHeight()/sidedef->Mid.ScaleY;
    float zOrg; // texture bottom
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      zOrg = seg->pobj->pofloor.TexZ;
    } else {
      // top of texture at top
      zOrg = seg->pobj->poceiling.TexZ-texh;
    }
    SetupTextureAxesOffset(seg, &sp->texinfo, MTex, &sidedef->Mid, zOrg);

    sp->texinfo.Alpha = linedef->alpha;
    sp->texinfo.Additive = !!(linedef->flags&ML_ADDITIVE);

    quad[0].x = quad[1].x = seg->v1->x;
    quad[0].y = quad[1].y = seg->v1->y;
    quad[2].x = quad[3].x = seg->v2->x;
    quad[2].y = quad[3].y = seg->v2->y;

    const float topz1 = po->poceiling.GetPointZ(*seg->v1);
    const float topz2 = po->poceiling.GetPointZ(*seg->v2);
    const float botz1 = po->pofloor.GetPointZ(*seg->v1);
    const float botz2 = po->pofloor.GetPointZ(*seg->v2);

    if (botz1 >= topz1 || botz2 >= topz2) {
      // nothing to do
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
        const float tz0 = zOrg;
        const float tz1 = zOrg+texh;
        if (tz0 >= max2(topz1, topz2) || tz1 <= max2(botz1, botz2)) {
          doit = false;
        } else {
          hgts[0] = max2(botz1, tz0);
          hgts[1] = min2(topz1, tz1);
          hgts[2] = min2(topz2, tz1);
          hgts[3] = max2(botz2, tz0);
        }
      }

      if (doit) {
        quad[0].z = hgts[0];
        quad[1].z = hgts[1];
        quad[2].z = hgts[2];
        quad[3].z = hgts[3];

        CreateWorldSurfFromWV(sub, seg, sp, quad, surface_t::TF_3DPOBJ|surface_t::TF_MIDDLE);
      }
    }
  } else {
    // empty midtexture
    SetupTextureAxesOffsetDummy(&sp->texinfo, MTex);
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
