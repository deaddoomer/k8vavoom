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
//**  The automap code
//**
//**************************************************************************
#include "gamedefs.h"
#ifdef CLIENT
# include "screen.h"
# include "client/cl_local.h"
# include "drawer.h"
# include "widgets/ui.h"
# include "input.h"
# include "sbar.h"
# include "psim/p_entity.h"
# include "psim/p_levelinfo.h"
# include "psim/p_player.h"
# include "client/client.h"
# include "language.h"
# include "lockdefs.h"
#endif
#include "text.h"
#include "menu.h"
#include "sbar.h"

// there is no need to do this anymore: OpenGL will do it for us
//#define AM_DO_CLIPPING


// player radius for movement checking
#define PLAYERRADIUS   (16.0f)
#define MAPBLOCKUNITS  (128)

/*
#define AM_W  (640)
#define AM_H  (480-sb_height)
*/

// scale on entry
#define INITSCALEMTOF  (0.2f)

#define AMSTR_FOLLOWON   "$AMSTR_FOLLOWON"
#define AMSTR_FOLLOWOFF  "$AMSTR_FOLLOWOFF"

#define AMSTR_GRIDON   "$AMSTR_GRIDON"
#define AMSTR_GRIDOFF  "$AMSTR_GRIDOFF"

#define AMSTR_MARKEDSPOT     "$AMSTR_MARKEDSPOT"
#define AMSTR_MARKSCLEARED   "$AMSTR_MARKSCLEARED"
#define AMSTR_MARKEDSPOTDEL  "$AMSTR_MARKEDSPOTDEL"


// automap bindings
// TODO: use normal console commands for this?
enum {
  AM_BIND_NOTHING,
  AM_BIND_PANLEFT,
  AM_BIND_PANRIGHT,
  AM_BIND_PANUP,
  AM_BIND_PANDOWN,
  AM_BIND_ZOOMIN,
  AM_BIND_ZOOMOUT,
  AM_BIND_GOBIG,
  AM_BIND_FOLLOW,
  AM_BIND_GRID,
  AM_BIND_TEXTURE,
  AM_BIND_ADDMARK,
  AM_BIND_NEXTMARK,
  AM_BIND_PREVMARK,
  AM_BIND_DELMARK,
  AM_BIND_CLEARMARKS,

  AM_BIND_MAX,
};

struct AmBindInfo {
  const char *bindname;
  int bindtype;
};

static const AmBindInfo ambinds[] = {
  {"amkey_panleft",    AM_BIND_PANLEFT},
  {"amkey_panright",   AM_BIND_PANRIGHT},
  {"amkey_panup",      AM_BIND_PANUP},
  {"amkey_pandown",    AM_BIND_PANDOWN},
  {"amkey_zoomin",     AM_BIND_ZOOMIN},
  {"amkey_zoomout",    AM_BIND_ZOOMOUT},
  {"amkey_gobig",      AM_BIND_GOBIG},
  {"amkey_follow",     AM_BIND_FOLLOW},
  {"amkey_grid",       AM_BIND_GRID},
  {"amkey_texture",    AM_BIND_TEXTURE},
  {"amkey_addmark",    AM_BIND_ADDMARK},
  {"amkey_nextmark",   AM_BIND_NEXTMARK},
  {"amkey_prevmark",   AM_BIND_PREVMARK},
  {"amkey_delmark",    AM_BIND_DELMARK},
  {"amkey_clearmarks", AM_BIND_CLEARMARKS},
  {nullptr, AM_BIND_NOTHING},
};

enum {
  AM_PTYPE_NOTMINE,
  AM_PTYPE_REPEAT,
  AM_PTYPE_PRESS,
  AM_PTYPE_RELEASE,
};

static int amkstate[AM_BIND_MAX];


/*
//#define AM_STARTKEY      K_TAB
#define AM_PANUPKEY      K_UPARROW
#define AM_PANDOWNKEY    K_DOWNARROW
#define AM_PANLEFTKEY    K_LEFTARROW
#define AM_PANRIGHTKEY   K_RIGHTARROW
#define AM_ZOOMINKEY     '='
#define AM_ZOOMOUTKEY    '-'
//#define AM_ENDKEY        K_TAB
#define AM_GOBIGKEY      '0'
#define AM_FOLLOWKEY     'h'
#define AM_GRIDKEY       'g'
#define AM_MARKKEY       'm'
#define AM_NEXTMARKKEY   'n'
#define AM_CLEARMARKKEY  'c'
#define AM_TOGGLETEXKEY  't'
*/

// how much the automap moves window per tic in frame-buffer coordinates
// moves 140 pixels in 1 second
#define F_PANINC  (4)
// how much zoom-in per tic
// goes to 2x in 1 second
#define M_ZOOMIN  (1.02f)
// how much zoom-out per tic
// pulls out to 0.5x in 1 second
#define M_ZOOMOUT  (1.0f/1.02f)

#define AM_NUMMARKPOINTS       (10)
#define AM_NUMMARKPOINTS_NUMS  (10)

// translates between frame-buffer and map distances
#define FTOM(x)   ((float)(x)*scale_ftom)
#define MTOF(x)   ((int)((x)*scale_mtof))
#define MTOFF(x)  ((x)*scale_mtof)
// translates between frame-buffer and map coordinates
#define CXMTOF(x)   (MTOF((x)-m_x)-f_x)
#define CYMTOF(y)   (f_h-MTOF((y)-m_y)+f_y)
#define CXMTOFF(x)  (MTOFF((x)-m_x)-f_x)
#define CYMTOFF(y)  (f_h-MTOFF((y)-m_y)+f_y)

#define FRACBITS  (16)
#define FRACUNIT  (1<<FRACBITS)

#define FL(x)  ((float)(x)/(float)FRACUNIT)
#define FX(x)  (fixed_t)((x)*FRACUNIT)


typedef int fixed_t;

struct fpoint_t {
  int x;
  int y;
};

struct fline_t {
  fpoint_t a;
  fpoint_t b;
};

struct mpoint_t {
  float x;
  float y;
};


struct MarkPoint {
  float x;
  float y;
  bool active;

  inline MarkPoint () noexcept : x(0.0f), y(0.0f), active(false) {}

  inline bool isActive () const noexcept { return active; }
  inline void setActive (bool v) noexcept { active = v; }
  inline void activate () noexcept { setActive(true); }
  inline void deactivate () noexcept { setActive(false); }
};

struct mline_t {
  mpoint_t a;
  mpoint_t b;
};


static int automapactive = 0; // In AutoMap mode? 0: no; 1: normal; -1: overlay
static bool automapUpdateSeen = true; // set to `true` to trigger line visibility update (autoresets)

static VCvarB am_active("am_active", false, "Is automap active?", CVAR_NoShadow);
extern VCvarI screen_size;
extern VCvarB ui_freemouse;

VCvarB am_always_update("am_always_update", true, "Update non-overlay automap?", CVAR_Archive|CVAR_NoShadow);

static VClass *keyClass = nullptr;


void AM_Dirty () { automapUpdateSeen = true; }

bool AM_IsActive () { return !!automapactive; }
bool AM_IsOverlay () { return (automapactive < 0); } // returns `false` if automap is not active
bool AM_IsFullscreen () { return (automapactive > 0); } // returns `false` if automap is not active


/*
#define AM_W  (640)
#define AM_H  (480-sb_height)
*/
static VVA_FORCEINLINE int getAMWidth () {
  if (VirtualWidth < 320) return 320;
  return VirtualWidth;
}

static VVA_FORCEINLINE int getAMHeight () {
  int res = VirtualWidth;
  if (res < 240) res = 240;
  if (screen_size < 11) res -= sb_height;
  return res;
}


