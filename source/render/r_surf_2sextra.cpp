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


// ////////////////////////////////////////////////////////////////////////// //
static VCvarB r_3dfloor_clip_both_sides("r_3dfloor_clip_both_sides", false, "Clip 3d floors with both sectors?", CVAR_Archive);


//**************************************************************************
//**
//**  Seg surfaces -- 3d floors (extra surfaces)
//**
//**************************************************************************

struct WallFace {
  float topz[2];
  float botz[2];
  const TVec *v[2];
  WallFace *next;

  inline WallFace () noexcept {
    topz[0] = topz[1] = 0;
    botz[0] = botz[1] = 0;
    v[0] = v[1] = nullptr;
    next = nullptr;
  }

  inline void setup (const sec_region_t *reg, const TVec *v1, const TVec *v2) noexcept {
    v[0] = v1;
    v[1] = v2;

    topz[0] = reg->eceiling.GetPointZ(*v1);
    topz[1] = reg->eceiling.GetPointZ(*v2);

    botz[0] = reg->efloor.GetPointZ(*v1);
    botz[1] = reg->efloor.GetPointZ(*v2);
  }

  inline void markInvalid () noexcept { topz[0] = topz[1] = -666; botz[0] = botz[1] = 666; }
  inline bool isValid () const noexcept { return (topz[0] > botz[0] && topz[1] > botz[1]); }
};


