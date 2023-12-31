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


static VCvarB gm_compat_corpses_can_hear("gm_compat_corpses_can_hear", false, "Can corpses hear sound propagation?", CVAR_Archive);
static VCvarB gm_compat_everything_can_hear("gm_compat_everything_can_hear", false, "Can everything hear sound propagation?", CVAR_Archive);
static VCvarF gm_compat_max_hearing_distance("gm_compat_max_hearing_distance", "0", "Maximum hearing distance (0 means unlimited)?", CVAR_Archive);
static VCvarB gm_compat_better_sound_distance("gm_compat_better_sound_distance", true, "Check line distance on sound propagation?", CVAR_Archive);
static VCvarB dbg_disable_sound_alert("dbg_disable_sound_alert", false, "Disable sound alerting?", CVAR_PreInit|CVAR_NoShadow);
VCvarB gm_compat_sector_sound("gm_compat_sector_sound", false, "Use sector bbox to calculate sector sound origin (as in Vanilla)?", CVAR_Archive);


// ////////////////////////////////////////////////////////////////////////// //
// inter-sector sound propagation code
// moved here 'cause levels like Vela Pax with ~10000 interconnected sectors
// causes a huge slowdown on shooting
// will be moved back to VM when i'll implement JIT compiler

//private transient array!Entity recSoundSectorEntities; // will be collected in native code

struct SoundSectorListItem {
  sector_t *sec;
  int sblock;
};


static TArray<SoundSectorListItem> recSoundSectorList;
static TMapNC<VEntity *, bool> recSoundSectorSeenEnts;


