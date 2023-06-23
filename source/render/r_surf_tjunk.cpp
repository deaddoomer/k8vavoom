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


//#define VV_TJUNCTION_VERBOSE

#ifdef VV_TJUNCTION_VERBOSE
static bool tjlog_verbose = false;
# define TJLOG(...)  if (tjlog_verbose && dbg_fix_tjunctions) GCon->Logf(__VA_ARGS__)
#else
# define TJLOG(...)  do {} while (0)
#endif


//==========================================================================
//
//  InsertEdgePoint
//
//  insert point into quad edge
//  returns `true` if the point was inserted
//
//==========================================================================
static void InsertEdgePoint (VRenderLevelShared *RLev, const float hz, surface_t *&surf, int vf0, int &vf1, const bool goUp, bool &wasChanged, const int linenum) {
  (void)linenum;
  TJLOG(NAME_Debug, "InsertEdgePoint: hz=%g; goUp=%d; vf0=%d; vf1=%d; vf0.z=%g; vf1.z=%g", hz, (int)goUp, vf0, vf1, surf->verts[vf0].z, surf->verts[vf1-1].z);
  // find the index to insert new vertex (before)
  int insidx = vf0;
  if (goUp) {
    // z goes up
    if (hz <= surf->verts[vf0].z) return; // too low
    if (hz >= surf->verts[vf1-1].z) return; // too high
    while (insidx < vf1 && hz > surf->verts[insidx].z) ++insidx;
  } else {
    // z goes down
    if (hz >= surf->verts[vf0].z) return; // too high
    if (hz <= surf->verts[vf1-1].z) return; // too low
    while (insidx < vf1 && hz < surf->verts[insidx].z) ++insidx;
  }

  #ifdef VV_TJUNCTION_VERBOSE
  if (!wasChanged) {
    TJLOG(NAME_Debug, "line #%d, adding points to range [%d..%d) (of %d) goUp=%d", linenum, vf0, vf1, surf->count, (int)goUp);

    TJLOG(NAME_Debug, "    found index %d [%d..%d) for hz=%g prev=(%g,%g,%g); next=(%g,%g,%g)", insidx, vf0, vf1, hz,
      (insidx > 0 ? surf->verts[insidx-1].x : -NAN), (insidx > 0 ? surf->verts[insidx-1].y : -NAN), (insidx > 0 ? surf->verts[insidx-1].z : -NAN),
      (insidx < surf->count ? surf->verts[insidx].x : -NAN), (insidx < surf->count ? surf->verts[insidx].y : -NAN), (insidx < surf->count ? surf->verts[insidx].z : -NAN));
  }
  #endif

  if (insidx >= vf1 || surf->verts[insidx].z == hz) return; // cannot insert, or duplicate point

  TJLOG(NAME_Debug, "      inserting before index %d, hz=%g", insidx, hz);
  surf = RLev->EnsureSurfacePoints(surf, surf->count+1, surf, nullptr);

  const TVec np = TVec(surf->verts[vf0].x, surf->verts[vf0].y, hz);
  surf->InsertVertexAt(insidx, np, nullptr, nullptr);

  ++vf1; // our range was increased
  wasChanged = true;
}


//==========================================================================
//
//  DumpSegParts
//
//==========================================================================
static VVA_OKUNUSED void DumpSegParts (const segpart_t *segpart) {
  while (segpart) {
    int scount = 0;
    const surface_t *surfs = segpart->surfs;
    while (surfs) { ++scount; surfs = surfs->next; }
    GCon->Logf(NAME_Debug, ":::: SPART:%08x : scount=%d ::::", (uintptr_t)segpart, scount);
    segpart = segpart->next;
  }
}


