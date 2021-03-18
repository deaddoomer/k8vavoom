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


// ////////////////////////////////////////////////////////////////////////// //
VCvarB r_hack_transparent_doors("r_hack_transparent_doors", true, "Transparent doors hack.", CVAR_Archive);
VCvarB r_hack_fake_floor_decorations("r_hack_fake_floor_decorations", true, "Fake floor/ceiling decoration fix.", /*CVAR_Archive|*/CVAR_PreInit);

VCvarB r_fix_tjunctions("r_fix_tjunctions", true, "Fix t-junctions to avoid occasional white dots?", CVAR_Archive);
VCvarB dbg_fix_tjunctions("dbg_fix_tjunctions", false, "Show debug messages from t-junctions fixer?", CVAR_PreInit);
VCvarB warn_fix_tjunctions("warn_fix_tjunctions", false, "Show t-junction fixer warnings?", CVAR_Archive);



//==========================================================================
//
//  VRenderLevelShared::SetupFakeFloors
//
//==========================================================================
void VRenderLevelShared::SetupFakeFloors (sector_t *sector) {
  if (!sector->deepref) {
    if (sector->heightsec->SectorFlags&sector_t::SF_IgnoreHeightSec) return;
  }

  if (!sector->fakefloors) sector->fakefloors = new fakefloor_t;
  memset((void *)sector->fakefloors, 0, sizeof(fakefloor_t));
  sector->fakefloors->floorplane = sector->floor;
  sector->fakefloors->ceilplane = sector->ceiling;
  sector->fakefloors->params = sector->params;

  sector->eregions->params = &sector->fakefloors->params;
}


//==========================================================================
//
//  VRenderLevelShared::SegMoved
//
//  called when polyobject moved
//  all seg fields (incliding dir and such) must be already updated
//
//==========================================================================
void VRenderLevelShared::SegMoved (seg_t *seg) {
  if (!seg->drawsegs) return; // drawsegs not created yet
  if (!seg->linedef) return;
  if (!seg->drawsegs->mid) return; // no midsurf

  const side_t *sidedef = seg->sidedef;

  VTexture *MTex = seg->drawsegs->mid->texinfo.Tex;
  seg->drawsegs->mid->texinfo.saxisLM = seg->dir;
  seg->drawsegs->mid->texinfo.saxis = seg->dir*(TextureSScale(MTex)*sidedef->Mid.ScaleX);
  seg->drawsegs->mid->texinfo.soffs = -DotProduct(*seg->v1, seg->drawsegs->mid->texinfo.saxis)+
                                      seg->offset*(TextureSScale(MTex)*sidedef->Mid.ScaleX)+
                                      sidedef->Mid.TextureOffset*TextureOffsetSScale(MTex);

  // force update
  //if (seg->pobj) GCon->Logf(NAME_Debug, "pobj #%d seg; sectors=%d:%d", seg->pobj->index, (seg->frontsector ? (int)(ptrdiff_t)(seg->frontsector-&Level->Sectors[0]) : -1), (seg->backsector ? (int)(ptrdiff_t)(seg->backsector-&Level->Sectors[0]) : -1));
  seg->drawsegs->mid->frontTopDist += 0.346f;
  //InvalidateSegPart(seg->drawsegs->mid);
}


//==========================================================================
//
//  VRenderLevelShared::PObjModified
//
//  called when polyobject moved/rotated
//  pobj is already linked (i.e. subsector lists are valid)
//  clipsegs aren't created yet, though
//
//==========================================================================
void VRenderLevelShared::PObjModified (polyobj_t *po) {
  // here we should offset and turn flats
  for (auto &&it : po->SegFirst()) SegMoved(it.seg());
}


//==========================================================================
//
//  VRenderLevelShared::CountSegParts
//
//==========================================================================
int VRenderLevelShared::CountSegParts (const seg_t *seg) {
  if (!seg->linedef) return 0; // miniseg
  if (!seg->backsector) return 2;
  //k8: each backsector 3d floor can require segpart
  int count = 4;
  if (!seg->pobj) {
    for (const sec_region_t *reg = seg->backsector->eregions; reg; reg = reg->next) count += 1; // no:just in case, reserve two
  }
  return count;
}


