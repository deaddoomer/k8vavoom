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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
//**
//**  INTERNAL DATA TYPES
//**  used by play and refresh
//**
//**************************************************************************
#include "textures/r_tex_id.h"


class VRenderLevelPublic;
class VTextureTranslation;
class VAcsLevel;
class VNetContext;

struct sector_t;
struct mapInfo_t;
struct fakefloor_t;
struct seg_t;
struct subsector_t;
struct node_t;
struct surface_t;
struct segpart_t;
struct drawseg_t;
struct sec_surface_t;
struct subregion_t;

struct decal_t;

class VThinker;
class VLevelInfo;
class VEntity;
class VBasePlayer;
class VWorldInfo;
class VGameInfo;


// line specials that are used by the loader
enum {
  LNSPEC_PolyStartLine        = 1,
  LNSPEC_PolyExplicitLine     = 5,
  LNSPEC_LineHorizon          = 9,
  LNSPEC_DoorLockedRaise      = 13,
  LNSPEC_ACSLockedExecute     = 83,
  LNSPEC_ACSLockedExecuteDoor = 85,
  LNSPEC_LineMirror           = 182,
  LNSPEC_StaticInit           = 190,
  LNSPEC_LineTranslucent      = 208,
  LNSPEC_TransferHeights      = 209,

  LNSPEC_ScrollTextureLeft = 100,// 100
  LNSPEC_ScrollTextureRight,
  LNSPEC_ScrollTextureUp,
  LNSPEC_ScrollTextureDown,

  LNSPEC_PlaneCopy = 118,
  LNSPEC_PlaneAlign = 181,

  /*
  LNSPEC_FloorLowerToNearest = 22,
  LNSPEC_FloorLowerToLowestChange = 241,
  LNSPEC_FloorLowerToHighest = 242,
  LNSPEC_ElevatorLowerToNearest = 247,
  */
};


//==========================================================================
//
//  Vertex
//
//==========================================================================

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
// like some DOOM-alikes ("wt", "WebView") did.
typedef TVec vertex_t;


//==========================================================================
//
//  DrawSeg
//
//  TODO: document this!
//
//==========================================================================
struct drawseg_t {
  seg_t *seg;
  drawseg_t *next;

  segpart_t *top;
  segpart_t *mid;
  segpart_t *bot;
  segpart_t *topsky;
  segpart_t *extra;

  surface_t *HorizonTop;
  surface_t *HorizonBot;
};


//==========================================================================
//
//  LineDef
//
//==========================================================================

// move clipping aid for LineDefs
enum {
  ST_HORIZONTAL,
  ST_VERTICAL,
  ST_POSITIVE,
  ST_NEGATIVE,
};

// If a texture is pegged, the texture will have the end exposed to air held
// constant at the top or bottom of the texture (stairs or pulled down
// things) and will move with a height change of one of the neighbor sectors.
// Unpegged textures allways have the first row of the texture at the top
// pixel of the line for both top and bottom textures (use next to windows).

// LineDef attributes
enum {
  ML_BLOCKING            = 0x00000001, // solid, is an obstacle
  ML_BLOCKMONSTERS       = 0x00000002, // blocks monsters only
  ML_TWOSIDED            = 0x00000004, // backside will not be present at all
  ML_DONTPEGTOP          = 0x00000008, // upper texture unpegged
  ML_DONTPEGBOTTOM       = 0x00000010, // lower texture unpegged
  ML_SECRET              = 0x00000020, // don't map as two sided: IT'S A SECRET!
  ML_SOUNDBLOCK          = 0x00000040, // don't let sound cross two of these
  ML_DONTDRAW            = 0x00000080, // don't draw on the automap
  ML_MAPPED              = 0x00000100, // set if already drawn in automap
  ML_REPEAT_SPECIAL      = 0x00000200, // special is repeatable
  ML_ADDITIVE            = 0x00000400, // additive translucency
  ML_MONSTERSCANACTIVATE = 0x00002000, // monsters (as well as players) can activate the line
  ML_BLOCKPLAYERS        = 0x00004000, // blocks players only
  ML_BLOCKEVERYTHING     = 0x00008000, // line blocks everything
  ML_ZONEBOUNDARY        = 0x00010000, // boundary of reverb zones
  ML_RAILING             = 0x00020000,
  ML_BLOCK_FLOATERS      = 0x00040000,
  ML_CLIP_MIDTEX         = 0x00080000, // automatic for every Strife line
  ML_WRAP_MIDTEX         = 0x00100000,
  ML_3DMIDTEX            = 0x00200000, // not implemented
  ML_CHECKSWITCHRANGE    = 0x00400000, // not implemented
  ML_FIRSTSIDEONLY       = 0x00800000, // actiavte only when crossed from front side
  ML_BLOCKPROJECTILE     = 0x01000000,
  ML_BLOCKUSE            = 0x02000000, // blocks all use actions through this line
  ML_BLOCKSIGHT          = 0x04000000, // blocks monster line of sight
  ML_BLOCKHITSCAN        = 0x08000000, // blocks hitscan attacks
  ML_3DMIDTEX_IMPASS     = 0x10000000, // (not implemented) [TP] if 3D midtex, behaves like a height-restricted ML_BLOCKING
  ML_KEEPDATA            = 0x20000000, // keep FloorData or CeilingData after activating them
                                       // used to simulate original Heretic behaviour
  ML_NODECAL             = 0x40000000, // don't spawn decals on this linedef
};

