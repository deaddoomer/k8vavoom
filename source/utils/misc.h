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
#ifndef VAVOOM_MISC_HEADER
#define VAVOOM_MISC_HEADER

//#define FRandom()  ((float)(rand() & 0x7fff) / (float)0x8000)
//#define FRandomFull()  ((float)(rand() & 0x7fff) / (float)0x7fff)


// this is used to compare floats like ints which is faster
#define FASI(var_) (*(const VVA_MAYALIAS int32_t *)&(var_))
#define FASU(var_) (*(const VVA_MAYALIAS uint32_t *)&(var_))

// used to work with double-linked lists
#define DLListAppendEx(item_,head_,tail_,prevname_,nextname_)  do { \
  (item_)->prevname_ = (tail_); \
  if ((item_)->prevname_) (item_)->prevname_->nextname_ = (item_); else (head_) = (item_); \
  (item_)->nextname_ = nullptr; /* just in case */ \
  (tail_) = (item_); \
} while (0)

#define DLListAppend(item_,head_,tail_)  DLListAppendEx((item_),(head_),(tail_),prev,next)

#define DLListRemoveEx(item_,head_,tail_,prevname_,nextname_)  do { \
  if ((item_)->prevname_) (item_)->prevname_->nextname_ = (item_)->nextname_; else (head_) = (item_)->nextname_; \
  if ((item_)->nextname_) (item_)->nextname_->prevname_ = (item_)->prevname_; else (tail_) = (item_)->prevname_; \
} while (0)

#define DLListRemove(item_,head_,tail_)  DLListRemoveEx((item_),(head_),(tail_),prev,next)


// An output device.
class FOutputDevice : public VLogListener {
public:
  // FOutputDevice interface
  virtual ~FOutputDevice () noexcept;

  // simple text printing
  void Log (const char *S) noexcept;
  void Log (EName Type, const char *S) noexcept;
  void Log (VStr S) noexcept;
  void Log (EName Type, VStr S) noexcept;
  void Logf (const char *Fmt, ...) noexcept __attribute__((format(printf, 2, 3)));
  void Logf (EName Type, const char *Fmt, ...) noexcept __attribute__((format(printf, 3, 4)));
};

// error logs
//extern FOutputDevice *GLogSysError;
//extern FOutputDevice *GLogHostError;


//VVA_CHECKRESULT int ParseHex (const char *Str);
VVA_CHECKRESULT vuint32 M_LookupColorName (const char *Name); // returns 0 if not found (otherwise high bit is set)
// this returns color with high byte set to `0xff` (and black color for unknown names)
// but when `retZeroIfInvalid` is `true`, it returns `0` for unknown color
// format: ff_RR_GG_BB
VVA_CHECKRESULT vuint32 M_ParseColor (const char *Name, bool retZeroIfInvalid=false, bool showError=true);

// this also parses numeric skills
// returns -1 on error, or skill number (0-based)
VVA_CHECKRESULT int M_SkillFromName (const char *skname);

static VVA_CHECKRESULT VVA_OKUNUSED inline vuint8 M_GetColorA (vuint32 clr) noexcept { return (clr>>24)&0xff; }
static VVA_CHECKRESULT VVA_OKUNUSED inline vuint8 M_GetColorR (vuint32 clr) noexcept { return (clr>>16)&0xff; }
static VVA_CHECKRESULT VVA_OKUNUSED inline vuint8 M_GetColorG (vuint32 clr) noexcept { return (clr>>8)&0xff; }
static VVA_CHECKRESULT VVA_OKUNUSED inline vuint8 M_GetColorB (vuint32 clr) noexcept { return clr&0xff; }

static VVA_CHECKRESULT VVA_OKUNUSED inline vuint32 M_RGBA (int r, int g, int b, int a=255) noexcept {
  return
    ((vuint32)clampToByte(a)<<24)|
    ((vuint32)clampToByte(r)<<16)|
    ((vuint32)clampToByte(g)<<8)|
    ((vuint32)clampToByte(b));
}


