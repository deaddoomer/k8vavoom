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
//**
//**  INTERNAL DATA TYPES
//**  used by play and refresh
//**
//**************************************************************************
#ifndef VAVOOM_PSIM_GAMEOBJECT_HEADER
#define VAVOOM_PSIM_GAMEOBJECT_HEADER


#include "../textures/r_tex_id.h"

class VRenderLevelPublic;
class VTextureTranslation;
class VAcsLevel;
class VNetContext;

class VThinker;
class VLevelInfo;
class VEntity;
class VBasePlayer;
class VWorldInfo;
class VGameInfo;

struct VMapInfo;

struct sector_t;
struct fakefloor_t;
struct line_t;
struct seg_t;
struct subsector_t;
struct node_t;
struct surface_t;
struct segpart_t;
struct drawseg_t;
struct sec_surface_t;
struct subregion_t;

struct decal_t;
struct opening_t;

struct polyobj_t;
struct polyobjpart_t;


struct VLineSpecInfo {
  vint32 Index;
  VStr Name;
  enum {
    Unimplemented = 1u<<0,
    NoScript      = 1u<<1,
    NoLine        = 1u<<2,
  };
  vuint32 Flags;

  VVA_FORCEINLINE void clear () noexcept { Index = 0; Name.clear(); Flags = 0u; }

  VVA_FORCEINLINE bool IsUnimplemented () const noexcept { return (Flags&Unimplemented); }
  VVA_FORCEINLINE bool IsNoScript () const noexcept { return (Flags&NoScript); }
  VVA_FORCEINLINE bool IsNoLine () const noexcept { return (Flags&NoLine); }

  VVA_FORCEINLINE bool IsScriptAllowed () const noexcept { return !(Flags&(Unimplemented|NoScript|NoLine)); }
};


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


enum {
  SPF_NOBLOCKING   = 1u, // Not blocking
  SPF_NOBLOCKSIGHT = 2u, // Do not block sight
  SPF_NOBLOCKSHOOT = 4u, // Do not block shooting
  SPF_ADDITIVE     = 8u, // Additive translucency

  SPF_MAX_FLAG     = 16u,
  SPF_FLAG_MASK    = 15u,

  // used only as tracer flags
  SPF_IGNORE_FAKE_FLOORS = 1u<<6, //64u,
  SPF_IGNORE_BASE_REGION = 1u<<7, //128u

  // for BSP tracer (can be used for additional blocking flags)
  SPF_PLAYER = 1u<<8, //256u
  SPF_MONSTER = 1u<<9, //512u
  SPF_HITINFO = 1u<<10, //1024u

  //
  SPF_NOTHING_ZERO = 0u,
};


enum {
  SPF_FLIP_X = 1u<<0,
  SPF_FLIP_Y = 1u<<1,
  SPF_BROKEN_FLIP_X = 1u<<2,
  SPF_BROKEN_FLIP_Y = 1u<<3,
};


// ////////////////////////////////////////////////////////////////////////// //
// WARNING! keep in sync with script code!
enum {
  CONTENTS_EMPTY,
  CONTENTS_WATER,
  CONTENTS_LAVA,
  CONTENTS_NUKAGE,
  CONTENTS_SLIME,
  CONTENTS_HELLSLIME,
  CONTENTS_BLOOD,
  CONTENTS_SLUDGE,
  CONTENTS_HAZARD,
  CONTENTS_BOOMWATER,

  CONTENTS_SOLID = -1
};


// ////////////////////////////////////////////////////////////////////////// //
// sprite orientations
enum {
  SPR_VP_PARALLEL_UPRIGHT, // 0 (default)
  SPR_FACING_UPRIGHT, // 1
  SPR_VP_PARALLEL, // 2: parallel to camera visplane
  SPR_ORIENTED, // 3
  SPR_VP_PARALLEL_ORIENTED, // 4 (xy billboard)
  SPR_VP_PARALLEL_UPRIGHT_ORIENTED, // 5
  SPR_ORIENTED_OFS, // 6 (offset slightly by pitch -- for floor/ceiling splats)
  SPR_FLAT, // 7 (offset slightly by pitch -- for floor/ceiling splats; ignore roll angle)
  SPR_WALL, // 8 (offset slightly by yaw -- for wall splats; ignore pitch and roll angle)
};


// ////////////////////////////////////////////////////////////////////////// //
// text colors, these must match the constants used in ACS
enum {
  //CR_UNDEFINED = -1,
  //
  CR_BRICK, //A
  CR_TAN, //B
  CR_GRAY, //C
  CR_GREEN, //D
  CR_BROWN, //E
  CR_GOLD, //F
  CR_RED, //G
  CR_BLUE, //H
  CR_ORANGE, //I
  CR_WHITE, //J
  CR_YELLOW, //K
  CR_UNTRANSLATED, //L
  CR_BLACK, //M
  CR_LIGHTBLUE, //N
  CR_CREAM, //O
  CR_OLIVE, //P
  CR_DARKGREEN, //Q
  CR_DARKRED, //R
  CR_DARKBROWN, //S
  CR_PURPLE, //T
  CR_DARKGRAY, //U
  CR_CYAN, //V
  CR_ICE, //W
  CR_FIRE, //X
  CR_SAPPHIRE, //Y
  CR_TEAL, //Z
  // special
  CR_RED_ERROR,
  CR_WARNING_YELLOW,
  CR_DEBUG_GREEN,
  CR_DEBUG_CYAN,
  CR_INIT_CYAN,
  CR_SLIDER_HI_KNOB,
  // no more
  NUM_TEXT_COLORS,
  //
  CR_UNDEFINED = 0x00adf00d,
};


//==========================================================================
//
//  VSplashInfo
//
//==========================================================================
struct VSplashInfo {
  VName Name; // always lowercased
  VStr OrigName; // as comes from the definition

  VClass *SmallClass;
  float SmallClip;
  VName SmallSound;

  VClass *BaseClass;
  VClass *ChunkClass;
  float ChunkXVelMul;
  float ChunkYVelMul;
  float ChunkZVelMul;
  float ChunkBaseZVel;
  VName Sound;
  enum {
    F_NoAlert = 0x00000001,
  };
  vuint32 Flags;
};


//==========================================================================
//
//  VTerrainInfo
//
//==========================================================================
// bootprints are moved to separate struct,
// becase they can be attached to both flats and terrains
struct VTerrainBootprint {
  VName Name; // bootprint name (this is empty for "forward declarations")
  VStr OrigName; // as comes from the definition, never empty
  float TimeMin;
  float TimeMax;
  float AlphaMin;
  float AlphaMax;
  float AlphaValue;
  vint32 Translation;
  vint32 ShadeColor; // -2: don't change; high byte == 0xed: low byte is value for `GetAverageColor()`
  VName Animator; // if not `NAME_None`, replace the animator
  enum {
    Flag_Optional = 1u<<0,
  };
  vuint32 Flags;

  VVA_FORCEINLINE bool isOptional () const noexcept { return (Flags&Flag_Optional); }
  VVA_FORCEINLINE void setOptional () noexcept { Flags |= Flag_Optional; }
  VVA_FORCEINLINE void resetOptional () noexcept { Flags &= ~Flag_Optional; }

  VVA_FORCEINLINE void genValues () noexcept {
    if (AlphaMin < 0.0f) {
      AlphaValue = -1.0f;
    } else {
      AlphaValue = FRandomBetween(AlphaMin, AlphaMax);
    }
  }
};


struct VTerrainInfo {
  VName Name; // always lowercased
  VStr OrigName; // as comes from the definition
  VName Splash;
  enum {
    F_Liquid          = 1<<0,
    F_AllowProtection = 1<<1,
    F_PlayerOnly      = 1<<2, // only for player mobj
    F_OptOut          = 1<<3, // can be turned off with "gm_vanilla_liquids" cvar
    F_DamageOnLand    = 1<<4, // whether or not damage is applied upon hitting the floor, in addition to the periodic damage
  };
  vuint32 Flags;
  float FootClip;
  vint32 DamageTimeMask;
  vint32 DamageAmount;
  VName DamageType;
  float Friction;
  float MoveFactor;
  // footsteps
  float WalkingStepVolume;
  float RunningStepVolume;
  float CrouchingStepVolume;
  float WalkingStepTime;
  float RunningStepTime;
  float CrouchingStepTime;
  VName LeftStepSound;
  VName RightStepSound;
  // first step sound (for players and monsters)
  float LandVolume;
  VName LandSound;
  float SmallLandVolume; // for small mass
  VName SmallLandSound; // for small mass
  // bootprints
  VTerrainBootprint *BootPrint;
};


