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

  int Width = MapBlock(MaxX-MinX)+1;
  int Height = MapBlock(MaxY-MinY)+1;

  GCon->Logf("blockmap size: %dx%d (%d,%d)-(%d,%d)", Width, Height, MinX, MinY, MaxX, MaxY);

  // add all lines to their corresponding blocks
  // but skip zero-length lines
  TArray<int> *BlockLines = new TArray<int>[Width*Height];
  for (int i = 0; i < NumLines; ++i) {
    // determine starting and ending blocks
    const line_t &line = Lines[i];

    const TVec ldir = (*line.v2)-(*line.v1);
    double ssq = ldir.x*ldir.x+ldir.y*ldir.y;
    if (ssq < 1.0f) continue;
    ssq = sqrt(ssq);
    if (ssq < 1.0f) continue;

    int X1 = MapBlock(line.v1->x-MinX);
    int Y1 = MapBlock(line.v1->y-MinY);
    int X2 = MapBlock(line.v2->x-MinX);
    int Y2 = MapBlock(line.v2->y-MinY);

    if (X1 > X2) {
      int Tmp = X2;
      X2 = X1;
      X1 = Tmp;
    }
    if (Y1 > Y2) {
      int Tmp = Y2;
      Y2 = Y1;
      Y1 = Tmp;
    }

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
          // check if line crosses the block
          if (line.slopetype == ST_POSITIVE) {
            int p1 = line.PointOnSide(TVec(MinX+x*128, MinY+(y+1)*128, 0));
            int p2 = line.PointOnSide(TVec(MinX+(x+1)*128, MinY+y*128, 0));
            if (p1 == p2) continue;
          } else {
            int p1 = line.PointOnSide(TVec(MinX+x*128, MinY+y*128, 0));
            int p2 = line.PointOnSide(TVec(MinX+(x+1)*128, MinY+(y+1)*128, 0));
            if (p1 == p2) continue;
          }
          BlockLines[x+y*Width].Append(i);
        }
      }
    }
  }

  // build blockmap lump
  TArray<vint32> BMap;
  BMap.SetNum(4+Width*Height);
  BMap[0] = (int)MinX;
  BMap[1] = (int)MinY;
  BMap[2] = Width;
  BMap[3] = Height;
  for (int i = 0; i < Width*Height; ++i) {
    // write offset
    BMap[i+4] = BMap.Num();
    TArray<int> &Block = BlockLines[i];
    // add dummy start marker
    BMap.Append(0);
    // add lines in this block
    for (int j = 0; j < Block.Num(); ++j) {
      BMap.Append(Block[j]);
    }
    // add terminator marker
    BMap.Append(-1);
  }

  // copy data
  BlockMapLump = new vint32[BMap.Num()+1];
  BlockMapLumpSize = BMap.Num();
  if (BMap.Num()) memcpy(BlockMapLump, BMap.Ptr(), BMap.Num()*sizeof(vint32));

  delete[] BlockLines;
}