enum {
  ML_SPAC_SHIFT = 10,
  ML_SPAC_MASK = 0x00001c00,
};

// extra flags
enum {
  ML_EX_PARTIALLY_MAPPED = 0x00000001, // some segs are visible, but not all
  ML_EX_CHECK_MAPPED     = 0x00000002, // check if all segs are mapped (done in automap drawer
};

// seg flags
enum {
  SF_MAPPED = 0x00000001, // some segs are visible, but not all
};

// Special activation types
enum {
  SPAC_Cross      = 0x0001, // when player crosses line
  SPAC_Use        = 0x0002, // when player uses line
  SPAC_MCross     = 0x0004, // when monster crosses line
  SPAC_Impact     = 0x0008, // when projectile hits line
  SPAC_Push       = 0x0010, // when player pushes line
  SPAC_PCross     = 0x0020, // when projectile crosses line
  SPAC_UseThrough = 0x0040, // SPAC_USE, but passes it through
  //  SPAC_PTouch is remapped as SPAC_Impact | SPAC_PCross
  SPAC_AnyCross   = 0x0080,
  SPAC_MUse       = 0x0100, // when monster uses line
  SPAC_MPush      = 0x0200, // when monster pushes line
  SPAC_UseBack    = 0x0400, // can be used from the backside
};


struct line_t : public TPlane {
  // vertices, from v1 to v2
  vertex_t *v1;
  vertex_t *v2;

  // precalculated v2-v1 for side checking
  TVec dir;

  vint32 flags;
  vint32 SpacFlags;
  vint32 exFlags; //ML_EX_xxx

  // visual appearance: SideDefs
  // sidenum[1] will be -1 if one sided
  vint32 sidenum[2];

  // neat. another bounding box, for the extent of the LineDef
  float bbox[4];

  // to aid move clipping
  vint32 slopetype;

  // front and back sector
  // note: redundant? can be retrieved from SideDefs
  sector_t *frontsector;
  sector_t *backsector;

  // if == validcount, already checked (used in various traversing, like LOS, and other line tracing)
  vint32 validcount;

  float alpha;

  vint32 special;
  vint32 arg1;
  vint32 arg2;
  vint32 arg3;
  vint32 arg4;
  vint32 arg5;

  vint32 LineTag;
  vint32 HashFirst;
  vint32 HashNext;

  seg_t *firstseg; // linked by lsnext

  vint32 decalMark; // uid of current decal placement loop, to avoid endless looping

  // lines connected to `v1`
  line_t **v1lines;
  vint32 v1linesCount;

  // lines connected to `v2`
  line_t **v2lines;
  vint32 v2linesCount;
};


//==========================================================================
//
//  SideDef
//
//==========================================================================
enum {
  SDF_ABSLIGHT = 0x0001, // light is absolute value
};


struct side_t {
  // add this to the calculated texture column
  float TopTextureOffset;
  float BotTextureOffset;
  float MidTextureOffset;

  // add this to the calculated texture top
  float TopRowOffset;
  float BotRowOffset;
  float MidRowOffset;

  // texture indices: we do not maintain names here
  // 0 means "no texture"; -1 means "i forgot what it is"
  VTextureID TopTexture;
  VTextureID BottomTexture;
  VTextureID MidTexture;

  // sector the SideDef is facing
  sector_t *Sector;