//==========================================================================
//
//  CutWallFace
//
//==========================================================================
static void CutWallFace (WallFace *face, sector_t *sec, sec_region_t *ignorereg, bool cutWithSwimmable, WallFace *&tail) {
  if (!face->isValid()) return;
  if (!sec) return;
  const vuint32 skipmask = sec_region_t::RF_OnlyVisual|(cutWithSwimmable ? 0u : sec_region_t::RF_NonSolid);
  for (sec_region_t *reg = sec->eregions; reg; reg = reg->next) {
    if (reg->regflags&skipmask) continue; // not interesting
    if (reg == ignorereg) continue; // ignore self
    if (reg->regflags&sec_region_t::RF_BaseRegion) {
      // base region, allow what is inside
      for (int f = 0; f < 2; ++f) {
        const float rtopz = reg->eceiling.GetPointZ(*face->v[f]);
        const float rbotz = reg->efloor.GetPointZ(*face->v[f]);
        // if above/below the sector, mark as invalid
        if (face->botz[f] >= rtopz || face->topz[f] <= rbotz) {
          face->markInvalid();
          return;
        }
        // cut with sector bounds
        face->topz[f] = min2(face->topz[f], rtopz);
        face->botz[f] = max2(face->botz[f], rbotz);
        if (face->topz[f] <= face->botz[f]) return; // everything was cut away
      }
    } else {
      //FIXME: for now, we can cut only by non-sloped 3d floors
      if (reg->eceiling.GetPointZ(*face->v[0]) != reg->eceiling.GetPointZ(*face->v[1]) ||
          reg->efloor.GetPointZ(*face->v[0]) != reg->efloor.GetPointZ(*face->v[1]))
      {
        continue;
      }
      //FIXME: ...and only non-sloped 3d floors
      if (ignorereg->eceiling.GetPointZ(*face->v[0]) != ignorereg->eceiling.GetPointZ(*face->v[1]) ||
          ignorereg->efloor.GetPointZ(*face->v[0]) != ignorereg->efloor.GetPointZ(*face->v[1]))
      {
        continue;
      }
      // 3d floor, allow what is outside
      for (int f = 0; f < 2; ++f) {
        const float rtopz = reg->eceiling.GetPointZ(*face->v[f]);
        const float rbotz = reg->efloor.GetPointZ(*face->v[f]);
        if (rtopz <= rbotz) continue; // invalid, or paper-thin, ignore
        // if completely above, or completely below, ignore
        if (rbotz >= face->topz[f] || rtopz <= face->botz[f]) continue;
        // if completely covered, mark as invalid and stop
        if (rbotz <= face->botz[f] && rtopz >= face->topz[f]) {
          face->markInvalid();
          return;
        }
        // if inside the face, split it to two faces
        if (rtopz > face->topz[f] && rbotz < face->botz[f]) {
          // split; top part
          {
            WallFace *ftop = new WallFace;
            *ftop = *face;
            ftop->botz[0] = ftop->botz[1] = rtopz;
            tail->next = ftop;
            tail = ftop;
          }
          // split; bottom part
          {
            WallFace *fbot = new WallFace;
            *fbot = *face;
            fbot->topz[0] = fbot->topz[1] = rbotz;
            tail->next = fbot;
            tail = fbot;
          }
          // mark this one as invalid, and stop
          face->markInvalid();
          return;
        }
        // partially covered; is it above?
        if (rbotz > face->botz[f]) {
          // it is above, cut top
          face->topz[f] = min2(face->topz[f], rbotz);
        } else if (rtopz < face->topz[f]) {
          // it is below, cut bottom
          face->botz[f] = max2(face->botz[f], rtopz);
        }
        if (face->topz[f] <= face->botz[f]) return; // everything was cut away
      }
    }
  }
}


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

  //ops = GetSectorOpenings(seg->frontsector); // skip non-solid

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
    GCon->Log(" === front openings ===");
    for (opening_t *bop = GetSectorOpenings2(seg->frontsector, true); bop; bop = bop->next) VLevel::DumpOpening(bop);
    GCon->Log(" === real openings ===");
    //for (opening_t *bop = ops; bop; bop = bop->next) VLevel::DumpOpening(bop);
  }

  if (seg->backsector && seg->backsector-&Level->Sectors[0] == 70) {
    doDump = true;
    GCon->Logf("::: BACK-SECTOR #70 (front #%d) EF: texture='%s'", (seg->frontsector ? (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]) : -1), *MTex->Name);
    GCon->Log(" === front regions ===");
    VLevel::dumpSectorRegions(seg->frontsector);
    GCon->Log(" === front openings ===");
    for (opening_t *bop = GetSectorOpenings2(seg->frontsector, true); bop; bop = bop->next) VLevel::DumpOpening(bop);
    GCon->Log(" === real openings ===");
    for (opening_t *bop = GetSectorOpenings(seg->frontsector); bop; bop = bop->next) VLevel::DumpOpening(bop);
  }
  */

  if (doDump) GCon->Logf(NAME_Debug, "*** line #%d (extra line #%d); seg side #%d ***", (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), (int)(ptrdiff_t)(linedef-&Level->Lines[0]), seg->side);
  if (doDump) {
    GCon->Log(NAME_Debug, "=== REGIONS:FRONT ===");
    VLevel::dumpSectorRegions(seg->frontsector);
    GCon->Log(NAME_Debug, "=== REGIONS:BACK ===");
    VLevel::dumpSectorRegions(seg->backsector);
    GCon->Log(NAME_Debug, "=== OPENINGS:FRONT ===");
    for (opening_t *bop = GetSectorOpenings(seg->frontsector); bop; bop = bop->next) VLevel::DumpOpening(bop);
    GCon->Log(NAME_Debug, "=== OPENINGS:BACK ===");
    for (opening_t *bop = GetSectorOpenings(seg->backsector); bop; bop = bop->next) VLevel::DumpOpening(bop);
    //ops = GetSectorOpenings(seg->frontsector); // this should be done to update openings
  }

  // apply offsets from seg side
  SetupTextureAxesOffsetEx(seg, &sp->texinfo, MTex, &sidedef->Mid, &segsidedef->Mid);

  //const float texh = DivByScale(MTex->GetScaledHeight(), texsideparm->Mid.ScaleY);
  const float texh = DivByScale2(MTex->GetScaledHeight(), sidedef->Mid.ScaleY, scale2Y);
  const float texhsc = MTex->GetHeight();
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
    TVec wv[4];

    //const bool wrapped = !!((linedef->flags&ML_WRAP_MIDTEX)|(sidedef->Flags&SDF_WRAPMIDTEX));
    // side 3d floor midtex should always be wrapped
    enum { wrapped = 1 };

    WallFace *facehead = nullptr;
    WallFace *facetail = nullptr;
    WallFace *face = new WallFace;
    face->setup(reg, seg->v1, seg->v2);

    const bool isNonSolid = !!(reg->regflags&sec_region_t::RF_NonSolid);
    facehead = facetail = face;
    for (WallFace *cface = facehead; cface; cface = cface->next) {
      if (isNonSolid || r_3dfloor_clip_both_sides) CutWallFace(cface, seg->frontsector, reg, isNonSolid, facetail);
      CutWallFace(cface, seg->backsector, reg, isNonSolid, facetail);
    }

    // now create all surfaces
    for (WallFace *cface = facehead; cface; cface = cface->next) {
      if (!cface->isValid()) continue;

      float topz1 = cface->topz[0];
      float topz2 = cface->topz[1];
      float botz1 = cface->botz[0];
      float botz2 = cface->botz[1];

      // check texture limits
      if (!wrapped) {
        if (max2(topz1, topz2) <= z_org-texhsc) continue;
        if (min2(botz1, botz2) >= z_org) continue;
      }

      wv[0].x = wv[1].x = seg->v1->x;
      wv[0].y = wv[1].y = seg->v1->y;
      wv[2].x = wv[3].x = seg->v2->x;
      wv[2].y = wv[3].y = seg->v2->y;

      if (wrapped) {
        wv[0].z = botz1;
        wv[1].z = topz1;
        wv[2].z = topz2;
        wv[3].z = botz2;
      } else {
        wv[0].z = max2(botz1, z_org-texhsc);
        wv[1].z = min2(topz1, z_org);
        wv[2].z = min2(topz2, z_org);
        wv[3].z = max2(botz2, z_org-texhsc);
      }

      if (doDump) for (int wf = 0; wf < 4; ++wf) GCon->Logf("   wf #%d: (%g,%g,%g)", wf, wv[wf].x, wv[wf].y, wv[wf].z);

      CreateWorldSurfFromWV(sub, seg, sp, wv, surface_t::TF_MIDDLE);
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