//==========================================================================
//
//  DrawSeg
//
//  each `seg_t` has one corresponding `drawseg_t`
//
//  `segpart_t` is defined in "render/r_local.h"
//  `surface_t` is defined in "render/r_shared.h"
//
//  see `VRenderLevelShared::CreateSegParts()`, and `CreateWorldSurfaces()`
//
//==========================================================================
struct drawseg_t {
  enum {
    Flag_Recreate = 1u<<0
  };

  seg_t *seg; // line segment for this drawseg

  segpart_t *top; // top part of the wall (that one with top texture)
  segpart_t *mid; // middle part of the wall (that one with middle texture)
  segpart_t *bot; // bottom part of the wall (that one with bottom texture)
  segpart_t *topsky; // who knows...
  segpart_t *extra; // for 3d floors

  surface_t *HorizonTop; // who knows...
  surface_t *HorizonBot; // who knows...

  vuint32 flags;

  VVA_FORCEINLINE bool NeedRecreate () const noexcept { return !!(flags&Flag_Recreate); }
  VVA_FORCEINLINE void SetRecreate () noexcept { flags |= Flag_Recreate; }
  VVA_FORCEINLINE void ResetRecreate () noexcept { flags &= ~Flag_Recreate; }
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
  ML_BLOCKING            = 0x00000001u, // solid, is an obstacle
  ML_BLOCKMONSTERS       = 0x00000002u, // blocks monsters only
  ML_TWOSIDED            = 0x00000004u, // backside will not be present at all
  ML_DONTPEGTOP          = 0x00000008u, // upper texture unpegged
  ML_DONTPEGBOTTOM       = 0x00000010u, // lower texture unpegged
  ML_SECRET              = 0x00000020u, // don't map as two sided: IT'S A SECRET!
  ML_SOUNDBLOCK          = 0x00000040u, // don't let sound cross two of these
  ML_DONTDRAW            = 0x00000080u, // don't draw on the automap
  ML_MAPPED              = 0x00000100u, // set if already drawn in automap
  ML_REPEAT_SPECIAL      = 0x00000200u, // special is repeatable
  ML_ADDITIVE            = 0x00000400u, // additive translucency
  ML_MONSTERSCANACTIVATE = 0x00002000u, // monsters (as well as players) can activate the line
  ML_BLOCKPLAYERS        = 0x00004000u, // blocks players only
  ML_BLOCKEVERYTHING     = 0x00008000u, // line blocks everything
  ML_ZONEBOUNDARY        = 0x00010000u, // boundary of reverb zones
  ML_RAILING             = 0x00020000u,
  ML_BLOCK_FLOATERS      = 0x00040000u,
  ML_CLIP_MIDTEX         = 0x00080000u, // automatic for every Strife line
  ML_WRAP_MIDTEX         = 0x00100000u,
  ML_3DMIDTEX            = 0x00200000u,
  ML_CHECKSWITCHRANGE    = 0x00400000u,
  ML_FIRSTSIDEONLY       = 0x00800000u, // actiavte only when crossed from front side
  ML_BLOCKPROJECTILE     = 0x01000000u,
  ML_BLOCKUSE            = 0x02000000u, // blocks all use actions through this line
  ML_BLOCKSIGHT          = 0x04000000u, // blocks monster line of sight
  ML_BLOCKHITSCAN        = 0x08000000u, // blocks hitscan attacks
  ML_3DMIDTEX_IMPASS     = 0x10000000u, // (not implemented) [TP] if 3D midtex, behaves like a height-restricted ML_BLOCKING
  ML_KEEPDATA            = 0x20000000u, // keep FloorData or CeilingData after activating them
                                        // used to simulate original Heretic behaviour
  ML_NODECAL             = 0x40000000u, // don't spawn decals on this linedef

  ML_NOTHING_ZERO = 0u,
};

enum {
  ML_SPAC_SHIFT = 10,
  ML_SPAC_MASK = 0x00001c00u,
};

// extra flags
enum {
  ML_EX_PARTIALLY_MAPPED = 1u<<0, // some segs are visible, but not all
  ML_EX_CHECK_MAPPED     = 1u<<1, // check if all segs are mapped (done in automap drawer
  ML_EX_NON_TRANSLUCENT  = 1u<<2, // set and used in renderer
};

// Special activation types
enum {
  SPAC_Cross      = 0x0001u, // when player crosses line
  SPAC_Use        = 0x0002u, // when player uses line
  SPAC_MCross     = 0x0004u, // when monster crosses line
  SPAC_Impact     = 0x0008u, // when projectile hits line
  SPAC_Push       = 0x0010u, // when player pushes line
  SPAC_PCross     = 0x0020u, // when projectile crosses line
  SPAC_UseThrough = 0x0040u, // SPAC_USE, but passes it through
  // SPAC_PTouch is remapped as (SPAC_Impact|SPAC_PCross)
  SPAC_AnyCross   = 0x0080u,
  SPAC_MUse       = 0x0100u, // when monster uses line
  SPAC_MPush      = 0x0200u, // when monster pushes line
  SPAC_UseBack    = 0x0400u, // can be used from the backside
};


struct TagHash;

/*
struct TagHashIter {
  TagHash *th;
  int tag;
  int index;
};
*/

TagHash *tagHashAlloc ();
void tagHashClear (TagHash *th);
void tagHashFree (TagHash *&th);
void tagHashPut (TagHash *th, int tag, void *ptr);
bool tagHashCheckTag (const TagHash *th, int tag, const void *ptr);
/*
bool tagHashFirst (TagHashIter *it, TagHash *th, int tag);
bool tagHashNext (TagHashIter *it);
void *tagHashCurrent (const TagHashIter *it);
*/
// returns -1 when finished
int tagHashFirst (const TagHash *th, int tag);
int tagHashNext (const TagHash *th, int index, int tag);
void *tagHashPtr (const TagHash *th, int index);
int tagHashTag (const TagHash *th, int index);


//==========================================================================
//
//  SideDef
//
//==========================================================================
enum {
  SDF_ABSLIGHT   = 1u<<0, // light is absolute value
  SDF_WRAPMIDTEX = 1u<<1,
  SDF_CLIPMIDTEX = 1u<<2,
  SDF_NOFAKECTX  = 1u<<3, // no fake contrast
  SDF_SMOOTHLIT  = 1u<<4, // smooth lighting, not implemented yet
  SDF_NODECAL    = 1u<<5,
  // was the corresponding textures "AASHITTY"?
  // this is required to fix bridges
  SDF_AAS_TOP    = 1u<<6,
  SDF_AAS_BOT    = 1u<<7,
  SDF_AAS_MID    = 1u<<8,
};

// texture params flag
enum {
  // texture flipping
  STP_FLIP_X = 1u<<0,
  STP_FLIP_Y = 1u<<1,
  // modifies flips, so they should work as broken GZDoom version
  STP_BROKEN_FLIP_X = 1u<<2,
  STP_BROKEN_FLIP_Y = 1u<<3,
};


struct side_tex_params_t {
  float TextureOffset; // x, s axis, column
  float RowOffset; // y, t axis, top
  float ScaleX, ScaleY;
  vuint32 Flags; // STP_XXX
  float BaseAngle;
  float Angle;
};


struct side_t {
  side_tex_params_t Top;
  side_tex_params_t Bot;
  side_tex_params_t Mid;

  // texture indices: we do not maintain names here
  // 0 means "no texture"; -1 means "i forgot what it is"
  VTextureID TopTexture;
  VTextureID BottomTexture;
  VTextureID MidTexture;

  // sector the SideDef is facing
  sector_t *Sector;

  vuint32 rendercount; // used to avoid rendering lines several times (with full segments)

  vint32 LineNum; // line index in `Lines`