  vint32 LineNum; // line index in `Lines`

  vuint32 Flags; // SDF_XXX

  vint32 Light;
};


//==========================================================================
//
//  Sector
//
//==========================================================================
enum {
  SPF_NOBLOCKING   = 1, // Not blocking
  SPF_NOBLOCKSIGHT = 2, // Do not block sight
  SPF_NOBLOCKSHOOT = 4, // Do not block shooting
  SPF_ADDITIVE     = 8, // Additive translucency
};

enum {
  SKY_FROM_SIDE = 0x8000
};


struct sec_plane_t : public TPlane {
  float minz;
  float maxz;

  // use for wall texture mapping
  float TexZ;

  VTextureID pic;

  float xoffs;
  float yoffs;

  float XScale;
  float YScale;

  float Angle;

  float BaseAngle;
  float BaseYOffs;

  vint32 flags;
  float Alpha;
  float MirrorAlpha;

  vint32 LightSourceSector;
  VEntity *SkyBox;
};


struct sec_params_t {
  vint32 lightlevel;
  vint32 LightColour;
  vint32 Fade;
  vint32 contents;
};


// "regions" are floor/ceiling info for sector
struct sec_region_t {
  // linked list of regions in bottom to top order
  sec_region_t *prev;
  sec_region_t *next;

  // planes
  sec_plane_t *floor;
  sec_plane_t *ceiling;

  sec_params_t *params;
  line_t *extraline;
};


//
// phares 3/14/98
//
// Sector list node showing all sectors an object appears in.
//
// There are two threads that flow through these nodes. The first thread
// starts at TouchingThingList in a sector_t and flows through the SNext
// links to find all mobjs that are entirely or partially in the sector.
// The second thread starts at TouchingSectorList in a VEntity and flows
// through the TNext links to find all sectors a thing touches. This is
// useful when applying friction or push effects to sectors. These effects
// can be done as thinkers that act upon all objects touching their sectors.
// As an mobj moves through the world, these nodes are created and
// destroyed, with the links changed appropriately.
//
// For the links, nullptr means top or end of list.
//
struct msecnode_t {
  sector_t *Sector; // a sector containing this object
  VEntity *Thing; // this object
  msecnode_t *TPrev; // prev msecnode_t for this thing
  msecnode_t *TNext; // next msecnode_t for this thing
  msecnode_t *SPrev; // prev msecnode_t for this sector
  msecnode_t *SNext; // next msecnode_t for this sector
  vint32 Visited; // killough 4/4/98, 4/7/98: used in search algorithms
};


// the SECTORS record, at runtime
// stores things/mobjs
struct sector_t {
  sec_plane_t floor;
  sec_plane_t ceiling;
  sec_params_t params;

  // floor/ceiling info for sector
  // for normal sectors, this is the same region, and
  // for complex 3d sectors (ROR) there can be more than one region
  // regions are sorted from bottom to top, so you can start
  // from `botregion` and follow `next` pointer
  sec_region_t *topregion; // highest region
  sec_region_t *botregion; // lowest region

  sector_t *deepref; // deep water hack

  vint32 special;
  vint32 tag;
  vint32 HashFirst;
  vint32 HashNext;

  float skyheight;

  // stone, metal, heavy, etc...
  vint32 seqType;

  // mapblock bounding box for height changes
  vint32 blockbox[4];

  // origin for any sounds played by the sector
  TVec soundorg;

  // if == validcount, already checked (used in various traversing, like LOS, and other line tracing)
  vint32 validcount;

  // list of subsectors in sector
  // used to check if client can see this sector
  // build in map loader; subsectors are linked with `seclink`
  subsector_t *subsectors;

  // list of things in sector
  VEntity *ThingList;
  msecnode_t *TouchingThingList;

  line_t **lines; // [linecount] size
  vint32 linecount;

  // Boom's fake floors (and deepwater)
  sector_t *heightsec;
  fakefloor_t *fakefloors; // info for rendering

