//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
// decal, decalgroup, decal animator
#ifndef VAVOOM_PSIM_DECAL_HEADER
#define VAVOOM_PSIM_DECAL_HEADER


// ////////////////////////////////////////////////////////////////////////// //
class VDecalDef;
class VDecalAnim;
class VDecalGroup;


// ////////////////////////////////////////////////////////////////////////// //
struct DecalFloatVal {
public:
  enum Type {
    T_Fixed,
    T_Random,
    T_Undefined,
  };

public:
  float value;
  int type;
  float rndMin, rndMax;

public:
  inline DecalFloatVal (ENoInit) noexcept {}

  inline DecalFloatVal () noexcept : value(0.0f), type(T_Undefined), rndMin(0.0f), rndMax(0.0f) {}
  inline DecalFloatVal (const float aval) noexcept : value(aval), type(T_Fixed), rndMin(aval), rndMax(aval) {}
  inline DecalFloatVal (const float amin, const float amax) noexcept : value(amin), type(T_Random), rndMin(amin), rndMax(amax) {}
  inline DecalFloatVal clone () noexcept;

  void genValue (float defval) noexcept;

  float getMinValue () const noexcept;
  float getMaxValue () const noexcept;

  void doIO (VStr prefix, VStream &strm, VNTValueIOEx &vio);

  VVA_CHECKRESULT inline bool isDefined () const noexcept { return (type != T_Undefined); }
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnim;
struct VDecalCloneParams {
  DecalFloatVal scaleX, scaleY;
  DecalFloatVal alpha;
  VDecalAnim *animator;
  int shadeclr;
  bool hasScaleX, hasScaleY, hasAlpha, hasAnimator, hasShade;

  inline VDecalCloneParams () noexcept
    : scaleX(1.0f), scaleY(1.0f), alpha(1.0f), animator(nullptr), shadeclr(-1)
    , hasScaleX(false), hasScaleY(false), hasAlpha(false), hasAnimator(false), hasShade(false)
  {}
};


// ////////////////////////////////////////////////////////////////////////// //
// linked list of all known decals
class VDecalDef {
public:
  enum {
    FlipNone = 0,
    FlipAlways = 1,
    FlipRandom = 2,
  };

private:
  static VDecalDef *listHead;
  static TMapNC<VName, VDecalDef *> decalNameMap; // all names are lowercased

  VDecalDef *next; // in decalDefHead
  VName animname;
  bool animoptional;

private:
  static void addToList (VDecalDef *dc) noexcept;
  static void removeFromList (VDecalDef *dc, bool deleteIt=false) noexcept;

  void fixup ();

public:
  // name is not parsed yet
  bool parse (VScriptParser *sc);

public:
  // decaldef properties
  VName name;
  //VName pic;
  int texid;
  int id;
  //float scaleX, scaleY;
  DecalFloatVal scaleX, scaleY;
  int flipX, flipY; // FlipXXX constant
  DecalFloatVal alpha; // decal alpha
  //DecalFloatVal addAlpha; // alpha for additive translucency (if zero, use `alpha`)
  bool additive;
  bool fuzzy; // draw decal with "fuzzy" effect (not supported yet)
  bool fullbright;
  bool noWall, noFlat;
  bool bloodSplat, bootPrint;
  bool flipXValue, flipYValue; // valid after `genValues()`
  DecalFloatVal angleWall;
  DecalFloatVal angleFlat;
  int shadeclr; // -1: no shade; >=0: shade rgb
  VName lowername;
  VName bootprintname;
  VDecalAnim *animator; // decal animator (can be nullptr)
  // the following values will be inited by `genValues()`
  DecalFloatVal boottime;
  VName bootanimator;
  int bootshade, boottranslation;
  float bootalpha;
  VTerrainBootprint *bootprint;
  bool hasFloorDamage;
  DecalFloatVal floorDamagePlayer; // default is true
  DecalFloatVal floorDamageMonsters; // default is false
  DecalFloatVal floorDamageSuitLeak; // default is 5
  VName floorDamageType;
  DecalFloatVal floorDamage;
  DecalFloatVal floorDamageTick; // damage each this tick

protected:
  bool useCommonScale;
  DecalFloatVal commonScale;

  enum {
    Scale_No_Special,
    ScaleX_Is_Y_Multiply,
    ScaleY_Is_X_Multiply,
  };
  int scaleSpecial;
  float scaleMultiply;

public:
  // used in various decal spreading code
  float bbox2d[4];
  float spheight; // spreading height