  vuint32 Flags; // SDF_XXX

  vint32 Light;
};


//==========================================================================
//
//  LineSeg
//
//  moved here, so we'll be able to use it in inline methods
//
//==========================================================================
// seg flags
enum {
  SF_MAPPED  = 1u<<0, // some segs of this linedef are visible, but not all
  SF_ZEROLEN = 1u<<1, // zero-length seg (it has some fake length)
  //SF_FULLSEG = 1u<<2, // is this a "fullseg"? (this was a bad idea); leave this bit unused for now
  // do not spam with warnings
  SF_SECWARNED = 1u<<3, // did we print any warnings for this seg? (used to avoid warning spam)
};

struct seg_t : public TPlane {
  TVec *v1;
  TVec *v2;

  float offset;
  float length;
  TVec ndir; // precalced segment direction, so i don't have to do it again in surface creator; normalized

  side_t *sidedef;
  line_t *linedef;
  seg_t *lsnext; // next seg in linedef; set by `VLevel::PostProcessForDecals()`

  // sector references
  // could be retrieved from linedef, too
  // backsector is nullptr for one sided lines
  sector_t *frontsector;
  sector_t *backsector;

  seg_t *partner; // from glnodes
  subsector_t *frontsub; // front subsector (we need this for self-referencing deep water)

  // side of line (for light calculations: 0 or 1)
  vint32 side;

  vuint32 flags; // SF_xxx

  drawseg_t *drawsegs;

  // original polyobject, or `nullptr` for world seg
  polyobj_t *pobj;

  // decal list
  decal_t *decalhead;
  decal_t *decaltail;

  void appendDecal (decal_t *dc) noexcept;
  void removeDecal (decal_t *dc) noexcept; // will not delete it
  void killAllDecals (VLevel *Level) noexcept;
};


//==========================================================================
//
//  LineDef
//
//==========================================================================
struct line_t : public TPlane {
  // vertices, from v1 to v2
  TVec *v1;
  TVec *v2;

  // precalculated *2D* (v2-v1) for side checking (i.e. z is zero)
  TVec dir;
  // normalised dir
  TVec ndir;

  vuint32 flags;
  vuint32 SpacFlags;
  vuint32 exFlags; //ML_EX_xxx

  // visual appearance: SideDefs
  // sidenum[1] will be -1 if one sided
  // sidenum[0] can be -1 too (some UDMF maps can do this)
  vint32 sidenum[2];

  // neat. another bounding box, for the extent of the LineDef
  float bbox2d[4];

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

  vint32 locknumber;

  TArray<vint32> moreTags; // except `lineTag`
  vint32 lineTag;

  inline bool IsTagEqual (int tag) const {
    if (!tag || tag == -1) return false;
    if (lineTag == tag) return true;
    //return tagHashCheckTag(tagHash, tag, this);
    for (int f = 0; f < moreTags.length(); ++f) if (moreTags[f] == tag) return true;
    return false;
  }

  seg_t *firstseg; // linked by lsnext

  // lines connected to `v1`
  line_t **v1lines;
  vint32 v1linesCount;

  // lines connected to `v2`
  line_t **v2lines;
  vint32 v2linesCount;

  VVA_FORCEINLINE int vxCount (int vidx) const noexcept { return (vidx ? v2linesCount : v1linesCount); }
  VVA_FORCEINLINE line_t *vxLine (int vidx, int idx) noexcept { return (vidx ? v2lines[idx] : v1lines[idx]); }
  VVA_FORCEINLINE const line_t *vxLine (int vidx, int idx) const noexcept { return (vidx ? v2lines[idx] : v1lines[idx]); }

  polyobj_t *pobject; // do not use this directly!

  VVA_FORCEINLINE polyobj_t *pobj () const noexcept { return pobject; }

  // so we'll be able to check those without `VLevel` object
  side_t *frontside;
  side_t *backside;
  vuint32 updateWorldFrame;

  // collision detection planes
  //   [0] is always the line itself
  //   [1] is a reversed line
  //   other possible planes are axis-aligned caps
  // this way, the line is a convex closed object, which can be considered solid
  // so we can use the usual Minkowski sum to inflate the line, and perform segment-plane intersection
  TPlane *cdPlanes; // this field is for VavoomC, so i'll be able to use slice there
  vint32 cdPlanesCount; // this field must follow the pointer!
  TPlane cdPlanesArray[6];

  // considers the line to be infinite
  // returns side 0 or 1, -1 if box crosses the line
  int Box2DSide (const float tmbox[4]) const noexcept;

  // returns `true` if the line hits the box
  // line is finite
  //FIXME: do not check for equality here?
  VVA_FORCEINLINE bool Box2DHit (const float tmbox[4]) const noexcept {
    if (tmbox[BOX2D_RIGHT] <= bbox2d[BOX2D_LEFT] ||
        tmbox[BOX2D_LEFT] >= bbox2d[BOX2D_RIGHT] ||
        tmbox[BOX2D_TOP] <= bbox2d[BOX2D_BOTTOM] ||
        tmbox[BOX2D_BOTTOM] >= bbox2d[BOX2D_TOP])
    {
      return false;
    } else {
      return (Box2DSide(tmbox) == -1);
    }
  }
};


//==========================================================================
//
//  Sector
//
//==========================================================================
enum {
  SKY_FROM_SIDE = 0x40000000u,
};


struct sec_plane_t : public TPlane {
  float minz;
  float maxz;

  // used for wall texture mapping
  float TexZ;

  VTextureID pic;

  float xoffs;
  float yoffs;

  float XScale;
  float YScale;

  float Angle;

  float BaseAngle;
  float BaseXOffs;
  float BaseYOffs;

  // pobj offset
  float PObjCX;
  float PObjCY;

  vuint32 flags; // SPF_xxx
  vuint32 flipFlags; // SPF_FLIP_xxx
  float Alpha;
  float MirrorAlpha;

  vint32 LightSourceSector;
  VEntity *SkyBox;

  //sector_t *parent; // can be `nullptr`, has meaning only for `SPF_ALLOCATED` planes
  //vuint32 exflags; // SPF_EX_xxx

  VVA_FORCEINLINE VVA_CHECKRESULT float GetPointZClamped (float x, float y) const noexcept {
    return clampval(GetPointZ(x, y), minz, maxz);
  }

  VVA_FORCEINLINE VVA_CHECKRESULT float GetPointZRevClamped (float x, float y) const noexcept {
    //FIXME: k8: should min and max be switched here?
    return clampval(GetPointZRev(x, y), minz, maxz);
  }

  VVA_FORCEINLINE VVA_CHECKRESULT float GetPointZClamped (const TVec &v) const noexcept {
    return GetPointZClamped(v.x, v.y);
  }

  VVA_FORCEINLINE VVA_CHECKRESULT float GetPointZRevClamped (const TVec &v) const noexcept {
    return GetPointZRevClamped(v.x, v.y);
  }

  VVA_FORCEINLINE void CopyOffsetsFrom (const sec_plane_t &src) noexcept {
    xoffs = src.xoffs;
    yoffs = src.yoffs;
    XScale = src.XScale;
    YScale = src.YScale;
    Angle = src.Angle;
    BaseAngle = src.BaseAngle;
    BaseYOffs = src.BaseYOffs;
  }
};


//==========================================================================
//
//  sector plane reference with flip flag
//
//==========================================================================
struct TSecPlaneRef {
  // WARNING! do not change this values, they are used to index things in other code!
  enum Type { Unknown = -1, Floor = 0, Ceiling = 1 };

  enum {
    Flags_None     = 0,
    Flag_Flipped   = 1<<0,
    Flag_UseMinMax = 1<<1,
  };

  sec_plane_t *splane;
  float minZ, maxZ;
  /*bool*/vint32 flags; // actually, bit flags

  VVA_FORCEINLINE TSecPlaneRef () noexcept
    : splane(nullptr)
    , minZ(0.0f)
    , maxZ(0.0f)
    , flags(Flags_None)
  {}

  VVA_FORCEINLINE TSecPlaneRef (const TSecPlaneRef &sp) noexcept
    : splane(sp.splane)
    , minZ(sp.minZ)
    , maxZ(sp.maxZ)
    , flags(sp.flags)
  {}

