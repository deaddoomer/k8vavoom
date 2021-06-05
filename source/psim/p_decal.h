//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
// decal, decalgroup, decal animator


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
  DecalFloatVal addAlpha; // alpha for additive translucency (not supported yet)
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
    , scaleX(1.0f), scaleY(1.0f), flipX(FlipNone), flipY(FlipNone), alpha(1.0f), addAlpha(0.0f)
    , fuzzy(false), fullbright(false), noWall(false), noFlat(false), bloodSplat(false), bootPrint(false)
    , flipXValue(false), flipYValue(false)
    , angleWall(0.0f), angleFlat(0.0f, 360.0f), shadeclr(-1)
    , lowername(NAME_None), bootprintname(NAME_None)
    , animator(nullptr)
    , boottime(4.0f, 8.0f), bootanimator(NAME_None)
    , bootshade(-2), boottranslation(-2), bootalpha(-1.0f)
    , bootprint(nullptr)
    , useCommonScale(false), scaleSpecial(Scale_No_Special), scaleMultiply(1.0f)
    {}
  ~VDecalDef () noexcept;

  void genValues () noexcept;

  // `genValues()` must be already called

  // calculate decal bbox, return spreading height
  void CalculateFlatBBox (const float worldx, const float worldy) noexcept;
  void CalculateWallBBox (const float worldx, const float worldy) noexcept;

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
  inline VDecalAnim () noexcept : next(nullptr), timePassed(0.0f), name(NAME_None) {}
  virtual ~VDecalAnim ();

  virtual vuint8 getTypeId () const noexcept;
  virtual const char *getTypeName () const noexcept = 0;

  inline bool isEmpty () const noexcept { return empty; }

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () = 0;

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
class VDecalAnimFader : public VDecalAnim {
public:
  enum { TypeId = TDecAnimFader };

public:
  // animator properties
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimFader (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimFader () noexcept : VDecalAnim(), startTime(0), actionTime(0) {}
  virtual ~VDecalAnimFader ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;

  virtual bool isFader () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimStretcher : public VDecalAnim {
public:
  enum { TypeId = TDecAnimStretcher };

public:
  // animator properties
  DecalFloatVal goalX, goalY;
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimStretcher (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimStretcher () noexcept : VDecalAnim(), goalX(), goalY(), startTime(0), actionTime(0) {}
  virtual ~VDecalAnimStretcher ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;
  virtual void calcMaxScales (float *sx, float *sy) const noexcept override;

  virtual bool isStretcher () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimSlider : public VDecalAnim {
public:
  enum { TypeId = TDecAnimSlider };

public:
  // animator properties
  DecalFloatVal distX, distY;
  DecalFloatVal startTime, actionTime; // in seconds
  bool k8reversey;

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimSlider (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimSlider () noexcept : VDecalAnim(), distX(), distY(), startTime(0), actionTime(0), k8reversey(false) {}
  virtual ~VDecalAnimSlider ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool isSlider () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimColorChanger : public VDecalAnim {
public:
  enum { TypeId = TDecAnimColorChanger };

public:
  // animator properties
  float dest[3];
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimColorChanger (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimColorChanger () noexcept : VDecalAnim(), startTime(0), actionTime(0) { dest[0] = dest[1] = dest[2] = 0; }
  virtual ~VDecalAnimColorChanger ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool isColorChanger () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimCombiner : public VDecalAnim {
public:
  enum { TypeId = TDecAnimCombiner };

private:
  bool mIsCloned;

protected:
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  // animator properties
  TArray<VName> nameList; // can be empty in cloned/loaded object
  TArray<VDecalAnim *> list; // can contain less items than `nameList`

protected:
  virtual void fixup () override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimCombiner (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimCombiner () noexcept : VDecalAnim(), mIsCloned(false), nameList(), list() {}
  virtual ~VDecalAnimCombiner ();

  virtual vuint8 getTypeId () const noexcept override;
  virtual const char *getTypeName () const noexcept override;

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;
  virtual void calcMaxScales (float *sx, float *sy) const noexcept override;

  virtual bool isCombiner () const noexcept override;

  virtual int getCount () const noexcept override;
  virtual VDecalAnim *getAt (int idx) noexcept override;
  virtual bool hasTypeId (vuint8 tid, int depth=0) const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
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
