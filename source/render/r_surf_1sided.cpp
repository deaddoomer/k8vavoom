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
//  VRenderLevelShared::SetupOneSidedSkyWSurf
//
//==========================================================================
void VRenderLevelShared::SetupOneSidedSkyWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  sp->texinfo.Tex = GTextureManager[skyflatnum];
  sp->texinfo.noDecals = sp->texinfo.Tex->noDecals;
  sp->texinfo.Alpha = 1.1f;
  sp->texinfo.Additive = false;
  sp->texinfo.ColorMap = CM_Default;

  // not for polyobjects
  if (sp->texinfo.Tex->Type != TEXTYPE_Null && !seg->pobj) {
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
//  VRenderLevelShared::SetupOneSidedMidWSurf
//
//==========================================================================
void VRenderLevelShared::SetupOneSidedMidWSurf (subsector_t *sub, seg_t *seg, segpart_t *sp, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling) {
  FreeWSurfs(sp->surfs);

  const line_t *linedef = seg->linedef;
  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = GTextureManager(sidedef->MidTexture);
  // k8: one-sided line should have a midtex
  if (!MTex) {
    GCon->Logf(NAME_Warning, "Sidedef #%d should have midtex, but it hasn't (%d)", (int)(ptrdiff_t)(sidedef-Level->Sides), sidedef->MidTexture.id);
    MTex = GTextureManager[GTextureManager.DefaultTexture];
  }

  const bool skipIt = false;//(seg->offset > 16.0f && (ptrdiff_t)(seg->linedef-&Level->Lines[0]) == 7);

  if (MTex->Type != TEXTYPE_Null && !skipIt) {
    TVec wv[4];

    // Doom "up" is positive `z`
    // texture origin is left bottom corner (don't even ask me why)
    const float texh = MTex->GetScaledHeightF()/sidedef->Mid.ScaleY;
    float zOrg; // texture bottom
    if (linedef->flags&ML_DONTPEGBOTTOM) {
      // bottom of texture at bottom
      zOrg = r_floor.splane->TexZ;
    } else if (linedef->flags&ML_DONTPEGTOP) {
      // top of texture at top of top region
      zOrg = seg->frontsub->sector->ceiling.TexZ-texh;
    } else {
      // top of texture at top
      zOrg = r_ceiling.splane->TexZ-texh;
    }
    // do not use "Mid" here, because we should ignore midtexture wrapping (anyway)
    SetupTextureAxesOffsetMid1S(seg, &sp->texinfo, MTex, &sidedef->Mid, zOrg);

    wv[0].x = wv[1].x = seg->v1->x;
    wv[0].y = wv[1].y = seg->v1->y;
    wv[2].x = wv[3].x = seg->v2->x;
    wv[2].y = wv[3].y = seg->v2->y;

    float topz1 = r_ceiling.GetPointZ(*seg->v1);
    float topz2 = r_ceiling.GetPointZ(*seg->v2);
    // those `min2()` fixes some slopes
    float botz1 = min2(topz1, r_floor.GetPointZ(*seg->v1));
    float botz2 = min2(topz2, r_floor.GetPointZ(*seg->v2));

    const sector_t *sec = sub->sector;

    //FIXME: cache this info
    if (!seg->pobj && (sec->SectorFlags&(sector_t::SF_IsTransDoorTop|sector_t::SF_IsTransDoorTrack))) {
      //GCon->Logf(NAME_Debug, "transdorrtrack; sector #%d; line #%d; fz=(%g : %g); cz=(%g : %g)", (int)(ptrdiff_t)(sec-&Level->Sectors[0]), (int)(ptrdiff_t)(linedef-&Level->Lines[0]), botz1, botz2, topz1, topz2);

      // height transfer
      const sector_t *hsec = sec->heightsec;
      if (hsec) {
        if (!r_ceiling.isSlope() && !hsec->ceiling.isSlope() && hsec->ceiling.minz > r_ceiling.GetRealDist()) {
          topz1 = hsec->ceiling.GetPointZ(*seg->v1);
          topz2 = hsec->ceiling.GetPointZ(*seg->v2);
          //GCon->Logf(NAME_Debug, "transdorrtrack;   hsector #%d; cz=(%g : %g)", (int)(ptrdiff_t)(hsec-&Level->Sectors[0]), topz1, topz2);
        } else {
          //GCon->Logf(NAME_Debug, "transdorrtrack;   NOT hsector #%d; cmz=%g; rcz=%g; cz=(%g : %g)", (int)(ptrdiff_t)(hsec-&Level->Sectors[0]), hsec->ceiling.minz, r_ceiling.GetRealDist(), hsec->ceiling.GetPointZ(*seg->v1), hsec->ceiling.GetPointZ(*seg->v2));
        }
        if (!r_floor.isSlope() && !hsec->floor.isSlope() && hsec->floor.maxz < r_floor.GetRealDist()) {
          botz1 = hsec->floor.GetPointZ(*seg->v1);
          botz2 = hsec->floor.GetPointZ(*seg->v2);
          //GCon->Logf(NAME_Debug, "transdorrtrack;   hsector #%d; fz=(%g : %g)", (int)(ptrdiff_t)(hsec-&Level->Sectors[0]), botz1, botz2);
        } else {
          //GCon->Logf(NAME_Debug, "transdorrtrack;   NOT hsector #%d; fmz=%g; rfz=%g; fz=(%g : %g)", (int)(ptrdiff_t)(hsec-&Level->Sectors[0]), hsec->floor.maxz, r_floor.GetRealDist(), hsec->floor.GetPointZ(*seg->v1), hsec->floor.GetPointZ(*seg->v2));
        }
      }

      if (hsec && (sec->SectorFlags&(sector_t::SF_IsTransDoorTop|sector_t::SF_IsTransDoorTrack)) == sector_t::SF_IsTransDoorTrack) {
        // do nothing
      } else {
        for (int f = 0; f < sec->linecount; ++f) {
          const line_t *ldef = sec->lines[f];
          //GCon->Logf(NAME_Debug, "  ...trying line #%d...", (int)(ptrdiff_t)(ldef-&Level->Lines[0]));

          if (ldef == linedef) continue;
          if (ldef->sidenum[0] < 0) continue;
          if ((ldef->flags&(ML_TWOSIDED|ML_3DMIDTEX)) != ML_TWOSIDED) continue;

          const sector_t *fsec = ldef->frontsector;
          const sector_t *bsec = ldef->backsector;
          if (!fsec || !bsec) continue; // one-sided
          if (fsec == bsec) continue; // self-referenced sector
          if (/*fsec != sec &&*/ bsec != sec) continue;

          const side_t *lsd = &Level->Sides[ldef->sidenum[0]];

          if ((sec->SectorFlags&(sector_t::SF_IsTransDoorTop|sector_t::SF_IsTransDoorTrack)) == sector_t::SF_IsTransDoorTrack) {
            // do nothing
          } else {
            if (GTextureManager.IsEmptyTexture(lsd->MidTexture)) continue;

            if ((ldef->flags&ML_ADDITIVE) == 0 && ldef->alpha >= 1.0f) {
              VTexture *tex = GTextureManager[lsd->MidTexture];
              if (!tex->isSeeThrough()) continue;
            }
          }

          // height transfer
          #if 0
          float cz1 = fsec->ceiling.GetPointZ(*linedef->v1);
          float cz2 = fsec->ceiling.GetPointZ(*linedef->v2);
          float fz1 = fsec->floor.GetPointZ(*linedef->v1);
          float fz2 = fsec->floor.GetPointZ(*linedef->v2);

          const sector_t *fhsec = fsec->heightsec;
          if (fhsec) {
            if (!fsec->ceiling.isSlope() && !fhsec->ceiling.isSlope() && fhsec->ceiling.minz < fsec->ceiling.GetRealDist()) {
              cz1 = fhsec->ceiling.GetPointZ(*seg->v1);
              cz2 = fhsec->ceiling.GetPointZ(*seg->v2);
            }
            if (!fsec->floor.isSlope() && !fhsec->floor.isSlope() && fhsec->floor.maxz > fsec->floor.GetRealDist()) {
              fz1 = fhsec->floor.GetPointZ(*seg->v1);
              fz2 = fhsec->floor.GetPointZ(*seg->v2);
            }
          }

          //GCon->Logf(NAME_Debug, "  ...fixing with line #%d", (int)(ptrdiff_t)(ldef-&Level->Lines[0]));
          topz1 = max2(topz1, cz1);
          topz2 = max2(topz2, cz2);
          botz1 = min2(botz1, fz1);
          botz2 = min2(botz2, fz2);
          #else
          topz1 = max2(topz1, fsec->ceiling.GetPointZ(*linedef->v1));
          topz2 = max2(topz2, fsec->ceiling.GetPointZ(*linedef->v2));
          botz1 = min2(botz1, fsec->floor.GetPointZ(*linedef->v1));
          botz2 = min2(botz2, fsec->floor.GetPointZ(*linedef->v2));
          #endif
        }
      }
    }

    // just in case
    wv[0].z = min2(topz1, botz1);
    wv[1].z = topz1;
    wv[2].z = topz2;
    wv[3].z = min2(topz2, botz2);

    //CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE);
    CreateWorldSurfFromWVSplit(sub, seg, sp, wv, surface_t::TF_MIDDLE);
  } else {
    SetupTextureAxesOffsetDummy(&sp->texinfo, MTex);
  }

  sp->frontTopDist = r_ceiling.splane->dist;
  sp->frontBotDist = r_floor.splane->dist;
  sp->backTopDist = 0.0f;
  sp->backBotDist = 0.0f;
  sp->TextureOffset = sidedef->Mid.TextureOffset;
  sp->RowOffset = sidedef->Mid.RowOffset;
  sp->ResetFixTJunk();
  SetupFakeDistances(seg, sp);
}