//==========================================================================
//
//  VRenderLevelShared::CreateSegParts
//
//  create world/wall surfaces
//
//==========================================================================
void VRenderLevelShared::CreateSegParts (subsector_t *sub, drawseg_t *dseg, seg_t *seg, TSecPlaneRef r_floor, TSecPlaneRef r_ceiling, sec_region_t *curreg) {
  if (!seg->linedef) return; // miniseg

  /*
  #if 1
  if (seg->drawsegs) {
    GCon->Logf(NAME_Error, "seg #%d (line #%d); pobj=%p; ofs=%g; subsector #%d (sector #%d): already has drawseg #%d",
      (int)(ptrdiff_t)(seg-&Level->Segs[0]), (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), seg->pobj, seg->offset,
      (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]),
      (int)(ptrdiff_t)(seg->drawsegs-AllocatedDrawSegs));
  } else {
    GCon->Logf(NAME_Debug, "seg #%d (line #%d); pobj=%p; ofs=%g; subsector #%d (sector #%d): allocated drawseg #%d",
      (int)(ptrdiff_t)(seg-&Level->Segs[0]), (int)(ptrdiff_t)(seg->linedef-&Level->Lines[0]), seg->pobj, seg->offset,
      (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]),
      (int)(ptrdiff_t)(dseg-AllocatedDrawSegs));
  }
  #endif
  */

  vassert(!seg->drawsegs);
  dseg->seg = seg;
  seg->drawsegs = dseg;

  segpart_t *sp;

  if (!seg->backsector) {
    // one sided line
    // middle wall
    dseg->mid = SurfCreatorGetPSPart();
    sp = dseg->mid;
    sp->basereg = curreg;
    SetupOneSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);

    // sky above line
    dseg->topsky = SurfCreatorGetPSPart();
    sp = dseg->topsky;
    sp->basereg = curreg;
    if (R_IsStrictlySkyFlatPlane(r_ceiling.splane)) SetupOneSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
  } else {
    // two sided line
    const bool transDoorHack = !!(sub->sector->SectorFlags&sector_t::SF_IsTransDoor);
    //sub->sector->SectorFlags &= ~sector_t::SF_IsTransDoor; //nope, we need this flag to create proper doortracks

    // top wall
    dseg->top = SurfCreatorGetPSPart();
    sp = dseg->top;
    sp->basereg = curreg;
    SetupTwoSidedTopWSurf(sub, seg, sp, r_floor, r_ceiling);

    // sky above top
    dseg->topsky = SurfCreatorGetPSPart();
    dseg->topsky->basereg = curreg;
    if (R_IsStrictlySkyFlatPlane(r_ceiling.splane) && !R_IsStrictlySkyFlatPlane(&seg->backsector->ceiling)) {
      sp = dseg->topsky;
      SetupTwoSidedSkyWSurf(sub, seg, sp, r_floor, r_ceiling);
    }

    // bottom wall
    dseg->bot = SurfCreatorGetPSPart();
    sp = dseg->bot;
    sp->basereg = curreg;
    SetupTwoSidedBotWSurf(sub, seg, sp, r_floor, r_ceiling);

    // middle wall
    dseg->mid = SurfCreatorGetPSPart();
    sp = dseg->mid;
    sp->basereg = curreg;
    SetupTwoSidedMidWSurf(sub, seg, sp, r_floor, r_ceiling);

    // create region sides
    // this creates sides for neightbour 3d floors
    if (!seg->pobj) {
      for (sec_region_t *reg = seg->backsector->eregions->next; reg; reg = reg->next) {
        if (!reg->extraline) continue; // no need to create extra side

        // hack: 3d floor with sky texture seems to be transparent in renderer
        const side_t *sidedef = &Level->Sides[reg->extraline->sidenum[0]];
        if (sidedef->MidTexture == skyflatnum) continue;

        sp = SurfCreatorGetPSPart();
        sp->basereg = reg;
        sp->next = dseg->extra;
        dseg->extra = sp;

        SetupTwoSidedMidExtraWSurf(reg, sub, seg, sp, r_floor, r_ceiling);
      }

      if (transDoorHack != !!(sub->sector->SectorFlags&sector_t::SF_IsTransDoor)) InvalidateWholeSeg(seg);
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreateWorldSurfaces
//
//==========================================================================
void VRenderLevelShared::CreateWorldSurfaces () {
  const bool oldIWC = inWorldCreation;
  inWorldCreation = true;

  vassert(!free_wsurfs);
  SetupSky();

  // set up fake floors
  for (auto &&sec : Level->allSectors()) {
    if (sec.heightsec || sec.deepref) {
      SetupFakeFloors(&sec);
    }
  }

  if (inWorldCreation) {
    R_OSDMsgShowSecondary("CREATING WORLD SURFACES");
    R_PBarReset();
  }

  Level->ResetPObjRenderCounts(); // so we won't process polyobjects several times

  // count regions in all subsectors
  GCon->Logf(NAME_Debug, "processing %d subsectors...", Level->NumSubsectors);
  int srcount = 0;
  int dscount = 0;
  int spcount = 0;
  for (auto &&sub : Level->allSubsectors()) {
    if (sub.isOriginalPObj()) continue;
    // subregion count
    for (sec_region_t *reg = sub.sector->eregions; reg; reg = reg->next) ++srcount;
    // segpart and drawseg count
    for (int j = 0; j < sub.numlines; ++j) {
      const seg_t *seg = &Level->Segs[sub.firstline+j];
      if (!seg->linedef || seg->pobj) continue; // miniseg has no drawsegs/segparts
      dscount += 1; // one drawseg for a good seg
      // segparts (including segparts for 3d floors)
      spcount += CountSegParts(&Level->Segs[sub.firstline+j]);
    }
    // polyobjects
    if (sub.HasPObjs()) {
      for (auto &&it : sub.PObjFirst()) {
        polyobj_t *pobj = it.pobj();
        if (pobj->rendercount != 1) {
          pobj->rendercount = 1; // mark as rendered
          // create floor and ceiling for it, so they can be used to render polyobj flats
          //if (pobj->region) ++srcount; //FIXME:NOT YET
          for (auto &&sit : pobj->SegFirst()) {
            const seg_t *seg = sit.seg();
            vassert(seg->pobj == pobj);
            if (!seg->linedef) continue; // miniseg has no drawsegs/segparts
            dscount += 1; // one drawseg for a good seg
            // segparts
            spcount += CountSegParts(seg);
          }
        }
      }
    }
  }

  GCon->Logf(NAME_Debug, "%d subregions, %d drawsegs, %d segparts", srcount, dscount, spcount);

  Level->ResetPObjRenderCounts(); // cleanup 'em, 'cause they were used above

  // get some memory
  NumSegParts = spcount;
  subregion_t *sreg = new subregion_t[srcount+1];
  drawseg_t *pds = new drawseg_t[dscount+1];
  pspart = new segpart_t[spcount+1];

  pspartsLeft = spcount+1;
  int sregLeft = srcount+1;
  int pdsLeft = dscount+1;

  memset((void *)sreg, 0, sizeof(subregion_t)*sregLeft);
  memset((void *)pds, 0, sizeof(drawseg_t)*pdsLeft);
  memset((void *)pspart, 0, sizeof(segpart_t)*pspartsLeft);

  AllocatedSubRegions = sreg;
  AllocatedDrawSegs = pds;
  AllocatedSegParts = pspart;

  // create sector surfaces
  for (auto &&it : Level->allSubsectorsIdx()) {
    subsector_t *sub = it.value();
    if (sub->isOriginalPObj()) continue;
    TSecPlaneRef main_floor = sub->sector->eregions->efloor;
    TSecPlaneRef main_ceiling = sub->sector->eregions->eceiling;

    subregion_t *lastsreg = sub->regions;

    int ridx = 0;
    for (sec_region_t *reg = sub->sector->eregions; reg; reg = reg->next, ++ridx) {
      if (sregLeft == 0) Sys_Error("out of subregions in surface creator");
      if (ridx == 0) {
        if (!(reg->regflags&sec_region_t::RF_BaseRegion)) Sys_Error("internal bug in region creation (base region is not marked as base)");
      } else {
        if (reg->regflags&sec_region_t::RF_BaseRegion) Sys_Error("internal bug in region creation (non-base region is marked as base)");
      }

      TSecPlaneRef r_floor, r_ceiling;
      r_floor = reg->efloor;
      r_ceiling = reg->eceiling;

      bool skipFloor = !!(reg->regflags&sec_region_t::RF_SkipFloorSurf);
      bool skipCeil = !!(reg->regflags&sec_region_t::RF_SkipCeilSurf);

      if (ridx != 0 && reg->extraline) {
        // hack: 3d floor with sky texture seems to be transparent in renderer
        const side_t *extraside = &Level->Sides[reg->extraline->sidenum[0]];
        if (extraside->MidTexture == skyflatnum) {
          skipFloor = skipCeil = true;
        }
      }

      sreg->sub = sub;
      sreg->secregion = reg;
      sreg->floorplane = r_floor;
      sreg->ceilplane = r_ceiling;
      sreg->realfloor = (skipFloor ? nullptr : CreateSecSurface(nullptr, sub, r_floor, sreg));
      sreg->realceil = (skipCeil ? nullptr : CreateSecSurface(nullptr, sub, r_ceiling, sreg));

      // create fake floor and ceiling
      if (ridx == 0 && sub->sector->fakefloors) {
        TSecPlaneRef fakefloor, fakeceil;
        fakefloor.set(&sub->sector->fakefloors->floorplane, false);
        fakeceil.set(&sub->sector->fakefloors->ceilplane, false);
        if (!fakefloor.isFloor()) fakefloor.Flip();
        if (!fakeceil.isCeiling()) fakeceil.Flip();
        sreg->fakefloor = (skipFloor ? nullptr : CreateSecSurface(nullptr, sub, fakefloor, sreg, true));
        sreg->fakeceil = (skipCeil ? nullptr : CreateSecSurface(nullptr, sub, fakeceil, sreg, true));
      } else {
        sreg->fakefloor = nullptr;
        sreg->fakeceil = nullptr;
      }

      // drawsegs for the main subregion
      // only main region has any drawsegs
      if (ridx == 0) {
        // create segparts for subsector segs
        if (sub->numlines > 0) {
          //GCon->Logf(NAME_Debug, "*** subsector #%d (sector #%d) ***", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
          seg_t *seg = &Level->Segs[sub->firstline];
          for (int j = sub->numlines; j--; ++seg) {
            if (!seg->linedef || seg->pobj) continue; // miniseg has no drawsegs/segparts
            if (pdsLeft < 1) Sys_Error("out of drawsegs in surface creator");
            --pdsLeft;
            CreateSegParts(sub, pds, seg, main_floor, main_ceiling, reg);
            ++pds;
          }
        }

        if (sub->HasPObjs()) {
          //GCon->Logf(NAME_Debug, "*** subsector #%d (sector #%d) -- polyobjects ***", (int)(ptrdiff_t)(sub-&Level->Subsectors[0]), (int)(ptrdiff_t)(sub->sector-&Level->Sectors[0]));
          for (auto &&poit : sub->PObjFirst()) {
            polyobj_t *pobj = poit.pobj();
            if (pobj->rendercount != 1) {
              pobj->rendercount = 1; // mark as rendered
              // create floor and ceiling for it, so they can be used to render polyobj flats
              if (pobj->region) {
                pobj->region->realfloor = CreateSecSurface(nullptr, sub, r_floor, sreg);
                pobj->region->realceil = CreateSecSurface(nullptr, sub, r_ceiling, sreg);
              }
              for (auto &&sit : pobj->SegFirst()) {
                seg_t *seg = sit.seg();
                if (!seg->linedef) continue; // miniseg has no drawsegs/segparts
                if (pdsLeft < 1) Sys_Error("out of drawsegs in surface creator");
                --pdsLeft;
                CreateSegParts(sub, pds, seg, main_floor, main_ceiling, nullptr);
                ++pds;
              }
            }
          }
        }
      }

      // proper append
      sreg->next = nullptr;
      if (lastsreg) lastsreg->next = sreg; else sub->regions = sreg;
      lastsreg = sreg;

      ++sreg;
      --sregLeft;
    }

    if (inWorldCreation) R_PBarUpdate("Surfaces", it.index(), Level->NumSubsectors);
  }

  GCon->Log(NAME_Debug, "performing initial world update...");
  InitialWorldUpdate();

  if (inWorldCreation) R_PBarUpdate("Surfaces", Level->NumSubsectors, Level->NumSubsectors, true);

  inWorldCreation = oldIWC;

  Level->ResetPObjRenderCounts(); // cleanup 'em, 'cause they were used above

  GCon->Log(NAME_Debug, "surface creation complete");
}