  explicit TSecPlaneRef (sec_plane_t *aplane, bool arev) noexcept
    : splane(aplane)
    , minZ(0.0f)
    , maxZ(0.0f)
    , flags(arev ? Flag_Flipped : Flags_None)
  {}

  VVA_FORCEINLINE void operator = (const TSecPlaneRef &sp) noexcept {
    if (this != &sp) {
      splane = sp.splane;
      minZ = sp.minZ;
      maxZ = sp.maxZ;
      flags = sp.flags;
    }
  }

  VVA_FORCEINLINE bool isValid () const noexcept { return !!splane; }

  VVA_FORCEINLINE bool isFloor () const noexcept { return (GetNormalZSafe() > 0.0f); }
  VVA_FORCEINLINE bool isCeiling () const noexcept { return (GetNormalZSafe() < 0.0f); }

  VVA_FORCEINLINE bool isSlope () const noexcept { return (fabsf(GetNormalZSafe()) != 1.0f); }

  // see enum at the top
  VVA_FORCEINLINE Type classify () const noexcept {
    const float z = GetNormalZSafe();
    return (z < 0.0f ? Ceiling : z > 0.0f ? Floor : Unknown);
  }

  VVA_FORCEINLINE bool isFlipped () const noexcept { return !!(flags&Flag_Flipped); }
  VVA_FORCEINLINE bool isOwnMinMax () const noexcept { return !!(flags&Flag_UseMinMax); }

  VVA_FORCEINLINE void set (sec_plane_t *aplane, bool arev) noexcept {
    splane = aplane;
    flags = (arev ? Flag_Flipped : Flags_None);
  }

  VVA_FORCEINLINE TVec GetNormal () const noexcept { return (flags&Flag_Flipped ? -splane->normal : splane->normal); }
  VVA_FORCEINLINE float GetNormalZ () const noexcept { return (flags&Flag_Flipped ? -splane->normal.z : splane->normal.z); }
  VVA_FORCEINLINE float GetNormalZSafe () const noexcept { return (splane ? (flags&Flag_Flipped ? -splane->normal.z : splane->normal.z) : 0.0f); }
  VVA_FORCEINLINE float GetDist () const noexcept { return (flags&Flag_Flipped ? -splane->dist : splane->dist); }
  VVA_FORCEINLINE TPlane GetPlane () const noexcept {
    TPlane res;
    res.normal = (flags&Flag_Flipped ? -splane->normal : splane->normal);
    res.dist = (flags&Flag_Flipped ? -splane->dist : splane->dist);
    return res;
  }

  VVA_FORCEINLINE float PointDistance (const TVec &p) const noexcept {
    return (flags&Flag_Flipped ? p.dotv2neg(splane->normal)+splane->dist : p.dot(splane->normal)-splane->dist);
  }

  // valid only for horizontal planes!
  //inline float GetRealDist () const noexcept { return (!flipped ? splane->dist*splane->normal.z : (-splane->dist)*(-splane->normal.z)); }
  VVA_FORCEINLINE float GetRealDist () const noexcept {
    return ((flags&Flag_Flipped ? -splane->dist : splane->dist)*splane->normal.z);
  }

  VVA_FORCEINLINE void Flip () noexcept { flags ^= Flag_Flipped; }

  // get z of point with given x and y coords
  // don't try to use it on a vertical plane
  VVA_FORCEINLINE VVA_CHECKRESULT float GetPointZ (float x, float y) const noexcept {
    return (flags&Flag_Flipped ? splane->GetPointZRev(x, y) : splane->GetPointZ(x, y));
  }

  VVA_FORCEINLINE VVA_CHECKRESULT float GetPointZClamped (float x, float y) const noexcept {
    //return clampval((!flipped ? splane->GetPointZ(x, y) : splane->GetPointZRev(x, y)), splane->minz, splane->maxz);
    //return (!flipped ? splane->GetPointZClamped(x, y) : splane->GetPointZRevClamped(x, y));
    if (flags&Flag_UseMinMax) {
      return clampval((flags&Flag_Flipped ? splane->GetPointZRev(x, y) : splane->GetPointZ(x, y)), minZ, maxZ);
    } else {
      return (flags&Flag_Flipped ? splane->GetPointZRevClamped(x, y) : splane->GetPointZClamped(x, y));
    }
  }

  VVA_FORCEINLINE VVA_CHECKRESULT float DotPoint (const TVec &point) const noexcept {
    return (flags&Flag_Flipped ? point.dotv2neg(splane->normal) : point.dot(splane->normal));
  }

  VVA_FORCEINLINE VVA_CHECKRESULT float GetPointZ (const TVec &v) const noexcept { return GetPointZ(v.x, v.y); }

  VVA_FORCEINLINE VVA_CHECKRESULT float GetPointZClamped (const TVec &v) const noexcept { return GetPointZClamped(v.x, v.y); }

  // returns side 0 (front) or 1 (back, or on plane)
  VVA_FORCEINLINE VVA_CHECKRESULT int PointOnSide (const TVec &point) const noexcept { return (PointDistance(point) <= 0.0f); }

  // returns side 0 (front) or 1 (back, or on plane)
  VVA_FORCEINLINE VVA_CHECKRESULT int PointOnSideThreshold (const TVec &point) const noexcept { return (PointDistance(point) < 0.1f); }

  // returns side 0 (front, or on plane) or 1 (back)
  // "fri" means "front inclusive"
  VVA_FORCEINLINE VVA_CHECKRESULT int PointOnSideFri (const TVec &point) const noexcept { return (PointDistance(point) < 0.0f); }

  // returns side 0 (front), 1 (back), or 2 (on)
  // used in line tracing (only)
  VVA_FORCEINLINE VVA_CHECKRESULT int PointOnSide2 (const TVec &point) const noexcept {
    const float dot = PointDistance(point);
    return (dot < -0.1f ? 1 : dot > 0.1f ? 0 : 2);
  }

  // returns side 0 (front), 1 (back)
  // if at least some part of the sphere is on a front side, it means "front"
  VVA_FORCEINLINE VVA_CHECKRESULT int SphereOnSide (const TVec &center, float radius) const noexcept { return (PointDistance(center) <= -radius); }

  VVA_FORCEINLINE VVA_CHECKRESULT bool SphereTouches (const TVec &center, float radius) const noexcept { return (fabsf(PointDistance(center)) < radius); }

  // returns side 0 (front), 1 (back), or 2 (collides)
  VVA_FORCEINLINE VVA_CHECKRESULT int SphereOnSide2 (const TVec &center, float radius) const noexcept {
    const float d = PointDistance(center);
    return (d < -radius ? 1 : d > radius ? 0 : 2);
  }

  // returns intersection time
  // negative means "no intersection" (and `*currhit` is not modified)
  // yeah, we have too many of those
  // this one is used in various plane checks in the engine (it happened so historically)
  inline float IntersectionTime (const TVec &linestart, const TVec &lineend, TVec *currhit=nullptr) const noexcept {
    const float d1 = PointDistance(linestart);
    if (d1 < 0.0f) return -1.0f; // don't shoot back side

    const float d2 = PointDistance(lineend);
    if (d2 >= 0.0f) return -1.0f; // didn't hit plane

    //if (d2 > 0.0f) return true; // didn't hit plane (was >=)
    //if (fabsf(d2-d1) < 0.0001f) return true; // too close to zero

    const float frac = d1/(d1-d2); // [0..1], from start
    if (!isFiniteF(frac) || frac < 0.0f || frac > 1.0f) return -1.0f; // just in case

    if (currhit) *currhit = linestart+(lineend-linestart)*frac;
    return frac;
  }

  // distance from point to plane
  // plane must be normalized
  /*
  inline VVA_CHECKRESULT float Distance (const TVec &p) const {
    //return (cast(double)normal.x*p.x+cast(double)normal.y*p.y+cast(double)normal.z*cast(double)p.z)/normal.dbllength;
    //return VSUM3(normal.x*p.x, normal.y*p.y, normal.z*p.z); // plane normal has length 1
    return PointDistance(p);
  }
  */
};