// cached color from cvar
struct ColorCV {
private:
  VCvarS *cvar;
  VCvarF *cvarAlpha;
  vuint32 color;
  VStr oldval; // as `VStr` are COWs, comparing the same string to itself is cheap
  float fltR, fltG, fltB, fltA; // negative alpha means "no color"
  bool mNoColorAllowed;

private:
  inline void updateCache () noexcept {
    VStr nval = cvar->asStr();
    if (nval == oldval && (!cvarAlpha || cvarAlpha->asFloat() == fltA)) {
      if (mNoColorAllowed && oldval.isEmpty()) {
        fltA = -1.0f;
        fltR = fltG = fltB = 0.0f;
      }
      return;
    }
    fltA = (cvarAlpha ? clampval(cvarAlpha->asFloat(), 0.0f, 1.0f) : 1.0f);
    oldval = nval;
    color = M_ParseColor(*nval, true, false); // return 0 if invalid, don't show errors
    if (mNoColorAllowed && !color) {
      fltA = -1.0f;
      fltR = fltG = fltB = 0.0f;
    } else {
      if (!color) color = 0x7fff00; // replace invalid color with my preferred one ;-)
      color &= 0xffffffu;
      color |= ((vuint32)(fltA*255.0f))<<24;
      fltR = M_GetColorR(color)/255.0f;
      fltG = M_GetColorG(color)/255.0f;
      fltB = M_GetColorB(color)/255.0f;
    }
  }

public:
  inline ColorCV (VCvarS *acvar, VCvarF *acvarAlpha=nullptr, bool aNoColorAllowed=false) noexcept
    : cvar(acvar)
    , cvarAlpha(acvarAlpha)
    , color(0)
    , oldval(VStr::EmptyString)
    , fltR(0)
    , fltG(0)
    , fltB(0)
    , fltA((acvarAlpha ? -666.0f : 1.0f))
    , mNoColorAllowed(aNoColorAllowed)
  {
  }

  inline bool getNoColorAllowed () const noexcept { return mNoColorAllowed; }
  inline void setNoColorAllowed (const bool v) noexcept { mNoColorAllowed = v; }

  // 0 may mean "no color" if no color is allowed
  inline vuint32 getColor () noexcept { updateCache(); return color; }

  inline bool isNoColor () noexcept { updateCache(); return (fltA < 0.0f); }

  inline float getFloatA () noexcept { updateCache(); return clampval(fltA, 0.0f, 1.0f); }
  inline float getFloatR () noexcept { updateCache(); return fltR; }
  inline float getFloatG () noexcept { updateCache(); return fltG; }
  inline float getFloatB () noexcept { updateCache(); return fltB; }

  inline operator vuint32 () noexcept { return getColor(); }
};


//==========================================================================
//
//  ClipLineToRect0
//
//  Based on Cohen-Sutherland clipping algorithm but with a slightly faster
//  reject and precalculated slopes. If the speed is needed, use a hash
//  algorithm to handle the common cases.
//
//==========================================================================
#define CLTR_LEFT   (1<<0)
#define CLTR_RIGHT  (1<<1)
#define CLTR_BOTTOM (1<<2)
#define CLTR_TOP    (1<<3)

#define CLTR_DOOUTCODE(oc,mx,my) \
  (oc) = 0; \
       if ((my) < 0) (oc) |= CLTR_TOP; \
  else if ((my) >= boxh) (oc) |= CLTR_BOTTOM; \
  /* fuck you, gshitcc */ \
       if ((mx) < 0) (oc) |= CLTR_LEFT; \
  else if ((mx) >= boxw) (oc) |= CLTR_RIGHT;

