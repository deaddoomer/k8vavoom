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
#include "../../gamedefs.h"
#include "loader_common.h"


//==========================================================================
//
//  VLevel::LoadVertexes
//
//==========================================================================
void VLevel::LoadVertexes (int Lump) {
  // determine number of vertexes: total lump length / vertex record length
  int NumBaseVerts = W_LumpLength(Lump)/4;
  if (NumBaseVerts <= 0) Host_Error("Map '%s' has no vertices!", *MapName);

  NumVertexes = NumBaseVerts;

  // allocate memory for vertexes
  Vertexes = new TVec[NumVertexes];
  memset((void *)Vertexes, 0, sizeof(TVec)*NumVertexes);

  // load base vertexes
  TVec *pDst;
  {
    VStream *lumpstream = W_CreateLumpReaderNum(Lump);
    VCheckedStream Strm(lumpstream);
    pDst = Vertexes;
    for (int i = 0; i < NumBaseVerts; ++i, ++pDst) {
      vint16 x, y;
      Strm << x << y;
      *pDst = TVec(x, y, 0);
    }
  }
}