//==========================================================================
//
//  sector lighting info
//
//==========================================================================
struct sec_params_t {
  enum {
    LFC_FloorLight_Abs    = 1u<<0,
    LFC_CeilingLight_Abs  = 1u<<1,
    LFC_FloorLight_Glow   = 1u<<2,
    LFC_CeilingLight_Glow = 1u<<3,
  };

  vuint32 lightlevel;
  vuint32 LightColor;
  vuint32 Fade;
  vuint32 contents;
  // bit0: floor light is absolute; bit1: the same for ceiling
  // bit2: has floor glow; bit3: has ceiling glow
  vuint32 lightFCFlags;
  // light levels
  vuint32 lightFloor;
  vuint32 lightCeiling;
  // glow colors
  vuint32 glowFloor;
  vuint32 glowCeiling;
  float glowFloorHeight;
  float glowCeilingHeight;
};


//==========================================================================
//
//  sector regions
//
//  "regions" keeps 3d floors for sector
//  region inside may be non-solid, if the corresponding flag is set
//
//==========================================================================
struct sec_region_t {
  sec_region_t *next;
  // planes
  // floor is bottom plane (and points bottom, like normal ceiling, if not base region)
  TSecPlaneRef efloor;
  // ceiling is top plane (and points top, like normal froor, if not base region)
  TSecPlaneRef eceiling;
  // contents and lighting
  sec_params_t *params;

  // side of this region
  // can be `nullptr`, cannot be changed after surface creation
  // this is from our floor to our ceiling
  line_t *extraline;

  // flags are here to implement various kinds of 3d floors
  enum {
    RF_BaseRegion    = 1u<<0, // sane k8vavoom-style region; only one, only first
    RF_NonSolid      = 1u<<1,
    // ignore this region in gap/opening processing
    // this flags affects collision detection
    RF_OnlyVisual    = 1u<<2,
    // the following flags are only for renderer, collision detection igonres them
    RF_SkipFloorSurf = 1u<<3, // do not create floor surface for this region
    RF_SkipCeilSurf  = 1u<<4, // do not create ceiling surface for this region
    RF_SaneRegion    = 1u<<5, // k8vavoom-style 3d floor region
    //
    RF_NothingZero = 0u,
  };
  vuint32 regflags;

  bool isBlockingExtraLine () const noexcept;
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
// k8: note that sector cannot appear in a list twice, so it is safe to
//     rely on this in various effectors.
//
// For the links, nullptr means top or end of list.
//
struct msecnode_t {
  sector_t *Sector; // a sector containing this object
  VEntity *Thing; // this object
  msecnode_t *TNext; // next msecnode_t for this thing
  msecnode_t *TPrev; // prev msecnode_t for this thing
  msecnode_t *SNext; // next msecnode_t for this sector (also, link for `HeadSecNode` aka "free nodes list")
  msecnode_t *SPrev; // prev msecnode_t for this sector
  vint32 Visited; // killough 4/4/98, 4/7/98: used in search algorithms
};


//==========================================================================
//
//  map sector
//
//==========================================================================
struct sector_t {
  sec_plane_t floor;
  sec_plane_t ceiling;
  sec_params_t params;

  // floor/ceiling info for sector
  // first region is always base sector region that keeps "emptyness"
  // other regions are "filled space" (yet that space can be non-solid)
  // region floor and ceiling are better have the same set of `SPF_NOxxx` flags.
  sec_region_t *eregions;

  // this is cached planes for thing gap determination
  // planes are sorted by... something
  //TArray<TSecPlaneRef> regplanes;

  sector_t *deepref; // deep water hack

  vint32 special;

  TArray<vint32> moreTags; // except `lineTag`
  vint32 sectorTag;

  inline bool IsTagEqual (int tag) const {
    if (!tag || tag == -1) return false;
    if (sectorTag == tag) return true;
    //return tagHashCheckTag(tagHash, tag, this);
    for (int f = 0; f < moreTags.length(); ++f) if (moreTags[f] == tag) return true;
    return false;
  }

  float skyheight;

  // stone, metal, heavy, etc...
  vint32 seqType;

  // mapblock bounding box for height changes (not used anymore)
  //vint32 blockbox[4];

  // origin for any sounds played by the sector (the middle of the bounding box)
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

  // this is allocated as a big pool, and `nbsecs` points to various parts of that pool
  // it is guaranteed that sector 0 has `nbsecs` pointing to the pool start
  // `lines` is never `nullptr`, even if `linecount` is zero
  line_t **lines; // [linecount] size
  vint32 linecount;
  vint32 prevlinecount; // temp store for 3d pobjs

  // neighbouring sectors
  // this is allocated as a big pool, and `nbsecs` points to various parts of that pool
  // it is guaranteed that sector 0 has `nbsecs` pointing to the pool start
  // `nbsecs` is never `nullptr`, even if `nbseccount` is zero
  sector_t **nbsecs;
  vint32 nbseccount;

  // Boom's fake floors (and deepwater)
  sector_t *heightsec;
  fakefloor_t *fakefloors; // info for rendering

  // flags
  enum {
    SF_HasExtrafloors   = 1u<<0, // this sector has extrafloors
    SF_ExtrafloorSource = 1u<<1, // this sector is a source of an extrafloor
    SF_TransferSource   = 1u<<2, // source of an heightsec or transfer light
    SF_FakeFloorOnly    = 1u<<3, // do not draw fake ceiling
    SF_ClipFakePlanes   = 1u<<4, // as a heightsec, clip planes to target sector's planes
    SF_NoFakeLight      = 1u<<5, // heightsec does not change lighting
    SF_IgnoreHeightSec  = 1u<<6, // heightsec is only for triggering sector actions (i.e. don't draw them)
    SF_UnderWater       = 1u<<7, // sector is underwater
    SF_Silent           = 1u<<8, // actors don't make noise in this sector
    SF_NoFallingDamage  = 1u<<9, // no falling damage in this sector
    SF_FakeCeilingOnly  = 1u<<10, // when used as heightsec in R_FakeFlat, only copies ceiling
    SF_HangingBridge    = 1u<<11, // fake hanging bridge
    SF_Has3DMidTex      = 1u<<12, // has any 3dmidtex linedef?
    // mask with this to check if this is "arg1==0" Boom crap
    SF_FakeBoomMask     = SF_FakeFloorOnly|SF_ClipFakePlanes|/*SF_UnderWater|*/SF_IgnoreHeightSec/*|SF_NoFakeLight*/,
    SF_IsTransDoor      = 1u<<13, // is this sector can be interpreted as "transparent door"?
    SF_Hidden           = 1u<<14, // this sector is not visible on the automap
    SF_NoPlayerRespawn  = 1u<<15, // ZDoom "player cannot respawn in this sector" flag (who may need it at all?)
    SF_IsTransDoorTop   = 1u<<16, // `SF_IsTransDoor` is always set with this
    SF_IsTransDoorBot   = 1u<<17, // `SF_IsTransDoor` is always set with this
    SF_IsTransDoorTrack = 1u<<18, // fix one-sided "door tracks" for this sector (Boom transparent walls hack)
    //
    SF_NothingZero = 0u,
  };
  vuint32 SectorFlags;

  VVA_FORCEINLINE bool IsUnderwater () const noexcept { return (SectorFlags&SF_UnderWater); }

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
  VName DamageType; // can be empty for "standard/unknown"
  vint32 DamageInterval; // in ticks; zero means `32` (Doom default)
  vint32 DamageLeaky; // <0: none; 0: default (5); 256 is 100% (i.e. it is actually a byte)

  float Friction;
  float MoveFactor;
  float Gravity; // Sector gravity (1.0 is normal)

  vint32 Sky; // if SKY_FROM_SIDE set, this is replaced sky; sky2 for index 0, or top texture from side[index-1]

  vint32 Zone; // reverb zone id

  // this is set to `Level->TicTime` when some sector lines were rendered
  // has no sense for servers (nothing is ever rendered)
  // `0` may mean "not rendered at all", or "rendered at the very first tic"
  vint32 LastRenderTic;

  vint32 ZExtentsCacheId;
  float LastMinZ, LastMaxZ;