static VCvarB draw_world_timer("draw_world_timer", false, "Draw playing time?", CVAR_Archive|CVAR_NoShadow);
static VCvarF draw_map_stats_alpha("draw_map_stats_alpha", "0.6", "Non-automap map stats opacity.", CVAR_Archive|CVAR_NoShadow);
static VCvarB draw_map_stats("draw_map_stats", false, "Draw map stats when not on automap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB draw_map_stats_title("draw_map_stats_title", true, "Draw map title when not on automap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB draw_map_stats_name("draw_map_stats_name", true, "Draw internal map name when not on automap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB draw_map_stats_kills("draw_map_stats_kills", true, "Draw map kill stats when not on automap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB draw_map_stats_items("draw_map_stats_items", false, "Draw map item stats when not on automap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB draw_map_stats_secrets("draw_map_stats_secrets", false, "Draw map secret stats when not on automap?", CVAR_Archive|CVAR_NoShadow);

static VCvarB minimap_active("minimap_active", false, "Is minimap active?", CVAR_Archive|CVAR_NoShadow);
static VCvarB minimap_rotate("minimap_rotate", false, "Rotate minimap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB minimap_draw_keys("minimap_draw_keys", true, "Draw seen keys on minimap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB minimap_draw_player("minimap_draw_player", true, "Draw player arrow on minimap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB minimap_draw_border("minimap_draw_border", false, "Draw minimap border rectangle?", CVAR_Archive|CVAR_NoShadow);
static VCvarB minimap_draw_blocking_things("minimap_draw_blocking_things", false, "Draw minimap blocking things?", CVAR_Archive|CVAR_NoShadow);
static VCvarF minimap_scale("minimap_scale", "8", "Minimap scale (inverted).", CVAR_Archive|CVAR_NoShadow);
static VCvarF minimap_darken("minimap_darken", "0.4", "Minimap widget darkening.", CVAR_Archive|CVAR_NoShadow);
static VCvarF minimap_alpha("minimap_alpha", "0.6", "Minimap opacity.", CVAR_Archive|CVAR_NoShadow);
static VCvarF minimap_position_x("minimap_position_x", "-1", "Horizontal position of the minimap.", CVAR_Archive|CVAR_NoShadow);
static VCvarF minimap_position_y("minimap_position_y", "-0.88", "Vertical position of the minimap.", CVAR_Archive|CVAR_NoShadow);
static VCvarF minimap_size_x("minimap_size_x", "0.2", "Horizontal size of the minimap.", CVAR_Archive|CVAR_NoShadow);
static VCvarF minimap_size_y("minimap_size_y", "0.24", "Vertical size of the minimap.", CVAR_Archive|CVAR_NoShadow);

static VCvarS minimap_color_border("minimap_color_border", "ff 7f 00", "Minimap color: border.", CVAR_Archive|CVAR_NoShadow);

static VCvarB am_overlay("am_overlay", true, "Show automap in overlay mode?", CVAR_Archive|CVAR_NoShadow);
static VCvarF am_back_darken("am_back_darken", "0", "Overlay automap darken factor", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_full_lines("am_full_lines", false, "Render full line even if only some parts of it were seen?", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_draw_grid("am_draw_grid", false, "Draw automap grid?", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_mark_blink("am_mark_blink", true, "Should marks blink (to make them better visible)?", CVAR_Archive|CVAR_NoShadow);

// automap colors
static VCvarS am_color_wall("am_color_wall", "d0 b0 85", "Automap color: normal walls.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_tswall("am_color_tswall", "61 64 5f", "Automap color: same-height two-sided walls.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_fdwall("am_color_fdwall", "a0 6c 40", "Automap color: floor level change.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_cdwall("am_color_cdwall", "94 94 ac", "Automap color: ceiling level change.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_exwall("am_color_exwall", "7b 4b 27", "Automap color: walls with extra floors.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_secretwall("am_color_secretwall", "ff 7f 00", "Automap color: secret walls.", CVAR_Archive|CVAR_NoShadow);
//static VCvarS am_color_power("am_color_power", "7d 83 79", "Automap color: autorevealed walls.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_power("am_color_power", "2f 4f 9f", "Automap color: autorevealed walls.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_grid("am_color_grid", "4d 9d 42", "Automap color: grid.", CVAR_Archive|CVAR_NoShadow);

static VCvarS am_color_thing("am_color_thing", "00 c0 00", "Automap color: thing.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_solid("am_color_solid", "e0 e0 e0", "Automap color: solid thing.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_monster("am_color_monster", "ff 00 00", "Automap color: monster.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_missile("am_color_missile", "cf 4f 00", "Automap color: missile.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_dead("am_color_dead", "80 80 80", "Automap color: dead thing.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_invisible("am_color_invisible", "f0 00 f0", "Automap color: invisible thing.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_mmap_blocking_thing("am_color_mmap_blocking_thing", "90 90 90", "Minimap color: blocking thing.", CVAR_Archive|CVAR_NoShadow);

static VCvarS am_color_current_mark("am_color_current_mark", "00 ff 00", "Automap color: current map mark.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_mark_blink("am_color_mark_blink", "df 5f 00", "Automap color: mark blink color.", CVAR_Archive|CVAR_NoShadow);

//static VCvarS am_color_light_static("am_color_light_static", "ff ff ff", "Automap color: static light.", CVAR_Archive|CVAR_NoShadow);
//static VCvarS am_color_light_dynamic("am_color_light_dynamic", "00 ff ff", "Automap color: dynamic light.", CVAR_Archive|CVAR_NoShadow);

static VCvarS am_color_player("am_color_player", "e6 e6 e6", "Automap color: player.", CVAR_Archive|CVAR_NoShadow);
static VCvarS am_color_miniseg("am_color_miniseg", "7f 00 7f", "Automap color: minisegs.", CVAR_Archive|CVAR_NoShadow);

static VCvarB dbg_am_no_player_arrow("dbg_am_no_player_arrow", false, "Type of player arrow.", CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);

static VCvarI am_player_arrow("am_player_arrow", 0, "Type of player arrow.", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_follow_player("am_follow_player", true, "Should automap follow player?", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_rotate("am_rotate", false, "Should automap rotate?", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_show_stats("am_show_stats", false, "Show stats on automap?", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_show_map_name("am_show_map_name", false, "Show internal map name on automap?", CVAR_Archive|CVAR_NoShadow);

static VCvarI am_cheating("am_cheating", "0", "Oops! Automap cheats!", CVAR_Cheat|CVAR_NoShadow);
static VCvarB am_show_keys_cheat("am_show_keys_cheat", false, "Show keys on automap.", CVAR_Cheat|CVAR_NoShadow);
static VCvarB am_show_secrets("am_show_secrets", false, "Show secret walls on automap!", CVAR_NoShadow);
static VCvarB am_show_minisegs("am_show_minisegs", false, "Show minisegs on automap (cheating should be turned on).", CVAR_NoShadow);
static VCvarB am_show_static_lights("am_show_static_lights", false, "Show static lights on automap (cheating should be turned on).", CVAR_NoShadow);
static VCvarB am_show_dynamic_lights("am_show_dynamic_lights", false, "Show static lights on automap (cheating should be turned on).", CVAR_NoShadow);
static VCvarB am_show_rendered_nodes("am_show_rendered_nodes", false, "Show rendered BSP nodes on automap (cheating should be turned on).", CVAR_NoShadow);
static VCvarB am_show_rendered_subs("am_show_rendered_subs", false, "Show rendered subsectors on automap (cheating should be turned on).", CVAR_NoShadow);

static VCvarI am_pobj_debug("am_pobj_debug", "0", "Oops! Automap cheats!", CVAR_Cheat|CVAR_NoShadow);

static VCvarB am_render_thing_sprites("am_render_thing_sprites", false, "Render sprites instead of triangles for automap things?", CVAR_Archive|CVAR_NoShadow);

static VCvarF am_overlay_alpha("am_overlay_alpha", "0.4", "Automap overlay alpha", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_show_parchment("am_show_parchment", true, "Show automap parchment?", CVAR_Archive|CVAR_NoShadow);

static VCvarF am_texture_alpha("am_texture_alpha", "0.6", "Automap texture alpha", CVAR_Archive|CVAR_NoShadow);
static VCvarI am_draw_type("am_draw_type", "0", "Automap rendering type (0:lines; 1:floors; 2:ceilings)", CVAR_Archive|CVAR_NoShadow);
static VCvarB am_draw_texture_lines("am_draw_texture_lines", true, "Draw automap lines on textured automap?", CVAR_Archive|CVAR_NoShadow);
// used by drawer
VCvarB am_draw_texture_with_bsp("am_draw_texture_with_bsp", true, "Draw textured automap using BSP tree?", CVAR_Archive|CVAR_NoShadow);


static VCvarB am_default_whole("am_default_whole", false, "Default scale is \"show all\"?", CVAR_Archive|CVAR_NoShadow);

static VCvarB am_draw_keys("am_draw_keys", true, "Draw keys on automap?", CVAR_Archive|CVAR_NoShadow);
static VCvarF am_keys_blink_time("am_keys_blink_time", "0.4", "Keys blinking time in seconds (set to 0 to disable)", CVAR_Archive|CVAR_NoShadow);


//static VCvarS am_cheat_pobj_active_color("am_pobj_active_color", "00 ff 00", "Automap color: active part of polyobject seg.", CVAR_Archive|CVAR_NoShadow);
//static VCvarS am_cheat_pobj_inactive_color("am_pobj_inactive_color", "ff 00 00", "Automap color: inactive part of polyobject seg.", CVAR_Archive|CVAR_NoShadow);


// cached colors
static ColorCV WallColor(&am_color_wall, &am_overlay_alpha);
static ColorCV TSWallColor(&am_color_tswall, &am_overlay_alpha);
static ColorCV FDWallColor(&am_color_fdwall, &am_overlay_alpha);
static ColorCV CDWallColor(&am_color_cdwall, &am_overlay_alpha);
static ColorCV EXWallColor(&am_color_exwall, &am_overlay_alpha);
static ColorCV SecretWallColor(&am_color_secretwall, &am_overlay_alpha);
static ColorCV PowerWallColor(&am_color_power, &am_overlay_alpha);
static ColorCV GridColor(&am_color_grid, &am_overlay_alpha);
static ColorCV ThingColor(&am_color_thing, &am_overlay_alpha);
static ColorCV InvisibleThingColor(&am_color_invisible, &am_overlay_alpha);
static ColorCV SolidThingColor(&am_color_solid, &am_overlay_alpha);
static ColorCV MMapBlockThingColor(&am_color_mmap_blocking_thing, &am_overlay_alpha);
static ColorCV MonsterColor(&am_color_monster, &am_overlay_alpha);
static ColorCV MissileColor(&am_color_missile, &am_overlay_alpha);
static ColorCV DeadColor(&am_color_dead, &am_overlay_alpha);
static ColorCV PlayerColor(&am_color_player, &am_overlay_alpha);
static ColorCV MinisegColor(&am_color_miniseg, &am_overlay_alpha);
static ColorCV CurrMarkColor(&am_color_current_mark, &am_overlay_alpha);
static ColorCV MarkBlinkColor(&am_color_mark_blink, &am_overlay_alpha);

static ColorCV MinimapBorderColor(&minimap_color_border, &minimap_alpha);

//static ColorCV PObjActiveColor(&am_cheat_pobj_active_color, &am_overlay_alpha);
//static ColorCV PObjInactiveColor(&am_cheat_pobj_inactive_color, &am_overlay_alpha);

static int leveljuststarted = 1; // kluge until AM_LevelInit() is called
static int amWholeScale = -1; // -1: unknown

// location of window on screen
static int f_x;
static int f_y;

// size of window on screen
static int f_w;
static int f_h;

static mpoint_t m_paninc; // how far the window pans each tic (map coords)
static float mtof_zoommul; // how far the window zooms in each tic (map coords)
static float ftom_zoommul; // how far the window zooms in each tic (fb coords)

static float m_x; // LL x,y where the window is on the map (map coords)
static float m_y;
static float m_x2; // UR x,y where the window is on the map (map coords)
static float m_y2;

// the rectangle that is guaranteed to cover the visible area (map coords)
// calculated in drawer, if necessary
static float mext_x0, mext_y0;
static float mext_x1, mext_y1;

// width/height of window on map (map coords)
static float m_w;
static float m_h;

// based on level size
static float min_x;
static float min_y;
static float max_x;
static float max_y;

static float max_w; // max_x-min_x
static float max_h; // max_y-min_y

// based on player size
static float min_w;
static float min_h;


static float min_scale_mtof; // used to tell when to stop zooming out
static float max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static float old_m_x;
static float old_m_y;
static float old_m_w;
static float old_m_h;

// old location used by the Follower routine
static mpoint_t f_oldloc;

// used by MTOF to scale from map-to-frame-buffer coords
static float scale_mtof = INITSCALEMTOF;
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static float scale_ftom;

static float start_scale_mtof = INITSCALEMTOF;

static mpoint_t oldplr;

static bool mapMarksAllowed = false;
static int marknums[AM_NUMMARKPOINTS_NUMS] = {0}; // numbers used for marking by the automap
static MarkPoint markpoints[AM_NUMMARKPOINTS]; // where the points are
static int markActive = -1;

static int mappic = 0;
static int mapheight = 64;
static short mapystart = 0; // y-value for the start of the map bitmap...used in the paralax stuff.
static short mapxstart = 0; //x-value for the bitmap.

static bool stopped = true;

static VName lastmap;
static VName lastmakrsmap;


//  The vector graphics for the automap.

//  A line drawing of the player pointing right, starting from the middle.
#define R  (8.0f*PLAYERRADIUS/7.0f)
static const mline_t player_arrow1[] = {
  { { -R+R/8.0f, 0.0f }, { R, 0.0f } }, // -----
  { { R, 0.0f }, { R-R*0.5f, R/4.0f } },  // ----->
  { { R, 0.0f }, { R-R*0.5f, -R/4.0f } },
  { { -R+R/8.0f, 0.0f }, { -R-R/8.0f, R/4.0f } }, // >---->
  { { -R+R/8.0f, 0.0f }, { -R-R/8.0f, -R/4.0f } },
  { { -R+3.0f*R/8.0f, 0.0f }, { -R+R/8.0f, R/4.0f } }, // >>--->
  { { -R+3.0f*R/8.0f, 0.0f }, { -R+R/8.0f, -R/4.0f } }
};
#define NUMPLYRLINES1  (sizeof(player_arrow1)/sizeof(mline_t))


static const mline_t player_arrow2[] = {
  { { -R+R/4.0f, 0.0f }, { 0.0f, 0.0f} }, // center line.
  { { -R+R/4.0f, R/8.0f }, { R, 0.0f} }, // blade
  { { -R+R/4.0f, -R/8.0f }, { R, 0.0f } },
  { { -R+R/4.0f, -R/4.0f }, { -R+R/4.0f, R/4.0f } }, // crosspiece
  { { -R+R/8.0f, -R/4.0f }, { -R+R/8.0f, R/4.0f } },
  { { -R+R/8.0f, -R/4.0f }, { -R+R/4.0f, -R/4.0f } }, //crosspiece connectors
  { { -R+R/8.0f, R/4.0f }, { -R+R/4.0f, R/4.0f } },
  { { -R-R/4.0f, R/8.0f }, { -R-R/4.0f, -R/8.0f } }, //pommel
  { { -R-R/4.0f, R/8.0f }, { -R+R/8.0f, R/8.0f } },
  { { -R-R/4.0f, -R/8 }, { -R+R/8.0f, -R/8.0f } }
};
#define NUMPLYRLINES2  (sizeof(player_arrow2)/sizeof(mline_t))


static const mline_t player_arrow_ddt[] = {
  { { -R+R/8, 0 }, { R, 0 } }, // -----
  { { R, 0 }, { R-R/2, R/6 } },  // ----->
  { { R, 0 }, { R-R/2, -R/6 } },
  { { -R+R/8, 0 }, { -R-R/8, R/6 } }, // >----->
  { { -R+R/8, 0 }, { -R-R/8, -R/6 } },
  { { -R+3*R/8, 0 }, { -R+R/8, R/6 } }, // >>----->
  { { -R+3*R/8, 0 }, { -R+R/8, -R/6 } },
  { { -R/2, 0 }, { -R/2, -R/6 } }, // >>-d--->
  { { -R/2, -R/6 }, { -R/2+R/6, -R/6 } },
  { { -R/2+R/6, -R/6 }, { -R/2+R/6, R/4 } },
  { { -R/6, 0 }, { -R/6, -R/6 } }, // >>-dd-->
  { { -R/6, -R/6 }, { 0, -R/6 } },
  { { 0, -R/6 }, { 0, R/4 } },
  { { R/6, R/4 }, { R/6, -R/7 } }, // >>-ddt->
  { { R/6, -R/7 }, { R/6+R/32, -R/7-R/32 } },
  { { R/6+R/32, -R/7-R/32 }, { R/6+R/10, -R/7 } }
};
#define NUMPLYRLINES3  (sizeof(player_arrow_ddt)/sizeof(mline_t))

#undef R

#define R (1.0f)
static const mline_t thintriangle_guy[] =
{
  { { -0.5f*R, -0.7f*R }, { R, 0.0f } },
  { { R, 0.0f }, { -0.5f*R, 0.7f*R } },
  { { -0.5f*R, 0.7f*R }, { -0.5f*R, -0.7f*R } }
};
#undef R
#define NUMTHINTRIANGLEGUYLINES  (sizeof(thintriangle_guy)/sizeof(mline_t))


static TMapNC<VClass *, int> spawnSprIndex;


// ////////////////////////////////////////////////////////////////////////// //
static VVA_OKUNUSED VVA_FORCEINLINE int VScrTransX640 (int x) { return (int)(x*VirtualWidth/640.0f); }
static VVA_OKUNUSED VVA_FORCEINLINE int VScrTransY480 (int y) { return (int)(y*VirtualHeight/480.0f); }


//==========================================================================
//
//  getSpriteIndex
//
//==========================================================================
static int getSpriteIndex (VClass *cls) {
  vint32 sprIdx = -1;
  auto spip = spawnSprIndex.get(cls);
  if (spip) return *spip;
  // find id
  VStateLabel *lbl = cls->FindStateLabel("Spawn");
  if (lbl && lbl->State) {
    if (R_AreSpritesPresent(lbl->State->SpriteIndex)) {
      //GCon->Logf("found spawn sprite for '%s'", cls->GetName());
      sprIdx = lbl->State->SpriteIndex;
    }
  }
  spawnSprIndex.put(cls, sprIdx);
  return sprIdx;
}


//==========================================================================
//
//  AM_addMark
//
//  adds a marker at the current location
//
//==========================================================================
static int AM_addMark () {
  if (!mapMarksAllowed) return -1;
  // if the player just deleted a mark, reuse its slot
  if (markActive >= 0 && markActive < AM_NUMMARKPOINTS && !markpoints[markActive].isActive()) {
    MarkPoint &mp = markpoints[markActive];
    mp.x = m_x+m_w*0.5f;
    mp.y = m_y+m_h*0.5f;
    mp.activate();
    return markActive;
  }
  // find empty mark slot
  for (int mn = 0; mn < AM_NUMMARKPOINTS; ++mn) {
    MarkPoint &mp = markpoints[mn];
    if (!mp.isActive()) {
      if (markActive == mn) markActive = -1;
      mp.x = m_x+m_w*0.5f;
      mp.y = m_y+m_h*0.5f;
      mp.activate();
      return mn;
    }
  }
  return -1;
}


//==========================================================================
//
//  AM_clearMarks
//
//==========================================================================
static bool AM_clearMarks () {
  bool res = false;
  markActive = -1;
  for (int mn = 0; mn < AM_NUMMARKPOINTS; ++mn) {
    MarkPoint &mp = markpoints[mn];
    res = (res || mp.isActive());
    mp.deactivate();
  }
  //GCon->Logf(NAME_Debug, "*** marks cleared!");
  return res;
}


//==========================================================================
//
//  AM_GetMaxMarks
//
//  automap marks API
//
//==========================================================================
int AM_GetMaxMarks () {
  return AM_NUMMARKPOINTS;
}


//==========================================================================
//
//  AM_IsMarkActive
//
//==========================================================================
bool AM_IsMarkActive (int index) {
  return (index >= 0 && index < AM_NUMMARKPOINTS ? markpoints[index].isActive() : false);
}


//==========================================================================
//
//  AM_GetMarkX
//
//==========================================================================
float AM_GetMarkX (int index) {
  return (index >= 0 && index < AM_NUMMARKPOINTS && markpoints[index].isActive() ? markpoints[index].x : 0.0f);
}


//==========================================================================
//
//  AM_GetMarkY
//
//==========================================================================
float AM_GetMarkY (int index) {
  return (index >= 0 && index < AM_NUMMARKPOINTS && markpoints[index].isActive() ? markpoints[index].y : 0.0f);
}


//==========================================================================
//
//  AM_ClearMarks
//
//==========================================================================
void AM_ClearMarks () {
  (void)AM_clearMarks();
}


//==========================================================================
//
//  AM_SetMarkXY
//
//==========================================================================
void AM_SetMarkXY (int index, float x, float y) {
  if (index < 0 || index >= AM_NUMMARKPOINTS) return;
  MarkPoint &mp = markpoints[index];
  mp.x = x;
  mp.y = y;
  mp.activate();
}


//==========================================================================
//
//  AM_Init
//
//==========================================================================
void AM_Init () {
  automapUpdateSeen = true;
  memset((void *)&amkstate[0], 0, sizeof(amkstate));
}


//==========================================================================
//
//  AM_ClearAutomap
//
//==========================================================================
void AM_ClearAutomap () {
  if (!GClLevel) return;
  for (int i = 0; i < GClLevel->NumLines; ++i) {
    line_t &line = GClLevel->Lines[i];
    line.flags &= ~ML_MAPPED;
    line.exFlags &= ~(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
  }
  for (int i = 0; i < GClLevel->NumSegs; ++i) {
    GClLevel->Segs[i].flags &= ~SF_MAPPED;
  }
  for (unsigned f = 0; f < (unsigned)GClLevel->NumSubsectors; ++f) {
    GClLevel->Subsectors[f].miscFlags &= ~subsector_t::SSMF_Rendered;
  }
  automapUpdateSeen = true;
}


//==========================================================================
//
//  AM_activateNewScale
//
//==========================================================================
static void AM_activateNewScale () {
  m_x += m_w*0.5f;
  m_y += m_h*0.5f;
  m_w = FTOM(f_w);
  m_h = FTOM(f_h);
  m_x -= m_w*0.5f;
  m_y -= m_h*0.5f;
  m_x2 = m_x+m_w;
  m_y2 = m_y+m_h;
}


//==========================================================================
//
//  AM_saveScaleAndLoc
//
//==========================================================================
static void AM_saveScaleAndLoc () {
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;
}


//==========================================================================
//
//  AM_restoreScaleAndLoc
//
//==========================================================================
static void AM_restoreScaleAndLoc () {
  m_w = old_m_w;
  m_h = old_m_h;
  if (!am_follow_player) {
    m_x = old_m_x;
    m_y = old_m_y;
  } else {
    m_x = cl->ViewOrg.x-m_w*0.5f;
    m_y = cl->ViewOrg.y-m_h*0.5f;
  }
  m_x2 = m_x+m_w;
  m_y2 = m_y+m_h;

  // change the scaling multipliers
  scale_mtof = (float)f_w/m_w;
  scale_ftom = 1.0f/scale_mtof;
}


//==========================================================================
//
//  AM_minOutWindowScale
//
//  set the window scale to the maximum size
//
//==========================================================================
static void AM_minOutWindowScale () {
  scale_mtof = (amWholeScale ? min_scale_mtof : min2(0.01f, min_scale_mtof));
  scale_ftom = 1.0f/scale_mtof;
  AM_activateNewScale();
}


//==========================================================================
//
//  AM_maxOutWindowScale
//
//  set the window scale to the minimum size
//
//==========================================================================
static void AM_maxOutWindowScale () {
  scale_mtof = max_scale_mtof;
  scale_ftom = 1.0f/scale_mtof;
  AM_activateNewScale();
}


//==========================================================================
//
//  AM_findMinMaxBoundaries
//
//  Determines bounding box of all vertices, sets global variables
//  controlling zoom range.
//
//==========================================================================
static void AM_findMinMaxBoundaries () {
  min_x = min_y =  99999.0f;
  max_x = max_y = -99999.0f;

  for (int i = 0; i < GClLevel->NumVertexes; ++i) {
         if (GClLevel->Vertexes[i].x < min_x) min_x = GClLevel->Vertexes[i].x;
    else if (GClLevel->Vertexes[i].x > max_x) max_x = GClLevel->Vertexes[i].x;
    // fuck you, gshitcc
         if (GClLevel->Vertexes[i].y < min_y) min_y = GClLevel->Vertexes[i].y;
    else if (GClLevel->Vertexes[i].y > max_y) max_y = GClLevel->Vertexes[i].y;
  }

  max_w = max_x-min_x;
  max_h = max_y-min_y;

  min_w = 2.0f*PLAYERRADIUS; // const? never changed?
  min_h = 2.0f*PLAYERRADIUS;

  float a = (float)f_w/max_w;
  float b = (float)f_h/max_h;

  min_scale_mtof = (a < b ? a : b);
  max_scale_mtof = (float)f_h/(2.0f*PLAYERRADIUS);
}


//==========================================================================
//
//  AM_ScrollParchment
//
//==========================================================================
static void AM_ScrollParchment (float dmapx, float dmapy) {
  mapxstart -= (short)(dmapx*scale_mtof/12)>>12;
  mapystart -= (short)(dmapy*scale_mtof/12)>>12;

  if (mappic > 0) {
    int pwidth = (int)GTextureManager.TextureWidth(mappic); //320;
    int pheight = (int)GTextureManager.TextureHeight(mappic);
    if (pwidth < 1) pwidth = 1;
    if (pheight < 1) pheight = 1;

    while (mapxstart > 0) mapxstart -= pwidth;
    while (mapxstart <= -pwidth) mapxstart += pwidth;
    while (mapystart > 0) mapystart -= pheight;
    while (mapystart <= -pheight) mapystart += pheight;
  }
}


//==========================================================================
//
//  AM_changeWindowLoc
//
//==========================================================================
static void AM_changeWindowLoc () {
  if (m_paninc.x || m_paninc.y) {
    am_follow_player = 0;
    f_oldloc.x = 99999.0f;
  }

  float oldmx = m_x, oldmy = m_y;

  m_x += m_paninc.x;
  m_y += m_paninc.y;

       if (m_x+m_w*0.5f > max_x) m_x = max_x-m_w*0.5f;
  else if (m_x+m_w*0.5f < min_x) m_x = min_x-m_w*0.5f;
  // fuck you, gshitcc
       if (m_y+m_h*0.5f > max_y) m_y = max_y-m_h*0.5f;
  else if (m_y+m_h*0.5f < min_y) m_y = min_y-m_h*0.5f;

  m_x2 = m_x+m_w;
  m_y2 = m_y+m_h;

  AM_ScrollParchment(m_x-oldmx, oldmy-m_y);
}


//==========================================================================
//
//  AM_initVariables
//
//==========================================================================
static void AM_initVariables () {
  automapactive = (am_overlay ? -1 : 1);

  f_oldloc.x = 99999.0f;

  m_paninc.x = m_paninc.y = 0.0f;
  ftom_zoommul = 1.0f;
  mtof_zoommul = 1.0f;

  m_w = FTOM(f_w);
  m_h = FTOM(f_h);

  oldplr.x = cl->ViewOrg.x;
  oldplr.y = cl->ViewOrg.y;
  m_x = cl->ViewOrg.x-m_w*0.5f;
  m_y = cl->ViewOrg.y-m_h*0.5f;
  AM_changeWindowLoc();

  // for saving & restoring
  old_m_x = m_x;
  old_m_y = m_y;
  old_m_w = m_w;
  old_m_h = m_h;
}


//==========================================================================
//
//  AM_loadPics
//
//==========================================================================
static void AM_loadPics () {
  if (W_CheckNumForName(NAME_ammnum0) >= 0) {
    mapMarksAllowed = true;
    for (int i = 0; i < AM_NUMMARKPOINTS_NUMS; ++i) {
      marknums[i] = GTextureManager.AddPatch(va("ammnum%d", i), TEXTYPE_Pic, true); // silent
      if (marknums[i] <= 0) { mapMarksAllowed = false; break; }
    }
  }
  mappic = GTextureManager.AddPatch(NAME_autopage, TEXTYPE_Autopage, true); // silent
  mapheight = (mappic > 0 ? (int)GTextureManager.TextureHeight(mappic) : 64);
  if (mapheight < 1) mapheight = 1;
}


//==========================================================================
//
//  AM_ClearMarksIfMapChanged
//
//==========================================================================
void AM_ClearMarksIfMapChanged (VLevel *currmap) {
  if (!currmap || lastmakrsmap != currmap->MapName) {
    (void)AM_clearMarks();
    lastmakrsmap = (currmap ? currmap->MapName : NAME_None);
  }
}


//==========================================================================
//
//  AM_LevelInit
//
//  should be called at the start of every level
//  right now, i figure it out myself
//
//==========================================================================
static void AM_LevelInit () {
  leveljuststarted = 0;
  amWholeScale = -1;

  f_x = f_y = 0;
  f_w = ScreenWidth;
  f_h = ScreenHeight-(screen_size < 11 ? SB_RealHeight() : 0);

  //AM_ClearMarksIfMapChanged(GClLevel);
  //if (clearMarks) (void)AM_clearMarks();
  mapxstart = mapystart = 0;

  AM_findMinMaxBoundaries();
  scale_mtof = min_scale_mtof/0.7f;
  if (scale_mtof > max_scale_mtof) scale_mtof = min_scale_mtof;
  scale_ftom = 1.0f/scale_mtof;
  start_scale_mtof = scale_mtof;

  lastmap = GClLevel->MapName;
  automapUpdateSeen = true;
}


//==========================================================================
//
//  AM_Stop
//
//==========================================================================
void AM_Stop () {
  automapactive = 0;
  stopped = true;
  am_active = false;
  memset((void *)&amkstate[0], 0, sizeof(amkstate));
#ifdef CLIENT
  GInput->SetAutomapActive(false);
#endif
}


//==========================================================================
//
//  AM_SetActive
//
//==========================================================================
void AM_SetActive (bool act) {
  am_active = act;
}


//==========================================================================
//
//  AM_Start
//
//==========================================================================
static void AM_Start () {
  //if (!stopped) AM_Stop();
  if (!keyClass) keyClass = VClass::FindClass("Key");
  stopped = false;
  if (lastmap != GClLevel->MapName) AM_LevelInit();
  AM_ClearMarksIfMapChanged(GClLevel);
  AM_initVariables(); // this sets `automapactive`
  AM_loadPics();
  if (amWholeScale < 0) {
    amWholeScale = (am_default_whole ? 1 : 0);
    if (amWholeScale) {
      AM_saveScaleAndLoc();
      AM_minOutWindowScale();
    }
  }
  mtof_zoommul = 1.0f;
  ftom_zoommul = 1.0f;
  am_active = true;
  memset((void *)&amkstate[0], 0, sizeof(amkstate));
#ifdef CLIENT
  GInput->SetAutomapActive(true);
#endif
}


//==========================================================================
//
//  AM_Check
//
//==========================================================================
static void AM_Check () {
  if (am_active != !!automapactive) {
    if (am_active) AM_Start(); else AM_Stop();
  }
  if (am_active) automapactive = (am_overlay ? -1 : 1);
}


//==========================================================================
//
//  AM_changeWindowScale
//
//  Zooming
//
//==========================================================================
static void AM_changeWindowScale () {
  // change the scaling multipliers
  scale_mtof = scale_mtof*mtof_zoommul;
  scale_ftom = 1.0f/scale_mtof;
       if (scale_mtof < (amWholeScale ? min_scale_mtof : min2(0.01f, min_scale_mtof))) AM_minOutWindowScale();
  else if (scale_mtof > max_scale_mtof) AM_maxOutWindowScale();
  else AM_activateNewScale();
}


//==========================================================================
//
//  AM_rotate
//
//  Rotation in 2D. Used to rotate player arrow line character.
//
//==========================================================================
static VVA_FORCEINLINE void AM_rotate (float *x, float *y, float a) {
  float s, c;
  msincos(a, &s, &c);
  const float tmpx = *x*c-*y*s;
  *y = *x*s+*y*c;
  *x = tmpx;
}


//==========================================================================
//
//  AM_rotateAround
//
//==========================================================================
static VVA_FORCEINLINE void AM_rotateAround (float *x, float *y, float a, float cx, float cy) {
  float s, c;
  msincos(a, &s, &c);
  const float x0 = (*x)-cx;
  const float y0 = (*y)-cy;
  *x = (x0*c-y0*s)+cx;
  *y = (x0*s+y0*c)+cy;
}


//==========================================================================
//
//  AM_rotatePoint
//
//==========================================================================
static VVA_FORCEINLINE void AM_rotatePoint (float *x, float *y) {
  const float cx = FTOM(MTOF(cl->ViewOrg.x));
  const float cy = FTOM(MTOF(cl->ViewOrg.y));
  *x -= cx;
  *y -= cy;
  AM_rotate(x, y, 90.0f-cl->ViewAngles.yaw);
  *x += cx;
  *y += cy;
}


//==========================================================================
//
//  AM_doFollowPlayer
//
//==========================================================================
static void AM_doFollowPlayer () {
  if (f_oldloc.x != cl->ViewOrg.x || f_oldloc.y != cl->ViewOrg.y) {
    m_x = FTOM(MTOF(cl->ViewOrg.x))-m_w*0.5f;
    m_y = FTOM(MTOF(cl->ViewOrg.y))-m_h*0.5f;
    m_x2 = m_x+m_w;
    m_y2 = m_y+m_h;
    // do the parallax parchment scrolling
    float sx = FTOM(MTOF(cl->ViewOrg.x-f_oldloc.x));
    float sy = FTOM(MTOF(f_oldloc.y-cl->ViewOrg.y));
    if (am_rotate) AM_rotate(&sx, &sy, cl->ViewAngles.yaw-90.0f);
    AM_ScrollParchment(sx, sy);
    f_oldloc.x = cl->ViewOrg.x;
    f_oldloc.y = cl->ViewOrg.y;
  }
}


//==========================================================================
//
//  AM_Ticker
//
//  Updates on Game Tick
//
//==========================================================================
void AM_Ticker () {
  AM_Check();
  if (!automapactive) return;

  if (am_follow_player) AM_doFollowPlayer();

  // change the zoom if necessary
  if (ftom_zoommul != 1.0f) AM_changeWindowScale();

  // change x,y location
  if (m_paninc.x || m_paninc.y) AM_changeWindowLoc();
}


//==========================================================================
//
//  AM_clearFB
//
//  Clear automap frame buffer.
//
//==========================================================================
static void AM_clearFB () {
  if (am_follow_player) {
    int dmapx = MTOF(cl->ViewOrg.x)-MTOF(oldplr.x);
    int dmapy = MTOF(oldplr.y)-MTOF(cl->ViewOrg.y);

    oldplr.x = cl->ViewOrg.x;
    oldplr.y = cl->ViewOrg.y;
    mapxstart -= dmapx>>1;
    mapystart -= dmapy>>1;

    while (mapxstart >= getAMWidth()) mapxstart -= getAMWidth();
    while (mapxstart < 0) mapxstart += getAMWidth();
    while (mapystart >= mapheight) mapystart -= mapheight;
    while (mapystart < 0) mapystart += mapheight;
  } else {
    mapxstart -= MTOF(m_paninc.x)>>1;
    mapystart += MTOF(m_paninc.y)>>1;
    if (mapxstart >= getAMWidth()) mapxstart -= getAMWidth();
    if (mapxstart < 0) mapxstart += getAMWidth();
    if (mapystart >= mapheight) mapystart -= mapheight;
    if (mapystart < 0) mapystart += mapheight;
  }

  // blit the automap background to the screen
  if (automapactive > 0) {
    if (mappic > 0 && am_show_parchment) {
      for (int y = mapystart-mapheight; y < getAMHeight(); y += mapheight) {
        for (int x = mapxstart-getAMWidth(); x < getAMWidth(); x += 320) {
          R_DrawPic(x, y, mappic);
        }
      }
    } else {
      Drawer->FillRect(0, 0, ScreenWidth, ScreenHeight, 0);
    }
  } else if (automapactive < 0) {
    if (am_back_darken >= 1.0f) {
      Drawer->FillRect(0, 0, ScreenWidth, ScreenHeight, 0);
    } else {
      Drawer->ShadeRect(0, 0, ScreenWidth, ScreenHeight, am_back_darken);
    }
  }
}


#ifdef AM_DO_CLIPPING
//==========================================================================
//
//  AM_clipMline
//
//  Automap clipping of lines.
//
//  Based on Cohen-Sutherland clipping algorithm but with a slightly faster
//  reject and precalculated slopes. If the speed is needed, use a hash
//  algorithm to handle the common cases.
//
//==========================================================================
#define DOOUTCODE(oc, mx, my) \
  (oc) = 0; \
  if ((my) < 0) (oc) |= TOP; \
  else if ((my) >= f_h) (oc) |= BOTTOM; \
  if ((mx) < 0) (oc) |= LEFT; \
  else if ((mx) >= f_w) (oc) |= RIGHT;

static bool AM_clipMline (mline_t *ml, fline_t *fl) {
  enum {
    LEFT   = 1,
    RIGHT  = 2,
    BOTTOM = 4,
    TOP    = 8,
  };

  int outcode1 = 0;
  int outcode2 = 0;
  int outside;

  fpoint_t tmp = {0, 0};
  int dx, dy;

  // do trivial rejects and outcodes
  if (ml->a.y > m_y2) outcode1 = TOP; else if (ml->a.y < m_y) outcode1 = BOTTOM;
  if (ml->b.y > m_y2) outcode2 = TOP; else if (ml->b.y < m_y) outcode2 = BOTTOM;
  if (outcode1&outcode2) return false; // trivially outside

  if (ml->a.x < m_x) outcode1 |= LEFT; else if (ml->a.x > m_x2) outcode1 |= RIGHT;
  if (ml->b.x < m_x) outcode2 |= LEFT; else if (ml->b.x > m_x2) outcode2 |= RIGHT;
  if (outcode1&outcode2) return false; // trivially outside

  // transform to frame-buffer coordinates.
  fl->a.x = CXMTOF(ml->a.x);
  fl->a.y = CYMTOF(ml->a.y);
  fl->b.x = CXMTOF(ml->b.x);
  fl->b.y = CYMTOF(ml->b.y);

  DOOUTCODE(outcode1, fl->a.x, fl->a.y);
  DOOUTCODE(outcode2, fl->b.x, fl->b.y);

  if (outcode1&outcode2) return false;

  while (outcode1|outcode2) {
    // may be partially inside box
    // find an outside point
    if (outcode1) outside = outcode1; else outside = outcode2;

    // clip to each side
    if (outside&TOP) {
      dy = fl->a.y-fl->b.y;
      dx = fl->b.x-fl->a.x;
      tmp.x = fl->a.x+(dx*(fl->a.y))/dy;
      tmp.y = 0;
    } else if (outside&BOTTOM) {
      dy = fl->a.y-fl->b.y;
      dx = fl->b.x-fl->a.x;
      tmp.x = fl->a.x+(dx*(fl->a.y-f_h))/dy;
      tmp.y = f_h-1;
    } else if (outside&RIGHT) {
      dy = fl->b.y-fl->a.y;
      dx = fl->b.x-fl->a.x;
      tmp.y = fl->a.y+(dy*(f_w-1-fl->a.x))/dx;
      tmp.x = f_w-1;
    } else if (outside&LEFT) {
      dy = fl->b.y-fl->a.y;
      dx = fl->b.x-fl->a.x;
      tmp.y = fl->a.y+(dy*(-fl->a.x))/dx;
      tmp.x = 0;
    }

    if (outside == outcode1) {
      fl->a = tmp;
      DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    } else {
      fl->b = tmp;
      DOOUTCODE(outcode2, fl->b.x, fl->b.y);
    }

    if (outcode1&outcode2) return false; // trivially outside
  }

  return true;
}
#undef DOOUTCODE


//==========================================================================
//
//  AM_drawFline
//
//  Classic Bresenham w/ whatever optimizations needed for speed
//
//==========================================================================
static void AM_drawFline (fline_t *fl, vuint32 color) {
  Drawer->DrawLineAM(fl->a.x, fl->a.y, color, fl->b.x, fl->b.y, color);
}
#endif


//==========================================================================
//
//  AM_drawMline
//
//  Clip lines, draw visible part sof lines.
//
//==========================================================================
static void AM_drawMline (mline_t *ml, vuint32 color) {
#ifdef AM_DO_CLIPPING
  fline_t fl;
  if (AM_clipMline(ml, &fl)) AM_drawFline(&fl, color); // draws it on frame buffer using fb coords
#else
  // do not send the line to GPU if it is not visible
  Drawer->DrawLineAM(CXMTOFF(ml->a.x), CYMTOFF(ml->a.y), color, CXMTOFF(ml->b.x), CYMTOFF(ml->b.y), color);
#endif
}


//==========================================================================
//
//  AM_drawGrid
//
//  Draws flat (floor/ceiling tile) aligned grid lines.
//
//==========================================================================
static void AM_drawGrid (vuint32 color) {
  float x, y;
  float start, end;
  mline_t ml;

  // calculate a minimum for how long the grid lines should be, so they
  // cover the screen at any rotation
  const float minlen = sqrtf(m_w*m_w+m_h*m_h);
  const float extx = (minlen-m_w)*0.5f;
  const float exty = (minlen-m_h)*0.5f;

  const float minx = m_x;
  const float miny = m_y;

  // figure out start of vertical gridlines
  start = m_x-extx;
  if ((FX(start-GClLevel->BlockMapOrgX))%(MAPBLOCKUNITS<<FRACBITS)) {
    start += FL((MAPBLOCKUNITS<<FRACBITS)-((FX(start-GClLevel->BlockMapOrgX))%(MAPBLOCKUNITS<<FRACBITS)));
  }
  end = minx+minlen-extx;

  // draw vertical gridlines
  for (x = start; x < end; x += (float)MAPBLOCKUNITS) {
    ml.a.x = x;
    ml.b.x = x;
    ml.a.y = miny-exty;
    ml.b.y = ml.a.y+minlen;
    if (am_rotate) {
      AM_rotatePoint(&ml.a.x, &ml.a.y);
      AM_rotatePoint(&ml.b.x, &ml.b.y);
    }
    AM_drawMline(&ml, color);
  }

  // figure out start of horizontal gridlines
  start = m_y-exty;
  if ((FX(start-GClLevel->BlockMapOrgY))%(MAPBLOCKUNITS<<FRACBITS)) {
    start += FL((MAPBLOCKUNITS<<FRACBITS)-((FX(start-GClLevel->BlockMapOrgY))%(MAPBLOCKUNITS<<FRACBITS)));
  }
  end = miny+minlen-exty;

  // draw horizontal gridlines
  for (y = start; y < end; y += (float)MAPBLOCKUNITS) {
    ml.a.x = minx-extx;
    ml.b.x = ml.a.x+minlen;
    ml.a.y = y;
    ml.b.y = y;
    if (am_rotate) {
      AM_rotatePoint(&ml.a.x, &ml.a.y);
      AM_rotatePoint(&ml.b.x, &ml.b.y);
    }
    AM_drawMline(&ml, color);
  }
}


//==========================================================================
//
//  AM_isBBox2DVisible
//
//==========================================================================
static VVA_FORCEINLINE bool AM_isBBox2DVisible (const float bbox2d[4]) {
  return
    mext_x1 >= bbox2d[BOX2D_LEFT] && mext_y1 >= bbox2d[BOX2D_BOTTOM] &&
    mext_x0 <= bbox2d[BOX2D_RIGHT] && mext_y0 <= bbox2d[BOX2D_TOP];
}


//==========================================================================
//
//  AM_isSubVisible
//
//  check bounding box
//
//==========================================================================
static VVA_FORCEINLINE bool AM_isSubVisible (const subsector_t *sub) {
  return (sub && AM_isBBox2DVisible(sub->bbox2d));
}


//==========================================================================
//
//  AM_isLineMapped
//
//==========================================================================
static VVA_FORCEINLINE vuint32 AM_isLineMapped (const line_t *line) {
  return (line->flags&ML_MAPPED)|(line->exFlags&(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED));
}


//==========================================================================
//
//  AM_isRevealed
//
//==========================================================================
static VVA_FORCEINLINE vuint32 AM_isRevealed () {
  return (cl->PlayerFlags&VBasePlayer::PF_AutomapRevealed);
}


//==========================================================================
//
//  AM_getLineColor
//
//==========================================================================
static vuint32 AM_getLineColor (const line_t *line, bool *cheatOnly) {
  *cheatOnly = false;
  // locked door
  if (line->special == LNSPEC_DoorLockedRaise) {
    VLockDef *ld = GetLockDef(line->arg4);
    if (ld && ld->MapColor) return ld->MapColor;
  }
  // locked ACS special
  if (line->special == LNSPEC_ACSLockedExecute || line->special == LNSPEC_ACSLockedExecuteDoor) {
    VLockDef *ld = GetLockDef(line->arg5);
    if (ld && ld->MapColor) return ld->MapColor;
  }
  // UDMF locked lines
  if (line->locknumber) {
    VLockDef *ld = GetLockDef(line->locknumber);
    if (ld && ld->MapColor) return ld->MapColor;
  }
  // unseen automap walls
  if (!am_cheating && !AM_isLineMapped(line) && AM_isRevealed()) {
    return PowerWallColor;
  }
  // normal wall
  if (!line->backsector) {
    return WallColor;
  }
  // secret door
  if (line->flags&ML_SECRET) {
    return (am_cheating || am_show_secrets ? SecretWallColor : WallColor);
  }
  // floor level change
  if (line->backsector->floor.minz != line->frontsector->floor.minz) {
    return FDWallColor;
  }
  // ceiling level change
  if (line->backsector->ceiling.maxz != line->frontsector->ceiling.maxz) {
    return CDWallColor;
  }
  // show extra floors
  if (line->backsector->SectorFlags&sector_t::SF_HasExtrafloors ||
      line->frontsector->SectorFlags&sector_t::SF_HasExtrafloors)
  {
    return EXWallColor;
  }
  // something other
  *cheatOnly = true;
  return TSWallColor;
}


//==========================================================================
//
//  AM_UpdateSeen
//
//  update "fully mapped" line flags
//
//==========================================================================
static void AM_UpdateSeen () {
  if (!automapUpdateSeen) return;
  automapUpdateSeen = false;

  for (auto &&line : GClLevel->allLines()) {
    if (line.flags&ML_DONTDRAW) {
      line.exFlags &= ~(ML_EX_PARTIALLY_MAPPED|ML_EX_CHECK_MAPPED);
      continue;
    }

    // check if we need to re-evaluate line visibility, and do it
    if (line.exFlags&ML_EX_CHECK_MAPPED) {
      line.exFlags &= ~(ML_MAPPED|ML_EX_PARTIALLY_MAPPED);
      if (line.firstseg && (line.sidenum[0] >= 0 || line.sidenum[1] >= 0)) {
        // mark subsector as renered only if we've seen at least one non-hidden single-sided line from it
        if (line.pobj()) {
          // for polyobjects, do it once and forever
          line.flags |= ML_MAPPED;
          for (seg_t *seg = line.firstseg; seg; seg = seg->lsnext) seg->flags |= SF_MAPPED;
        } else {
          // not a polyobject
          int unseenSides[2] = {0, 0};
          int seenSides[2] = {0, 0};
          for (const seg_t *seg = line.firstseg; seg; seg = seg->lsnext) {
            if (seg->flags&SF_MAPPED) {
              seenSides[seg->side] = 1;
              // mark front subsector as rendered
              if (!seg->frontsub) continue;
              if (seg->frontsub->miscFlags&subsector_t::SSMF_Rendered) continue;
              if (seg->frontsub->sector->SectorFlags&sector_t::SF_Hidden) continue;
              seg->frontsub->miscFlags |= subsector_t::SSMF_Rendered;
            } else {
              unseenSides[seg->side] = 1;
            }
          }
          // if any line side is fully seen, this line is fully mapped
          // that is, we should have some seen segs, and no unseen segs for a side to consider it "fully seen"
          if ((unseenSides[0] == 0 && seenSides[0] != 0) || (unseenSides[1] == 0 && seenSides[1] != 0)) {
            // fully mapped
            line.flags |= ML_MAPPED;
          } else if (seenSides[0]|seenSides[1]) { // not a typo!
            // partially mapped, because some segs were seen
            line.exFlags |= ML_EX_PARTIALLY_MAPPED;
          }
        }
      }
    }

    // resech "check me" flag
    line.exFlags &= ~ML_EX_CHECK_MAPPED;

    // just in case
    if (line.flags&ML_MAPPED) line.exFlags &= ~ML_EX_PARTIALLY_MAPPED;
  }
}


//==========================================================================
//
//  AM_CalcBM_ByMapCoords
//
//==========================================================================
static bool AM_CalcBM_ByMapCoords (float x0, float y0, float x1, float y1,
                                   int *bx0, int *by0, int *bx1, int *by1,
                                   bool doRotate)
{
  int xl = (int)roundf((x0-GClLevel->BlockMapOrgX)/128.0f);
  int xh = (int)roundf((x1-GClLevel->BlockMapOrgX)/128.0f);
  int yl = (int)roundf((y0-GClLevel->BlockMapOrgY)/128.0f);
  int yh = (int)roundf((y1-GClLevel->BlockMapOrgY)/128.0f);

  if (doRotate) {
    // calculate a minimum for how long the grid lines should be, so they
    // cover the screen at any rotation
    const float bw = (float)(xh-xl);
    const float bh = (float)(yh-yl);
    const float minlen = sqrtf(bw*bw+bh*bh);
    const float extx = (minlen-bw)*0.5f;
    const float exty = (minlen-bh)*0.5f;

    xh = (int)roundf(xl+minlen-extx);
    xl = (int)roundf(xl-extx);
    yh = (int)roundf(yl+minlen-exty);
    yl = (int)roundf(yl-exty);
  }

  if (xh < 0 || yh < 0) return false;
  if (xl >= GClLevel->BlockMapWidth || yl >= GClLevel->BlockMapHeight) return false;

  *bx0 = min2(max2(0, xl), GClLevel->BlockMapWidth-1);
  *by0 = min2(max2(0, yl), GClLevel->BlockMapHeight-1);
  *bx1 = min2(max2(0, xh), GClLevel->BlockMapWidth-1);
  *by1 = min2(max2(0, yh), GClLevel->BlockMapHeight-1);

  return true;
}


//==========================================================================
//
//  AM_CalcBM
//
//  calculate blockmap rect
//
//==========================================================================
static inline bool AM_CalcBM (int *bx0, int *by0, int *bx1, int *by1) {
  return AM_CalcBM_ByMapCoords(m_x, m_y, m_x2, m_y2, bx0, by0, bx1, by1, am_rotate);
}


//==========================================================================
//
//  AM_drawOneWall
//
//==========================================================================
static void AM_drawOneWall (const line_t &line, const bool debugPObj, TArrayNC<polyobj_t *> &pobjs) {
  if (!line.firstseg) return; // just in case
  // do not send the line to GPU if it is not visible
  // simplified check with line bounding box
  //if (!AM_isBBox2DVisible(line.bbox2d)) return;

  if (!am_cheating) {
    if (line.flags&ML_DONTDRAW) return;
    if (!(AM_isLineMapped(&line)|AM_isRevealed())) return;
  }

  bool cheatOnly = false;
  vuint32 clr = AM_getLineColor(&line, &cheatOnly);
  if (cheatOnly && !am_cheating) return; //FIXME: should we draw these lines if automap powerup is active?

  // special rendering for polyobject
  if (debugPObj && line.pobj()) {
    bool found = false;
    for (int f = 0; f < pobjs.length(); ++f) if (pobjs[f] == line.pobj()) { found = true; break; }
    if (!found) pobjs.append(line.pobj());
    return;
  }

  // fully mapped or automap revealed?
  if (am_full_lines || am_cheating || (line.flags&ML_MAPPED) ||
      (cl->PlayerFlags&VBasePlayer::PF_AutomapRevealed))
  {
    mline_t l;
    l.a.x = line.v1->x;
    l.a.y = line.v1->y;
    l.b.x = line.v2->x;
    l.b.y = line.v2->y;

    if (am_rotate) {
      AM_rotatePoint(&l.a.x, &l.a.y);
      AM_rotatePoint(&l.b.x, &l.b.y);
    }

    AM_drawMline(&l, clr);
  } else {
    // render segments
    for (const seg_t *seg = line.firstseg; seg; seg = seg->lsnext) {
      if (!seg->drawsegs || !(seg->flags&SF_MAPPED)) continue;

      mline_t l;
      l.a.x = seg->v1->x;
      l.a.y = seg->v1->y;
      l.b.x = seg->v2->x;
      l.b.y = seg->v2->y;

      if (am_rotate) {
        AM_rotatePoint(&l.a.x, &l.a.y);
        AM_rotatePoint(&l.b.x, &l.b.y);
      }

      AM_drawMline(&l, clr);
    }
  }
}


//==========================================================================
//
//  AM_drawWalls
//
//  Determines visible lines, draws them.
//
//==========================================================================
static void AM_drawWalls () {
  const bool debugPObj = (am_pobj_debug.asInt()&0x01);
  TArrayNC<polyobj_t *> pobjs;

  if (am_follow_player) {
    AM_doFollowPlayer();
    int bx0, by0, bx1, by1;
    if (!AM_CalcBM(&bx0, &by0, &bx1, &by1)) return;
    GClLevel->IncrementValidCount();
    for (int bx = bx0; bx <= bx1; ++bx) {
      for (int by = by0; by <= by1; ++by) {
        for (auto &&it : GClLevel->allBlockLines(bx, by)) {
          AM_drawOneWall(*it.line(), debugPObj, pobjs);
        }
      }
    }
  } else {
    for (auto &&line : GClLevel->allLines()) {
      AM_drawOneWall(line, debugPObj, pobjs);
    }
  }

  if (pobjs.length()) {
    const vuint32 aclrs[2] = { 0x60c09000, 0x600090c0 };
    for (int pn = 0; pn < pobjs.length(); ++pn) {
      const polyobj_t *pobj = pobjs[pn];
      unsigned aidx = 0u;
      for (auto &&it : pobj->SubFirst()) {
        for (auto &&sit : it.part()->SegFirst()) {
          const seg_t *seg = sit.seg();

          mline_t l;
          l.a.x = seg->v1->x;
          l.a.y = seg->v1->y;
          l.b.x = seg->v2->x;
          l.b.y = seg->v2->y;

          if (am_rotate) {
            AM_rotatePoint(&l.a.x, &l.a.y);
            AM_rotatePoint(&l.b.x, &l.b.y);
          }

          AM_drawMline(&l, aclrs[aidx]);
        }
        aidx ^= 1u;
      }
    }
  }
}


//==========================================================================
//
//  AM_mapxy2fbxy
//
//==========================================================================
static void AM_mapxy2fbxy (float *destx, float *desty, float x, float y) {
  if (am_rotate) AM_rotatePoint(&x, &y);
  if (destx) *destx = CXMTOFF(x);
  if (desty) *desty = CYMTOFF(y);
}


//==========================================================================
//
//  amIsHiddenSubsector
//
//  this is called to render "revealed" subsector bluish
//
//==========================================================================
static bool amIsHiddenSubsector (const subsector_t *sub) {
  return ((sub->miscFlags&subsector_t::SSMF_Rendered) == 0);
}


//==========================================================================
//
//  amShouldRenderTextured
//
//  this is called to decide if the subsector should be rendered at all
//
//==========================================================================
static bool amShouldRenderTextured (const subsector_t *sub) {
  // check bounding box
  if (!AM_isSubVisible(sub)) return false;
  // ignore "hidden" flag if we're cheating or "automap revealed" item is taken
  // "revealed hidden" sectors may still be hidden, but meh
  if (am_cheating >= 1 || (cl->PlayerFlags&VBasePlayer::PF_AutomapRevealed)) return true;
  // check if it is visible at all
  if ((sub->miscFlags&subsector_t::SSMF_Rendered) == 0) return false; // not seen
  // check if parent sector is not hidden
  if (sub->sector && (sub->sector->SectorFlags&sector_t::SF_Hidden)) return false;
  // it is visible
  return true;
}


//==========================================================================
//
//  AM_drawFloors
//
//==========================================================================
static void AM_drawFlats () {
  if (!Drawer || !Drawer->RendLev || (am_draw_type&3) == 0) return;
  float alpha = (am_overlay ? clampval(am_texture_alpha.asFloat(), 0.0f, 1.0f) : 1.0f);
  if (alpha <= 0.0f) return;
  const bool amDoFloors = ((am_draw_type&3) == 1);
  //Drawer->RendLev->RenderTexturedAutomap(m_x, m_y, m_x2, m_y2, amDoFloors, alpha, &amShouldRenderTextured, &amIsHiddenSubsector, &AM_mapxy2fbxy);
  Drawer->RendLev->RenderTexturedAutomap(mext_x0, mext_y0, mext_x1, mext_y1,
                                         amDoFloors, alpha, &amShouldRenderTextured,
                                         &amIsHiddenSubsector, &AM_mapxy2fbxy);
}


//==========================================================================
//
//  AM_DrawSimpleLine
//
//==========================================================================
static void AM_DrawSimpleLine (float x0, float y0, float x1, float y1, vuint32 color) {
  mline_t l;
  l.a.x = x0;
  l.a.y = y0;
  l.b.x = x1;
  l.b.y = y1;
  if (am_rotate) {
    AM_rotatePoint(&l.a.x, &l.a.y);
    AM_rotatePoint(&l.b.x, &l.b.y);
  }
  AM_drawMline(&l, color);
}


//==========================================================================
//
//  AM_DrawBox
//
//==========================================================================
static void AM_DrawBox (float x0, float y0, float x1, float y1, vuint32 color) {
  AM_DrawSimpleLine(x0, y0, x1, y0, color);
  AM_DrawSimpleLine(x1, y0, x1, y1, color);
  AM_DrawSimpleLine(x1, y1, x0, y1, color);
  AM_DrawSimpleLine(x0, y1, x0, y0, color);
}


//==========================================================================
//
//  AM_DrawMinisegs
//
//==========================================================================
static void AM_DrawMinisegs () {
  const bool drawNormal = (am_cheating && am_show_minisegs);
  const bool drawPObj = (am_pobj_debug.asInt()&0x02);
  const seg_t *seg = &GClLevel->Segs[0];
  for (unsigned i = GClLevel->NumSegs; i--; ++seg) {
    if (seg->linedef) continue; // not a miniseg
    if (seg->pobj) {
      if (drawPObj) AM_DrawSimpleLine(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y, 0xffa000a0);
    } else {
      if (drawNormal) AM_DrawSimpleLine(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y, MinisegColor);
    }
  }
}


//==========================================================================
//
//  AM_DrawRenderedNodes
//
//==========================================================================
static void AM_DrawRenderedNodes () {
  const node_t *node = &GClLevel->Nodes[0];
  for (unsigned i = GClLevel->NumNodes; i--; ++node) {
    if (!Drawer->RendLev->IsNodeRendered(node)) continue;
    AM_DrawBox(node->bbox[0][0], node->bbox[0][1], node->bbox[0][3], node->bbox[0][4], 0xffff7f00);
    AM_DrawBox(node->bbox[1][0], node->bbox[1][1], node->bbox[1][3], node->bbox[1][4], 0xffffffff);
  }
}


//==========================================================================
//
//  AM_DrawSubsectorSegs
//
//==========================================================================
static void AM_DrawSubsectorSegs (const subsector_t *sub, vuint32 color, bool drawMinisegs) {
  if (!sub) return;
  const seg_t *seg = &GClLevel->Segs[sub->firstline];
  for (unsigned i = sub->numlines; i--; ++seg) {
    if (!drawMinisegs && !seg->linedef) continue;
    AM_DrawSimpleLine(seg->v1->x, seg->v1->y, seg->v2->x, seg->v2->y, color);
  }
}


//==========================================================================
//
//  AM_DrawRenderedSubs
//
//==========================================================================
static void AM_DrawRenderedSubs () {
  // use buggy vanilla algo here, because this is what used for world linking
  subsector_t *mysub = GClLevel->PointInSubsector_Buggy(cl->ViewOrg);
  vassert(mysub);

  const subsector_t *sub = &GClLevel->Subsectors[0];
  for (unsigned scount = GClLevel->NumSubsectors; scount--; ++sub) {
    if (mysub == sub) continue;
    if (!AM_isSubVisible(sub)) continue;
    if (!Drawer->RendLev->IsSubsectorRendered(sub)) continue;
    AM_DrawSubsectorSegs(sub, 0xff00ffff, true);
  }

  if (mysub) AM_DrawSubsectorSegs(mysub, (Drawer->RendLev->IsSubsectorRendered(mysub) ? 0xff00ff00 : 0xffff0000), true);
}


//==========================================================================
//
//  AM_drawLineCharacter
//
//==========================================================================
static void AM_drawLineCharacter (const mline_t *lineguy, int lineguylines,
  float scale, float angle, vuint32 color, float x, float y)
{
  float msinAngle = msin(angle);
  float mcosAngle = mcos(angle);

  for (int i = 0; i < lineguylines; ++i) {
    mline_t l = lineguy[i];

    if (scale) {
      l.a.x = scale*l.a.x;
      l.a.y = scale*l.a.y;
      l.b.x = scale*l.b.x;
      l.b.y = scale*l.b.y;
    }

    if (angle) {
      float oldax = l.a.x;
      float olday = l.a.y;
      float oldbx = l.b.x;
      float oldby = l.b.y;

      l.a.x = oldax*mcosAngle-olday*msinAngle;
      l.a.y = oldax*msinAngle+olday*mcosAngle;
      l.b.x = oldbx*mcosAngle-oldby*msinAngle;
      l.b.y = oldbx*msinAngle+oldby*mcosAngle;
    }

    l.a.x += x;
    l.a.y += y;
    l.b.x += x;
    l.b.y += y;

    AM_drawMline(&l, color);
  }
}


//==========================================================================
//
//  AM_drawOnePlayer
//
//==========================================================================
static void AM_drawOnePlayer (float posx, float posy, float yaw, bool isMain) {
  const mline_t *player_arrow;
  float angle;
  int line_count;

  if (dbg_am_no_player_arrow.asBool()) return;

  if (am_cheating) {
    player_arrow = player_arrow_ddt;
    line_count = NUMPLYRLINES3;
  } else if (am_player_arrow == 1) {
    player_arrow = player_arrow2;
    line_count = NUMPLYRLINES2;
  } else {
    player_arrow = player_arrow1;
    line_count = NUMPLYRLINES1;
  }

  if (am_rotate) {
    if (isMain) {
      angle = 90.0f;
    } else {
      angle = yaw+90.0f;
    }
  } else {
    angle = yaw;
  }

  //FIXME: use correct colors for other players
  AM_drawLineCharacter(player_arrow, line_count, 0.0f, angle, PlayerColor, FTOM(MTOF(posx)), FTOM(MTOF(posy)));
}


//==========================================================================
//
//  AM_drawPlayers
//
//==========================================================================
static void AM_drawPlayers () {
  // draw self
  AM_drawOnePlayer(cl->ViewOrg.x, cl->ViewOrg.y, cl->ViewAngles.yaw, true);
  // draw other players in coop mode
  if (!GClGame || GGameInfo->NetMode <= NM_Standalone || /*GGameInfo->deathmatch*/GClGame->deathmatch) return;
  //GCon->Logf(NAME_Debug, "AUTOMAP: looking for other players");
  for (TThinkerIterator<VEntity> Ent(GClLevel); Ent; ++Ent) {
    if (!Ent->IsPlayer() || Ent->IsRealCorpse()) continue;
    if (*Ent == cl->MO) continue;
    //if (!(Ent->Player->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
    AM_drawOnePlayer(Ent->Origin.x, Ent->Origin.y, Ent->Angles.yaw, false);
  }
}


//==========================================================================
//
//  AM_drawOneThing
//
//==========================================================================
static void AM_drawOneThing (VEntity *mobj, bool &inSpriteMode) {
  if (!mobj) return;
  if (!mobj->State || mobj->IsGoingToDie()) return;

  bool invisible = ((mobj->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible|VEntity::EF_NoBlockmap)) || mobj->RenderStyle == STYLE_None);
  if (invisible && am_cheating != 3) {
    if (!mobj->IsMissile()) return;
    if (am_cheating < 3) return;
  }
  //if (!(mobj->FlagsEx&VEntity::EFEX_Rendered)) return;

  vuint32 color;
       if (invisible) color = InvisibleThingColor;
  else if (mobj->EntityFlags&VEntity::EF_Corpse) color = DeadColor;
  else if (mobj->FlagsEx&VEntity::EFEX_Monster) color = MonsterColor;
  else if (mobj->EntityFlags&VEntity::EF_Missile) color = MissileColor;
  else if (mobj->EntityFlags&VEntity::EF_Solid) color = SolidThingColor;
  else color = ThingColor;

  int sprIdx = (am_render_thing_sprites ? getSpriteIndex(mobj->GetClass()) : -1);

  TVec morg = mobj->GetDrawOrigin();

  if (sprIdx > 0) {
    float x = morg.x;
    float y = morg.y;
    if (am_rotate) AM_rotatePoint(&x, &y);
    if (!inSpriteMode) { inSpriteMode = true; Drawer->EndAutomap(); }
    R_DrawSpritePatch(CXMTOFF(x), CYMTOFF(y), sprIdx, 0, 0, 0, 0, 0, scale_mtof, true); // draw, ignore vscr
  } else {
    if (inSpriteMode) { inSpriteMode = false; Drawer->StartAutomap(am_overlay); }

    float x = FTOM(MTOF(morg.x));
    float y = FTOM(MTOF(morg.y));
    float angle = mobj->/*Angles*/GetInterpolatedDrawAngles().yaw; // anyway

    if (am_rotate) {
      AM_rotatePoint(&x, &y);
      angle += 90.0f-cl->ViewAngles.yaw;
    }
    AM_drawLineCharacter(thintriangle_guy, NUMTHINTRIANGLEGUYLINES, 16.0f, angle, color, x, y);
  }

  // draw object box
  if (am_cheating == 3 || am_cheating == 5) {
    if (inSpriteMode) { inSpriteMode = false; Drawer->StartAutomap(am_overlay); }
    //AM_DrawBox(mobj->Origin.x-mobj->Radius, mobj->Origin.y-mobj->Radius, mobj->Origin.x+mobj->Radius, mobj->Origin.y+mobj->Radius, color);
    const float rad = mobj->GetMoveRadius();
    AM_DrawBox(morg.x-rad, morg.y-rad, morg.x+rad, morg.y+rad, color);
    #if 0
    // DEBUG: draw slide force
    if (mobj->IsRealCorpse() &&
        /*((mobj->FlagsEx&VEntity::EFEX_SomeSectorMoved) != 0 ||
         (mobj->FlagsEx&VEntity::EFEX_CorpseSlideChecked) == 0) &&*/
        mobj->Velocity.x == 0.0f && mobj->Velocity.y == 0.0f && mobj->Velocity.z >= 0.0f)
    {
      tmtrace_t tm;
      mobj->CheckRelPositionPoint(tm, mobj->Origin);
      if (tm.FloorZ < mobj->Origin.z) {
        //if (strcmp(mobj->GetClass()->GetName(), "DoomImp") != 0) return;
        //GCon->Logf("%s: fz=%g; oz=%g", mobj->GetClass()->GetName(), tm.FloorZ, mobj->Origin.z);
        AM_DrawBox(morg.x-rad, morg.y-rad, morg.x+rad, morg.y+rad, 0xFFFF0000);
        //
        float mybbox[4];
        VLevel *XLevel = mobj->XLevel;
        mobj->Create2DBox(mybbox);
        DeclareMakeBlockMapCoordsBBox2D(mybbox, xl, yl, xh, yh);
        //GCon->Logf(" rad=%g; xl=%d; yl=%d; xh=%d; yh=%d", rad, xl, yl, xh, yh);
        XLevel->IncrementValidCount(); // used to make sure we only process a line once
        TVec snorm = TVec(0.0f, 0.0f, 0.0f);
        for (int bx = xl; bx <= xh; ++bx) {
          for (int by = yl; by <= yh; ++by) {
            //GCon->Logf("  bx=%d; by=%d", bx, by);
            for (auto &&it : XLevel->allBlockLines(bx, by)) {
              line_t *ld = it.line();
              if ((ld->flags&ML_TWOSIDED) == 0) continue;
              if (!mobj->LineIntersects(ld)) continue;
              bool ok = false, high = false;
              for (int snum = 0; snum < 2 && !high; snum += 1) {
                const sector_t *sec = (snum ? ld->backsector : ld->frontsector);
                vassert(sec);
                const float fz = sec->floor.GetPointZClamped(mobj->Origin);
                const float zdiff = fz-mobj->Origin.z;
                ok = ok || (zdiff == 0.0f);
                high = (zdiff > 0.0f);
              }
              if (high) continue;
              const int side = ld->PointOnSide(mobj->Origin);
              #if 0
              GCon->Logf("  line: side=%d; norm:(%g,%g,%g)",
                         side, ld->normal.x, ld->normal.y, ld->normal.z);
              #endif
              snorm += (side ? -ld->normal : ld->normal);
              snorm = snorm.normalise();
              #if 0
              GCon->Logf("  snorm: (%g,%g,%g)", snorm.x, snorm.y, snorm.z);
              #endif
            }
          }
        }
        if (snorm.x != 0.0f || snorm.y != 0.0f) {
          mline_t l;
          l.a.x = morg.x;
          l.a.y = morg.y;
          l.b.x = (morg + snorm*40).x;
          l.b.y = (morg + snorm*40).y;
          if (am_rotate) {
            AM_rotatePoint(&l.a.x, &l.a.y);
            AM_rotatePoint(&l.b.x, &l.b.y);
          }
          AM_drawMline(&l, 0xFFFFFF00);
        }
        mobj->Velocity.x = snorm.x*15;
        mobj->Velocity.y = snorm.y*15;
      }
    }
    #endif
  }
}


//==========================================================================
//
//  AM_drawThings
//
//==========================================================================
static void AM_drawThings () {
  bool inSpriteMode = false;

  if (am_cheating >= 3 || !am_follow_player) {
    for (TThinkerIterator<VEntity> Ent(GClLevel); Ent; ++Ent) {
      VEntity *mobj = *Ent;
      AM_drawOneThing(mobj, inSpriteMode);
    }
  } else {
    int bx0, by0, bx1, by1;
    if (!AM_CalcBM(&bx0, &by0, &bx1, &by1)) return;
    GClLevel->IncrementValidCount();
    for (int bx = bx0; bx <= bx1; ++bx) {
      for (int by = by0; by <= by1; ++by) {
        for (auto &&it : GClLevel->allBlockThings(bx, by)) {
          AM_drawOneThing(it.entity(), inSpriteMode);
        }
      }
    }
  }

  // restore automap rendering mode
  if (inSpriteMode) Drawer->StartAutomap(am_overlay);
}


//==========================================================================
//
//  AM_drawOneKey
//
//==========================================================================
static void AM_drawOneKey (VEntity *mobj, bool &inSpriteMode, bool allKeys) {
  if (!mobj) return;
  if (!mobj->State || mobj->IsGoingToDie()) return;
  if (mobj->RenderStyle == STYLE_None) return;
  if ((mobj->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible|VEntity::EF_NoBlockmap)) != 0) return;

  // check for seen subsector
  if (!mobj->SubSector) return;
  if (!allKeys && !(mobj->SubSector->miscFlags&subsector_t::SSMF_Rendered)) return;
  //if (!(mobj->FlagsEx&VEntity::EFEX_Rendered)) return;

  // check if this is a key
  VClass *cls = mobj->GetClass();
  if (!cls->IsChildOf(keyClass)) return;

  // check if it has a spawn sprite
  int sprIdx = getSpriteIndex(cls);
  if (sprIdx < 0) return;

  // ok, this looks like a valid key, render it
  TVec morg = mobj->GetDrawOrigin();
  float x = morg.x;
  float y = morg.y;
  if (am_rotate) AM_rotatePoint(&x, &y);
  if (!inSpriteMode) {
    inSpriteMode = true;
    Drawer->EndAutomap();
  }
  R_DrawSpritePatch(CXMTOFF(x), CYMTOFF(y), sprIdx, 0, 0, 0, 0, 0, scale_mtof, true); // draw, ignore vscr
}


//==========================================================================
//
//  AM_drawKeys
//
//==========================================================================
static void AM_drawKeys () {
  if (!keyClass) return;

  // keys should blink
  float cbt = am_keys_blink_time.asFloat();
  if (isFiniteF(cbt) && cbt > 0.0f) {
    if (cbt > 10.0f) cbt = 10.0f; // arbitrary limit
    const float xtm = fmod(Sys_Time(), cbt*2.0);
    if (xtm < cbt) return;
  }

  const bool allKeys = (am_show_keys_cheat.asBool() || am_cheating.asInt() > 1);
  bool inSpriteMode = false;

  if (!am_follow_player) {
    for (TThinkerIterator<VEntity> Ent(GClLevel); Ent; ++Ent) {
      AM_drawOneKey(*Ent, inSpriteMode, allKeys);
    }
  } else {
    int bx0, by0, bx1, by1;
    if (!AM_CalcBM(&bx0, &by0, &bx1, &by1)) return;
    GClLevel->IncrementValidCount();
    for (int bx = bx0; bx <= bx1; ++bx) {
      for (int by = by0; by <= by1; ++by) {
        for (auto &&it : GClLevel->allBlockThings(bx, by)) {
          AM_drawOneKey(it.entity(), inSpriteMode, allKeys);
        }
      }
    }
  }

  // restore automap rendering mode
  if (inSpriteMode) Drawer->StartAutomap(am_overlay);
}


//==========================================================================
//
//  AM_drawOneLight
//
//==========================================================================
static void AM_drawOneLight (const VRenderLevelPublic::LightInfo &lt) {
  if (!lt.active || lt.radius < 1) return;
  float x = lt.origin.x;
  float y = lt.origin.y;
  if (am_rotate) AM_rotatePoint(&x, &y);
  Drawer->DrawLineAM(CXMTOFF(x-2), CYMTOFF(y-2), lt.color, CXMTOFF(x+2), CYMTOFF(y-2), lt.color);
  Drawer->DrawLineAM(CXMTOFF(x+2), CYMTOFF(y-2), lt.color, CXMTOFF(x+2), CYMTOFF(y+2), lt.color);
  Drawer->DrawLineAM(CXMTOFF(x+2), CYMTOFF(y+2), lt.color, CXMTOFF(x-2), CYMTOFF(y+2), lt.color);
  Drawer->DrawLineAM(CXMTOFF(x-2), CYMTOFF(y+2), lt.color, CXMTOFF(x-2), CYMTOFF(y-2), lt.color);
  // draw circle
  float px = 0, py = 0;
  for (float angle = 0; angle <= 360; angle += 10) {
    float x0 = x+msin(angle)*lt.radius;
    float y0 = y-mcos(angle)*lt.radius;
    if (angle) {
      Drawer->DrawLineAM(CXMTOFF(px), CYMTOFF(py), lt.color, CXMTOFF(x0), CYMTOFF(y0), lt.color);
    }
    px = x0;
    py = y0;
  }
}


//==========================================================================
//
//  AM_drawStaticLights
//
//==========================================================================
static void AM_drawStaticLights () {
  if (!GClLevel->Renderer) return;
  int count = GClLevel->Renderer->GetStaticLightCount();
  for (int f = 0; f < count; ++f) {
    auto lt = GClLevel->Renderer->GetStaticLight(f);
    AM_drawOneLight(lt);
  }
}


//==========================================================================
//
//  AM_drawDynamicLights
//
//==========================================================================
static void AM_drawDynamicLights () {
  if (!GClLevel->Renderer) return;
  int count = GClLevel->Renderer->GetDynamicLightCount();
  for (int f = 0; f < count; ++f) {
    auto lt = GClLevel->Renderer->GetDynamicLight(f);
    AM_drawOneLight(lt);
  }
}


//==========================================================================
//
//  AM_drawMarks
//
//==========================================================================
static void AM_drawMarks () {
  if (!mapMarksAllowed) return;

  for (int i = 0; i < AM_NUMMARKPOINTS; ++i) {
    MarkPoint &mp = markpoints[i];
    if (!mp.isActive()) continue;

    //int w = LittleShort(GTextureManager.TextureWidth(marknums[i]));
    //int h = LittleShort(GTextureManager.TextureHeight(marknums[i]));

    mpoint_t pt;
    pt.x = markpoints[i].x;
    pt.y = markpoints[i].y;

    if (am_rotate) AM_rotatePoint(&pt.x, &pt.y);

    float fx = (CXMTOF(pt.x)/fScaleX);
    float fy = (CYMTOF(pt.y)/fScaleY);
    /*
    if (fx >= f_x && fx <= f_w-w && fy >= f_y && fy <= f_h-h) {
      R_DrawPicFloat(fx, fy, marknums[i]);
    }
    */

    const char *numstr = va("%d", i);
    // calc size
    int wdt = 0;
    int hgt = 0;
    for (const char *s = numstr; *s; ++s) {
      int w = LittleShort(GTextureManager.TextureWidth(marknums[*s-'0']));
      int h = LittleShort(GTextureManager.TextureHeight(marknums[*s-'0']));
      wdt += w;
      if (hgt < h) hgt = h;
    }

    // center it on the screen
    fx -= wdt/2;
    fy -= hgt/2;

    if (i == markActive) {
      Drawer->FillRect((fx-1)*fScaleX, (fy-1)*fScaleY, (fx+wdt+2)*fScaleX, (fy+hgt+2)*fScaleY, CurrMarkColor, 0.5f);
    } else if (GClLevel && (GClLevel->TicTime&0x0f) >= 8) {
      Drawer->FillRect((fx-1)*fScaleX, (fy-1)*fScaleY, (fx+wdt+2)*fScaleX, (fy+hgt+2)*fScaleY, MarkBlinkColor, 0.5f);
    }

    // render it
    for (const char *s = numstr; *s; ++s) {
      R_DrawPicFloat(fx, fy, marknums[*s-'0']);
      int w = LittleShort(GTextureManager.TextureWidth(marknums[*s-'0']));
      fx += w;
    }
  }
}


//===========================================================================
//
//  AM_DrawWorldTimer
//
//===========================================================================
/*
static void AM_DrawWorldTimer () {
  // moved to VavoomC code
  int days;
  int hours;
  int minutes;
  int seconds;
  int worldTimer;
  char timeBuffer[64];
  char dayBuffer[20];

  if (!cl || !GClLevel) return;
  if (!draw_world_timer) return;

  worldTimer = (int)cl->WorldTimer;
  if (worldTimer < 0) worldTimer = 0;
  //if (!worldTimer) return;

  days = worldTimer/86400;
  worldTimer -= days*86400;
  hours = worldTimer/3600;
  worldTimer -= hours*3600;
  minutes = worldTimer/60;
  worldTimer -= minutes*60;
  seconds = worldTimer;

  T_SetFont(SmallFont);
  T_SetAlign(hleft, vtop);

  //k8: sorry!
  static int sx = -666666;
  if (sx == -666666) {
    sx = max2(74, T_StringWidth("00 : 00 : 00"));
    sx = max2(sx, T_StringWidth("88 : 88 : 88"));
    sx = VirtualWidth-sx-4;
  }

  snprintf(timeBuffer, sizeof(timeBuffer), "%.2d : %.2d : %.2d", hours, minutes, seconds);
  T_DrawText(sx, 8, timeBuffer, SB_GetTextColor(SBTS_AutomapGameTotalTime));

  if (days) {
    if (days == 1) {
      snprintf(dayBuffer, sizeof(dayBuffer), "%.2d DAY", days);
    } else {
      snprintf(dayBuffer, sizeof(dayBuffer), "%.2d DAYS", days);
    }
    T_DrawText(sx, 18, dayBuffer, SB_GetTextColor(SBTS_AutomapGameTotalTime));
    if (days >= 5) T_DrawText(sx-10, 28, "YOU FREAK!!!", SB_GetTextColor(SBTS_AutomapGameTotalTime));
  }
}
*/


//===========================================================================
//
//  AM_DrawLevelStats
//
//===========================================================================
static void AM_DrawLevelStats (bool drawMapName, bool drawStats) {
  char lnamebuf[128];

  if (!cl || !GClLevel) return;

  const float alpha = 1.0;

  T_SetFont(SmallFont);
  T_SetAlign(hleft, vbottom);

  int currY = VirtualHeight-sb_height-7-2;

  VStr lname = GClLevel->LevelInfo->GetLevelName().xstrip();
  if (lname.length()) {
    size_t lbpos = 0;
    for (int f = 0; f < lname.length(); ++f) {
      char ch = lname[f];
      if (ch < ' ') continue;
      if (ch == 127) ch = '_';
      if (lbpos == 124) break;
      lnamebuf[lbpos++] = ch;
    }
    lnamebuf[lbpos] = 0;
    T_DrawText(20, currY, lnamebuf, SB_GetTextColor(SBTC_AutomapMapName), alpha);
    currY -= T_FontHeight();
  }

  if (drawMapName) {
    lname = VStr(GClLevel->MapName);
    lname = lname.xstrip();
    T_DrawText(20, currY, va("%s (n%d:c%d)", *lname, GClLevel->LevelInfo->LevelNum, GClLevel->LevelInfo->Cluster), SB_GetTextColor(SBTC_AutomapMapCluster), alpha);
    currY -= T_FontHeight();
  }

  if (drawStats) {
    currY -= 4;
    //currY -= 3*T_FontHeight();
    const int kills = cl->KillCount;
    const int totalkills = GClLevel->LevelInfo->TotalKills;
    const int items = cl->ItemCount;
    const int totalitems = GClLevel->LevelInfo->TotalItems;
    const int secrets = cl->SecretCount;
    const int totalsecrets = GClLevel->LevelInfo->TotalSecret;

    snprintf(lnamebuf, sizeof(lnamebuf), "Secrets: %.2d / %.2d", secrets, totalsecrets);
    T_DrawText(8, currY, lnamebuf, SB_GetTextColor(SBTC_AutomapSecrets), alpha);
    currY -= T_FontHeight();

    snprintf(lnamebuf, sizeof(lnamebuf), "Items: %.2d / %.2d", items, totalitems);
    T_DrawText(8, currY, lnamebuf, SB_GetTextColor(SBTC_AutomapItems), alpha);
    currY -= T_FontHeight();

    snprintf(lnamebuf, sizeof(lnamebuf), "Kills: %.2d / %.2d", kills, totalkills);
    T_DrawText(8, currY, lnamebuf, SB_GetTextColor(SBTC_AutomapKills), alpha);
  }
}


//==========================================================================
//
//  AM_CheckVariables
//
//==========================================================================
static void AM_CheckVariables () {
  // check for screen resolution change
  if (f_w != ScreenWidth || f_h != ScreenHeight-SB_RealHeight()) {
    float old_mtof_zoommul = mtof_zoommul;
    mtof_zoommul = scale_mtof/start_scale_mtof;

    f_w = ScreenWidth;
    f_h = ScreenHeight-(screen_size < 11 ? SB_RealHeight() : 0);

    float a = (float)f_w/max_w;
    float b = (float)f_h/max_h;

    min_scale_mtof = (a < b ? a : b);
    max_scale_mtof = (float)f_h/(2.0f*PLAYERRADIUS);

    scale_mtof = min_scale_mtof/0.7f;
    if (scale_mtof > max_scale_mtof) scale_mtof = min_scale_mtof;
    scale_ftom = 1.0f/scale_mtof;
    start_scale_mtof = scale_mtof;

    AM_changeWindowScale();

    mtof_zoommul = old_mtof_zoommul;
  }
}


//==========================================================================
//
//  AM_Drawer
//
//==========================================================================
void AM_Drawer () {
  AM_Check();

  if (!automapactive) {
    // moved to VC code
    //AM_DrawWorldTimer();
    //if (draw_map_stats) AM_DrawLevelStats(false, false, true);
    return;
  }

  //if (am_overlay) glColor4f(1, 1, 1, (am_overlay_alpha < 0.1f ? 0.1f : am_overlay_alpha > 1.0f ? 1.0f : am_overlay_alpha));

  AM_CheckVariables();
  AM_clearFB();

  AM_UpdateSeen();

  // calculate a rectangle that should cover the whole screen, even if rotated
  // (in world coords)
  {
    const float minlen = sqrtf(m_w*m_w+m_h*m_h);
    const float extx = (minlen-m_w)*0.5f;
    const float exty = (minlen-m_h)*0.5f;
    mext_x0 = m_x-extx;
    mext_y0 = m_y-exty;
    mext_x1 = m_x+minlen-extx;
    mext_y1 = m_y+minlen-exty;
  }

  Drawer->StartAutomap(am_overlay);
  if (am_draw_grid) AM_drawGrid(GridColor);
  if (am_draw_type&3) {
    Drawer->EndAutomap();
    AM_drawFlats();
    Drawer->StartAutomap(am_overlay);
    if (am_draw_texture_lines) AM_drawWalls();
  } else {
    AM_drawWalls();
  }
  AM_drawPlayers();
  if (am_cheating >= 2 || (cl->PlayerFlags&VBasePlayer::PF_AutomapShowThings)) {
    AM_drawThings();
  }
  if (am_draw_keys || am_show_keys_cheat) AM_drawKeys();
  if (am_cheating && am_show_static_lights) AM_drawStaticLights();
  if (am_cheating && am_show_dynamic_lights) AM_drawDynamicLights();
  if ((am_cheating && am_show_minisegs) || (am_pobj_debug.asInt()&0x02)) AM_DrawMinisegs();
  if (am_cheating && am_show_rendered_nodes) AM_DrawRenderedNodes();
  if (am_cheating && am_show_rendered_subs) AM_DrawRenderedSubs();
  Drawer->EndAutomap();
  //AM_DrawWorldTimer();
  if (am_show_stats || am_show_map_name) AM_DrawLevelStats(am_show_map_name, am_show_stats);
  if (mapMarksAllowed) AM_drawMarks();

  //if (am_overlay) glColor4f(1, 1, 1, 1);
}


#if 0

#define WidgetTranslateXY(destx_,desty_,srcx_,srcy_)  \
  const int destx_ = (int)roundf(((srcx_)*c-(srcy_)*s)+halfwdt); \
  const int desty_ = (int)roundf(((srcy_)*c+(srcx_)*s)+halfhgt)

#define WidgetRotateXYAdd(destx_,desty_,srcx_,srcy_,addx_,addy_)  \
  const int destx_ = (int)roundf((srcx_)*c-(srcy_)*s+(addx_)); \
  const int desty_ = (int)roundf((srcy_)*c+(srcx_)*s+(addy_))

#else

#define WidgetTranslateXY(destx_,desty_,srcx_,srcy_)  \
  const float destx_ = ((srcx_)*c-(srcy_)*s)+halfwdt; \
  const float desty_ = ((srcy_)*c+(srcx_)*s)+halfhgt

#define WidgetRotateXYAdd(destx_,desty_,srcx_,srcy_,addx_,addy_)  \
  const float destx_ = (srcx_)*c-(srcy_)*s+(addx_); \
  const float desty_ = (srcy_)*c+(srcx_)*s+(addy_)

#endif


//==========================================================================
//
//  AM_Minimap_DrawMarks
//
//==========================================================================
static void AM_Minimap_DrawMarks (VWidget *w, float xc, float yc, float scale, float angle, float alpha) {
  if (!mapMarksAllowed) return;

  float s, c;
  msincos(angle, &s, &c);

  const float halfwdt = w->GetWidth()*0.5f;
  const float halfhgt = w->GetHeight()*0.5f;

  for (int i = 0; i < AM_NUMMARKPOINTS; ++i) {
    MarkPoint &mp = markpoints[i];
    if (!mp.isActive()) continue;

    //int w = LittleShort(GTextureManager.TextureWidth(marknums[i]));
    //int h = LittleShort(GTextureManager.TextureHeight(marknums[i]));

    // convert line coordinates
    const float mx1o = (markpoints[i].x-xc)*scale;
    const float my1o = -(markpoints[i].y-yc)*scale;

    WidgetTranslateXY(mx1, my1, mx1o, my1o);

    // do not send the line to GPU if it is not visible
    /*
    if (max2(mx1, mx2) < 0 || max2(my1, my2) < 0 ||
        min2(mx1, mx2) >= w->GetWidth() || min2(my1, my2) >= w->GetHeight())
    {
      continue;
    }
    */

    const char *numstr = va("%d", i);
    // calc size
    int wdt = 0;
    int hgt = 0;
    for (const char *tmps = numstr; *tmps; ++tmps) {
      int ww = LittleShort(GTextureManager.TextureWidth(marknums[*tmps-'0']));
      int hh = LittleShort(GTextureManager.TextureHeight(marknums[*tmps-'0']));
      wdt += ww;
      if (hgt < hh) hgt = hh;
    }
    if (wdt < 1 || hgt < 1) continue;

    // clamp coords
    float mx = clampval(mx1, 0.0f, float(w->GetWidth()-wdt));
    float my = clampval(my1, 0.0f, float(w->GetHeight()-hgt));

    if (i == markActive) {
      w->FillRectF(mx, my, wdt, hgt, CurrMarkColor, 0.5f);
    } else if (GClLevel && (GClLevel->TicTime&0x0f) >= 8) {
      w->FillRectF(mx, my, wdt, hgt, MarkBlinkColor, 0.5f);
    }

    // render it
    for (const char *tmps = numstr; *tmps; ++tmps) {
      //R_DrawPicFloat(fx, fy, marknums[*tmps-'0']);
      VTexture *Tex = GTextureManager(marknums[*tmps-'0']);
      if (Tex && Tex->Type != TEXTYPE_Null) {
        //float px = mx-Tex->GetScaledSOffsetF();
        //float py = my-Tex->GetScaledTOffsetF();
        w->DrawPic(mx, my, Tex, alpha);
        mx += GTextureManager.TextureWidth(marknums[*tmps-'0']);
      }
    }
  }
}


//==========================================================================
//
//  AM_MiniMap_CalcBM
//
//  calculate blockmap rect
//
//==========================================================================
static inline bool AM_MiniMap_CalcBM (VWidget *w, float xc, float yc, float scale,
                                      bool doRotate, int *bx0, int *by0, int *bx1, int *by1)
{
  const float halfwdt = w->GetWidth()*0.5f;
  const float halfhgt = w->GetHeight()*0.5f;

  const float wscale = halfwdt/scale;
  const float hscale = halfhgt/scale;

  // convert widget coords to map coords
  const float x0 = xc-wscale;
  const float y0 = yc-hscale;
  const float x1 = xc+wscale;
  const float y1 = yc+hscale;

  return AM_CalcBM_ByMapCoords(x0, y0, x1, y1, bx0, by0, bx1, by1, doRotate);
}


//==========================================================================
//
//  AM_Minimap_DrawWalls
//
//==========================================================================
static void AM_Minimap_DrawWalls (VWidget *w, float xc, float yc, float scale, float angle, float alpha) {
  if (scale == 0.0f) return;

  float s, c;
  msincos(angle, &s, &c);

  const float halfwdt = w->GetWidth()*0.5f;
  const float halfhgt = w->GetHeight()*0.5f;

  // use blockmap
  int bx0, by0, bx1, by1;
  if (!AM_MiniMap_CalcBM(w, xc, yc, scale, (angle != 0.0f), &bx0, &by0, &bx1, &by1)) return;
  GClLevel->IncrementValidCount();
  for (int bx = bx0; bx <= bx1; ++bx) {
    for (int by = by0; by <= by1; ++by) {
      for (auto &&it : GClLevel->allBlockLines(bx, by)) {
        const line_t &line = *it.line();
        if (!line.firstseg) continue; // just in case

        if (!am_cheating) {
          if (line.flags&ML_DONTDRAW) continue;
          if (!(AM_isLineMapped(&line)|AM_isRevealed())) continue;
        }

        // convert line coordinates
        const float lx1o = (line.v1->x-xc)*scale;
        const float ly1o = -(line.v1->y-yc)*scale;
        const float lx2o = (line.v2->x-xc)*scale;
        const float ly2o = -(line.v2->y-yc)*scale;

        WidgetTranslateXY(lx1, ly1, lx1o, ly1o);
        WidgetTranslateXY(lx2, ly2, lx2o, ly2o);

        // do not send the line to GPU if it is not visible
        if (max2(lx1, lx2) <= 0.0f || max2(ly1, ly2) <= 0.0f ||
            min2(lx1, lx2) >= w->GetWidth() || min2(ly1, ly2) >= w->GetHeight())
        {
          continue;
        }

        bool cheatOnly = false;
        vuint32 clr = AM_getLineColor(&line, &cheatOnly);
        if (cheatOnly && !am_cheating) continue; //FIXME: should we draw these lines if automap powerup is active?
        clr |= 0xff000000u;

        w->DrawLineF(lx1, ly1, lx2, ly2, clr, alpha);
      }
    }
  }
}


//==========================================================================
//
//  AM_Minimap_DrawPlayer
//
//==========================================================================
static void AM_Minimap_DrawPlayer (VWidget *w, float /*xc*/, float /*yc*/, float scale, float plrangle, float alpha) {
  if (!minimap_draw_player) return;
  float s, c;
  const mline_t *player_arrow;
  int line_count;

  const float halfwdt = w->GetWidth()*0.5f;
  const float halfhgt = w->GetHeight()*0.5f;

  if (am_cheating) {
    player_arrow = player_arrow_ddt;
    line_count = NUMPLYRLINES3;
  } else if (am_player_arrow == 1) {
    player_arrow = player_arrow2;
    line_count = NUMPLYRLINES2;
  } else {
    player_arrow = player_arrow1;
    line_count = NUMPLYRLINES1;
  }

  plrangle = AngleMod(360.0f-(plrangle+90.0f));
  msincos(plrangle, &s, &c);
  vuint32 pclr = PlayerColor;
  pclr |= 0xff000000u;

  for (int i = 0; i < line_count; ++i) {
    const mline_t &l = player_arrow[i];

    const float lx1o = (l.a.x)*scale;
    const float ly1o = -(l.a.y)*scale;
    const float lx2o = (l.b.x)*scale;
    const float ly2o = -(l.b.y)*scale;

    WidgetTranslateXY(lx1, ly1, lx1o, ly1o);
    WidgetTranslateXY(lx2, ly2, lx2o, ly2o);

    w->DrawLineF(lx1, ly1, lx2, ly2, pclr, alpha);
  }
}


//==========================================================================
//
//  AM_Minimap_DrawOneThing
//
//==========================================================================
static void AM_Minimap_DrawOneThing (VWidget *w, VEntity *mobj, float xc, float yc,
                                     float scale, float angle, float alpha,
                                     bool onlyBlock)
{
  if (!mobj) return;
  if (!mobj->State || mobj->IsGoingToDie()) return;

  const bool invisible = ((mobj->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible|VEntity::EF_NoBlockmap)) ||
                          mobj->RenderStyle == STYLE_None);
  if (invisible && am_cheating != 3) {
    if (!mobj->IsMissile()) return;
    if (am_cheating < 3) return;
  }
  //if (!(mobj->FlagsEx&VEntity::EFEX_Rendered)) return;

  const bool block = onlyBlock && mobj->IsSolid() && mobj->GetMoveRadius() > 0 &&
                     !mobj->IsPlayerOrMissileOrMonster() && !mobj->IsAnyCorpse();

  if (onlyBlock && !block) return;

  vuint32 color;
       if (invisible) color = InvisibleThingColor;
  else if (mobj->EntityFlags&VEntity::EF_Corpse) color = DeadColor;
  else if (mobj->FlagsEx&VEntity::EFEX_Monster) color = MonsterColor;
  else if (mobj->EntityFlags&VEntity::EF_Missile) color = MissileColor;
  else if (mobj->EntityFlags&VEntity::EF_Solid) color = SolidThingColor;
  else color = ThingColor;
  color |= 0xff000000u;

  //int sprIdx = (am_render_thing_sprites ? getSpriteIndex(mobj->GetClass()) : -1);

  const TVec morg = mobj->GetDrawOrigin();

  const float halfwdt = w->GetWidth()*0.5f;
  const float halfhgt = w->GetHeight()*0.5f;

  // only box
  if (am_cheating < 1 && block) {
    float s, c;
    msincos(angle, &s, &c);

    color = MMapBlockThingColor;
    color |= 0xff000000u;

    const float morgx = (morg.x-xc)*scale;
    const float morgy = -(morg.y-yc)*scale;

    const float mcx = (morgx*c-morgy*s)+halfwdt;
    const float mcy = (morgy*c+morgx*s)+halfhgt;

    const float rad = max2(1.0f, mobj->GetMoveRadius()*scale);
    if (mcx+rad <= 0.0 || mcy+rad <= 0.0f || mcx-rad >= w->GetWidth() || mcy-rad >= w->GetHeight()) return;

    //FIXME: rotate rect too
    const float sz = max2(1.0f, mobj->GetMoveRadius()*scale);
    w->DrawRectF(mcx-sz, mcy-sz, sz*2, sz*2, color, alpha);
    return;
  }

  /*
  if (sprIdx > 0) {
    float x = morg.x;
    float y = morg.y;
    if (am_rotate) AM_rotatePoint(&x, &y);
    if (!inSpriteMode) { inSpriteMode = true; Drawer->EndAutomap(); }
    R_DrawSpritePatch(CXMTOFF(x), CYMTOFF(y), sprIdx, 0, 0, 0, 0, 0, scale_mtof, true); // draw, ignore vscr
  } else
  */
  {
    float s, c;
    msincos(angle, &s, &c);

    const float morgx = (morg.x-xc)*scale;
    const float morgy = -(morg.y-yc)*scale;

    const float mcx = (morgx*c-morgy*s)+halfwdt;
    const float mcy = (morgy*c+morgx*s)+halfhgt;

    const float rad = max2(1.0f, mobj->GetMoveRadius()*scale);
    if (mcx+rad <= 0.0 || mcy+rad <= 0.0f || mcx-rad >= w->GetWidth() || mcy-rad >= w->GetHeight()) return;

    const float sprangle = AngleMod(mobj->/*Angles*/GetInterpolatedDrawAngles().yaw-90.0f+angle); // anyway
    msincos(sprangle, &s, &c);

    const float triscale = 12.0f;
    for (size_t i = 0; i < NUMTHINTRIANGLEGUYLINES; ++i) {
      const mline_t &l = thintriangle_guy[i];

      const float lx1o = (l.a.x*triscale)*scale;
      const float ly1o = -(l.a.y*triscale)*scale;
      const float lx2o = (l.b.x*triscale)*scale;
      const float ly2o = -(l.b.y*triscale)*scale;

      WidgetRotateXYAdd(lx1, ly1, lx1o, ly1o, mcx, mcy);
      WidgetRotateXYAdd(lx2, ly2, lx2o, ly2o, mcx, mcy);

      w->DrawLineF(lx1, ly1, lx2, ly2, color, alpha);
    }

    // draw object box
    if (am_cheating == 3 || am_cheating == 5) {
      #if 0
      const int x0 = (int)roundf(mcx);
      const int y0 = (int)roundf(mcy);
      const int sz = (int)roundf(max2(1.0f, mobj->GetMoveRadius()*scale));
      w->DrawRect(x0-sz, y0-sz, sz*2, sz*2, color, alpha);
      #else
      //FIXME: rotate rect too
      const float sz = max2(1.0f, mobj->GetMoveRadius()*scale);
      w->DrawRectF(mcx-sz, mcy-sz, sz*2, sz*2, color, alpha);
      #endif
    }
  }
}


//==========================================================================
//
//  AM_Minimap_DrawThings
//
//==========================================================================
static void AM_Minimap_DrawThings (VWidget *w, float xc, float yc, float scale, float angle, float alpha) {
  bool onlyBlock = minimap_draw_blocking_things.asBool();

  if (am_cheating < 2 && (cl->PlayerFlags&VBasePlayer::PF_AutomapShowThings) == 0 && !onlyBlock) return;
  if (am_cheating >= 2) onlyBlock = false;

  if (am_cheating >= 3) {
    for (TThinkerIterator<VEntity> Ent(GClLevel); Ent; ++Ent) {
      AM_Minimap_DrawOneThing(w, *Ent, xc, yc, scale, angle, alpha, onlyBlock);
    }
  } else {
    int bx0, by0, bx1, by1;
    if (!AM_MiniMap_CalcBM(w, xc, yc, scale, (angle != 0.0f), &bx0, &by0, &bx1, &by1)) return;
    GClLevel->IncrementValidCount();
    for (int bx = bx0; bx <= bx1; ++bx) {
      for (int by = by0; by <= by1; ++by) {
        for (auto &&it : GClLevel->allBlockThings(bx, by)) {
          AM_Minimap_DrawOneThing(w, it.entity(), xc, yc, scale, angle, alpha, onlyBlock);
        }
      }
    }
  }
}


//==========================================================================
//
//  AM_Minimap_DrawKeys
//
//==========================================================================
static void AM_Minimap_DrawKeys (VWidget *w, float xc, float yc, float scale, float angle, float alpha) {
  if (!keyClass || !minimap_draw_keys) return;
  if (am_cheating >= 2 || (cl->PlayerFlags&VBasePlayer::PF_AutomapShowThings)) return;

  const float halfwdt = w->GetWidth()*0.5f;
  const float halfhgt = w->GetHeight()*0.5f;

  // keys should blink
  float cbt = am_keys_blink_time.asFloat();
  if (isFiniteF(cbt) && cbt > 0.0f) {
    if (cbt > 10.0f) cbt = 10.0f; // arbitrary limit
    const float xtm = fmod(Sys_Time(), cbt*2.0);
    if (xtm < cbt) return;
  }

  const bool allKeys = (am_show_keys_cheat.asBool() || am_cheating.asInt() > 1);

  int bx0, by0, bx1, by1;
  if (!AM_MiniMap_CalcBM(w, xc, yc, scale, (angle != 0.0f), &bx0, &by0, &bx1, &by1)) return;
  GClLevel->IncrementValidCount();
  for (int bx = bx0; bx <= bx1; ++bx) {
    for (int by = by0; by <= by1; ++by) {
      for (auto &&it : GClLevel->allBlockThings(bx, by)) {
        VEntity *mobj = it.entity();
        if (!mobj->State || mobj->IsGoingToDie()) continue;
        if (mobj->RenderStyle == STYLE_None) continue;
        if ((mobj->EntityFlags&(VEntity::EF_NoSector|VEntity::EF_Invisible|VEntity::EF_NoBlockmap)) != 0) continue;

        // check for seen subsector
        if (!mobj->SubSector) continue;
        if (!allKeys && !(mobj->SubSector->miscFlags&subsector_t::SSMF_Rendered)) continue;
        //if (!(mobj->FlagsEx&VEntity::EFEX_Rendered)) continue;

        // check if this is a key
        VClass *cls = mobj->GetClass();
        if (!cls->IsChildOf(keyClass)) continue;

        // check if it has a spawn sprite
        int sprIdx = getSpriteIndex(cls);
        if (sprIdx < 0) continue;

        // ok, this looks like a valid key, render it
        const TVec morg = mobj->GetDrawOrigin();

        float s, c;
        msincos(angle, &s, &c);

        const float morgx = (morg.x-xc)*scale;
        const float morgy = -(morg.y-yc)*scale;

        const float mcx = (morgx*c-morgy*s)+halfwdt;
        const float mcy = (morgy*c+morgx*s)+halfhgt;

        //const float rad = max2(1.0f, mobj->GetMoveRadius()*scale);
        //if (mcx+rad <= 0.0 || mcy+rad <= 0.0f || mcx-rad >= w->GetWidth() || mcy-rad >= w->GetHeight()) continue;

        vint32 color = 0xffffff50; //temp

        w->DrawRectF(mcx-0.5f, mcy-0.5f, 1.0f, 1.0f, color, alpha);
        w->DrawRectF(mcx-1.0f, mcy-1.0f, 2.0f, 2.0f, color, alpha);
        //w->DrawRectF(mcx-2.0f, mcy-2.0f, 4.0f, 4.0f, color, alpha);
      }
    }
  }
}


//==========================================================================
//
//  AM_DrawAtWidget
//
//==========================================================================
void AM_DrawAtWidget (VWidget *w, float xc, float yc, float scale, float angle, float plrangle, float alpha) {
  if (!w || scale <= 0.0f || alpha <= 0.0f || !GClLevel) return;
  //AM_Check();
  angle = AngleMod(angle);
  if (alpha > 1.0f) alpha = 1.0f;

  if (!keyClass) keyClass = VClass::FindClass("Key");

  AM_Minimap_DrawWalls(w, xc, yc, scale, angle, alpha);
  AM_Minimap_DrawThings(w, xc, yc, scale, angle, alpha);
  AM_Minimap_DrawMarks(w, xc, yc, scale, angle, alpha);
  AM_Minimap_DrawKeys(w, xc, yc, scale, angle, alpha); // rendering keys is not working right yet, and it is VERY SLOW
  AM_Minimap_DrawPlayer(w, xc, yc, scale, plrangle, alpha);

  if (minimap_draw_border) {
    vuint32 bclr = MinimapBorderColor;
    bclr |= 0xff000000u;
    w->DrawRect(0, 0, w->GetWidth(), w->GetHeight(), bclr, alpha);
  }
}


//==========================================================================
//
//  AM_TranslateKeyEvent
//
//  Handle events (user inputs) in automap mode
//
//==========================================================================
static int AM_TranslateKeyEvent (event_t *ev, int *cmdtype) {
  *cmdtype = AM_BIND_NOTHING;

#ifdef CLIENT
  bool kdown;
       if (ev->type == ev_keydown) kdown = true;
  else if (ev->type == ev_keyup) kdown = false;
  else return AM_PTYPE_NOTMINE;

  VStr cdown, cup;
  GInput->GetBinding(ev->keycode, cdown, cup);
  cdown = cdown.xstrip();
  cup = cup.xstrip();
  // do not intercept automap toggle key
  //FIXME: this should be made more flexible
  if (cdown.strEquCI("toggle_automap")) return AM_PTYPE_NOTMINE;

  if (kdown) {
    if (cdown.isEmpty()) return AM_PTYPE_NOTMINE;
  } else {
    if (cup.isEmpty()) return AM_PTYPE_NOTMINE;
    cdown = cup;
  }
  // just in case
  if (cdown.isEmpty()) return AM_PTYPE_NOTMINE;

  const char *cmd = *cdown;
  while (cmd[0] == '+' || cmd[0] == '-') ++cmd;
  if (!cmd[0]) return AM_PTYPE_NOTMINE;

  for (const AmBindInfo *nfo = ambinds; nfo->bindname; ++nfo) {
    if (VStr::strEquCI(nfo->bindname, cmd)) {
      *cmdtype = nfo->bindtype;
      if (kdown) {
        if (amkstate[nfo->bindtype]) return AM_PTYPE_REPEAT;
        amkstate[nfo->bindtype] = 1;
        return AM_PTYPE_PRESS;
      } else {
        amkstate[nfo->bindtype] = 0;
        return AM_PTYPE_RELEASE;
      }
    }
  }
#endif

  return AM_PTYPE_NOTMINE;
}


//==========================================================================
//
//  AM_Responder
//
//  Handle events (user inputs) in automap mode
//
//==========================================================================
bool AM_Responder (event_t *ev) {
  AM_Check();
  if (!automapactive) {
    ui_freemouse = false;
    return false;
  }

  if (am_cheating == 3) {
    ui_freemouse = true;
    if (ev->type == ev_keydown && ev->keycode == K_MOUSE1) {
      //float x = FTOM(MTOF(morg.x));
      //float y = FTOM(MTOF(morg.y));
      float x = FTOM(ev->x-f_x)+m_x;
      float y = FTOM(f_h-ev->y-f_y)+m_y;
      GCon->Logf(NAME_Debug, "ms=(%d,%d); map=(%g,%g) (%g,%g)", ev->x, ev->y, x, y, cl->ViewOrg.x, cl->ViewOrg.y);
      return true;
    }
  } else {
    ui_freemouse = false;
  }

  bool rc = false;
  int cmdtype = AM_BIND_NOTHING;
  const int kp = AM_TranslateKeyEvent(ev, &cmdtype);
  if (kp != AM_PTYPE_NOTMINE) {
    rc = true;
    switch (cmdtype) {
      case AM_BIND_PANRIGHT:
        if (!am_follow_player) {
          m_paninc.x = (kp == AM_PTYPE_RELEASE ? 0.0f : FTOM(F_PANINC*0.5f));
        } else {
          rc = false;
        }
        break;
      case AM_BIND_PANLEFT:
        if (!am_follow_player) {
          m_paninc.x = (kp == AM_PTYPE_RELEASE ? 0.0f : -FTOM(F_PANINC*0.5f));
        } else {
          rc = false;
        }
        break;
      case AM_BIND_PANUP:
        if (!am_follow_player) {
          m_paninc.y = (kp == AM_PTYPE_RELEASE ? 0.0f : FTOM(F_PANINC*0.5f));
        } else {
          rc = false;
        }
        break;
      case AM_BIND_PANDOWN:
        if (!am_follow_player) {
          m_paninc.y = (kp == AM_PTYPE_RELEASE ? 0.0f : -FTOM(F_PANINC*0.5f));
        } else {
          rc = false;
        }
        break;
      case AM_BIND_ZOOMOUT:
        if (!amWholeScale && kp != AM_PTYPE_RELEASE) {
          mtof_zoommul = M_ZOOMOUT;
          ftom_zoommul = M_ZOOMIN;
        } else {
          mtof_zoommul = 1.0f;
          ftom_zoommul = 1.0f;
        }
        break;
      case AM_BIND_ZOOMIN:
        if (!amWholeScale && kp != AM_PTYPE_RELEASE) {
          mtof_zoommul = M_ZOOMIN;
          ftom_zoommul = M_ZOOMOUT;
        } else {
          mtof_zoommul = 1.0f;
          ftom_zoommul = 1.0f;
        }
        break;
      case AM_BIND_GOBIG:
        if (kp == AM_PTYPE_PRESS) {
          mtof_zoommul = 1.0f;
          ftom_zoommul = 1.0f;
          amWholeScale = (amWholeScale ? 0 : 1);
          if (amWholeScale) {
            AM_saveScaleAndLoc();
            AM_minOutWindowScale();
          } else {
            AM_restoreScaleAndLoc();
          }
        }
        break;
      case AM_BIND_FOLLOW:
        if (kp == AM_PTYPE_PRESS) {
          am_follow_player = !am_follow_player;
          f_oldloc.x = 99999.0f;
          cl->Printf("%s", *GLanguage.Translate(am_follow_player ? AMSTR_FOLLOWON : AMSTR_FOLLOWOFF));
        }
        break;
      case AM_BIND_GRID:
        if (kp == AM_PTYPE_PRESS) {
          am_draw_grid = !am_draw_grid;
          cl->Printf("%s", *GLanguage.Translate(am_draw_grid ? AMSTR_GRIDON : AMSTR_GRIDOFF));
        }
        break;
      case AM_BIND_ADDMARK:
        if (kp == AM_PTYPE_PRESS) {
          if (mapMarksAllowed) {
            int mnum = AM_addMark();
            if (mnum >= 0) cl->Printf("%s %d", *GLanguage.Translate(AMSTR_MARKEDSPOT), mnum);
          }
        }
        break;
      case AM_BIND_NEXTMARK:
        if (kp == AM_PTYPE_PRESS) {
          if (mapMarksAllowed) {
            int oldmark = markActive;
            ++markActive;
            if (markActive < 0) markActive = 0;
            // find next active mark
            while (markActive < AM_NUMMARKPOINTS) {
              if (markpoints[markActive].isActive()) break;
              ++markActive;
            }
            if (markActive >= AM_NUMMARKPOINTS) markActive = oldmark;
          }
        }
        break;
      case AM_BIND_PREVMARK:
        if (kp == AM_PTYPE_PRESS) {
          if (mapMarksAllowed) {
            int oldmark = markActive;
            --markActive;
            if (markActive < 0) markActive = 0;
            // find next active mark
            while (markActive >= 0) {
              if (markpoints[markActive].isActive()) break;
              --markActive;
            }
            if (markActive < 0) markActive = oldmark;
          }
        }
        break;
      case AM_BIND_DELMARK:
        if (kp == AM_PTYPE_PRESS) {
          if (mapMarksAllowed) {
            if (markActive >= 0 && markActive < AM_NUMMARKPOINTS && markpoints[markActive].isActive()) {
              markpoints[markActive].deactivate();
              cl->Printf("%s %d", *GLanguage.Translate(AMSTR_MARKEDSPOTDEL), markActive);
            }
          }
        }
        break;
      case AM_BIND_CLEARMARKS:
        if (kp == AM_PTYPE_PRESS) {
          if (mapMarksAllowed) {
            if (AM_clearMarks()) cl->Printf("%s", *GLanguage.Translate(AMSTR_MARKSCLEARED));
          }
        }
        break;
      case AM_BIND_TEXTURE:
        if (kp == AM_PTYPE_PRESS) {
          am_draw_type = (am_draw_type+1)%3;
        }
        break;
      default: break;
    }
  }

  return rc;
}


//==========================================================================
//
//  COMMAND Iddt
//
//==========================================================================
COMMAND(Iddt) {
  //if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) return;
#ifdef CLIENT
  if (MN_Active()) return;
  if (!cl || !GClGame || !GGameInfo || GClGame->InIntermission() || GGameInfo->NetMode <= NM_TitleMap) return;
  am_cheating = (am_cheating+1)%3;
#endif
}


//==========================================================================
//
//  COMMAND toggle_automap
//
//==========================================================================
COMMAND(toggle_automap) {
#ifdef CLIENT
  if (MN_Active()) return;
  if (!cl || !GClGame || !GGameInfo || GClGame->InIntermission() || GGameInfo->NetMode <= NM_TitleMap) {
    GCon->Log(NAME_Warning, "Cannot toggle automap while not in game!");
    return;
  }
/*k8: why?
  if (GGameInfo->IsPaused()) {
    if (cl) cl->Printf("Cannot toggle automap while the game is paused!"); else GCon->Log(NAME_Warning, "Cannot toggle automap while the game is paused!");
    return;
  }
*/
  am_active = !am_active;
#endif
}
