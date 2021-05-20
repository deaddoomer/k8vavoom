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
  bool flipXValue, flipYValue; // valid after `genValues()`
  DecalFloatVal angleWall;
  DecalFloatVal angleFlat;
  VName lowername;
  VDecalAnim *animator; // decal animator (can be nullptr)

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
    : next(nullptr), animname(NAME_None), name(NAME_None), texid(-1)/*pic(NAME_None)*/, id(-1)
    , scaleX(1.0f), scaleY(1.0f), flipX(FlipNone), flipY(FlipNone), alpha(1.0f), addAlpha(0.0f)
    , fuzzy(false), fullbright(false), flipXValue(false), flipYValue(false)
    , angleWall(0.0f), angleFlat(0.0f, 360.0f)
    , lowername(NAME_None), animator(nullptr)
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
// base decal animator class
class VDecalAnim {
public:
  enum { TypeId = 0 };

private:
  VDecalAnim *next; // animDefHead

private:
  static void addToList (VDecalAnim *anim) noexcept;
  static void removeFromList (VDecalAnim *anim, bool deleteIt=false) noexcept;

protected:
  // working data
  float timePassed;

  virtual vuint8 getTypeId () const noexcept;
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) = 0;
  virtual void fixup ();

public:
  virtual bool parse (VScriptParser *sc) = 0;

public:
  // decaldef properties
  VName name;

protected:
  inline void copyBaseFrom (const VDecalAnim *src) noexcept { timePassed = src->timePassed; name = src->name; }

public:
  inline VDecalAnim (ENoInit) noexcept : next(nullptr), name(NAME_None) {}
  inline VDecalAnim () noexcept : next(nullptr), timePassed(0.0f), name(NAME_None) {}
  virtual ~VDecalAnim ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () = 0;

  virtual void genValues () noexcept = 0;

  // return `false` to stop continue animation; set decal alpha to 0 (or negative) to remove decal on next cleanup
  virtual bool animate (decal_t *decal, float timeDelta) = 0;

  virtual bool calcWillDisappear () const noexcept;
  virtual void calcMaxScales (float *sx, float *sy) const noexcept;

  static void SerialiseNested (VStream &strm, VNTValueIOEx &vio, VDecalAnim *&aptr);
  static void Serialise (VStream &Strm, VDecalAnim *&aptr);

public:
  static VDecalAnim *find (const char *aname) noexcept;
  static VDecalAnim *find (VStr aname) noexcept;
  static VDecalAnim *find (VName aname) noexcept;

private:
  static VDecalAnim *listHead;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimFader : public VDecalAnim {
public:
  enum { TypeId = 1 };

public:
  // animator properties
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual vuint8 getTypeId () const noexcept override;
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimFader (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimFader () noexcept : VDecalAnim(), startTime(0), actionTime(0) {}
  virtual ~VDecalAnimFader ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimStretcher : public VDecalAnim {
public:
  enum { TypeId = 2 };

public:
  // animator properties
  DecalFloatVal goalX, goalY;
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual vuint8 getTypeId () const noexcept override;
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimStretcher (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimStretcher () noexcept : VDecalAnim(), goalX(), goalY(), startTime(0), actionTime(0) {}
  virtual ~VDecalAnimStretcher ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;
  virtual void calcMaxScales (float *sx, float *sy) const noexcept override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimSlider : public VDecalAnim {
public:
  enum { TypeId = 3 };

public:
  // animator properties
  DecalFloatVal distX, distY;
  DecalFloatVal startTime, actionTime; // in seconds
  bool k8reversey;

protected:
  virtual vuint8 getTypeId () const noexcept override;
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimSlider (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimSlider () noexcept : VDecalAnim(), distX(), distY(), startTime(0), actionTime(0), k8reversey(false) {}
  virtual ~VDecalAnimSlider ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimColorChanger : public VDecalAnim {
public:
  enum { TypeId = 4 };

public:
  // animator properties
  float dest[3];
  DecalFloatVal startTime, actionTime; // in seconds

protected:
  virtual vuint8 getTypeId () const noexcept override;
  virtual void doIO (VStream &strm, VNTValueIOEx &vio) override;

public:
  virtual bool parse (VScriptParser *sc) override;

public:
  inline VDecalAnimColorChanger (ENoInit) noexcept : VDecalAnim(E_NoInit) {}
  inline VDecalAnimColorChanger () noexcept : VDecalAnim(), startTime(0), actionTime(0) { dest[0] = dest[1] = dest[2] = 0; }
  virtual ~VDecalAnimColorChanger ();

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  friend void ParseDecalDef (VScriptParser *sc);
  friend void ProcessDecalDefs ();
};


// ////////////////////////////////////////////////////////////////////////// //
class VDecalAnimCombiner : public VDecalAnim {
public:
  enum { TypeId = 5 };

private:
  bool mIsCloned;

protected:
  virtual vuint8 getTypeId () const noexcept override;
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

  // this does deep clone, so we can attach it to the actual decal object
  virtual VDecalAnim *clone () override;

  virtual void genValues () noexcept override;

  virtual bool animate (decal_t *decal, float timeDelta) override;

  virtual bool calcWillDisappear () const noexcept override;
  virtual void calcMaxScales (float *sx, float *sy) const noexcept override;

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