  // this is used to check for "floor holes" that should be filled to emulate original flat floodfill bug
  // if sector has more than one neighbour, this is `nullptr`
  sector_t *othersecFloor;
  sector_t *othersecCeiling;

  polyobj_t *ownpobj; // polyobject that has this sector as its "inner" one


  VVA_FORCEINLINE bool Has3DFloors () const noexcept { return !!eregions->next; }
  VVA_FORCEINLINE bool HasAnyExtraFloors () const noexcept { return (!!eregions->next) || (!!heightsec); }

  inline bool Has3DSlopes () const noexcept {
    for (const sec_region_t *reg = eregions->next; reg; reg = reg->next) {
      if (reg->regflags&(sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual)) continue;
      if (reg->efloor.isSlope() || reg->eceiling.isSlope()) return true;
    }
    return false;
  }

  // should be called for new sectors to setup base region
  void CreateBaseRegion ();
  void DeleteAllRegions ();

  // `next` is set, everything other is zeroed
  sec_region_t *AllocRegion ();

  VVA_FORCEINLINE bool isOriginalPObj () const noexcept { return (linecount == 0); }
  VVA_FORCEINLINE bool isInnerPObj () const noexcept { return (linecount && ownpobj); }
  VVA_FORCEINLINE bool isAnyPObj () const noexcept { return (linecount == 0 || ownpobj); }
};


//==========================================================================
//
//  PolyobjectPart
//
//  visible part of the polyobject
//  this is what resides in subsectors
//  segs are clipped to the correspoinding subsector
//  one subsector can contain a lot of parts; they're all linked by `nextsub`
//
//==========================================================================
struct polyobjpart_t {
public:
  polyobj_t *pobj; // owning polyobject
  subsector_t *sub; // subsector containing this node
  polyobjpart_t *nextpobj; // next polyobject part for this polyobject (used or free)
  polyobjpart_t *nextsub; // next polyobject part for this subsector

  enum {
    CLIPSEGS_CREATED = 1u<<0,
  };
  vuint32 flags;

  // clipped segs for this polypart (to use in clipper)
  seg_t *segs;
  TVec *verts; // amount*2
  vint32 count; // number of used elements in `segs`
  vint32 amount; // number of allocated elements for `segs`

  VVA_FORCEINLINE void reset () noexcept { count = 0; flags &= ~CLIPSEGS_CREATED; }

  void Free () noexcept;

  VVA_FORCEINLINE void EnsureClipSegs (VLevel *level) { if (!(flags&CLIPSEGS_CREATED)) CreateClipSegs(level); }

protected:
  void CreateClipSegs (VLevel *level);

  seg_t *allocSeg (const seg_t *oldseg) noexcept;

public:
  class PolySegIter {
  private:
    seg_t *segs;
    int count;
    int index;
  public:
    VVA_FORCEINLINE PolySegIter (seg_t *asegs, int acount, int aindex=0) noexcept : segs(asegs), count(acount), index(aindex) {}
    VVA_FORCEINLINE PolySegIter begin () noexcept { return PolySegIter(segs, count, index); }
    VVA_FORCEINLINE PolySegIter end () noexcept { return PolySegIter(segs, count, count); }
    VVA_FORCEINLINE bool operator == (const PolySegIter &b) const noexcept { return (segs == b.segs && count == b.count && index == b.index); }
    VVA_FORCEINLINE bool operator != (const PolySegIter &b) const noexcept { return (segs != b.segs || count != b.count || index != b.index); }
    VVA_FORCEINLINE PolySegIter operator * () const noexcept { return PolySegIter(segs, count, index); } /* required for iterator */
    VVA_FORCEINLINE void operator ++ () noexcept { if (index < count) ++index; } /* this is enough for iterator */
    // accessors
    VVA_FORCEINLINE seg_t *seg () const noexcept { return (index < count ? &segs[index] : nullptr); }
  };

  VVA_FORCEINLINE PolySegIter SegFirst () const noexcept { return PolySegIter(segs, count); }
};


//==========================================================================
//
//  Polyobject
//
//==========================================================================
struct polyobj_t {
public:
  class PolySubIter {
  private:
    polyobjpart_t *ppart;
  public:
    VVA_FORCEINLINE PolySubIter (polyobjpart_t *apart) noexcept : ppart(apart) {}
    VVA_FORCEINLINE PolySubIter begin () noexcept { return PolySubIter(ppart); }
    VVA_FORCEINLINE PolySubIter end () noexcept { return PolySubIter(nullptr); }
    VVA_FORCEINLINE bool operator == (const PolySubIter &b) const noexcept { return (ppart == b.ppart); }
    VVA_FORCEINLINE bool operator != (const PolySubIter &b) const noexcept { return (ppart != b.ppart); }
    VVA_FORCEINLINE PolySubIter operator * () const noexcept { return PolySubIter(ppart); } /* required for iterator */
    VVA_FORCEINLINE void operator ++ () noexcept { if (ppart) ppart = ppart->nextpobj; } /* this is enough for iterator */
    // accessors
    VVA_FORCEINLINE polyobjpart_t *part () const noexcept { return ppart; }
    VVA_FORCEINLINE subsector_t *sub () const noexcept { return (ppart ? ppart->sub : nullptr); }
  };

  class PolyLineIter {
  private:
    line_t **lines;
    int count;
    int index;
  public:
    VVA_FORCEINLINE PolyLineIter (line_t **alines, int acount, int aindex=0) noexcept : lines(alines), count(acount), index(aindex) {}
    VVA_FORCEINLINE PolyLineIter begin () noexcept { return PolyLineIter(lines, count, index); }
    VVA_FORCEINLINE PolyLineIter end () noexcept { return PolyLineIter(lines, count, count); }
    VVA_FORCEINLINE bool operator == (const PolyLineIter &b) const noexcept { return (lines == b.lines && count == b.count && index == b.index); }
    VVA_FORCEINLINE bool operator != (const PolyLineIter &b) const noexcept { return (lines != b.lines || count != b.count || index != b.index); }
    VVA_FORCEINLINE PolyLineIter operator * () const noexcept { return PolyLineIter(lines, count, index); } /* required for iterator */
    VVA_FORCEINLINE void operator ++ () noexcept { if (index < count) ++index; } /* this is enough for iterator */
    // accessors
    VVA_FORCEINLINE line_t *line () const noexcept { return (index < count ? lines[index] : nullptr); }
  };

