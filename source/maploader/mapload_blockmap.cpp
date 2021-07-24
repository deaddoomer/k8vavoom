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
//  VLevel::CreateBlockMap
//
//==========================================================================
void VLevel::CreateBlockMap () {
  GCon->Logf("creating new blockmap (%d lines)...", NumLines);

  // determine bounds of the map

  /*
  float MinXflt = Vertexes[0].x;
  float MaxXflt = MinXflt;
  float MinYflt = Vertexes[0].y;
  float MaxYflt = MinYflt;
  for (int i = 0; i < NumVertexes; ++i) {
    if (MinXflt > Vertexes[i].x) MinXflt = Vertexes[i].x;
    if (MaxXflt < Vertexes[i].x) MaxXflt = Vertexes[i].x;
    if (MinYflt > Vertexes[i].y) MinYflt = Vertexes[i].y;
    if (MaxYflt < Vertexes[i].y) MaxYflt = Vertexes[i].y;
  }
  */

  float MinXflt = FLT_MAX;
  float MaxXflt = -MinXflt;
  float MinYflt = FLT_MAX;
  float MaxYflt = -MinYflt;
  for (int i = 0; i < NumLines; ++i) {
    const line_t &line = Lines[i];

    MinXflt = min2(MinXflt, line.v1->x);
    MinYflt = min2(MinYflt, line.v1->y);
    MaxXflt = max2(MaxXflt, line.v1->x);
    MaxYflt = max2(MaxYflt, line.v1->y);

    MinXflt = min2(MinXflt, line.v2->x);
    MinYflt = min2(MinYflt, line.v2->y);
    MaxXflt = max2(MaxXflt, line.v2->x);
    MaxYflt = max2(MaxYflt, line.v2->y);
  }

  // they should be integers, but just in case round them
  int MinX = floorf(MinXflt);
  int MinY = floorf(MinYflt);
  int MaxX = ceilf(MaxXflt);
  int MaxY = ceilf(MaxYflt);

  const int Width = MapBlock(MaxX-MinX)+1;
  const int Height = MapBlock(MaxY-MinY)+1;

  GCon->Logf("blockmap size: %dx%d (%d,%d)-(%d,%d)", Width, Height, MinX, MinY, MaxX, MaxY);

  // add all lines to their corresponding blocks
  // but skip zero-length lines
  TArray<int> *BlockLines = new TArray<int>[Width*Height];
  for (auto &&line : allLines()) {
    // ignore very short lines
    // this may be not right, because technically "dots" should still block the movement, but meh
    if (((*line.v2)-(*line.v1)).length2DSquared() < 1.0f) continue; // too short, ignore it

    // determine starting and ending blocks
    int X1 = MapBlock(line.v1->x-MinX);
    int Y1 = MapBlock(line.v1->y-MinY);
    int X2 = MapBlock(line.v2->x-MinX);
    int Y2 = MapBlock(line.v2->y-MinY);

    if (X1 > X2) { int tmp = X2; X2 = X1; X1 = tmp; }
    if (Y1 > Y2) { int tmp = Y2; Y2 = Y1; Y1 = tmp; }
    // just in case
    X1 = clampval(X1, 0, Width-1);
    Y1 = clampval(Y1, 0, Height-1);
    X2 = clampval(X2, 0, Width-1);
    Y2 = clampval(Y2, 0, Height-1);

    // line index
    const int i = (int)(ptrdiff_t)(&line-&Lines[0]);

    if (X1 == X2 && Y1 == Y2) {
      // line is inside a single block
      BlockLines[X1+Y1*Width].Append(i);
    } else if (Y1 == Y2) {
      // horisontal line of blocks
      for (int x = X1; x <= X2; x++) {
        BlockLines[x+Y1*Width].Append(i);
      }
    } else if (X1 == X2) {
      // vertical line of blocks
      for (int y = Y1; y <= Y2; y++) {
        BlockLines[X1+y*Width].Append(i);
      }
    } else {
      // diagonal line
      for (int x = X1; x <= X2; ++x) {
        for (int y = Y1; y <= Y2; ++y) {
          // check if the line crosses the block
          if (line.slopetype == ST_POSITIVE) {
            int p1 = line.PointOnSide(TVec(MinX+x*128, MinY+(y+1)*128, 0.0f));
            int p2 = line.PointOnSide(TVec(MinX+(x+1)*128, MinY+y*128, 0.0f));
            if (p1 == p2) continue;
          } else {
            int p1 = line.PointOnSide(TVec(MinX+x*128, MinY+y*128, 0.0f));
            int p2 = line.PointOnSide(TVec(MinX+(x+1)*128, MinY+(y+1)*128, 0.0f));
            if (p1 == p2) continue;
          }
          BlockLines[x+y*Width].Append(i);
        }
      }
    }
  }

  // build blockmap lump
  // k8vavoom extension:
  //   for each cell, we will store the center point subsector
  //   this is stored after offsets table
  //   it will be filled later
  const int tableSize = Width*Height;
  TArray<vint32> BMap;
  BMap.setLength(4+tableSize*2); // info, offsets table, subsectors table
  memset(BMap.ptr(), 0, sizeof(vint32)*BMap.length()); // just in case
  // create header
  BMap[0] = (int)MinX;
  BMap[1] = (int)MinY;
  BMap[2] = Width;
  BMap[3] = Height;
  for (int i = 0; i < tableSize; ++i) {
    // write offset
    BMap[i+4] = BMap.length();
    TArray<int> &Block = BlockLines[i];
    // add dummy start marker
    BMap.Append(0);
    // add lines in this block
    for (auto &&val : Block) BMap.Append(val);
    // add terminator marker
    BMap.Append(-1);
  }

  // copy data
  BlockMapLump = new vint32[BMap.length()];
  BlockMapLumpSize = BMap.length();
  memcpy(BlockMapLump, BMap.ptr(), BMap.length()*sizeof(vint32));

  delete[] BlockLines;
}