  inline float bbWidth () const noexcept { return bbox2d[BOX2D_MAXX]-bbox2d[BOX2D_MINX]; }
  inline float bbHeight () const noexcept { return bbox2d[BOX2D_MAXY]-bbox2d[BOX2D_MINY]; }

public:
  inline VDecalDef () noexcept
    : next(nullptr), animname(NAME_None), animoptional(false), name(NAME_None), texid(-1)/*pic(NAME_None)*/, id(-1)
    , scaleX(1.0f), scaleY(1.0f), flipX(FlipNone), flipY(FlipNone), alpha(1.0f)/*, addAlpha(0.0f)*/
    , additive(false) , fuzzy(false), fullbright(false), noWall(false), noFlat(false), bloodSplat(false), bootPrint(false)
    , flipXValue(false), flipYValue(false)
    , angleWall(0.0f), angleFlat(0.0f, 360.0f), shadeclr(-1)
    , lowername(NAME_None), bootprintname(NAME_None)
    , animator(nullptr)
    , boottime(4.0f, 8.0f), bootanimator(NAME_None)
    , bootshade(-2), boottranslation(-2), bootalpha(-1.0f)
    , bootprint(nullptr)
    , hasFloorDamage(false), floorDamagePlayer(1.0f), floorDamageMonsters(0.0f), floorDamageSuitLeak(5.0f)
    , floorDamageType(NAME_None), floorDamage(0.0f), floorDamageTick(0.0f)
    , useCommonScale(false), scaleSpecial(Scale_No_Special), scaleMultiply(1.0f)
    {}
  ~VDecalDef () noexcept;

  void genValues () noexcept;

  // `genValues()` must be already called

  // calculate decal bbox, sets spreading height
  void CalculateBBox (const float worldx, const float worldy, const float angle) noexcept;

  // used to generate maximum scale for animators
  // must be called after attaching animator (obviously)
  void genMaxScales (float *sx, float *sy) const noexcept;

public:
  static VDecalDef *find (const char *aname) noexcept;
  static VDecalDef *find (VStr aname) noexcept;
  static VDecalDef *find (VName aname) noexcept;
  static VDecalDef *findById (int id) noexcept;

  static VDecalDef *getDecal (const char *aname) noexcept;
  static VDecalDef *getDecal (VStr aname) noexcept;
  static VDecalDef *getDecal (VName aname) noexcept;
  static VDecalDef *getDecalById (int id) noexcept;

  static bool hasDecal (VName aname) noexcept;

public:
  static void parseNumOrRandom (VScriptParser *sc, DecalFloatVal *value, bool withSign=false);

private:
  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
  friend class VDecalGroup;
};


// ////////////////////////////////////////////////////////////////////////// //
// will choose a random decal
class VDecalGroup {
private:
  static VDecalGroup *listHead;
  static TMapNC<VName, VDecalGroup *> decalNameMap; // all names are lowercased

  VDecalGroup *next; // in decalDefHead

public:
  struct NameListItem {
    VName name;
    vuint16 weight;

    inline NameListItem () noexcept : name(NAME_None), weight(0) {}
    inline NameListItem (VName aname, vuint16 aweight) noexcept : name(aname), weight(aweight) {}
  };

  struct ListItem {
    VDecalDef *dd;
    VDecalGroup *dg;

    inline ListItem () noexcept : dd(nullptr), dg(nullptr) {}
    inline ListItem (VDecalDef *add, VDecalGroup *adg) noexcept : dd(add), dg(adg) {}
  };

private:
  static void addToList (VDecalGroup *dg) noexcept;
  static void removeFromList (VDecalGroup *dg, bool deleteIt=false) noexcept;

  void fixup ();

public:
  // name is not parsed yet
  bool parse (VScriptParser *sc);

public:
  // decaldef properties
  VName name;
  TArray<NameListItem> nameList; // can be empty in cloned/loaded object
  //FIXME: it can refer another decal group
  TWeightedList</*VDecalDef*/ListItem *> list; // can contain less items than `nameList`

public:
  inline VDecalGroup () noexcept : next(nullptr), name(NAME_None), nameList(), list() {}
  inline ~VDecalGroup () noexcept {}

  VDecalDef *chooseDecal (int reclevel=0) noexcept;

public:
  static VDecalGroup *find (const char *aname) noexcept;
  static VDecalGroup *find (VStr aname) noexcept;
  static VDecalGroup *find (VName aname) noexcept;

private:
  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
  friend class VDecalDef;
};


// ////////////////////////////////////////////////////////////////////////// //
enum {
  TDecAnimBase = 0,
  TDecAnimFader = 1,
  TDecAnimStretcher = 2,
  TDecAnimSlider = 3,
  TDecAnimColorChanger = 4,
  TDecAnimCombiner = 5,
};


// ////////////////////////////////////////////////////////////////////////// //
// base decal animator class
class VDecalAnim {
public:
  enum { TypeId = TDecAnimBase };

private:
  VDecalAnim *next; // animDefHead

private:
  static void addToList (VDecalAnim *anim) noexcept;
  static void removeFromList (VDecalAnim *anim, bool deleteIt=false) noexcept;

protected:
  bool empty;
  // working data
  float timePassed;