  class PolySegIter {
  private:
    seg_t **segs;
    int count;
    int index;
  public:
    VVA_FORCEINLINE PolySegIter (seg_t **asegs, int acount, int aindex=0) noexcept : segs(asegs), count(acount), index(aindex) {}
    VVA_FORCEINLINE PolySegIter begin () noexcept { return PolySegIter(segs, count, index); }
    VVA_FORCEINLINE PolySegIter end () noexcept { return PolySegIter(segs, count, count); }
    VVA_FORCEINLINE bool operator == (const PolySegIter &b) const noexcept { return (segs == b.segs && count == b.count && index == b.index); }
    VVA_FORCEINLINE bool operator != (const PolySegIter &b) const noexcept { return (segs != b.segs || count != b.count || index != b.index); }
    VVA_FORCEINLINE PolySegIter operator * () const noexcept { return PolySegIter(segs, count, index); } /* required for iterator */
    VVA_FORCEINLINE void operator ++ () noexcept { if (index < count) ++index; } /* this is enough for iterator */
    // accessors
    VVA_FORCEINLINE seg_t *seg () const noexcept { return (index < count ? segs[index] : nullptr); }
  };

public:
  seg_t **segs; // individual elements points inside the level's `Segs`
  vint32 numsegs; // number of segs for this polyobject
  line_t **lines; // individual elements points inside the level's `Lines`
  vint32 numlines; // number of lines for this polyobject
  TVec startSpot; // current offset for `originalPts`
  TVec *originalPts; // used as the base for the rotations; array
  vint32 originalPtsCount; // number of elements in each `*Pts` array
  TVec **segPts; // all points we need to modify, to avoid looping over segs again and again
  vint32 segPtsCount; // number of elements in each `*Pts` array
  TVec *prevPts; // need to be here due to linked polyobjects
  vint32 prevPtsCount; // need to be here due to linked polyobjects
  TVec origStartSpot; // saved starting spot
  sec_plane_t savedFloor;
  sec_plane_t savedCeiling;
  polyobjpart_t *parts; // list of all polyoject parts
  polyobjpart_t *freeparts; // list of all unused polyoject parts (so we can avoid constant reallocation)
  // note that polyobjects can't have sloped flats (yet)
  sec_plane_t pofloor; // inverted, i.e. looks down
  sec_plane_t poceiling; // inverted, i.e. looks up
  sector_t *posector; // for 3d polyobjects this is their "inside" sector
  sector_t *refsector; // for normal polyobjects this is their spawn sector
  float refsectorOldFloorHeight; // to check if we need to move polyobject vertically
  polyobj_t *polink; // next polyobject in link chain
  polyobj_t *polinkprev; // previous polyobject in link chain
  float angle;
  vint32 tag; // reference tag assigned in HereticEd
  vint32 mirror; // 0 usually means "none"
  vint32 bmbbox[4]; //WARNING! this is in blockmap coords, not unit coords
  float bbox2d[4]; // *CURRENT* bounding box
  vint32 validcount;
  vuint32 rendercount; // used in renderer; there is no need to save this for portals
    // no need to save, because this is used only in renderer, to mark already rendered polyobjects in BSP walker
  vuint32 updateWorldFrame; // to avoid excessive updates; valid only for segs, for flats, aponymous subsector field is used
  enum {
    PF_Crush         = 1u<<0, // should the polyobj attempt to crush mobjs?
    PF_HurtOnTouch   = 1u<<1,
    PF_NoCarry       = 1u<<2, // 3d pobj should not carry things (only for horizontal; for vertical -- only up, down by gravity)
    PF_NoAngleChange = 1u<<3, // 3d pobj should not change thing angle on rotation
    PF_SideCrush     = 1u<<4, // 3d pobj side should not try to thrust away (only if `PF_Crush` is set)
    // cache some info
    PF_HasTopBlocking = 1u<<5, // 3d pobj has some top-blocking textures?
  };
  vuint32 PolyFlags;
  vint32 seqType;
  VThinker *SpecialData; // pointer to a thinker, if the poly is moving
  vint32 index; // required for LevelInfo sound sequences

  // will only free memory, but won't clear any fields
  // will free parts, but won't remove them from subsector (i.e. `sub->polyparts` will be invalid)
  void Free ();

  //TODO: make this faster by calculating overlapping rectangles or something
  void RemoveAllSubsectors ();
  void AddSubsector (subsector_t *sub);

  // this is done by `RemoveAllSubsectors()`, you don't need to call it manually
  void ResetClipSegs ();

  VVA_CHECKRESULT VVA_FORCEINLINE PolySubIter SubFirst () const noexcept { return PolySubIter(parts); }
  VVA_CHECKRESULT VVA_FORCEINLINE PolyLineIter LineFirst () const noexcept { return PolyLineIter(lines, numlines); }
  VVA_CHECKRESULT VVA_FORCEINLINE PolySegIter SegFirst () const noexcept { return PolySegIter(segs, numsegs); }

  VVA_CHECKRESULT VVA_FORCEINLINE bool Is3D () const noexcept { return posector; }
  // valid only for 3d pobjs
  VVA_CHECKRESULT VVA_FORCEINLINE sector_t *GetSector () const noexcept { return posector; }
};


//==========================================================================
//
//  PolyobjectBlock
//
//  for polyobj blockmap
//
//==========================================================================
struct polyblock_t {
  polyobj_t *polyobj;
  polyblock_t *prev;
  polyblock_t *next;
};


//==========================================================================
//
//  PolyobjectAnchor
//
//==========================================================================
struct PolyAnchorPoint_t {
  float x;
  float y;
  float height;
  vint32 tag;
  int thingidx; // can be -1
};


//==========================================================================
//
//  subsector region
//
//  this is created for each `sec_region_t`
//
//==========================================================================
struct subregion_t {
  sec_region_t *secregion;
  subregion_t *next;
  TSecPlaneRef floorplane;
  TSecPlaneRef ceilplane;
  sec_surface_t *realfloor;
  sec_surface_t *realceil;
  sec_surface_t *fakefloor; // can be `nullptr`
  sec_surface_t *fakeceil; // can be `nullptr`
  enum {
    SRF_ZEROSKY_FLOOR_HACK = 1u<<0,
    SRF_FORCE_RECREATE = 1u<<1, // autoreset
  };
  vuint32 flags;
  subsector_t *sub; // subsector for this region
  vuint32 rdindex; // unique id in renderer (sequential, can be used to index various arrays)

  VVA_FORCEINLINE void ForceRecreation () noexcept { flags |= SRF_FORCE_RECREATE; }
  VVA_FORCEINLINE void ResetForceRecreation () noexcept { flags &= ~SRF_FORCE_RECREATE; }
  VVA_FORCEINLINE bool IsForcedRecreation () const noexcept { return (flags&SRF_FORCE_RECREATE); }
};


//==========================================================================
//
//  Subsector
//
//  a subsector; references a sector
//  basically, this is a list of LineSegs, indicating
//  the visible walls that define (all or some) sides of a convex BSP leaf
//
//==========================================================================
struct subsector_t {
public:
  enum {
    SSMF_Rendered   = 1u<<0u,
    SSMF_SurfWarned = 1u<<1u,
  };

public:
  class PolySubIter {
  private:
    polyobjpart_t *ppart;
  public:
    VVA_FORCEINLINE PolySubIter (polyobjpart_t *apart) noexcept : ppart(apart) {}
    VVA_FORCEINLINE PolySubIter begin () noexcept { return PolySubIter(ppart); }
    VVA_FORCEINLINE PolySubIter end () noexcept { return PolySubIter(nullptr); }
    VVA_FORCEINLINE bool operator == (const PolySubIter &b) const noexcept { return (ppart == b.ppart); }
    VVA_FORCEINLINE bool operator != (const PolySubIter &b) const noexcept { return (ppart != b.ppart); }
    VVA_FORCEINLINE PolySubIter operator * () const noexcept { return PolySubIter(ppart); } /* required for iterator */
    VVA_FORCEINLINE void operator ++ () noexcept { if (ppart) ppart = ppart->nextsub; } /* this is enough for iterator */
    // accessors
    VVA_FORCEINLINE polyobjpart_t *part () const noexcept { return ppart; }
    VVA_FORCEINLINE polyobj_t *pobj () const noexcept { return (ppart ? ppart->pobj : nullptr); }
  };

public:
  sector_t *sector;
  subsector_t *seclink; // next subsector for this sector
  vint32 numlines; // number of segs in this subsector
  vint32 firstline; // first seg of this subsector

  polyobjpart_t *polyparts; // list of all polyparts in this subsector; linked by `nextsub`

  node_t *parent; // bsp node for this subsector
  vuint32 parentChild; // our child index in parent node

  vuint32 VisFrame;
  vuint32 updateWorldFrame;

  sector_t *deepref; // for deepwater

  //k8: i love bounding boxes! (this one doesn't store z, though)
  float bbox2d[4];

  vuint32 dlightbits; // bitmask of active dynamic lights
  vuint32 dlightframe; // `dlightbits` validity counter
  subregion_t *regions;

  polyobj_t *ownpobj; // polyobject that has this subsector as its "inner" one

  vuint32 miscFlags; // SSMF_xxx

  // reset polyobject clipsegs for this subsector
  void ResetClipSegs ();

  VVA_FORCEINLINE bool HasPObjs () const noexcept { return !!polyparts; }
  VVA_FORCEINLINE PolySubIter PObjFirst () const noexcept { return PolySubIter(polyparts); }

  VVA_FORCEINLINE bool Has3DPObjs () const noexcept {
    for (const polyobjpart_t *part = polyparts; part; part = part->nextsub) {
      if (part->pobj->Is3D()) return true;
    }
    return false;
  }

