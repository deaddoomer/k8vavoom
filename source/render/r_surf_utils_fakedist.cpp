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
// this is directly included where necessary
//**************************************************************************


//==========================================================================
//
//  SetupFakeDistances
//
//==========================================================================
static inline void SetupFakeDistances (const seg_t *seg, segpart_t *sp) {
  if (seg->frontsector->heightsec) {
    sp->frontFakeFloorDist = seg->frontsector->heightsec->floor.dist;
    sp->frontFakeCeilDist = seg->frontsector->heightsec->ceiling.dist;
  } else {
    sp->frontFakeFloorDist = 0.0f;
    sp->frontFakeCeilDist = 0.0f;
  }

  if (seg->backsector && seg->backsector->heightsec) {
    sp->backFakeFloorDist = seg->backsector->heightsec->floor.dist;
    sp->backFakeCeilDist = seg->backsector->heightsec->ceiling.dist;
  } else {
    sp->backFakeFloorDist = 0.0f;
    sp->backFakeCeilDist = 0.0f;
  }
}