static VVA_OKUNUSED inline bool ClipLineToRect0 (float *ax, float *ay, float *bx, float *by,
                                                 const float boxw, const float boxh) noexcept
{
  if (boxw < 1 || boxh < 1) return false;

  int outcode1 = 0;
  int outcode2 = 0;

  // do trivial rejects and outcodes
  if (*ay >= boxh) outcode1 = CLTR_TOP; else if (*ay < 0) outcode1 = CLTR_BOTTOM;
  if (*by >= boxh) outcode2 = CLTR_TOP; else if (*by < 0) outcode2 = CLTR_BOTTOM;
  if (outcode1&outcode2) return false; // trivially outside

  if (*ax < 0) outcode1 |= CLTR_LEFT; else if (*ax > boxw) outcode1 |= CLTR_RIGHT;
  if (*bx < 0) outcode2 |= CLTR_LEFT; else if (*bx > boxw) outcode2 |= CLTR_RIGHT;
  if (outcode1&outcode2) return false; // trivially outside

  // use doubles here for better precision
  float x0 = *ax, y0 = *ay;
  float x1 = *bx, y1 = *by;

  CLTR_DOOUTCODE(outcode1, x0, y0);
  CLTR_DOOUTCODE(outcode2, x1, y1);

  if (outcode1&outcode2) return false;

  float tmpx = 0.0, tmpy = 0.0;

  while (outcode1|outcode2) {
    // may be partially inside box
    // find an outside point
    int outside = (outcode1 ? outcode1 : outcode2);

    // clip to each side
    if (outside&CLTR_TOP) {
      const float dy = y0-y1;
      const float dx = x1-x0;
      tmpx = x0+(dx*(y0))/dy;
      tmpy = 0;
    } else if (outside&CLTR_BOTTOM) {
      const float dy = y0-y1;
      const float dx = x1-x0;
      tmpx = x0+(dx*(y0-boxh))/dy;
      tmpy = boxh-1;
    } else if (outside&CLTR_RIGHT) {
      const float dy = y1-y0;
      const float dx = x1-x0;
      tmpy = y0+(dy*(boxw-1-x0))/dx;
      tmpx = boxw-1;
    } else if (outside&CLTR_LEFT) {
      const float dy = y1-y0;
      const float dx = x1-x0;
      tmpy = y0+(dy*(-x0))/dx;
      tmpx = 0;
    }

    if (outside == outcode1) {
      x0 = tmpx;
      y0 = tmpy;
      CLTR_DOOUTCODE(outcode1, x0, y0);
    } else {
      x1 = tmpx;
      y1 = tmpy;
      CLTR_DOOUTCODE(outcode2, x1, y1);
    }

    if (outcode1&outcode2) return false; // trivially outside
  }

  *ax = x0;
  *ay = y0;
  *bx = x1;
  *by = y1;
  return true;
}

#undef CLTR_DOOUTCODE
#undef CLTR_TOP
#undef CLTR_LEFT
#undef CLTR_RIGHT
#undef CLTR_BOTTOM


// ////////////////////////////////////////////////////////////////////////// //
//** A weighted list template class
//** Copyright 1998-2006 Randy Heit
//** All rights reserved.
//**
//** Redistribution and use in source and binary forms, with or without
//** modification, are permitted provided that the following conditions
//** are met:
//**
//** 1. Redistributions of source code must retain the above copyright
//**    notice, this list of conditions and the following disclaimer.
//** 2. Redistributions in binary form must reproduce the above copyright
//**    notice, this list of conditions and the following disclaimer in the
//**    documentation and/or other materials provided with the distribution.
//** 3. The name of the author may not be used to endorse or promote products
//**    derived from this software without specific prior written permission.
//**
//** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
//** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
//** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
//** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
//** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
//** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//**---------------------------------------------------------------------------
// taken from GZDoom
template<class T> class TWeightedList {
public:
  template<class U> struct Choice {
    Choice<U> *Next;
    vuint16 Weight;
    vuint8 RandomVal; // 0 (never) - 255 (always)
    T Value;

    inline Choice (vuint16 w, U v) noexcept : Next(nullptr), Weight(w), RandomVal(0), Value(v) {}
  };

public:
  Choice<T> *Choices;

private:
  void RecalcRandomVals () noexcept;

public:
  TWeightedList (const TWeightedList &) = delete;
  void operator = (const TWeightedList &) = delete;

  inline TWeightedList () noexcept : Choices(nullptr) {}

  ~TWeightedList () noexcept {
    Choice<T> *choice = Choices;
    while (choice != nullptr) {
      Choice<T> *next = choice->Next;
      delete choice;
      choice = next;
    }
  }

  void AddEntry (T value, vuint16 weight) noexcept;
  T PickEntry () const noexcept;
  void ReplaceValues (T oldval, T newval) noexcept;
  TWeightedList<T> Clone () noexcept;
};