  VVA_FORCEINLINE bool isOriginalPObj () const noexcept { return sector->isOriginalPObj(); }
  VVA_FORCEINLINE bool isInnerPObj () const noexcept { return sector->isInnerPObj(); }
  VVA_FORCEINLINE bool isAnyPObj () const noexcept { return sector->isAnyPObj(); }
};


//==========================================================================
//
//  Node
//
//==========================================================================

#define BSPIDX_IS_LEAF(bidx_)         ((bidx_)&(NF_SUBSECTOR))
#define BSPIDX_IS_NON_LEAF(bidx_)     (!((bidx_)&(NF_SUBSECTOR)))
#define BSPIDX_LEAF_SUBSECTOR(bidx_)  ((bidx_)&(~(NF_SUBSECTOR)))

// indicate a leaf
enum {
  NF_SUBSECTOR = 0x80000000,
};

// BSP node
struct node_t : public TPlane {
  // bounding box for each child
  // (x,y,z) triples (min and max)
  float bbox[2][6];

  // if NF_SUBSECTOR its a subsector
  vuint32 children[2];

  node_t *parent;
  vuint32 visframe;
  vuint32 index;
  // linedef used for this node (can be nullptr if nodes builder don't have this info)
  //line_t *splitldef;
  // from the original nodes; needed to emulate original buggy "point in subsector"
  vint32 sx, sy, dx, dy; // 16.16
};


//==========================================================================
//
//  Thing
//
//  map thing definition with initialised fields for global use
//
//==========================================================================
struct mthing_t {
  vint32 tid;
  float x;
  float y;
  float height;
  /*vint32 angle;*/
  float angle, pitch, roll;
  vint32 type;
  vint32 options;
  vint32 SkillClassFilter;
  vint32 special;
  vint32 args[5]; // was `arg1`..`arg5`
  float health; // initial health; 0 means "default"
  float scaleX, scaleY; // 0 means "default"
  float gravity; // see flags
  VStr arg0str;
  vint32 renderStyle;
  float renderAlpha;
  vint32 stencilColor;
  vint32 conversationId;
  // flags
  enum {
    MTHF_UseHealth         = 1u<<0,
    MTHF_UseScaleX         = 1u<<1,
    MTHF_UseScaleY         = 1u<<2,
    MTHF_UseGravity        = 1u<<3,
    MTHF_UseArg0Str        = 1u<<4,
    MTHF_UsePitch          = 1u<<5,
    MTHF_UseRoll           = 1u<<6,
    MTHF_UseRenderStyle    = 1u<<7,
    MTHF_UseRenderAlpha    = 1u<<8,
    MTHF_UseStencilColor   = 1u<<9,
    MTHF_UseConversationId = 1u<<10,
    MTHF_CountAsSecret     = 1u<<11,
  };
  vuint32 udmfExFlags;
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
// PathTraverse flags
enum {
  PT_ADDLINES  = 1<<0, // add lines to interception list
  PT_ADDTHINGS = 1<<1, // add things to interception list
  PT_NOOPENS   = 1<<2, // no opening/sector planes checks (and no 3d pobj checks); note that pobj heights are still checked
  PT_COMPAT    = 1<<3, // compat_trace (ignore self-referenced lines, don't add them to list)
  PT_AIMTHINGS = 1<<4, // do not reject things by their z value, do not adjust thing hitpoint (used in autoaim code)
  PT_RAILINGS  = 1<<5, // block on railings
};


struct intercept_t {
  float frac; // along trace line
  enum {
    IF_NothingZero     = 0u,
    //
    IF_IsALine         = 1u<<0,
    IF_IsABlockingLine = 1u<<1, // not yet
    IF_IsAPlane        = 1u<<2, // plane hit
    IF_IsASky          = 1u<<3, // sky plane/line hit
  };
  vuint32 Flags;
  VEntity *thing;
  line_t *line;
  vint32 side; // for lines: hit side (0 or 1); otherwise undefined
  // for plane hit; `sector` is nullptr if pobj was hit
  sector_t *sector;
  polyobj_t *po;
  TPlane plane;
  // always valid
  TVec hitpoint;
};


struct linetrace_t {
public:
  TVec Start;
  TVec End;
  // subsector we ended in (can be arbitrary if trace doesn't end in map boundaries)
  // valid only for `TraceLine()` call (i.e. BSP trace)
  subsector_t *EndSubsector;
  // the following fields are valid only if trace was failed (i.e. we hit something)
  TPlane HitPlane; // set both for a line and for a flat
  line_t *HitLine; // can be `nullptr` if we hit a floor/ceiling
  float HitTime; // will be 1.0f if nothing was hit
};


struct VStateCall {
  VEntity *Item;
  VState *State;
  vint32 WasFunCall; // internal flag
  vuint8 Result; // `0` means "don't change"; non-zero means "succeed"
};


struct VMapMarkerInfo {
  vint32 id; // for convenienience; -1 means "unused"
  float x, y;
  sector_t *sector; // marker sector
  vint32 thingTid; // follow this thing (0: none)
  enum {
    F_Visible = 1u<<0,
    F_Active  = 1u<<1,
  };
  vuint32 flags;
};


// ////////////////////////////////////////////////////////////////////////// //
struct opening_t {
  float top;
  float bottom;
  float range; // top-bottom, to avoid calculations
  float lowfloor; // this is used for dropoffs: floor height on the other side (always lower then, or equal to bottom)
  TSecPlaneRef efloor;
  TSecPlaneRef eceiling;
  TSecPlaneRef elowfloor;
  // for this list
  opening_t *next;
  //opening_t *prev;
  // allocated list is double-linked, free list only using `listnext`
  opening_t *listprev;
  opening_t *listnext;

  VVA_FORCEINLINE void copyFrom (const opening_t *op) {
    if (op == this) return;
    if (op) {
      top = op->top;
      bottom = op->bottom;
      range = op->range;
      lowfloor = op->lowfloor;
      efloor = op->efloor;
      eceiling = op->eceiling;
      elowfloor = op->elowfloor;
    } else {
      top = bottom = range = lowfloor = 0.0f;
      efloor.splane = eceiling.splane = elowfloor.splane = nullptr;
    }
  }
};


struct fakefloor_t {
  sec_plane_t floorplane;
  sec_plane_t ceilplane;
  sec_params_t params;
  enum {
    FLAG_CreatedByLoader = 1u<<0,
    // not used
    FLAG_SkipFloor = 1u<<1,
    FLAG_SkipCeiling = 1u<<1,
  };
  vuint32 flags;

  bool IsCreatedByLoader () const noexcept { return (flags&FLAG_CreatedByLoader); }
  bool SkipFloor () const noexcept { return (flags&FLAG_SkipFloor); }
  bool SkipCeiling () const noexcept { return (flags&FLAG_SkipCeiling); }

  void SetCreatedByLoader () noexcept { flags |= FLAG_CreatedByLoader; }
  void SetSkipFloor (const bool v) noexcept { if (v) flags |= FLAG_SkipFloor; else flags &= FLAG_SkipFloor; }
  void SetSkipCeiling (const bool v) noexcept { if (v) flags |= FLAG_SkipCeiling; else flags &= FLAG_SkipCeiling; }
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

  DECLARE_FUNCTION(spGetNormal)
  DECLARE_FUNCTION(spGetNormalZ)
  DECLARE_FUNCTION(spGetDist)
  DECLARE_FUNCTION(spGetPointZ)
  DECLARE_FUNCTION(spDotPoint)
  DECLARE_FUNCTION(spPointDistance)
  DECLARE_FUNCTION(spPointOnSide)
  DECLARE_FUNCTION(spPointOnSideThreshold)
  DECLARE_FUNCTION(spPointOnSideFri)
  DECLARE_FUNCTION(spPointOnSide2)
  DECLARE_FUNCTION(spSphereTouches)
  DECLARE_FUNCTION(spSphereOnSide)
  DECLARE_FUNCTION(spSphereOnSide2)

  DECLARE_FUNCTION(GetPointZClamped)
  DECLARE_FUNCTION(GetPointZRevClamped)

  DECLARE_FUNCTION(GetSectorFloorPointZ)
  DECLARE_FUNCTION(SectorHas3DFloors)
  DECLARE_FUNCTION(SectorHas3DSlopes)

  DECLARE_FUNCTION(CheckPlanePass)
  DECLARE_FUNCTION(CheckPassPlanes)
  DECLARE_FUNCTION(CheckPObjPassPlanes)
};


#endif
