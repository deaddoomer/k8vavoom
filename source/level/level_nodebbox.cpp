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


//==========================================================================
//
//  VLevel::CalcSkyHeight
//
//==========================================================================
float VLevel::CalcSkyHeight () const {
  if (NumSectors == 0) return 0.0f; // just in case
  // calculate sky height
  float skyheight = -99999.0f;
  for (unsigned i = 0; i < (unsigned)NumSectors; ++i) {
    if (Sectors[i].ceiling.pic == skyflatnum &&
        Sectors[i].ceiling.maxz > skyheight)
    {
      skyheight = Sectors[i].ceiling.maxz;
    }
  }
  // make it a bit higher to avoid clipping of the sprites
  skyheight += 8*1024;
  return skyheight;
}


//==========================================================================
//
//  VLevel::CalcSecMinMaxs
//
//==========================================================================
void VLevel::CalcSecMinMaxs (sector_t *sector) {
  if (!sector) return; // k8: just in case

  enum { SlopedFloor = 1u, SlopedCeiling = 2u };

  unsigned slopedFC = 0;

  if (sector->floor.normal.z == 1.0f || sector->linecount == 0) {
    // horizontal floor
    sector->floor.minz = sector->floor.dist;
    sector->floor.maxz = sector->floor.dist;
  } else {
    // sloped floor
    slopedFC |= SlopedFloor;
  }

  if (sector->ceiling.normal.z == -1.0f || sector->linecount == 0) {
    // horisontal ceiling
    sector->ceiling.minz = -sector->ceiling.dist;
    sector->ceiling.maxz = -sector->ceiling.dist;
  } else {
    // sloped ceiling
    slopedFC |= SlopedCeiling;
  }

  // calculate extents for sloped flats
  if (slopedFC) {
    float minzf = +99999.0f;
    float maxzf = -99999.0f;
    float minzc = +99999.0f;
    float maxzc = -99999.0f;
    line_t **llist = sector->lines;
    for (int cnt = sector->linecount; cnt--; ++llist) {
      line_t *ld = *llist;
      if (slopedFC&SlopedFloor) {
        float z = sector->floor.GetPointZ(*ld->v1);
        minzf = min2(minzf, z);
        maxzf = max2(maxzf, z);
        z = sector->floor.GetPointZ(*ld->v2);
        minzf = min2(minzf, z);
        maxzf = max2(maxzf, z);
      }
      if (slopedFC&SlopedCeiling) {
        float z = sector->ceiling.GetPointZ(*ld->v1);
        minzc = min2(minzc, z);
        maxzc = max2(maxzc, z);
        z = sector->ceiling.GetPointZ(*ld->v2);
        minzc = min2(minzc, z);
        maxzc = max2(maxzc, z);
      }
    }
    if (slopedFC&SlopedFloor) {
      sector->floor.minz = minzf;
      sector->floor.maxz = maxzf;
    }
    if (slopedFC&SlopedCeiling) {
      sector->ceiling.minz = minzc;
      sector->ceiling.maxz = maxzc;
    }
  }
}