//==========================================================================
//
//  VLevel::CreateBlockMap
//
//  this will remove polyobject and invalid lines from blockmap
//  it will also fill blockmap subsector info
//
//==========================================================================
void VLevel::CleanupBlockMap () {
  // remove all polyobject lines from blockmap
  // we don't need 'em there, pobjs are checked with the separate blockmap anyway
  // to do this, process all blockmap blocks, and remove any lines we don't need
  if (NumPolyObjs > 0) {
    //GCon->Logf("removing polyobject lines from blockmap");
    int ofsofs = 0;
    for (int y = 0; y < BlockMapHeight; ++y) {
      for (int x = 0; x < BlockMapWidth; ++x, ++ofsofs) {
        #if 0
        int ofs = BlockMap[ofsofs]; // first item is always 0 (dunno why, prolly because vanilla did it this way)
        GCon->Logf(NAME_Debug, "blockmap cell (%d,%d); ofsofs=%d; ofs=%d)", x, y, ofsofs, ofs);
        do {
          GCon->Logf(NAME_Debug, "  %8d: %d", ofs, BlockMapLump[ofs]);
        } while (BlockMapLump[ofs++] != -1);
        #else
        int ofs = BlockMap[ofsofs]+1; // first item is always 0 (dunno why, prolly because vanilla did it this way)
        if (BlockMapLump[ofs-1] != 0) GCon->Logf(NAME_Debug, "blockmap shit at (%d,%d) == %d (ofsofs=%d; ofs=%d)", x, y, BlockMapLump[ofs-1], ofsofs, ofs);
        vassert(BlockMapLump[ofs-1] == 0);
        while (BlockMapLump[ofs] != -1) {
          const int lidx = BlockMapLump[ofs];
          vassert(lidx >= 0 && lidx < NumLines);
          const line_t *ld = &Lines[lidx];
          if (ld->pobj()) {
            //GCon->Logf(NAME_Debug, "removing line #%d from blockmap cell (%d,%d) (pobj #%d)", lidx, x, y, ld->pobj()->tag);
            for (int c = ofs; BlockMapLump[c] != -1; ++c) BlockMapLump[c] = BlockMapLump[c+1];
          } else {
            ++ofs;
          }
        }
        #endif
      }
    }
  }
}