template<class T> TWeightedList<T> TWeightedList<T>::Clone () noexcept {
  TWeightedList<T> *res = new TWeightedList<T>();
  for (const Choice<T> *c = Choices; c; c = c->Next) {
    res->AddEntry(c->Weight, c->Value);
  }
  return res;
}


// replace all values that match oldval with newval
template<class T> void TWeightedList<T>::ReplaceValues (T oldval, T newval) noexcept {
  Choice<T> *choice;
  for (choice = Choices; choice != nullptr; choice = choice->Next) {
    if (choice->Value == oldval) choice->Value = newval;
  }
}


template<class T> void TWeightedList<T>::AddEntry (T value, vuint16 weight) noexcept {
  if (weight == 0) return; // if the weight is 0, don't bother adding it, since it will never be chosen

  Choice<T> **insAfter = &Choices;
  Choice<T> *insBefore = Choices;
  Choice<T> *theNewOne;

  while (insBefore != nullptr && insBefore->Weight < weight) {
    insAfter = &insBefore->Next;
    insBefore = insBefore->Next;
  }

  theNewOne = new Choice<T>(weight, value);
  *insAfter = theNewOne;
  theNewOne->Next = insBefore;
  RecalcRandomVals();
}


template<class T> T TWeightedList<T>::PickEntry () const noexcept {
  const vuint8 randomnum = (vuint8)P_Random(); // [0..255]
  Choice<T> *choice = Choices;
  while (choice != nullptr && randomnum > choice->RandomVal) choice = choice->Next;
  return (choice != nullptr ? choice->Value : nullptr);
}