  virtual void doIO (VStream &strm, VNTValueIOEx &vio) = 0;
  virtual void fixup ();

public:
  virtual bool parse (VScriptParser *sc) = 0;

public:
  // decaldef properties
  VName name;

protected:
  inline void copyBaseFrom (const VDecalAnim *src) noexcept { empty = src->empty; timePassed = src->timePassed; name = src->name; }

public:
  inline VDecalAnim (ENoInit) noexcept : next(nullptr), empty(true), name(NAME_None) {}
  inline VDecalAnim () noexcept : next(nullptr), empty(true), timePassed(0.0f), name(NAME_None) {}
  virtual ~VDecalAnim ();

  virtual vuint8 getTypeId () const noexcept;
  virtual const char *getTypeName () const noexcept = 0;

  inline bool isEmpty () const noexcept { return empty; }

  // this does deep clone, so we can attach it to the actual decal object
  // by default, returns `nullptr` for empty animators
  virtual VDecalAnim *clone (bool forced=false) = 0;

  virtual void genValues () noexcept = 0;

  // return `false` to stop continue animation; set decal alpha to 0 (or negative) to remove decal on next cleanup
  virtual bool animate (decal_t *decal, float timeDelta) = 0;

  virtual bool calcWillDisappear () const noexcept;
  virtual void calcMaxScales (float *sx, float *sy) const noexcept;

  virtual bool isFader () const noexcept;
  virtual bool isStretcher () const noexcept;
  virtual bool isSlider () const noexcept;
  virtual bool isColorChanger () const noexcept;
  virtual bool isCombiner () const noexcept;

  // for combiners, let it be here too
  virtual int getCount () const noexcept;
  virtual VDecalAnim *getAt (int idx) noexcept;
  virtual bool hasTypeId (vuint8 tid, int depth=0) const noexcept;

  static void SerialiseNested (VStream &strm, VNTValueIOEx &vio, VDecalAnim *&aptr);
  static void Serialise (VStream &Strm, VDecalAnim *&aptr);

public:
  static VDecalAnim *find (const char *aname) noexcept;
  static VDecalAnim *find (VStr aname) noexcept;
  static VDecalAnim *find (VName aname) noexcept;

  // used by decal spawners
  static VDecalAnim *GetAnimatorByName (VName animator) noexcept;

private:
  static VDecalAnim *listHead;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
struct decal_t {
  // flags bit values
  enum {
    FlagNothingZero = 0u,
    //
    SlideFloor = 0x00001U, // curz: offset with floorz, slide with it
    SlideCeil  = 0x00002U, // curz: offset with ceilingz, slide with it
    FlipX      = 0x00010U,
    FlipY      = 0x00020U,
    Fullbright = 0x00100U,
    Fuzzy      = 0x00200U,
    Additive   = 0x00400U,
    // sides
    SideDefOne = 0x00800U,
    NoMidTex   = 0x01000U, // don't render on middle texture
    NoTopTex   = 0x02000U, // don't render on top texture
    NoBotTex   = 0x04000U, // don't render on bottom texture
    FromV2     = 0x08000U, // use `v2` vertex as base (for wall decals on partner segs)
    // special decal types
    BloodSplat = 0x10000U,
    BootPrint  = 0x20000U,
    Permanent  = 0x40000U, // will not be removed by limiter
  };

  // dcsurf bit values
  enum {
    Wall = 0u,
    Floor = 1u, // either base region floor, or 3d subregion floor
    Ceiling = 2u, // either base region ceiling, or 3d subregion ceiling
    // note that `3u` is invalid
    FakeFlag = 4u,
    SurfTypeMask = 0x03u,
  };

  decal_t *prev; // in seg/sreg/slidesec
  decal_t *next; // in seg/sreg/slidesec
  seg_t *seg; // this is non-null for wall decals
  subregion_t *sreg; // this is non-null for floor/ceiling decals (only set if renderer is active)
  vuint32 dcsurf; // decal type
  sector_t *slidesec; // backsector for SlideXXX
  subsector_t *sub; // owning subsector for floor/ceiling decal
  int eregindex; // sector region index for floor/ceiling decal (only in VLevel list)
  VDecalDef *proto; // prototype for this decal; was needed to recreate textures
  float boottime; // how long it bootprints should be emited after stepping onto this? (copied from proto)
  VName bootanimator; // NAME_None means "don't change"; 'none' means "reset"
  int bootshade; // -2: use this decal shade
  int boottranslation; // <0: use this decal translation
  float bootalpha; // <0: use current decal alpha
  VTextureID texture;
  int translation;
  int shadeclr; // -1: no shade
  int origshadeclr; // for animators
  unsigned flags;
  // z and x positions has no image offset added
  float worldx, worldy; // world coordinates for floor/ceiling decals
  float angle; // decal rotation angle (around its center point)
  float orgz; // original z position for wall decals
  float curz; // z position (offset with floor/ceiling TexZ if not midtex, see `flags`)
  float xdist; // offset from linedef start, in map units
  float ofsX, ofsY; // for animators
  float origScaleX, origScaleY; // for animators
  float scaleX, scaleY; // actual
  float origAlpha; // for animators
  float alpha; // decal alpha
  VDecalAnim *animator; // decal animator (can be nullptr)
  decal_t *prevanimated; // so we can skip static decals
  decal_t *nextanimated; // so we can skip static decals
  // for flat decals
  float bbox2d[4]; // 2d bounding box for the original (maximum) flat decal size
  /* will take it from proto
  int flatDamageTicks; // >0: do flat damage
  int flatDamageMin;
  int flatDamageMax;
  VName flatDamageType;
  */
  // in VLevel subsector decal list
  decal_t *subprev;
  decal_t *subnext;
  // in renderer region decal list
  decal_t *sregprev;
  decal_t *sregnext;