  // flags
  enum {
    SF_HasExtrafloors   = 0x0001, // this sector has extrafloors
    SF_ExtrafloorSource = 0x0002, // this sector is a source of an extrafloor
    SF_TransferSource   = 0x0004, // source of an heightsec or transfer light
    SF_FakeFloorOnly    = 0x0008, // when used as heightsec in R_FakeFlat, only copies floor
    SF_ClipFakePlanes   = 0x0010, // as a heightsec, clip planes to target sector's planes
    SF_NoFakeLight      = 0x0020, // heightsec does not change lighting
    SF_IgnoreHeightSec  = 0x0040, // heightsec is only for triggering sector actions
    SF_UnderWater       = 0x0080, // sector is underwater
    SF_Silent           = 0x0100, // actors don't make noise in this sector
    SF_NoFallingDamage  = 0x0200, // no falling damage in this sector
    SF_FakeCeilingOnly  = 0x0400, // when used as heightsec in R_FakeFlat, only copies ceiling
  };
  vuint32 SectorFlags;

  // 0 = untraversed, 1,2 = sndlines -1
  vint32 soundtraversed;

  // thing that made a sound (or null)
  VEntity *SoundTarget;

  // thinker for reversable actions
  VThinker *FloorData;
  VThinker *CeilingData;
  VThinker *LightingData;
  VThinker *AffectorData;

  // sector action triggers
  VEntity *ActionList;

  vint32 Damage;

  float Friction;
  float MoveFactor;
  float Gravity; // Sector gravity (1.0 is normal)

  vint32 Sky; //k8:??? document this!

  vint32 Zone; // reverb zone id

  // this is used to check for "floor holes" that should be filled to emulate original flat floodfill bug
  // if sector has more than one neighbour, this is `nullptr`
  sector_t *othersecFloor;
  sector_t *othersecCeiling;
};


//==========================================================================
//
//  Polyobject
//
//==========================================================================

// polyobj data
struct polyobj_t {
  seg_t **segs;
  vint32 numsegs;
  TVec startSpot;
  vertex_t *originalPts; // used as the base for the rotations
  vertex_t *prevPts; // use to restore the old point values
  float angle;
  vint32 tag; // reference tag assigned in HereticEd
  vint32 bbox[4];
  vint32 validcount;
  enum {
    PF_Crush        = 0x01, // should the polyobj attempt to crush mobjs?
    PF_HurtOnTouch  = 0x02,
  };
  vuint32 PolyFlags;
  vint32 seqType;
  subsector_t *subsector;
  VThinker *SpecialData; // pointer a thinker, if the poly is moving
};


struct polyblock_t {
  polyobj_t *polyobj;
  polyblock_t *prev;
  polyblock_t *next;
};


struct PolyAnchorPoint_t {
  float x;
  float y;
  vint32 tag;
};


//==========================================================================
//
//  LineSeg
//
//==========================================================================
struct seg_t : public TPlane {
  vertex_t *v1;
  vertex_t *v2;

  float offset;
  float length;

  side_t *sidedef;
  line_t *linedef;
  seg_t *lsnext; // next seg in linedef; set by `VLevel::PostProcessForDecals()`

  // sector references
  // could be retrieved from linedef, too
  // backsector is nullptr for one sided lines
  sector_t *frontsector;
  sector_t *backsector;

  seg_t *partner; // from glnodes
  struct subsector_t *front_sub; // front subsector (we need this for self-referencing deep water)

  // side of line (for light calculations: 0 or 1)
  vint32 side;

  vint32 flags; // SF_xxx

  drawseg_t *drawsegs;
  decal_t *decals;
};


//==========================================================================
//
//  SubRegion
//
//  TODO: document this!
//
//==========================================================================
struct subregion_t {
  sec_region_t *secregion;
  subregion_t *next;
  sec_plane_t *floorplane;
  sec_plane_t *ceilplane;
  sec_surface_t *floor;
  sec_surface_t *ceil;
  vint32 count;
  drawseg_t *lines;
};


//==========================================================================
//
//  Subsector
//
//==========================================================================

// a subsector; references a sector
// basically, this is a list of LineSegs, indicating
// the visible walls that define (all or some) sides of a convex BSP leaf
struct subsector_t {
  sector_t *sector;
  subsector_t *seclink;
  vint32 numlines;
  vint32 firstline;
  polyobj_t *poly;

  node_t *parent;
  vint32 VisFrame;
  vint32 SkyVisFrame;
  vint32 updateWorldFrame;

  sector_t *deepref; // for deepwater

  //k8: i love bounding boxes! (this one doesn't store z, though)
  float bbox[4];

  vuint32 dlightbits; // bitmask of active dynamic lights
  vint32 dlightframe; // `dlightbits` validity counter
  subregion_t *regions;
};


//==========================================================================
//
//  Node
//
//==========================================================================

