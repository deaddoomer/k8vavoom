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
#include "../../gamedefs.h"


//==========================================================================
//
//  VLevel::LoadThings1
//
//==========================================================================
void VLevel::LoadThings1 (int Lump) {
  NumThings = W_LumpLength(Lump)/10;
  if (NumThings < 0) Host_Error("Map '%s' has invalid THINGS lump!", *MapName);
  Things = new mthing_t[NumThings+1];
  memset((void *)Things, 0, sizeof(mthing_t)*(NumThings+1));

  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  mthing_t *th = Things;
  for (int i = 0; i < NumThings; ++i, ++th) {
    vint16 x, y, angle, type, options;
    Strm << x << y << angle << type << options;

    th->x = x;
    th->y = y;
    th->angle = angle;
    th->type = type;
    th->options = options&~7;

    th->SkillClassFilter = 0xffff0000;
    if (options&1) th->SkillClassFilter |= 0x03; // skill 1 and 2 ("baby", "easy")
    if (options&2) th->SkillClassFilter |= 0x04; // skill 3 ("normal")
    if (options&4) th->SkillClassFilter |= 0x18; // skill 4 and 5 ("hard", "nightmare")
  }
}


//==========================================================================
//
//  VLevel::LoadThings2
//
//==========================================================================
void VLevel::LoadThings2 (int Lump) {
  NumThings = W_LumpLength(Lump)/20;
  if (NumThings < 0) Host_Error("Map '%s' has invalid THINGS lump!", *MapName);
  Things = new mthing_t[NumThings+1];
  memset((void *)Things, 0, sizeof(mthing_t)*(NumThings+1));

  VStream *lumpstream = W_CreateLumpReaderNum(Lump);
  VCheckedStream Strm(lumpstream);
  mthing_t *th = Things;
  for (int i = 0; i < NumThings; ++i, ++th) {
    vint16 tid, x, y, height, angle, type, options;
    vuint8 special, arg1, arg2, arg3, arg4, arg5;
    Strm << tid << x << y << height << angle << type << options
      << special << arg1 << arg2 << arg3 << arg4 << arg5;

    th->tid = tid;
    th->x = x;
    th->y = y;
    th->height = height;
    th->angle = angle;
    th->type = type;
    th->options = options&~0xe7; //k8: remove ambush flag; why?
    th->SkillClassFilter = (options&0xe0)<<11;
    if (options&1) th->SkillClassFilter |= 0x03; // skill 1 and 2 ("baby", "easy")
    if (options&2) th->SkillClassFilter |= 0x04; // skill 3 ("normal")
    if (options&4) th->SkillClassFilter |= 0x18; // skill 4 and 5 ("hard", "nightmare")
    // hack for morons doing "hexen mods" without realising that hexen map format cannot have more than 3 class flags
    // no, it won't be done in any other way
    //if ((th->SkillClassFilter&MTF2_CLASS_MASK) == 0) GCon->Logf(NAME_Debug, "*** thing with id %d has no class mask (%d,%d)", th->type, x, y);
    if ((th->SkillClassFilter&MTF2_CLASS_MASK) == (MTF2_FIGHTER|MTF2_CLERIC|MTF2_MAGE)) {
      // the thing without class mask will appear for any class, but let's play safe
      th->SkillClassFilter |= MTF2_CLASS_MASK;
      //if ((th->SkillClassFilter&MTF2_CLASS_MASK) == 0) GCon->Logf(NAME_Debug, "*** thing with id %d is for all classes", th->type);
    }
    th->special = special;
    th->args[0] = arg1;
    th->args[1] = arg2;
    th->args[2] = arg3;
    th->args[3] = arg4;
    th->args[4] = arg5;
  }
}


//==========================================================================
//
//  VLevel::SetupThingsFromMapinfo
//
//==========================================================================
void VLevel::SetupThingsFromMapinfo () {
  // use hashmap to avoid schlemiel's lookups
  TMapNC<int, mobjinfo_t *> id2nfo;
  for (int tidx = 0; tidx < NumThings; ++tidx) {
    mthing_t *th = &Things[tidx];
    if (th->type == 0) continue;
    mobjinfo_t *nfo = nullptr;
    auto fp = id2nfo.get(th->type);
    if (fp) {
      nfo = *fp;
    } else {
      nfo = VClass::FindMObjId(th->type, GGameInfo->GameFilterFlag);
      id2nfo.put(th->type, nfo);
    }
    if (nfo) {
      // allskills
      if (nfo->flags&mobjinfo_t::FlagNoSkill) {
        //GCon->Logf("*** THING %d got ALLSKILLS", th->type);
        th->SkillClassFilter |= 0x03; // skill 1 and 2 ("baby", "easy")
        th->SkillClassFilter |= 0x04; // skill 3 ("normal")
        th->SkillClassFilter |= 0x18; // skill 4 and 5 ("hard", "nightmare")
      }
      // special
      if (nfo->flags&mobjinfo_t::FlagSpecial) {
        th->special = nfo->special;
        th->args[0] = nfo->args[0];
        th->args[1] = nfo->args[1];
        th->args[2] = nfo->args[2];
        th->args[3] = nfo->args[3];
        th->args[4] = nfo->args[4];
        th->arg0str.clear();
      }
    }
  }
}