  /*
  unsigned uid; // unique id, so we can process all clones
  // linked list of all decals with the given uid
  decal_t *uidprev;
  decal_t *uidnext;
  */

  // cache
  unsigned lastFlags;
  float lastScaleX, lastScaleY;
  float lastWorldX, lastWorldY;
  float lastAngle;
  float lastOfsX, lastOfsY;
  float lastCurZ;
  float lastPlaneDist, lastTexZ;
  // calculated cached values
  TVec saxis, taxis;
  float soffs, toffs;
  TVec v1, v2, v3, v4;

  inline bool isAdditive () const noexcept { return (flags&Additive); }
  inline bool isPermanent () const noexcept { return (flags&Permanent); }

  // nore that floor/ceiling type should be correctly set for 3d floor subregions
  // i.e. decal on top of 3d floor is ceiling decal

  inline bool isWall () const noexcept { return (dcsurf == Wall); }
  inline bool isNonWall () const noexcept { return ((dcsurf&SurfTypeMask) != Wall); }
  inline bool isFloor () const noexcept { return ((dcsurf&SurfTypeMask) == Floor); }
  inline bool isCeiling () const noexcept { return ((dcsurf&SurfTypeMask) == Ceiling); }
  // for fake floor/ceiling? (not implemented yet)
  inline bool isFake () const noexcept { return (dcsurf&FakeFlag); }

  inline bool isFloorBloodSplat () const noexcept { return (((dcsurf&SurfTypeMask) == Floor) && (flags&BloodSplat)); }

  // WARNING! call this ONLY for known non-wall decals!
  // non-base regions has their floor and ceiling switched
  // so floor decal for 3d floor region is actually ceiling decal, and vice versa
  //inline bool isFloorEx () const noexcept { return (eregindex ? isCeiling() : isFloor()); }

  // should be called after animator was set
  // might be called by animation thinker too
  void calculateBBox () noexcept;

  inline bool needRecalc (const float pdist, const float tz) const noexcept {
    return
      lastFlags != flags ||
      FASI(lastPlaneDist) != FASI(pdist) ||
      FASI(lastTexZ) != tz ||
      FASI(lastScaleX) != FASI(scaleX) ||
      FASI(lastScaleY) != FASI(scaleY) ||
      FASI(lastWorldX) != FASI(worldx) ||
      FASI(lastWorldY) != FASI(worldy) ||
      FASI(lastOfsX) != FASI(ofsX) ||
      FASI(lastOfsY) != FASI(ofsY) ||
      FASI(lastCurZ) != FASI(curz) ||
      FASI(lastAngle) != FASI(angle);
  }

  // doesn't update axes and offsets
  inline void updateCache (const float pdist, const float tz) noexcept {
    lastFlags = flags;
    lastPlaneDist = pdist;
    lastTexZ = tz;
    lastScaleX = scaleX;
    lastScaleY = scaleY;
    lastWorldX = worldx;
    lastWorldY = worldy;
    lastOfsX = ofsX;
    lastOfsY = ofsY;
    lastCurZ = curz;
    lastAngle = angle;
  }

  inline void invalidateCache () noexcept { lastPlaneDist = FLT_MAX; } // arbitrary number
};


// ////////////////////////////////////////////////////////////////////////// //
// used to calculate decal bboxes
// returns vertical spread height
// zero means "no texture found or invalid arguments" (bbox2d is zeroed too)
float CalculateTextureBBox (float bbox2d[4], int texid,
                            const float worldx, const float worldy, const float angle,
                            const float scaleX, const float scaleY) noexcept;

void ParseDecalDef (VScriptParser *sc);

void ProcessDecalDefs ();

extern VDecalAnim *DummyDecalAnimator; // used to set empty decal animator


#endif