//==========================================================================
//
//  VLevel::processSoundSector
//
//==========================================================================
void VLevel::processSoundSector (int validcount, TArrayNC<VEntity *> &elist, sector_t *sec, int soundblocks, VEntity *soundtarget, float maxdistSq, const TVec sndorigin) {
  if (!sec || sec->isOriginalPObj()) return;

  // `validcount` and other things were already checked in caller
  // also, caller already set `soundtraversed` and `SoundTarget`
  // that is, you MUST NOT check `soundtraversed` here!
  const bool distUnlim = (maxdistSq == 0.0f);

  unsigned hmask = 0/*, exmask = VEntity::EFEX_NoInteraction*/;
  if (!gm_compat_everything_can_hear) {
    hmask = VEntity::EF_NoSector|VEntity::EF_NoBlockmap;
    if (!gm_compat_corpses_can_hear) hmask |= VEntity::EF_Corpse;
  }

  for (VEntity *Ent = sec->ThingList; Ent; Ent = Ent->SNext) {
    if (Ent->IsGoingToDie()) continue;
    //GCon->Logf(NAME_Debug, "  sound at sector #%d, entity %s(%u)", (int)(ptrdiff_t)(sec-&Sectors[0]), Ent->GetClass()->GetName(), Ent->GetUniqueId());
    //FIXME: skip some entities that cannot (possibly) react
    //       this can break some code, but... meh
    //       maybe don't omit corpses?
    if ((Ent->EntityFlags&hmask)|(Ent->FlagsEx&VEntity::EFEX_NoInteraction)) continue;
    if (Ent == soundtarget) continue; // skip target
    // check max distance
    if (!distUnlim && (sndorigin-Ent->Origin).length2DSquared() > maxdistSq) continue;
    if (!recSoundSectorSeenEnts.put(Ent, true)) {
      // register for processing
      elist.append(Ent);
    }
  }

  // no need to scan 3d pobj lines
  if (sec->isInnerPObj()) return;

  line_t **slinesptr = sec->lines;
  for (int i = sec->linecount; i--; ++slinesptr) {
    const line_t *line = *slinesptr;

    // ignore one-sided lines
    if (line->sidenum[1] == -1 || !(line->flags&ML_TWOSIDED)) continue;

    // ignore self-referenced sectors
    if (!line->frontsector || line->frontsector == line->backsector) continue;

    sector_t *other = (line->frontsector == sec ? line->backsector : line->frontsector);
    if (!other) continue; // just in case
    if (other == sec || other->isAnyPObj()) continue;

    bool addIt = false;
    int sblock = 0;

    if (line->flags&ML_SOUNDBLOCK) {
      if (soundblocks == 0) {
        //RecursiveSound(other, 1, soundtarget, Splash, maxdist!optional, emmiter!optional);
        addIt = true;
        sblock = 1;
      }
    } else {
      //RecursiveSound(other, soundblocks, soundtarget, Splash, maxdist!optional, emmiter!optional);
      addIt = true;
      sblock = soundblocks;
    }

    if (addIt) {
      // don't add one sector several times
      if (other->validcount == validcount && other->soundtraversed <= sblock+1) continue; // already flooded
      // check for closed door
      opening_t *op = LineOpenings(line, *line->v1, 0xffffffff);
      while (op && op->range <= 0.0f) op = op->next;
      if (!op) {
        op = LineOpenings(line, *line->v2, 0xffffffff);
        while (op && op->range <= 0.0f) op = op->next;
        if (!op) continue; // closed door
      }
      //GCon->Logf(NAME_Debug, "  sound to sector: scount=%d; from sector=%d; sector=%d from line %d", sblock, (int)(ptrdiff_t)(sec-&Sectors[0]), (int)(ptrdiff_t)(other-&Sectors[0]), (int)(ptrdiff_t)(line-&Lines[0]));
      // if both vertices are too far, don't travel this line
      if (!distUnlim && gm_compat_better_sound_distance.asBool()) {
        if ((sndorigin-(*line->v1)).length2DSquared() > maxdistSq && (sndorigin-(*line->v2)).length2DSquared() > maxdistSq) continue;
      }
      // set flags
      other->validcount = validcount;
      other->soundtraversed = sblock+1;
      other->SoundTarget = soundtarget;
      // add to processing list
      SoundSectorListItem &sl = recSoundSectorList.alloc();
      sl.sec = other;
      sl.sblock = sblock;
    }
  }

  if (Has3DPolyObjects()) {
    // and 3d pobjs too
    //FIXME: process pobj "block sound" lines?
    for (subsector_t *sub = sec->subsectors; sub; sub = sub->seclink) {
      for (auto &&it : sub->PObjFirst()) {
        polyobj_t *po = it.pobj();
        sector_t *other = po->GetSector();
        if (other && po->validcount != validcount) {
          po->validcount = validcount;
          const int sblock = 0;
          if (other->validcount == validcount && other->soundtraversed <= sblock+1) continue;
          /*
          if (!distUnlim && gm_compat_better_sound_distance.asBool()) {
            if ((sndorigin-(*line->v1)).length2DSquared() > maxdistSq && (sndorigin-(*line->v2)).length2DSquared() > maxdistSq) continue;
          }
          */
          // set flags
          other->validcount = validcount;
          other->soundtraversed = sblock+1;
          other->SoundTarget = soundtarget;
          // add to processing list
          SoundSectorListItem &sl = recSoundSectorList.alloc();
          sl.sec = other;
          sl.sblock = sblock;
        }
      }
    }
  }
}