template<class T> void TWeightedList<T>::RecalcRandomVals () noexcept {
  // redistribute the RandomVals so that they form the correct
  // distribution (as determined by the range of weights)

  if (Choices == nullptr) return; // no choices, so nothing to do

  Choice<T> *choice;

  int numChoices = 1;
  int weightSums = 0;

  for (choice = Choices; choice->Next != nullptr; choice = choice->Next) {
    ++numChoices;
    weightSums += choice->Weight;
  }

  weightSums += choice->Weight;
  choice->RandomVal = 255;  // the last choice is always randomval 255

  float randVal = 0.0f;
  float weightDenom = 1.0f/(float)weightSums;

  for (choice = Choices; choice->Next != nullptr; choice = choice->Next) {
    randVal += (float)choice->Weight*weightDenom;
    choice->RandomVal = (vuint8)(randVal*255.0f);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// i think that there's no need to use doubles there
//#define DDA_USE_DOUBLES

// this is blockmap walker
// note that due to using floating point numbers, exact corner hits may be missed.
// this is usually not a problem with Doom, because the line is registered in all
// blockmap cells it touches, so you will hardly get a geometry where missing one
// of the adjacent cells while moving through the corner will also miss some lines.
// but if you'll want to use this code in some other context, you'd better be
// aware of this fact.
// also note that this code may not work right with negative coordinates.
// higher-level blockmap walking code will make sure to never pass negatives here.
// but please note that `next()` may still return negative tile coords sometimes.
// this is not a bug, and you'd better filter bad coords in the caller.
template<unsigned tileWidth, unsigned tileHeight> struct DDALineWalker {
public:
#ifdef DDA_USE_DOUBLES
  typedef double DDAFloat;
  #define DDAfloor floor
  #define DDAfabs  fabs
#else
  typedef float DDAFloat;
  #define DDAfloor floorf
  #define DDAfabs  fabsf
#endif
public:
  // worker variables
  // use doubles here for better precision
  int currTileX, currTileY;
  int endTileX, endTileY;
  DDAFloat deltaDistX, deltaDistY; // length of ray from one x or y-side to next x or y-side
  DDAFloat sideDistX, sideDistY; // length of ray from current position to next x-side
  int stepX, stepY; // what direction to step in x/y (either +1 or -1)
  int cornerHit; // 0: no; 1: return horiz cell; 2: return vert cell; 3: do step on both dirs; 4: abort (i.e. we're done)

public:
  inline DDALineWalker () noexcept {}
  inline DDALineWalker (int x0, int y0, int x1, int y1) noexcept { start(x0, y0, x1, y1); }

  void start (int x0, int y0, int x1, int y1) noexcept {
    cornerHit = 0;

    #if 0
    // this seems to be better for negative coords, but negatives should not arrive here
    const int tileSX = int(DDAfloor(DDAFloat(x0)/DDAFloat(tileWidth)));
    const int tileSY = int(DDAfloor(DDAFloat(y0)/DDAFloat(tileHeight)));
    endTileX = int(DDAfloor(DDAFloat(x1)/DDAFloat(tileWidth)));
    endTileY = int(DDAfloor(DDAFloat(y1)/DDAFloat(tileHeight)));
    #else
    const int tileSX = x0/tileWidth;
    const int tileSY = y0/tileHeight;
    endTileX = x1/tileWidth;
    endTileY = y1/tileHeight;
    #endif

    currTileX = tileSX;
    currTileY = tileSY;

    // it is ok to waste some time here
    if (tileSX == endTileX || tileSY == endTileY) {
      if (tileSX == endTileX && tileSY == endTileY) {
        // nowhere to go (but still return the starting tile)
        stepX = stepY = 0; // this will be used as a "stop signal"
      } else if (tileSX == endTileX) {
        // vertical
        vassert(tileSY != endTileY);
        stepX = 0;
        stepY = (y0 < y1 ? 1 : -1);
      } else {
        // horizontal
        vassert(tileSY == endTileY);
        stepX = (x0 < x1 ? 1 : -1);
        stepY = 0;
      }
      // init variables to shut up the compiler
      deltaDistX = deltaDistY = sideDistX = sideDistY = 0;
      return;
    }

    // inverse length, so we can use multiply instead of division (marginally faster)
    const DDAFloat absdx = DDAFloat(1)/DDAfabs(DDAFloat(x1)-DDAFloat(x0));
    const DDAFloat absdy = DDAFloat(1)/DDAfabs(DDAFloat(y1)-DDAFloat(y0));

    if (x0 < x1) {
      stepX = 1;
      sideDistX = ((tileSX+1)*tileWidth-x0)*absdx;
    } else {
      stepX = -1;
      sideDistX = (x0-tileSX*tileWidth)*absdx;
    }

    if (y0 < y1) {
      stepY = 1;
      sideDistY = (DDAFloat)((tileSY+1)*tileHeight-y0)*absdy;
    } else {
      stepY = -1;
      sideDistY = (DDAFloat)(y0-tileSY*tileHeight)*absdy;
    }

    deltaDistX = DDAFloat(tileWidth)*absdx;
    deltaDistY = DDAFloat(tileHeight)*absdy;
  }

  // returns `false` if we're done (and the coords are undefined)
  // i.e. you can use `while (w.next(...))` loop
  inline bool next (int &tilex, int &tiley) noexcept {
    // check for a special condition
    if (cornerHit) {
      switch (cornerHit++) {
        case 1: // check adjacent horizontal tile
          if (!stepX) { ++cornerHit; goto doadjy; } // this shouldn't happen, but better play safe
          tilex = currTileX+stepX;
          tiley = currTileY;
          return true;
        case 2: // check adjacent vertical tile
         doadjy:
          if (!stepY) goto donextc;
          tilex = currTileX;
          tiley = currTileY+stepY;
          return true;
        case 3: // move to the next tile
         donextc:
          // overshoot check (see the note below)
          // this should not happen, but let's play safe
          if (currTileX == endTileX) stepX = 0;
          if (currTileY == endTileY) stepY = 0;
          // move through the corner
          sideDistX += deltaDistX;
          currTileX += stepX;
          sideDistY += deltaDistY;
          currTileY += stepY;
          // resume normal processing
          cornerHit = 0;
          // another overshoot check (see the note below)
          // this should not happen, but let's play safe
          if (currTileX == endTileX) stepX = 0;
          if (currTileY == endTileY) stepY = 0;
          break;
        default:
          return false; // no more
      }
    }

    // return current tile coordinates
    tilex = currTileX;
    tiley = currTileY;

    // note: we need to stop one of the orthogonal cell movements when we'll
    // arive at the destination horizontal or vercital cell. this is because
    // we may go past the end of the original line. it has little to do with
    // floating point inexactness, because integer-based tracer has the same
    // problem. this is not a bug in the code, but the consequence of our
    // "edge sticking" movement.

    // check if we're done (sorry for this bitop mess); checks step vars just to be sure
    if (!((currTileX^endTileX)|(currTileY^endTileY)) || !(stepX|stepY)) {
      cornerHit = 4; // no more
    } else if (stepX && stepY) {
      // jump to the next map square, either in x-direction, or in y-direction
      if (sideDistX == sideDistY) {
        // this will jump through a corner, so we have to process adjacent cells first (see the code above)
        cornerHit = 1;
      } else if (sideDistX < sideDistY) {
        // horizontal step
        sideDistX += deltaDistX;
        currTileX += stepX;
        // don't overshoot (see the note above)
        if (currTileX == endTileX) stepX = 0;
      } else {
        // vertical step
        sideDistY += deltaDistY;
        currTileY += stepY;
        // don't overshoot (see the note above)
        if (currTileY == endTileY) stepY = 0;
      }
    } else {
      // strictly orthogonal movement; don't bother with distances here
      currTileX += stepX;
      currTileY += stepY;
    }

    return true;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
// this does periodic reports for lenghty operations
template<EName T> class TimedReportBase {
private:
  const char *mName;
  const char *mMsg2;
  int mCheckDelta; // check time once per this
  int mCurrent;
  int mTotal; // can be 0
  int mCounter2;
  bool wasReport;
  double sttime;
  double lasttime;

public:
  VV_DISABLE_COPY(TimedReportBase)
  TimedReportBase () = delete;

  inline TimedReportBase (const char *aName, int aTotal=0, int aCheckDelta=128) noexcept
    : mName(aName)
    , mMsg2(nullptr)
    , mCheckDelta(aCheckDelta > 0 ? aCheckDelta : 1)
    , mCurrent(-1)
    , mTotal(aTotal > 0 ? aTotal : 0)
    , mCounter2(0)
    , wasReport(false)
  {
    sttime = lasttime = -Sys_Time();
  }

  inline TimedReportBase (const char *aName, const char *aMsg2, int aTotal=0, int aCheckDelta=128) noexcept
    : mName(aName)
    , mMsg2(aMsg2)
    , mCheckDelta(aCheckDelta > 0 ? aCheckDelta : 1)
    , mCurrent(-1)
    , mTotal(aTotal > 0 ? aTotal : 0)
    , mCounter2(0)
    , wasReport(false)
  {
    sttime = lasttime = -Sys_Time();
  }

  inline bool getWasReport () const noexcept { return wasReport; }

  inline int getCurrent () const noexcept { return mCurrent; }
  inline int getTotal () const noexcept { return mTotal; }
  inline int getCheckDelta () const noexcept { return mCheckDelta; }

  inline void setCurrent (int v) noexcept { mCurrent = v; }
  inline void setTotal (int v) noexcept { mTotal = (v > 0 ? v : 0); }
  inline void setCheckDelta (int v) noexcept { mCheckDelta = (v > 0 ? v : 1); }

  inline void setCounter2 (int v) noexcept { mCounter2 = v; }
  inline void stepCounter2 (int v=1) noexcept { mCounter2 += v; }

  inline bool getName () const noexcept { return mName; }

  inline void step (int delta=1) noexcept {
    if (delta <= 0) return;
    const int od = mCurrent/mCheckDelta;
    mCurrent += delta;
    const int nd = mCurrent/mCheckDelta;
    if (od == nd) return; // no need to check anything yet
    const double ctt = Sys_Time();
    const double tout = lasttime+ctt;
    if (tout >= 1.5) {
      // report it
      wasReport = true;
      lasttime = -ctt;
      if (mMsg2) {
        if (mTotal) {
          GLog.Logf(T, "%s %d of %d items processed (%d %s) (%d seconds)", mName, mCurrent, mTotal, mCounter2, mMsg2, (int)(sttime+ctt));
        } else {
          GLog.Logf(T, "%s %d items processed (%d %s) (%d seconds)", mName, mCurrent, mCounter2, mMsg2, (int)(sttime+ctt));
        }
      } else {
        if (mTotal) {
          GLog.Logf(T, "%s %d of %d items processed (%d seconds)", mName, mCurrent, mTotal, (int)(sttime+ctt));
        } else {
          GLog.Logf(T, "%s %d items processed (%d seconds)", mName, mCurrent, (int)(sttime+ctt));
        }
      }
    }
  }

  inline void finalReport (bool totalAlways=false) noexcept {
    if (wasReport || (totalAlways && mTotal)) {
      const double et = Sys_Time()+sttime;
      if (mMsg2) {
        GLog.Logf(T, "%s %d items processed (%d %s) (%d seconds)", mName, mCurrent, mCounter2, mMsg2, (int)et);
      } else {
        GLog.Logf(T, "%s %d items processed (%d seconds)", mName, mCurrent, (int)et);
      }
    }
  }
};

typedef TimedReportBase<NAME_Init> TimedReportInit;


// ////////////////////////////////////////////////////////////////////////// //
// poor man's profiler
class MiniStopTimer {
private:
  const char *mName;
  bool mEnabled;
  bool mActive;
  bool mCPUTime;
  double sttime;
  double accum; // accumulates time for pauses

public:
  VV_DISABLE_COPY(MiniStopTimer)
  MiniStopTimer () = delete;

  inline MiniStopTimer (const char *aName, const bool aEnabled, const bool aStartNow=true, const bool aUseCPUTime=true) noexcept
    : mName(aName)
    , mEnabled(aEnabled)
    , mActive(aEnabled)
    , mCPUTime(aUseCPUTime)
    , sttime(0.0)
    , accum(0.0)
  {
    if (aEnabled) {
      if (aStartNow) sttime = -(aUseCPUTime ? Sys_Time_CPU() : Sys_Time()); else mActive = false;
    }
  }

  inline bool getName () const noexcept { return mName; }

  inline bool isEnabled () const noexcept { return mEnabled; }
  inline bool isActive () const noexcept { return mActive; }
  inline bool isPaused () const noexcept { return !mActive; }

  inline void addTime (const double tm) noexcept { accum += tm; }

  inline void pause () noexcept {
    if (mEnabled && mActive) {
      accum += (mCPUTime ? Sys_Time_CPU() : Sys_Time())+sttime;
      mActive = false;
    }
  }

  inline void resume () noexcept {
    if (mEnabled && !mActive) {
      mActive = true;
      sttime = -(mCPUTime ? Sys_Time_CPU() : Sys_Time());
    }
  }

  inline double getCurrent () const noexcept {
    return (mEnabled ? (accum+(mActive ? (mCPUTime ? Sys_Time_CPU() : Sys_Time())+sttime : 0.0)) : 0.0);
  }

  // this does a report
  inline double stopAndReport () noexcept {
    if (!mEnabled) return 0.0;
    if (mActive) {
      accum += (mCPUTime ? Sys_Time_CPU() : Sys_Time())+sttime;
      mActive = false;
    }
    GLog.Logf(NAME_Debug, "PROF: %s time is %g seconds (%d msesc)", mName, ((int)accum == 0 ? 0.0 : accum), (int)(accum*1000.0));
    return accum;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
void ConDumpMatrix (const VMatrix4 &mat);


#endif
