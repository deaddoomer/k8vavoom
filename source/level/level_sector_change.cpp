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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "../psim/p_entity.h"
#ifdef CLIENT
# include "../drawer.h"  /* VRenderLevelPublic */
#endif


//**************************************************************************
//
//  SECTOR HEIGHT CHANGING
//
//  After modifying a sectors floor or ceiling height, call this routine to
//  adjust the positions of all things that touch the sector.
//  If anything doesn't fit anymore, true will be returned. If crunch is
//  true, they will take damage as they are being crushed.
//  If Crunch is false, you should set the sector height back the way it was
//  and call P_ChangeSector again to undo the changes.
//
//**************************************************************************

//==========================================================================
//
//  VLevel::ChangeOneSectorInternal
//
//  resets all caches and such
//  used to "fake open" doors and move lifts in bot pathfinder
//
//  doesn't move things (so you *HAVE* to restore sector heights)
//
//==========================================================================
void VLevel::ChangeOneSectorInternal (sector_t *sector) {
  if (!sector) return;
  CalcSecMinMaxs(sector);
}


//==========================================================================
//
//  VLevel::ResetVisitedCount
//
//==========================================================================
void VLevel::ResetVisitedCount () {
  tsVisitedCount = 1;
  for (auto &&sector : allSectors()) {
    for (msecnode_t *n = sector.TouchingThingList; n; n = n->SNext) n->Visited = 0;
  }
}


//==========================================================================
//
//  VLevel::ChangeSectorInternal
//
//  jff 3/19/98 added to just check monsters on the periphery of a moving
//  sector instead of all in bounding box of the sector. Both more accurate
//  and faster.
//
//  `-1` for `crunch` means "ignore stuck mobj"
//
//  returns `true` if movement was blocked
//
//==========================================================================
bool VLevel::ChangeSectorInternal (sector_t *sector, int crunch) {
  vassert(sector);
  const int secnum = (int)(ptrdiff_t)(sector-Sectors);
  if ((csTouched[secnum]&0x7fffffffU) == csTouchCount) return !!(csTouched[secnum]&0x80000000U);
  csTouched[secnum] = csTouchCount;

  if (sector->isOriginalPObj()) return false; // just in case

  // do not recalc bounds for inner 3d pobj sectors
  if (!sector->isInnerPObj()) {
    CalcSecMinMaxs(sector);
    // notify renderer, so it may schedule adjacent surfaces for t-junction fixing
    #ifdef CLIENT
    if (Renderer) Renderer->SectorModified(sector);
    #endif
  }

  bool ret = false;

  // killough 4/4/98: scan list front-to-back until empty or exhausted,
  // restarting from beginning after each thing is processed. Avoids
  // crashes, and is sure to examine all things in the sector, and only
  // the things which are in the sector, until a steady-state is reached.
  // Things can arbitrarily be inserted and removed and it won't mess up.
  //
  // killough 4/7/98: simplified to avoid using complicated counter
  //
  // ketmar: mostly rewritten, and reintroduced the counter

  // mark all things invalid
  //for (msecnode_t *n = sector->TouchingThingList; n; n = n->SNext) n->Visited = 0;

  if (sector->TouchingThingList) {
    const int visCount = nextVisitedCount();
    msecnode_t *n = nullptr;
    do {
      // go through list
      for (n = sector->TouchingThingList; n; n = n->SNext) {
        if (n->Visited != visCount) {
          // unprocessed thing found, mark thing as processed
          n->Visited = visCount;
          if (n->Thing->IsGoingToDie()) continue;
          // process it
          // set "some sector was moved" flag for corpse physics
          n->Thing->FlagsEx |= VEntity::EFEX_SomeSectorMoved;
          //const TVec oldOrg = n->Thing->Origin;
          //GCon->Logf("CHECKING Thing '%s'(%u) hit (zpos:%g) (sector #%d)", *n->Thing->GetClass()->GetFullName(), n->Thing->GetUniqueId(), n->Thing->Origin.z, (int)(ptrdiff_t)(sector-&Sectors[0]));
          if (!n->Thing->eventSectorChanged(crunch, sector)) {
            // doesn't fit, keep checking (crush other things)
            //GCon->Logf("Thing '%s'(%u) hit (old: %g; new: %g) (sector #%d)", *n->Thing->GetClass()->GetFullName(), n->Thing->GetUniqueId(), oldOrg.z, n->Thing->Origin.z, (int)(ptrdiff_t)(sector-&Sectors[0]));
            if (ret) csTouched[secnum] |= 0x80000000U;
            ret = true;
          }
          // exit and start over
          break;
        }
      }
    } while (n); // repeat from scratch until all things left are marked valid
  }

  // this checks the case when 3d control sector moved (3d lifts)
  for (auto it = IterControlLinks(sector); !it.isEmpty(); it.next()) {
    //GCon->Logf("*** src=%d; dest=%d", secnum, it.getDestSectorIndex());
    bool r2 = ChangeSectorInternal(it.getDestSector(), crunch);
    if (r2) { ret = true; csTouched[secnum] |= 0x80000000U; }
  }

  return ret;
}


//==========================================================================
//
//  VLevel::ChangeSector
//
//  `-1` for `crunch` means "ignore stuck mobj"
//
//  returns `true` if movement was blocked
//
//==========================================================================
bool VLevel::ChangeSector (sector_t *sector, int crunch) {
  if (!sector || NumSectors == 0) return false; // just in case
  if (sector->isOriginalPObj()) return false; // this can be done accidentally, and it doesn't do anything anyway
  if (!csTouched) {
    csTouched = (vuint32 *)Z_Calloc(NumSectors*sizeof(csTouched[0]));
    csTouchCount = 0;
  }
  if (++csTouchCount == 0x80000000U) {
    memset(csTouched, 0, NumSectors*sizeof(csTouched[0]));
    csTouchCount = 1;
  }
  IncrementSZValidCount();
  return ChangeSectorInternal(sector, crunch);
}