// indicate a leaf
enum {
  NF_SUBSECTOR = 0x80000000,
};

// BSP node
struct node_t : public TPlane {
  // bounding box for each child
  // [0] is min, [1] is max
  // (x,y,z) triples (min and max)
  float bbox[2][6];

  // if NF_SUBSECTOR its a subsector
  vuint32 children[2];

  node_t *parent;
  vint32 VisFrame;
  vint32 SkyVisFrame;
};


//==========================================================================
//
//  Thing
//
//==========================================================================

// map thing definition with initialised fields for global use
struct mthing_t {
  vint32 tid;
  float x;
  float y;
  float height;
  vint32 angle;
  vint32 type;
  vint32 options;
  vint32 SkillClassFilter;
  vint32 special;
  vint32 arg1;
  vint32 arg2;
  vint32 arg3;
  vint32 arg4;
  vint32 arg5;
};


//==========================================================================
//
//  Strife conversation scripts
//
//==========================================================================
struct FRogueConChoice {
  vint32 GiveItem; // item given on success
  vint32 NeedItem1; // required item 1
  vint32 NeedItem2; // required item 2
  vint32 NeedItem3; // required item 3
  vint32 NeedAmount1; // amount of item 1
  vint32 NeedAmount2; // amount of item 2
  vint32 NeedAmount3; // amount of item 3
  VStr Text; // text of the answer
  VStr TextOK; // message displayed on success
  vint32 Next;  // dialog to go on success, negative values to go here immediately
  vint32 Objectives; // mission objectives, LOGxxxx lump
  VStr TextNo; // message displayed on failure (player doesn't
               // have needed thing, it haves enough health/ammo,
               // item is not ready, quest is not completed)
};


struct FRogueConSpeech {
  vint32 SpeakerID; // type of the object (MT_xxx)
  vint32 DropItem; // item dropped when killed
  vint32 CheckItem1; // item 1 to check for jump
  vint32 CheckItem2; // item 2 to check for jump
  vint32 CheckItem3; // item 3 to check for jump
  vint32 JumpToConv; // jump to conversation if have certain item(s)
  VStr Name; // name of the character
  VName Voice; // voice to play
  VName BackPic; // picture of the speaker
  VStr Text; // message
  FRogueConChoice Choices[5]; // choices
};


//==========================================================================
//
//  Misc game structs
//
//==========================================================================
enum {
  PT_ADDLINES  = 1,
  PT_ADDTHINGS = 2,
  PT_EARLYOUT  = 4,
};


struct intercept_t {
  float frac; // along trace line
  enum {
    IF_IsALine  = 0x01,
    IF_BackSide = 0x02, // not yet
  };
  vuint32 Flags;
  VEntity *thing;
  line_t *line;
};


struct linetrace_t {
  TVec Start;
  TVec End;
  TVec Delta;
  TPlane Plane; // from t1 to t2
  TVec LineStart;
  TVec LineEnd;
  vuint32 PlaneNoBlockFlags;
  TVec HitPlaneNormal;
  bool SightEarlyOut;
};


struct VStateCall {
  VEntity *Item;
  VState *State;
  vuint8 Result;
};


//==========================================================================
//
//  VGameObject
//
//==========================================================================
class VGameObject : public VObject {
  DECLARE_CLASS(VGameObject, VObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VGameObject)

public:
  VObject *_stateRouteSelf; // used to replace state self for uservars if not none

  // WARNING: keep in sync with VC code!
  enum UserVarFieldType {
    None, // field is missing, or type is invalid
    Int,
    Float,
    IntArray,
    FloatArray,
  };

  // -0 index is ok for non-arrays
  int _get_user_var_int (VName fldname, int index=0);
  float _get_user_var_float (VName fldname, int index=0);

  void _set_user_var_int (VName fldname, int value, int index=0);
  void _set_user_var_float (VName fldname, float value, int index=0);

  UserVarFieldType _get_user_var_type (VName fldname);
  int _get_user_var_dim (VName fldname); // array dimension; -1: not an array, or absent

  DECLARE_FUNCTION(_get_user_var_int)
  DECLARE_FUNCTION(_get_user_var_float)
  DECLARE_FUNCTION(_set_user_var_int)
  DECLARE_FUNCTION(_set_user_var_float)
  DECLARE_FUNCTION(_get_user_var_type)
  DECLARE_FUNCTION(_get_user_var_dim)
};
