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
//
//  BLOCK MAP ITERATORS
//
//  For each line/thing in the given mapblock, call the passed PIT_*
//  function. If the function returns false, exit with false without checking
//  anything else.
//
//**************************************************************************
#ifndef VAVOOM_PSIM_WORLD_HEADER
#define VAVOOM_PSIM_WORLD_HEADER


//==========================================================================
//
//  VBlockPObjIterator
//
//  The validcount flags are used to avoid checking pobjs that are marked in
//  multiple mapblocks, so increment validcount before creating this object.
//
//==========================================================================
class VBlockPObjIterator {
private:
  VLevel *Level;
  polyobj_t **PolyPtr;
  polyblock_t *PolyLink;
  bool returnAll;

public:
  // if `all` is false, return only 3d pobjs
  VBlockPObjIterator (VLevel *Level, int x, int y, polyobj_t **APolyPtr, bool all=false);
  bool GetNext ();
};


//==========================================================================
//
//  VRadiusThingsIterator
//
//==========================================================================
class VRadiusThingsIterator : public VScriptIterator {
private:
  VThinker *Self;
  VEntity **EntPtr;
  VEntity *Ent;
  int x, y;
  int xl, xh;
  int yl, yh;

public:
  VRadiusThingsIterator (VThinker *ASelf, VEntity **AEntPtr, TVec Org, float Radius);
  virtual bool GetNext () override;
};


//==========================================================================
//
//  VPathTraverse
//
//  Traces a line from x1,y1 to x2,y2, calling the traverser function for
//  each. Returns true if the traverser function returns true for all lines.
//
//==========================================================================
class VPathTraverse : public VScriptIterator {
private:
  VLevel *Level;
  TPlane trace_plane;
  TVec trace_org;
  TVec trace_dest;
  TVec trace_delta;
  TVec trace_dir;
  TVec trace_org3d;
  TVec trace_dir3d; // normalised
  float trace_len;
  float trace_len3d;
  float max_frac;
  unsigned scanflags; // PT_xxx

  int poolStart, poolEnd;

  intercept_t *InPtr;
  int Count;
  int Index;

public:
  VPathTraverse (VThinker *Self, intercept_t *AInPtr, const TVec &p0, const TVec &p1, int flags,
                 vuint32 planeflags=SPF_NOBLOCKING|SPF_NOBLOCKSHOOT, vuint32 lineflags=ML_BLOCKEVERYTHING|ML_BLOCKHITSCAN);

  virtual ~VPathTraverse ();

  virtual bool GetNext () override;

private:
  void Init (VThinker *Self, const TVec &p0, const TVec &p1, int flags, vuint32 planeflags, vuint32 lineflags);
  void AddLineIntercepts (int mapx, int mapy, vuint32 planeflags, vuint32 lineflags);
  void AddThingIntercepts (int mapx, int mapy);
  intercept_t &NewIntercept (const float frac);

  inline int InterceptCount () const noexcept { return Level->CurrentPathInterceptionIndex()-poolStart; }
  inline intercept_t *GetIntercept (int idx) noexcept { return Level->GetPathIntercept(poolStart+idx); }

  inline bool NeedCheckBMCell (const int bmx, const int bmy) const noexcept {
    if (bmx >= 0 && bmy >= 0 && bmx < Level->BlockMapWidth && bmy < Level->BlockMapHeight) {
      vint32 *pp = Level->processedBMCells+(bmy*Level->BlockMapWidth+bmx);
      if (*pp == validcount) return false;
      *pp = validcount;
      return true;
    } else {
      return false;
    }
  }
};


#endif