//==========================================================================
//
//  VLevel::doRecursiveSound
//
//  Called by NoiseAlert. Recursively traverse adjacent sectors, sound
//  blocking lines cut off traversal.
//
//==========================================================================
void VLevel::doRecursiveSound (TArrayNC<VEntity *> &elist, sector_t *sec, VEntity *soundtarget, float maxdist, const TVec sndorigin) {
  if (!sec || sec->isOriginalPObj()) return;
  IncrementValidCount();

  // wake up all monsters in this sector
  if (sec->validcount == validcount && sec->soundtraversed <= /*soundblocks*/0+1) return; // this should never pass, but...

  //GCon->Log(NAME_Debug, "=== sound propagation ===");

  if (maxdist < 0.0f) maxdist = 0.0f;
  if (gm_compat_max_hearing_distance.asFloat() > 0.0f && (maxdist == 0.0f || maxdist > gm_compat_max_hearing_distance.asFloat())) {
    maxdist = gm_compat_max_hearing_distance.asFloat();
  }
  maxdist *= maxdist; // squared

  sec->validcount = validcount;
  sec->soundtraversed = /*soundblocks*/0+1;
  sec->SoundTarget = soundtarget;

  recSoundSectorList.resetNoDtor();
  recSoundSectorSeenEnts.reset();
  processSoundSector(validcount, elist, sec, /*soundblocks*/0, soundtarget, maxdist, sndorigin);

  // don't use `foreach` here!
  int rspos = 0;
  while (rspos < recSoundSectorList.length()) {
    const SoundSectorListItem *sli = recSoundSectorList.ptr()+rspos;
    processSoundSector(validcount, elist, sli->sec, sli->sblock, soundtarget, maxdist, sndorigin);
    ++rspos;
  }

  //if (recSoundSectorList.length > 1) print("RECSOUND: len=%d", recSoundSectorList.length);
  //recSoundSectorList.resetNoDtor();
  //recSoundSectorSeenEnts.reset();
}


//native final void doRecursiveSound (ref array!Entity elist, sector_t *sec, Entity soundtarget, float maxdist, const TVec sndorigin);
IMPLEMENT_FUNCTION(VLevel, doRecursiveSound) {
  TArrayNC<VEntity *> *elist;
  sector_t *sec;
  VEntity *soundtarget;
  float maxdist;
  TVec sndorigin;
  vobjGetParamSelf(elist, sec, soundtarget, maxdist, sndorigin);
  elist->resetNoDtor();
  if (!dbg_disable_sound_alert) {
    Self->doRecursiveSound(*elist, sec, soundtarget, maxdist, sndorigin);
  }
}


//==========================================================================
//
// CalcSectorSoundOrg
//
//  Returns the perceived sound origin for a sector. If the listener is
//  inside the sector, then the origin is their location. Otherwise, the
//  origin is from the nearest wall on the sector.
//
//  `lstOrg` is the origin of the listener
//
//==========================================================================
TVec VLevel::CalcSectorSoundOrigin (const sector_t *sector, TVec lstOrg) {
  if (!sector) return lstOrg;

  // 3d pobj?
  if (sector->isInnerPObj()) return sector->ownpobj->startSpot;

  TVec res;
  if (gm_compat_sector_sound.asBool()) {
    // vanilla
    res = sector->soundorg;
  } else {
    // are we inside the sector?
    if (IsPointInSector2D(sector, lstOrg)) {
      // yes, the closest point is the one we're on
      res = lstOrg;
      #if 0
      GCon->Logf(NAME_Debug, "CalcSectorSoundOrigin: in sector #%d", (int)(ptrdiff_t)(sector-&Sectors[0]));
      #endif
    } else {
      // find the closest point on the sector's boundary lines and use
      // that as the perceived origin of the sound
      res = P_SectorClosestPoint(sector, lstOrg, nullptr/*resline*/);
      #if 0
      GCon->Logf(NAME_Debug, "CalcSectorSoundOrigin: sector #%d closest point is (%g,%g) to (%g,%g)",
                 (int)(ptrdiff_t)(sector-&Sectors[0]), res.x, res.y, lstOrg.x, lstOrg.y);
      #endif
    }
  }

  const float topZ = sector->ceiling.GetPointZClamped(lstOrg);
  const float botZ = sector->floor.GetPointZClamped(lstOrg);
  res.z = clampval(lstOrg.z, botZ, topZ);
  #if 0
  GCon->Logf(NAME_Debug, "CalcSectorSoundOrigin: sector #%d z=%g (min=%g; max=%g; org=%g)",
                 (int)(ptrdiff_t)(sector-&Sectors[0]), res.z, botZ, topZ, lstOrg.z);
  #endif
  return res;
}