//==========================================================================
//
//  DumpDrawSegs
//
//==========================================================================
static VVA_OKUNUSED void DumpDrawSegs (const drawseg_t *drawsegs) {
  GCon->Logf(NAME_Debug, ":: DRAWSEGS:%08x ::", (uintptr_t)drawsegs);
  if (drawsegs->top) {
    GCon->Logf(NAME_Debug, "::: SPART:TOP:%08x :::", (uintptr_t)drawsegs->top);
    DumpSegParts(drawsegs->top);
  }
  if (drawsegs->mid) {
    GCon->Logf(NAME_Debug, "::: SPART:MID:%08x :::", (uintptr_t)drawsegs->mid);
    DumpSegParts(drawsegs->mid);
  }
  if (drawsegs->bot) {
    GCon->Logf(NAME_Debug, "::: SPART:BOT:%08x :::", (uintptr_t)drawsegs->bot);
    DumpSegParts(drawsegs->bot);
  }
  if (drawsegs->extra) {
    GCon->Logf(NAME_Debug, "::: SPART:EXTRA:%08x :::", (uintptr_t)drawsegs->extra);
    DumpSegParts(drawsegs->extra);
  }
}


//==========================================================================
//
//  VRenderLevelShared::FixSegTJunctions
//
//  new code, that will not create two triangles,
//  but will use centroid instead
//
//  note that we should get a single valid vertical quad here
//
//  we'll fix t-junctions here
//
//  i mean, real fixing is done when the renderer hits the subsector
//  this may be wrong, because we may hit adjacent subsectors first,
//  and only then updater will mark them for fixing.
//
//  this is not fatal, because it will be fixed on the next frame, but
//  it may be better to mark adjacents in `ChangeSector()`, for example
//
//  the text above is not true anymore, because `ChangeSector()` calls
//  `SectorModified()`, which in turn calls `MarkAdjacentTJunctions()`
//  for all sector lines. this may be excessive, but why not?
//
//  i am also using `updateWorldFrame` as validcounter, so no lines
//  will be processed twice in one frame.
//
//==========================================================================
surface_t *VRenderLevelShared::FixSegTJunctions (surface_t *surf, seg_t *seg) {
  //if (lastRenderQuality) TJLOG(NAME_Debug, "FixSegTJunctions: line #%d, seg #%d: count=%d; next=%p", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]), surf->count, surf->next);
  // wall segment should always be a quad, and it should be the only one
  if (!lastRenderQuality) return surf; // just in case
  if (seg->pobj) return surf; // do not fix polyobjects (yet?)

  const line_t *line = seg->linedef;

  #if 0
  const int lidx = (int)(ptrdiff_t)(line-&Level->Lines[0]);
  //tjlog_verbose = (lidx == 10424 || lidx == 10445 || lidx == 9599 || lidx == 10444 || lidx == 9606);
  // 9599 is the interesting line; goes from the right to the left; 9604 is the line with subdivisions
  tjlog_verbose = (lidx == 9604 || lidx == 9599 /*|| lidx == 9600*/);
  if (!tjlog_verbose) return surf;
  #endif
  //tjlog_verbose = false;

  //if (line) TJLOG(NAME_Debug, "FixSegTJunctions:000: line #%d, seg #%d: surf->count=%d; surf->next=%p", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]), surf->count, surf->next);
  const sector_t *mysec = seg->frontsector;
  if (!line || line->pobj() || !mysec || mysec->isAnyPObj()) return surf; // just in case

  // invariant, actually
  // it is called from `CreateWSurf()`, so it can't have any linked surfaces
  // the only case it can is when it was subdivided, but advanced render doesn't do any subdivisions
  if (surf->next) {
    GCon->Logf(NAME_Warning, "line #%d, seg #%d: has subdivided surfaces", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    return surf;
  }

  if (surf->count != 4) {
    TJLOG(NAME_Debug, "line #%d, seg #%d: ignored surface %p due to not being a quad (%d)", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]), surf, surf->count);
    return surf;
  }

  if (surf->isCentroidCreated()) {
    GCon->Logf(NAME_Error, "line #%d, seg #%d: ignored surface %p due to centroid presence", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]), surf);
    return surf;
  }

  // ignore paper-thin surfaces
  if (surf->verts[0].z == surf->verts[1].z &&
      surf->verts[2].z == surf->verts[3].z)
  {
    TJLOG(NAME_Debug, "line #%d, seg #%d: ignore due to being paper-thin", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    return surf;
  }

  // good wall quad should consist of two vertical lines
  if (surf->verts[0].x != surf->verts[1].x || surf->verts[0].y != surf->verts[1].y ||
      surf->verts[2].x != surf->verts[3].x || surf->verts[2].y != surf->verts[3].y)
  {
    if (warn_fix_tjunctions) GCon->Logf(NAME_Warning, "line #%d, seg #%d: bad quad (0)", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    if (warn_fix_tjunctions) {
      GCon->Logf(NAME_Debug, " (%g,%g,%g)-(%g,%g,%g)", seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
      for (int f = 0; f < surf->count; ++f) GCon->Logf(NAME_Debug, "  %d: (%g,%g,%g)", f, surf->verts[f].x, surf->verts[f].y, surf->verts[f].z);
    }
    return surf;
  }

  //GCon->Logf(NAME_Debug, "*** checking line #%d...", (int)(ptrdiff_t)(line-&Level->Lines[0]));

  float minz[2];
  float maxz[2];
  int v0idx;
       if (fabsf(surf->verts[0].x-seg->v1->x) < 0.1f && fabsf(surf->verts[0].y-seg->v1->y) < 0.1f) v0idx = 0;
  else if (fabsf(surf->verts[0].x-seg->v2->x) < 0.1f && fabsf(surf->verts[0].y-seg->v2->y) < 0.1f) v0idx = 2;
  else {
    if (warn_fix_tjunctions) GCon->Logf(NAME_Warning, "line #%d, seg #%d: bad quad (1)", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]));
    if (warn_fix_tjunctions) {
      GCon->Logf(NAME_Debug, " (%g,%g,%g)-(%g,%g,%g)", seg->v1->x, seg->v1->y, seg->v1->z, seg->v2->x, seg->v2->y, seg->v2->z);
      for (int f = 0; f < surf->count; ++f) GCon->Logf(NAME_Debug, "  %d: (%g,%g,%g)", f, surf->verts[f].x, surf->verts[f].y, surf->verts[f].z);
    }
    return surf;
  }
  minz[0] = min2(surf->verts[v0idx].z, surf->verts[v0idx+1].z);
  maxz[0] = max2(surf->verts[v0idx].z, surf->verts[v0idx+1].z);
  minz[1] = min2(surf->verts[2-v0idx].z, surf->verts[2-v0idx+1].z);
  maxz[1] = max2(surf->verts[2-v0idx].z, surf->verts[2-v0idx+1].z);

  TJLOG(NAME_Debug, "FixSegTJunctions:003: line #%d, seg #%d: minz=%g : %g; maxz=%g : %g", (int)(ptrdiff_t)(line-&Level->Lines[0]), (int)(ptrdiff_t)(seg-&Level->Segs[0]), minz[0], minz[1], maxz[0], maxz[1]);

  bool wasChanged = false;
  const int linenum = (int)(ptrdiff_t)(line-&Level->Lines[0]);

  //TJLOG(NAME_Debug, "*** minz=%g; maxz=%g", minz, maxz);
  // for each seg vertex
  for (int vidx = 0; vidx < 2; ++vidx) {
    // do not fix anything for seg vertex that doesn't touch line vertex
    // this is to avoid introducing cracks in the middle of the wall that was splitted by BSP
    // we can be absolutely sure that vertices are reused, because we're creating segs by our own nodes builder
    const TVec *segv = (vidx == 0 ? seg->v1 : seg->v2);
    int lvidx;
    if (vidx == 0) {
           if (segv == line->v1) lvidx = 0;
      else if (segv == line->v2) lvidx = 1;
      else continue;
    } else {
           if (segv == line->v1) lvidx = 0;
      else if (segv == line->v2) lvidx = 1;
      else continue;
    }
    const TVec lv = (lvidx ? *line->v2 : *line->v1);

    // collect all possible height fixes
    const int lvxCount = line->vxCount(lvidx);
    TJLOG(NAME_Debug, "FixSegTJunctions:003:   vidx=%d; lvidx=%d; lvxCount=%d", vidx, lvidx, lvxCount);
    if (!lvxCount) continue;

    // get side indices
    int vf0 = 0;
    while (vf0 < surf->count && (surf->verts[vf0].x != segv->x || surf->verts[vf0].y != segv->y)) ++vf0;
    if (vf0 >= surf->count-1) {
      GCon->Logf(NAME_Error, "SOMETHING IS WRONG (0) with a seg of line #%d!", linenum);
      continue;
    }

    int vf1 = vf0+1;
    while (vf1 < surf->count && surf->verts[vf1].x == segv->x && surf->verts[vf1].y == segv->y) ++vf1;
    if (vf1-vf0 < 2) {
      GCon->Logf(NAME_Error, "SOMETHING IS WRONG (1) with a seg of line #%d!", linenum);
      continue;
    }

    const bool goUp = (surf->verts[vf0].z <= surf->verts[vf1-1].z);
    // our edge is [vf0..vf1)

    //TJLOG(NAME_Debug, "line #%d, adding points to range [%d..%d) (of %d) goUp=%d", linenum, vf0, vf1, surf->count, (int)goUp);

    // insert points; order doesn't matter
    for (int f = 0; f < lvxCount; ++f) {
      const line_t *ln = line->vxLine(lvidx, f);
      if (ln == line) continue;
      TJLOG(NAME_Debug, "  vidx=%d; other line #%d...", vidx, (int)(ptrdiff_t)(ln-&Level->Lines[0]));

      #if 0
      if ((int)(ptrdiff_t)(ln-&Level->Lines[0]) == 9604 && ln->firstseg) {
        const seg_t *ssx = ln->firstseg;
        while (ssx) {
          if (ssx->drawsegs) {
            TJLOG("...WOW! DRAWSEGS!");
            DumpDrawSegs(ssx->drawsegs);
            //ssx = nullptr;
          } else {
            TJLOG("...SHIT! NO DRAWSEGS!");
          }
          ssx = ssx->lsnext;
        }
      }
      #endif

      for (int sn = 0; sn < 2; ++sn) {
        const sector_t *sec = (sn ? ln->backsector : ln->frontsector);
        TJLOG(NAME_Debug, "   sn=%d; mysec=%d; sec=%d", sn, (int)(ptrdiff_t)(mysec-&Level->Sectors[0]), (sec ? (int)(ptrdiff_t)(sec-&Level->Sectors[0]) : -1));
        if (sec && sec != mysec) {
          if (sn && ln->frontsector == ln->backsector) {
            // self-referenced line
            TJLOG(NAME_Debug, "    ignored self-referenced line");
            continue;
          }

          /*const*/ float fz = sec->floor.GetPointZClamped(lv);
          /*const*/ float cz = sec->ceiling.GetPointZClamped(lv);
          TJLOG(NAME_Debug, "    fz=%g; cz=%g", fz, cz);
          if (fz > cz) continue; // just in case
          if (cz <= minz[vidx] || fz >= maxz[vidx]) continue; // no need to introduce any new vertices
          //TJLOG(NAME_Debug, "  other line #%d: sec=%d; fz=%g; cz=%g", (int)(ptrdiff_t)(ln-&Level->Lines[0]), (int)(ptrdiff_t)(sec-&Level->Sectors[0]), fz, cz);
          if (fz > minz[vidx]) {
            TJLOG(NAME_Debug, "      adding fz as edge point: %g", fz);
            InsertEdgePoint(this, fz, surf, vf0, vf1, goUp, wasChanged, linenum);
          }
          if (cz != fz && cz < maxz[vidx]) {
            TJLOG(NAME_Debug, "      adding cz as edge point: %g", cz);
            InsertEdgePoint(this, cz, surf, vf0, vf1, goUp, wasChanged, linenum);
          }

          // fake floors
          if (sec->heightsec) {
            const sector_t *fsec = sec->heightsec;
            cz = fsec->ceiling.GetPointZ(lv);
            fz = fsec->floor.GetPointZ(lv);
            TJLOG(NAME_Debug, "  other line #%d: sec=%d; hsec=%d; fz=%g; cz=%g", (int)(ptrdiff_t)(ln-&Level->Lines[0]), (int)(ptrdiff_t)(sec-&Level->Sectors[0]), (int)(ptrdiff_t)(fsec-&Level->Sectors[0]), fz, cz);
            if (fz > minz[vidx]) {
              InsertEdgePoint(this, fz, surf, vf0, vf1, goUp, wasChanged, linenum);
            }
            if (cz != fz && cz < maxz[vidx]) {
              InsertEdgePoint(this, cz, surf, vf0, vf1, goUp, wasChanged, linenum);
            }
          }

          // collect 3d floors too, because we have to split textures by 3d floors for proper lighting
          if (sec->Has3DFloors()) {
            //FIXME: make this faster! `isPointInsideSolidReg()` is SLOW!
            for (sec_region_t *reg = sec->eregions->next; reg; reg = reg->next) {
              if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual|sec_region_t::RF_BaseRegion)) continue;
              if (!reg->isBlockingExtraLine()) continue;
              cz = reg->eceiling.GetPointZ(lv);
              fz = reg->efloor.GetPointZ(lv);
              if (cz < fz) continue; // invisible region
              // paper-thin regions will split planes too
              if (fz > minz[vidx] && !isPointInsideSolidReg(lv, fz, sec->eregions->next, reg)) {
                InsertEdgePoint(this, fz, surf, vf0, vf1, goUp, wasChanged, linenum);
              }
              if (cz != fz && cz < maxz[vidx] && !isPointInsideSolidReg(lv, cz, sec->eregions->next, reg)) {
                InsertEdgePoint(this, cz, surf, vf0, vf1, goUp, wasChanged, linenum);
              }
            }
          }
        }

        /* actually, the whole thing up there is not necessary, because we can
           use surfaces instead. tbh, we *NEED* to use surfaces for lightmapped
           renderer, due to subdivisions.
           the problem is that a surface can be already fixed, and contain extra
           points we are not interested in. so take only points touching the proper
           vertical side.
        */

        const seg_t *ssx = ln->firstseg;
        /*
        if ((int)(ptrdiff_t)(ln-&Level->Lines[0]) != 9604) {
          ssx = nullptr;
        } else {
          TJLOG(NAME_Debug, "### BOOO! ###");
        }
        */
        while (ssx) {
          if (ssx->drawsegs) {
            TJLOG(NAME_Debug, ":: DRAWSEGS:%08x ::", (uintptr_t)ssx->drawsegs);
            for (int dsidx = 0; dsidx < 4; ++dsidx) {
              const segpart_t *xsegpart =
                dsidx == 0 ? ssx->drawsegs->top :
                dsidx == 1 ? ssx->drawsegs->mid :
                dsidx == 2 ? ssx->drawsegs->bot :
                dsidx == 3 ? ssx->drawsegs->extra :
                nullptr;
              while (xsegpart) {
                TJLOG(NAME_Debug, "::: SPART:%08x :::", (uintptr_t)xsegpart);
                const surface_t *xsurf = xsegpart->surfs;
                while (xsurf) {
                  TJLOG(NAME_Debug, ":::: surf:%08x (count=%d) :::", (uintptr_t)xsurf, xsurf->count);
                  for (int sfvids = 0; sfvids < xsurf->count; ++sfvids) {
                    const TVec xsv = xsurf->verts[sfvids].vec();
                    TJLOG(NAME_Debug, "::::: xsv #%d:(%f,%f,%f) : lv(%f,%f) :::::",
                          sfvids, xsv.x, xsv.y, xsv.x, lv.x, lv.y);
                    // check if this vertex touches the right line vertical edge
                    // i.e. it should have x and y equal to the proper side
                    if (xsv.x == lv.x && xsv.y == lv.y) {
                      TJLOG(NAME_Debug, "*** HIT! ***");
                      InsertEdgePoint(this, xsv.z, surf, vf0, vf1, goUp, wasChanged, linenum);
                    }
                  }
                  xsurf = xsurf->next;
                }
                xsegpart = xsegpart->next;
              }
            }
          }
          ssx = ssx->lsnext;
        }
      }
    }
  }

  // create centroid if necessary
  vassert(!surf->isCentroidCreated());
  if (wasChanged && surf->count > 4) {
    surf = EnsureSurfacePoints(surf, surf->count+2, surf, nullptr);
    surf->AddCentroid();
  }

  if (wasChanged && dbg_fix_tjunctions.asBool()) GCon->Logf(NAME_Debug, "line #%d, seg #%d: fixed t-junctions", linenum, (int)(ptrdiff_t)(seg-&Level->Segs[0]));

  return surf;
}
