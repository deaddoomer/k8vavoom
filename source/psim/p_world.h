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
//
//  BLOCK MAP ITERATORS
//
//  For each line/thing in the given mapblock, call the passed PIT_*
//  function. If the function returns false, exit with false without checking
//  anything else.
//
//**************************************************************************


//==========================================================================
//
//  VBlockLinesIterator
//
//  The validcount flags are used to avoid checking lines that are marked in
//  multiple mapblocks, so increment validcount before creating this object.
//
//==========================================================================
class VBlockLinesIterator {
public:
  enum {
    POBJ_LINES = 1u<<0,
    POBJ_POBJ  = 1u<<1,
    POBJ_ALL = (POBJ_LINES|POBJ_POBJ),
  };
private:
  VLevel *Level;
  line_t **LinePtr;
  polyblock_t *PolyLink;
  vint32 PolySegIdx;
  vint32 *List;

public:
  VBlockLinesIterator (VLevel *Level, int x, int y, line_t **ALinePtr, unsigned pobjMode=POBJ_ALL);
  bool GetNext ();
};


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

public:
  VBlockPObjIterator (VLevel *Level, int x, int y, polyobj_t **APolyPtr);
  bool GetNext ();
};


//==========================================================================
//
//  VBlockThingsIterator
//
//==========================================================================
class VBlockThingsIterator {
private:
  VEntity *Ent;

public:
  inline VBlockThingsIterator (VLevel *Level, int x, int y) noexcept {
    if (x < 0 || x >= Level->BlockMapWidth || y < 0 || y >= Level->BlockMapHeight) {
      Ent = nullptr;
    } else {
      Ent = Level->BlockLinks[y*Level->BlockMapWidth+x];
      while (Ent && Ent->IsGoingToDie()) Ent = Ent->BlockMapNext;
    }
  }

  inline operator bool () const noexcept { return !!Ent; }
  inline void operator ++ () noexcept { if (Ent) do { Ent = Ent->BlockMapNext; } while (Ent && Ent->IsGoingToDie()); }
  inline VEntity *operator * () const noexcept { return Ent; }
  inline VEntity *operator -> () const noexcept { return Ent; }
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
  TArray<intercept_t> Intercepts;

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
  bool compat_trace;
  //bool seen3DSlopes;
  //bool seenThing;
  float max_frac;
  bool was_block_line;

  int Count;
  intercept_t *In;
  intercept_t **InPtr;

public:
  VPathTraverse (VThinker *Self, intercept_t **AInPtr, const TVec &p0, const TVec &p1, int flags);
  virtual bool GetNext () override;

private:
  void Init (VThinker *Self, const TVec &p0, const TVec &p1, int flags);
  void AddLineIntercepts (VThinker *Self, int mapx, int mapy, bool doadd, bool doopening);
  void AddThingIntercepts (VThinker *Self, int mapx, int mapy, bool doopening);
  intercept_t &NewIntercept (const float frac);
  void RemoveInterceptsAfter (const float frac); // >=

  // calculates proper thing hit and so on
  void AddProperThingHit (VEntity *th, const float frac, bool doopening);
};
