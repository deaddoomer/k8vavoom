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
#include <limits.h>
#include <float.h>

#include "../gamedefs.h"
#include "../host.h"  /* host_frametime */
#include "../psim/p_decal.h"
#include "../psim/p_entity.h"
#include "../psim/p_levelinfo.h"
#include "../psim/p_playerreplicationinfo.h"
#include "../psim/p_player.h"
#include "../client/client.h"
#include "../sound/sound.h"
#include "../text.h"
#include "../screen.h"
#include "../automap.h"
#include "../sbar.h"
#include "r_local.h"

//#define VAVOOM_DEBUG_PORTAL_POOL

extern VCvarB r_advlight_opt_optimise_scissor;
extern VCvarB dbg_clip_dump_added_ranges;
extern VCvarI gl_release_ram_textures_mode;

VCvarB r_dbg_proj_aspect("r_dbg_proj_aspect", true, "Apply aspect correction to projection matrix?", CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);
static bool prev_r_dbg_proj_aspect = true;

static VCvarB dbg_autoclear_automap("dbg_autoclear_automap", false, "Clear automap before rendering?", CVAR_NoShadow/*|CVAR_Archive*/);
VCvarB dbg_vischeck_time("dbg_vischeck_time", false, "Show frame vischeck time?", CVAR_NoShadow/*|CVAR_Archive*/);

static VCvarB r_clip_maxdist("r_clip_maxdist", true, "Clip with max view distance? This can speedup huge levels, trading details for speed.", CVAR_Archive|CVAR_NoShadow);
extern VCvarF gl_maxdist;
//extern VCvarB r_disable_world_update;

VCvarB dbg_show_lightmap_cache_messages("dbg_show_lightmap_cache_messages", false, "Show various lightmap debug messages?", CVAR_Archive|CVAR_NoShadow);

VCvarB r_allow_cameras("r_allow_cameras", true, "Allow rendering live cameras?", CVAR_Archive|CVAR_NoShadow);

//VCvarB dbg_dlight_vis_check_messages("dbg_dlight_vis_check_messages", false, "Show dynlight vischeck debug messages?", CVAR_NoShadow);
VCvarB r_vis_check_flood("r_vis_check_flood", false, "Use floodfill to perform dynlight visibility checks? (AT MAJORITY OF CASES THIS IS SLOWER THAN BSP!)", CVAR_Archive|CVAR_Hidden|CVAR_NoShadow);

static VCvarI r_tonemap("r_tonemap", "0", "Tonemap mode (0:off, 1:palette).", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_tonemap_psprites("r_tonemap_psprites", true, "Apply tonemap after rendering psprites?", CVAR_Archive);

static VCvarI r_underwater("r_underwater", "1", "Underwater shader (0:off, 1:Quake-like).", CVAR_Archive);

static VCvarI r_cas_filter("r_cas_filter", "0", "Use adaptive sharpening posprocess filter?", CVAR_Archive|CVAR_NoShadow);
static VCvarF r_cas_filter_coeff("r_cas_filter_coeff", "0.4", "Sharpen coeffecient for CAS, [0..1].", CVAR_Archive|CVAR_NoShadow);

static VCvarI r_dbg_force_colormap("r_dbg_force_colormap", "0", "DEBUG: force colormap.", CVAR_NoShadow);

static VCvarF r_hack_aspect_scale("r_hack_aspect_scale", "1.2", "Aspect ratio scale. As my aspect code is FUBARed, you can use this to make things look right.", CVAR_Archive|CVAR_NoShadow);
static float prev_r_hack_aspect_scale = 0.0f;

static VCvarF r_hack_psprite_yofs("r_hack_psprite_yofs", "0", "PSprite offset for non-standard ration scales. Positive is down.", CVAR_Archive|CVAR_NoShadow);
static float prev_r_hack_psprite_yofs = 0.0f;
static float prev_r_light_globvis = -666.0f;

static VCvarI k8ColormapInverse("k8ColormapInverse", "2", "Inverse colormap replacement (0: original inverse; 1: black-and-white; 2: gold; 3: green; 4: red).", CVAR_Archive|CVAR_NoShadow);
static VCvarI k8ColormapLightAmp("k8ColormapLightAmp", "9", "LightAmp colormap replacement (0: original; 1: black-and-white; 2: gold; 3: green; 4: red).", CVAR_Archive|CVAR_NoShadow);

static VCvarI r_colormap_mode("r_colormap_mode", "0", "Powerups colormap mode: 0:default; 1:ignore fullbright; 2:ignore colormap; 3:ignore both.", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_colortint_mode("r_colortint_mode", "0", "Powerups colormap mode: 0:default; 1:disabled.", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_sectorflick_mode("r_sectorflick_mode", "0", "Sector light flickering mode: 0:default; 1:only when the sector is invisible.", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_force_sector_light("r_force_sector_light", "0", "Force sector light level (0 means \"don't force\").", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_level_extra_lighting("r_level_extra_lighting", "0", "Add extra light to all sectors ([0..255]).", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_extralight_mode("r_extralight_mode", "0", "Extra light from weapon firing: 0:default; 1:disabled.", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_dynlightfx_mode("r_dynlightfx_mode", "0", "Dynamic light change mode: 0:default; 1:disabled.", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_sprlight_mode("r_sprlight_mode", "0", "Dynamic light change mode: 0:default; 1:disabled.", CVAR_Archive|CVAR_NoShadow);

VCvarI r_fullbright_sprites_mode("r_fullbright_sprites_mode", "0", "Always render sprites as fullbright (0:none; 1:interesting; 2:all)?", CVAR_Archive|CVAR_NoShadow);
VCvarI r_fullbright_splevel_player("r_fullbright_splevel_player", "255", "Fullbright level for players (0 means \"disable fullbright\").", CVAR_Archive|CVAR_NoShadow);
VCvarI r_fullbright_splevel_missile("r_fullbright_splevel_missile", "255", "Fullbright level for projectiles (0 means \"disable fullbright\").", CVAR_Archive|CVAR_NoShadow);
VCvarI r_fullbright_splevel_corpse("r_fullbright_splevel_corpse", "255", "Fullbright level for corpses (0 means \"disable fullbright\").", CVAR_Archive|CVAR_NoShadow);
VCvarI r_fullbright_splevel_monster("r_fullbright_splevel_monster", "255", "Fullbright level for monsters (0 means \"disable fullbright\").", CVAR_Archive|CVAR_NoShadow);
VCvarI r_fullbright_splevel_decoration("r_fullbright_splevel_decoration", "255", "Fullbright level for decorations (0 means \"disable fullbright\").", CVAR_Archive|CVAR_NoShadow);
VCvarI r_fullbright_splevel_pickup("r_fullbright_splevel_pickup", "255", "Fullbright level for pickups (0 means \"disable fullbright\").", CVAR_Archive|CVAR_NoShadow);
VCvarI r_fullbright_splevel_other("r_fullbright_splevel_other", "255", "Fullbright level for other things (0 means \"disable fullbright\").", CVAR_Archive|CVAR_NoShadow);

static VCvarI r_screen_shake_mode("r_screen_shake_mode", "0", "Screen shake (from earthquakes and such) mode: 0:default, 1:disabled.", CVAR_Archive|CVAR_NoShadow);

static VCvarI r_cm_extralight_add("r_cm_extralight_add", "48", "Extra lighting added for \"Light Amp\" colormaps ([0..255]).", CVAR_Archive|CVAR_NoShadow);

VCvarB r_underwater_colored_light("r_underwater_colored_light", true, "Allow underwater colored lighting? WARNING: DOESN'T WORK RIGHT YET!", CVAR_Archive|CVAR_NoShadow);


static const char *videoDrvName = nullptr;
static int cli_DisableSplash = 0;
/*static*/ bool cliRegister_rmain_args =
  VParsedArgs::RegisterStringOption("-video", "!", &videoDrvName) &&
  VParsedArgs::RegisterFlagSet("-nosplash", "disable startup splash screen", &cli_DisableSplash);


void R_FreeSkyboxData ();


vuint8 light_remap[256];
int screenblocks = 0; // viewport size
bool render_last_quality_setting = false;

static VCvarF r_aspect_pixel("r_aspect_pixel", "1", "Pixel aspect ratio.", CVAR_Rom|CVAR_NoShadow);
static VCvarI r_aspect_horiz("r_aspect_horiz", "4", "Horizontal aspect multiplier.", CVAR_Rom|CVAR_NoShadow);
static VCvarI r_aspect_vert("r_aspect_vert", "3", "Vertical aspect multiplier.", CVAR_Rom|CVAR_NoShadow);

VCvarB r_chasecam("r_chasecam", false, "Chasecam mode.", CVAR_NoShadow/*|CVAR_Archive*/);
VCvarB r_chase_front("r_chase_front", false, "Position chasecam in the front of the player (can be used to view weapons/player sprite, for example).", CVAR_NoShadow/*|CVAR_Archive*/); // debug setting
VCvarF r_chase_delay("r_chase_delay", "0.1", "Chasecam interpolation delay.", CVAR_Archive|CVAR_NoShadow);
VCvarF r_chase_raise("r_chase_raise", "32", "Chasecam z raise before offseting by view direction.", CVAR_Archive|CVAR_NoShadow);
VCvarF r_chase_dist("r_chase_dist", "32", "Chasecam distance.", CVAR_Archive|CVAR_NoShadow);
VCvarF r_chase_up("r_chase_up", "32", "Chasecam offset up (using view direction).", CVAR_Archive|CVAR_NoShadow);
VCvarF r_chase_right("r_chase_right", "0", "Chasecam offset right (using view direction).", CVAR_Archive|CVAR_NoShadow);
VCvarF r_chase_radius("r_chase_radius", "16", "Chasecam entity radius (used for offsetting coldet).", CVAR_Archive|CVAR_NoShadow);

//VCvarI r_fog("r_fog", "0", "Fog mode (0:GL_LINEAR; 1:GL_LINEAR; 2:GL_EXP; 3:GL_EXP2; add 4 to get \"nicer\" fog).", CVAR_NoShadow);
VCvarB r_fog_test("r_fog_test", false, "Is fog testing enabled?", CVAR_NoShadow);
VCvarF r_fog_r("r_fog_r", "0.5", "Fog color: red component.", CVAR_NoShadow);
VCvarF r_fog_g("r_fog_g", "0.5", "Fog color: green component.", CVAR_NoShadow);
VCvarF r_fog_b("r_fog_b", "0.5", "Fog color: blue component.", CVAR_NoShadow);
VCvarF r_fog_start("r_fog_start", "1", "Fog start distance.", CVAR_NoShadow);
VCvarF r_fog_end("r_fog_end", "2048", "Fog end distance.", CVAR_NoShadow);
VCvarF r_fog_density("r_fog_density", "0.5", "Fog density.", CVAR_NoShadow);

VCvarI r_aspect_ratio("r_aspect_ratio", "1", "Aspect ratio correction mode.", CVAR_Archive|CVAR_NoShadow);
VCvarB r_interpolate_frames("r_interpolate_frames", true, "Use frame interpolation for smoother rendering?", CVAR_Archive|CVAR_NoShadow);
VCvarB r_interpolate_frames_voxels("r_interpolate_frames_voxels", false, "Interpolate voxel model frames?", CVAR_Archive|CVAR_NoShadow);
VCvarB r_vsync("r_vsync", true, "VSync mode.", CVAR_Archive|CVAR_NoShadow);
VCvarB r_vsync_adaptive("r_vsync_adaptive", true, "Use adaptive VSync mode.", CVAR_Archive|CVAR_NoShadow);
VCvarB r_fade_light("r_fade_light", "0", "Fade light with distance?", CVAR_Archive|CVAR_NoShadow);
VCvarF r_fade_factor("r_fade_factor", "7", "Fading light coefficient.", CVAR_Archive|CVAR_NoShadow);
VCvarF r_fade_mult_regular("r_fade_mult_regular", "1", "Light fade multiplier for regular renderer.", CVAR_Archive|CVAR_NoShadow);
VCvarF r_fade_mult_advanced("r_fade_mult_advanced", "0.8", "Light fade multiplier for advanced renderer.", CVAR_Archive|CVAR_NoShadow);
VCvarF r_sky_bright_factor("r_sky_bright_factor", "1", "Skybright actor factor.", CVAR_Archive|CVAR_NoShadow);

// was 3072
VCvarF r_lights_radius("r_lights_radius", "6192", "Lights out of this radius (from camera) will be dropped.", CVAR_Archive|CVAR_NoShadow);
//static VCvarB r_lights_cast_many_rays("r_lights_cast_many_rays", false, "Cast more rays to better check light visibility (usually doesn't make visuals any better)?", CVAR_Archive);
//static VCvarB r_light_opt_separate_vis("r_light_opt_separate_vis", false, "Calculate light and render vis intersection as separate steps?", CVAR_Archive|CVAR_PreInit);

VCvarB r_shadows("r_shadows", true, "Allow shadows from lights?", CVAR_Archive);

VCvarB r_lit_semi_translucent("r_lit_semi_translucent", true, "Lit semi-translucent textures?", CVAR_Archive);

VCvarB r_lmap_overbright("r_lmap_overbright", true, "Use Quake-like (but gentlier) overbright for static lights (lightmap mode)?", CVAR_Archive);
VCvarB r_adv_overbright("r_adv_overbright", false, "Use Quake-like (but gentlier) overbright for static lights (advrender mode)?", CVAR_Archive);
VCvarF r_overbright_specular("r_overbright_specular", "0.1", "Specular light in regular renderer.", CVAR_Archive);

static VCvarF r_hud_fullscreen_alpha("r_hud_fullscreen_alpha", "0.44", "Alpha for fullscreen HUD", CVAR_Archive|CVAR_NoShadow);

extern VCvarB r_light_opt_shadow;

VDrawer *Drawer;

float PixelAspect;
float BaseAspect;
float PSpriteOfsAspect;
float EffectiveFOV;
float AspectFOVX;
float AspectEffectiveFOVX; // focaltangent
static float ViewCenterX;
static float RefScrWidth = 800.0f;
static float RefScrHeight = 600.0f;
static float CachedGlobVis = 1280.0f;

static FDrawerDesc *DrawerList[DRAWER_MAX];

VCvarI screen_size("screen_size", "12", "Screen size.", CVAR_Archive|CVAR_NoShadow); // default is "fullscreen with stats"
VCvarB allow_small_screen_size("allow_small_screen_size", false, "Allow small screen sizes.", /*CVAR_Archive*/CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);
static bool set_resolution_needed = true; // should we update screen size, FOV, and other such things?

// angles in the SCREENWIDTH wide window
VCvarF fov_main("fov", "90", "Field of vision.", CVAR_NoShadow);
VCvarF cl_fov("cl_fov", "0", "Client-enforced FOV (0 means 'default'.", CVAR_NoShadow);
VCvarB r_vertical_fov("r_vertical_fov", true, "Maintain vertical FOV for widescreen modes (i.e. keep vertical view area, and widen horizontal)?", CVAR_NoShadow);

// translation tables
VTextureTranslation *PlayerTranslations[MAXPLAYERS+1];

static VCvarB r_dbg_disable_all_precaching("r_dbg_disable_all_precaching", false, "Disable all texture precaching?", CVAR_PreInit|CVAR_NoShadow);
static VCvarB r_reupload_level_textures("r_reupload_level_textures", true, "Reupload level textures to GPU when new map is loaded?", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_precache_textures("r_precache_textures", true, "Precache level textures?", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_precache_models("r_precache_models", true, "Precache alias model textures?", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_precache_sprite_textures("r_precache_sprite_textures", true, "Precache sprite textures?", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_precache_weapon_sprite_textures("r_precache_weapon_sprite_textures", true, "Precache weapon textures?", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_precache_ammo_sprite_textures("r_precache_ammo_sprite_textures", true, "Precache ammo textures?", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_precache_player_sprite_textures("r_precache_player_sprite_textures", false, "Precache player sprite textures?", CVAR_Archive|CVAR_NoShadow);
static VCvarB r_precache_all_sprite_textures("r_precache_all_sprite_textures", false, "Precache sprite textures?", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_precache_max_sprites("r_precache_max_sprites", "3072", "Maxumum number of sprite textures to precache?", CVAR_Archive|CVAR_NoShadow);
static VCvarI r_level_renderer("r_level_renderer", "0", "Level renderer type (0:auto; 1:lightmap; 2:stenciled).", CVAR_Archive|CVAR_NoShadow);

int r_precache_textures_override = -1;

VCvarB r_dbg_lightbulbs_static("r_dbg_lightbulbs_static", false, "Draw lighbulbs for static lights?", CVAR_NoShadow);
VCvarB r_dbg_lightbulbs_dynamic("r_dbg_lightbulbs_dynamic", false, "Draw lighbulbs for dynamic lights?", CVAR_NoShadow);
VCvarF r_dbg_lightbulbs_zofs_static("r_dbg_lightbulbs_zofs_static", "0", "Z offset for static lightbulbs.", CVAR_NoShadow);
VCvarF r_dbg_lightbulbs_zofs_dynamic("r_dbg_lightbulbs_zofs_dynamic", "0", "Z offset for dynamic lightbulbs.", CVAR_NoShadow);


VCvarB prof_r_world_prepare("prof_r_world_prepare", false, "Show pre-render world preparation time.", CVAR_NoShadow);
VCvarB prof_r_bsp_collect("prof_r_bsp_collect", false, "Show BSP surface collection time.", CVAR_NoShadow);
VCvarB prof_r_bsp_world_render("prof_r_bsp_world_render", false, "Show world rendering time (GPU).", CVAR_NoShadow);
VCvarB prof_r_bsp_mobj_render("prof_r_bsp_mobj_render", false, "Show total mobj rendering time (including collection time).", CVAR_NoShadow);
VCvarB prof_r_bsp_mobj_collect("prof_r_bsp_mobj_collect", false, "Show total mobj collecting time.", CVAR_NoShadow);


// ////////////////////////////////////////////////////////////////////////// //
// pool allocator for portal data
// ////////////////////////////////////////////////////////////////////////// //
VRenderLevelShared::PPNode *VRenderLevelShared::pphead = nullptr;
VRenderLevelShared::PPNode *VRenderLevelShared::ppcurr = nullptr;
int VRenderLevelShared::ppMinNodeSize = 0;


struct AspectInfo {
  const int horiz;
  const int vert;
  const char *dsc;
};

static const AspectInfo AspectList[] = {
  { .horiz =  1, .vert =  1, .dsc = "Vanilla" }, // 1920x1200: 1.2f
  { .horiz =  4, .vert =  3, .dsc = "Standard 4:3" }, // 1920x1200: 1.0f
  { .horiz = 16, .vert =  9, .dsc = "Widescreen 16:9" }, // 1920x1200: 1.3333335f
  { .horiz = 16, .vert = 10, .dsc = "Widescreen 16:10" }, // 1920x1200: 1.2f
  { .horiz = 17, .vert = 10, .dsc = "Widescreen 17:10" }, // 1920x1200: 1.275f
  //{ .horiz = 21, .vert =  9, .dsc = "Widescreen 21:9" }, // 1920x1200: 1.75f
  //{ .horiz =  5, .vert =  4, .dsc = "Normal 5:4" }, // 1920x1200: 0.93750006f
};

#define ASPECT_COUNT  ((unsigned)(sizeof(AspectList)/sizeof(AspectList[0])))
//static_assert(ASPECT_COUNT == 4, "wtf?!");

int R_GetAspectRatioCount () noexcept { return (int)ASPECT_COUNT; }
int R_GetAspectRatioHoriz (int idx) noexcept { if (idx < 0 || idx >= (int)ASPECT_COUNT) idx = 0; return AspectList[idx].horiz; }
int R_GetAspectRatioVert (int idx) noexcept { if (idx < 0 || idx >= (int)ASPECT_COUNT) idx = 0; return AspectList[idx].vert; }
const char *R_GetAspectRatioDsc (int idx) noexcept { if (idx < 0 || idx >= (int)ASPECT_COUNT) idx = 0; return AspectList[idx].dsc; }


//==========================================================================
//
//  CalcAspect
//
//==========================================================================
static float CalcAspect (int aspectRatio, int scrwdt, int scrhgt, int *aspHoriz=nullptr, int *aspVert=nullptr) {
  // multiply with 1.2, because this is vanilla graphics scale
  if (aspectRatio < 0 || aspectRatio >= (int)ASPECT_COUNT) aspectRatio = 0;
  if (aspHoriz) *aspHoriz = AspectList[aspectRatio].horiz;
  if (aspVert) *aspVert = AspectList[aspectRatio].vert;
  if (aspectRatio == 0) return r_hack_aspect_scale.asFloat()*VDrawer::GetWindowAspect();
  #if 0
  return ((float)scrhgt*(float)AspectList[aspectRatio].horiz)/((float)scrwdt*(float)AspectList[aspectRatio].vert)*r_hack_aspect_scale.asFloat()*VDrawer::GetWindowAspect();
  #else
  const int sh = min2(scrhgt, scrwdt);
  const int sw = max2(scrhgt, scrwdt);
  return ((float)sh*(float)AspectList[aspectRatio].horiz)/((float)sw*(float)AspectList[aspectRatio].vert)*r_hack_aspect_scale.asFloat()*VDrawer::GetWindowAspect();
  //return (200.0f*(float)AspectList[aspectRatio].vert)/(320.0f*(float)AspectList[aspectRatio].horiz)*r_hack_aspect_scale.asFloat()*VDrawer::GetWindowAspect();
  #endif
}


//==========================================================================
//
//  CalcBaseAspectRatio
//
//==========================================================================
static float CalcBaseAspectRatio (int aspectRatio) {
  if (aspectRatio < 0 || aspectRatio >= (int)ASPECT_COUNT) aspectRatio = 0;
  return (float)AspectList[aspectRatio].horiz/(float)AspectList[aspectRatio].vert;
}


//==========================================================================
//
//  SetAspectRatioCVars
//
//==========================================================================
static void SetAspectRatioCVars (int aspectRatio, int scrwdt, int scrhgt) {
  int h = 1, v = 1;
  r_aspect_pixel = CalcAspect(aspectRatio, scrwdt, scrhgt, &h, &v);
  r_aspect_horiz = h;
  r_aspect_vert = v;
}


//==========================================================================
//
//  IsAspectTallerThanWide
//
//==========================================================================
static VVA_OKUNUSED inline bool IsAspectTallerThanWide (const float baseAspect) noexcept {
  return (baseAspect < 1.333f);

}


//==========================================================================
//
//  GetAspectBaseWidth
//
//==========================================================================
static VVA_OKUNUSED int GetAspectBaseWidth (float baseAspect) noexcept {
  return (int)roundf(240.0f*baseAspect*3.0f);
}


//==========================================================================
//
//  GetAspectBaseHeight
//
//==========================================================================
static VVA_OKUNUSED int GetAspectBaseHeight (float baseAspect) {
  if (!IsAspectTallerThanWide(baseAspect)) {
    return (int)roundf(200.0f*(320.0f/(GetAspectBaseWidth(baseAspect)/3.0f))*3.0f);
  } else {
    return (int)roundf((200.0f*(4.0f/3.0f))/baseAspect*3.0f);
  }
}


//==========================================================================
//
//  GetAspectMultiplier
//
//==========================================================================
static VVA_OKUNUSED int GetAspectMultiplier (float baseAspect) {
  if (!IsAspectTallerThanWide(baseAspect)) {
    return (int)roundf(320.0f/(GetAspectBaseWidth(baseAspect)/3.0f)*48.0f);
  } else {
    return (int)roundf(200.0f/(GetAspectBaseHeight(baseAspect)/3.0f)*48.0f);
  }
}


//==========================================================================
//
//  R_GetAspectRatio
//
//==========================================================================
float R_GetAspectRatio () {
  return CalcAspect(r_aspect_ratio, ScreenWidth, ScreenHeight);
}


//==========================================================================
//
//  R_GetAspectRatioValue
//
//==========================================================================
float R_GetAspectRatioValue () {
  int aspectRatio = r_aspect_ratio.asInt();
  if (aspectRatio < 0 || aspectRatio >= (int)ASPECT_COUNT) aspectRatio = 0;
  return (float)AspectList[aspectRatio].horiz/(float)AspectList[aspectRatio].vert;
}


//==========================================================================
//
//  R_CalcGlobVis
//
//  FIXME: totally wrong for cameras!
//
//==========================================================================
float R_CalcGlobVis () {
  return CachedGlobVis;
}


//==========================================================================
//
//  CalcGlobVis
//
//  FIXME: totally wrong for cameras!
//
//==========================================================================
static float CalcGlobVis () {
  float vis = r_light_globvis.asFloat();
  if (!isFiniteF(vis)) vis = 8.0f;
  vis = clampval(vis, -204.7f, 204.7f); // (205 and larger do not work in 5:4 aspect ratio)

  float virtwidth = RefScrWidth;
  float virtheight = RefScrHeight;

  /*
    // convert to vertical aspect ratio
    const float centerx = refdef.width*0.5f;

    // for widescreen displays, increase the FOV so that the middle part of the
    // screen that would be visible on a 4:3 display has the requested FOV
    // taken from GZDoom
    const float baseAspect = CalcBaseAspectRatio(r_aspect_ratio); // PixelAspect
    const float centerxwide = centerx*(IsAspectTallerThanWide(baseAspect) ? 1.0f : GetAspectMultiplier(baseAspect)/48.0f);
    if (centerxwide != centerx) {
      // centerxwide is what centerx would be if the display was not widescreen
      effectiveFOV = RAD2DEGF(2.0f*atanf(centerx*tanf(DEG2RADF(effectiveFOV)*0.5f)/centerxwide));
      // just in case
      if (effectiveFOV >= 180.0f) effectiveFOV = 179.5f;
    }
  */
  //const float baseAspect = CalcBaseAspectRatio(r_aspect_ratio); // PixelAspect

  if (IsAspectTallerThanWide(BaseAspect)) {
    virtheight = (virtheight*GetAspectMultiplier(BaseAspect))/48.0f;
  } else {
    virtwidth = (virtwidth*GetAspectMultiplier(BaseAspect))/48.0f;
  }

  const float r_Yaspect = 200.0f;

  const float YaspectMul = 320.0f*virtheight/(200.0f*virtwidth);
  float InvZtoScale = YaspectMul*ViewCenterX;

  float wallVisibility = vis;

  // prevent overflow on walls
  //FIXME: pass real width here for cameras?
  const float viewwidth = RefScrWidth;

  float maxVisForWall = (InvZtoScale*(RefScrWidth*r_Yaspect)/(viewwidth*RefScrHeight*AspectEffectiveFOVX));
  maxVisForWall = 32767.0 / maxVisForWall;
       if (vis < 0 && vis < -maxVisForWall) wallVisibility = -maxVisForWall;
  else if (vis > 0 && vis > maxVisForWall) wallVisibility = maxVisForWall;

  wallVisibility = InvZtoScale*RefScrWidth*GetAspectBaseHeight(BaseAspect)/(viewwidth*RefScrHeight*3.0f)*(wallVisibility*AspectEffectiveFOVX);

  const float res = wallVisibility/AspectEffectiveFOVX;
  //GCon->Logf(NAME_Debug, "R_CalcGlobVis=%f", res);

  return res/32.0f; // for shader
}


//==========================================================================
//
//  VRenderLevelShared::RegisterAllThinkers
//
//  called from `PreRender()` to register all
//  level thinkers with `ThinkerAdded()`
//
//==========================================================================
void VRenderLevelShared::RegisterAllThinkers () {
  //suid2ent.reset();
  for (TThinkerIterator<VThinker> th(Level); th; ++th) ThinkerAdded(*th);
}


//==========================================================================
//
//  VRenderLevelShared::SetMinPoolNodeSize
//
//==========================================================================
void VRenderLevelShared::SetMinPoolNodeSize (int minsz) {
  if (minsz < 0) minsz = 0;
  minsz = (minsz|0xfff)+1;
  if (ppMinNodeSize < minsz) {
#ifdef VAVOOM_DEBUG_PORTAL_POOL
    fprintf(stderr, "PORTALPOOL: new min node size is %d\n", minsz);
#endif
    ppMinNodeSize = minsz;
  }
}


//==========================================================================
//
//  VRenderLevelShared::CreatePortalPool
//
//==========================================================================
void VRenderLevelShared::CreatePortalPool () {
  KillPortalPool();
#ifdef VAVOOM_DEBUG_PORTAL_POOL
  fprintf(stderr, "PORTALPOOL: new\n");
#endif
}


//==========================================================================
//
//  VRenderLevelShared::KillPortalPool
//
//==========================================================================
void VRenderLevelShared::KillPortalPool () {
#ifdef VAVOOM_DEBUG_PORTAL_POOL
  int count = 0, total = 0;
#endif
  while (pphead) {
    PPNode *n = pphead;
    pphead = n->next;
#ifdef VAVOOM_DEBUG_PORTAL_POOL
    ++count;
    total += n->size;
#endif
    Z_Free(n->mem);
    Z_Free(n);
  }
#ifdef VAVOOM_DEBUG_PORTAL_POOL
  if (count) fprintf(stderr, "PORTALPOOL: freed %d nodes (%d total bytes)\n", count, total);
#endif
  pphead = ppcurr = nullptr;
  ppMinNodeSize = 0;
}


//==========================================================================
//
//  VRenderLevelShared::ResetPortalPool
//
//  called on frame start
//
//==========================================================================
void VRenderLevelShared::ResetPortalPool () {
  ppcurr = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::MarkPortalPool
//
//==========================================================================
void VRenderLevelShared::MarkPortalPool (PPMark *mark) {
  if (!mark) return;
  mark->curr = ppcurr;
  mark->currused = (ppcurr ? ppcurr->used : 0);
}


//==========================================================================
//
//  VRenderLevelShared::RestorePortalPool
//
//==========================================================================
void VRenderLevelShared::RestorePortalPool (PPMark *mark) {
  if (!mark || mark->currused == -666) return;
  ppcurr = mark->curr;
  if (ppcurr) ppcurr->used = mark->currused;
  mark->currused = -666; // just in case
}


//==========================================================================
//
//  VRenderLevelShared::AllocPortalPool
//
//==========================================================================
vuint8 *VRenderLevelShared::AllocPortalPool (int size) {
  if (size < 1) size = 1;
  int reqsz = size;
  if (reqsz%16 != 0) reqsz = (reqsz|0x0f)+1;
  int allocsz = (reqsz|0xfff)+1;
       if (allocsz < ppMinNodeSize) allocsz = ppMinNodeSize;
  else if (allocsz > ppMinNodeSize) ppMinNodeSize = allocsz;
  // was anything allocated?
  if (ppcurr == nullptr && pphead && pphead->size < allocsz) {
    // cannot use first node, free pool (it will be recreated later)
#ifdef VAVOOM_DEBUG_PORTAL_POOL
    fprintf(stderr, "PORTALPOOL: freeing allocated nodes (old size is %d, minsize is %d)\n", pphead->size, allocsz);
#endif
    while (pphead) {
      PPNode *n = pphead;
      pphead = n->next;
      Z_Free(n->mem);
      Z_Free(n);
    }
  }
  // no nodes at all?
  if (pphead == nullptr) {
    if (ppcurr != nullptr) Sys_Error("PortalPool: ppcur is not empty");
    // allocate first node
    pphead = (PPNode *)Z_Malloc(sizeof(PPNode));
    pphead->mem = (vuint8 *)Z_Malloc(allocsz);
    pphead->size = allocsz;
    pphead->used = 0;
    pphead->next = nullptr;
  }
  // was anything allocated?
  if (ppcurr == nullptr) {
    // nope, we should have a good first node here
    if (!pphead) Sys_Error("PortalPool: pphead is empty");
    if (pphead->size < reqsz) Sys_Error("PortalPool: pphead is too small");
    ppcurr = pphead;
    ppcurr->used = 0;
  }
  // check for easy case
  if (ppcurr->used+reqsz <= ppcurr->size) {
    vuint8 *res = ppcurr->mem+ppcurr->used;
    ppcurr->used += reqsz;
    return res;
  }
  // check if we have enough room in the next pool node
  if (ppcurr->next && reqsz <= ppcurr->next->size) {
    // yes; move to the next node, and use it
    ppcurr = ppcurr->next;
    ppcurr->used = reqsz;
    return ppcurr->mem;
  }
  // next node is absent or too small: kill "rest" nodes
  if (ppcurr->next) {
#ifdef VAVOOM_DEBUG_PORTAL_POOL
    fprintf(stderr, "PORTALPOOL: freeing \"rest\" nodes (old size is %d, minsize is %d)\n", ppcurr->size, allocsz);
#endif
    while (ppcurr->next) {
      PPNode *n = ppcurr;
      ppcurr->next = n->next;
      Z_Free(n->mem);
      Z_Free(n);
    }
  }
  // allocate a new node
  PPNode *nnn = (PPNode *)Z_Malloc(sizeof(PPNode));
  nnn->mem = (vuint8 *)Z_Malloc(allocsz);
  nnn->size = allocsz;
  nnn->used = reqsz;
  nnn->next = nullptr;
  ppcurr->next = nnn;
  ppcurr = nnn;
  return ppcurr->mem;
}


//==========================================================================
//
//  FDrawerDesc::FDrawerDesc
//
//==========================================================================
FDrawerDesc::FDrawerDesc (int Type, const char *AName, const char *ADescription, const char *ACmdLineArg, VDrawer *(*ACreator) ())
  : Name(AName)
  , Description(ADescription)
  , CmdLineArg(ACmdLineArg)
  , Creator(ACreator)
{
  DrawerList[Type] = this;
}


//==========================================================================
//
//  R_Init
//
//==========================================================================
void R_Init () {
  R_InitSkyBoxes();
  // create light remapping table
  for (int i = 0; i < 256; ++i) {
    int n = i*i/255;
    /*
         if (n == 0) n = 4;
    //else if (n < 64) n += n/2;
    else if (n < 128) n += n/3;
    */
    if (n < 8) n = 8;
    if (n > 255) n = 255; else if (n < 0) n = 0;
    light_remap[i] = clampToByte(n);
  }
}


//==========================================================================
//
//  R_InitAliasModels
//
//==========================================================================
void R_InitAliasModels () {
  R_InitModels();
  R_LoadAllModelsSkins();
}


//==========================================================================
//
//  R_Start
//
//==========================================================================
void R_Start (VLevel *ALevel) {
  SCR_Update(false); // partial update
  if (r_level_renderer > 1 && !Drawer->SupportsShadowVolumeRendering() && !Drawer->SupportsShadowMapRendering()) {
    GCon->Logf(NAME_Warning, "Your GPU doesn't support neither Shadow Volume Renderer, nor Shadow Maps, so I will switch to the lightmapped one.");
    r_level_renderer = 1;
  } else if (r_level_renderer <= 0) {
    // prefer shadowmaps
    if (Drawer->SupportsShadowMapRendering()) {
      GCon->Logf("Your GPU supports ShadowMap Renderer, so i will use it.");
      r_level_renderer = 2;
      r_shadowmaps = true;
    } else if (Drawer->SupportsShadowVolumeRendering()) {
      r_shadowmaps = false;
      if (Drawer->IsShittyGPU()) {
        GCon->Logf("Your GPU is... not quite good, so I will use the lightmapped renderer.");
        r_level_renderer = 1;
      } else {
        GCon->Logf("Your GPU supports Shadow Volume Renderer, so i will use it.");
        r_level_renderer = 2;
      }
    } else {
      GCon->Logf(NAME_Warning, "Your GPU doesn't support neither Shadow Volume Renderer, nor Shadow Maps, so I will switch to the lightmapped one.");
      r_level_renderer = 1;
    }
  }
  // now create renderer
  Drawer->LevelRendererDestroyed();
  if (r_level_renderer <= 1) {
    ALevel->Renderer = new VRenderLevelLightmap(ALevel);
  } else {
    ALevel->Renderer = new VRenderLevelShadowVolume(ALevel);
    // force shadowmaps if shadow volumes are not supported
    if (!Drawer->SupportsShadowVolumeRendering()) {
      if (!r_shadowmaps) {
        r_shadowmaps = true;
        GCon->Logf("Forced shadowmap renderer, because shadow volumes are not supported.");
      }
    }
  }
  Drawer->LevelRendererCreated(ALevel->Renderer);
}


//==========================================================================
//
//  VRenderLevelShared::VRenderLevelShared
//
//==========================================================================
VRenderLevelShared::VRenderLevelShared (VLevel *ALevel)
  : VRenderLevelDrawer(ALevel)
  //, Level(ALevel)
  , ViewEnt(nullptr)
  , CurrPortal(nullptr)
  , MirrorLevel(0)
  , PortalLevel(0)
  , BspVisData(nullptr)
  , BspVisSectorData(nullptr)
  , r_viewleaf(nullptr)
  , r_viewregion(nullptr)
  , old_fov(90.0f)
  , prev_aspect_ratio(666)
  , prev_vertical_fov_flag(false)
  , ExtraLight(0)
  , FixedLight(0)
  , Particles(0)
  , ActiveParticles(0)
  , FreeParticles(0)
  , CurrentSky1Texture(-1)
  , CurrentSky2Texture(-1)
  , CurrentDoubleSky(false)
  , CurrentLightning(false)
  , SkyWasInited(false)
  , free_wsurfs(nullptr)
  , AllocatedWSurfBlocks(nullptr)
  , AllocatedSubRegions(nullptr)
  , AllocatedDrawSegs(nullptr)
  , AllocatedSegParts(nullptr)
  , SubRegionInfo(nullptr)
  , inWorldCreation(false)
  , updateWorldFrame(1u) // let it be `1` for lightmapped lights
  //, litSurfacesValidFrame(1u)
  , bspVisRadius(nullptr)
  , bspVisRadiusFrame(0)
  , NumSegParts(0)
  , pspart(nullptr)
  , pspartsLeft(0)
{
  lastRenderQuality = r_fix_tjunctions.asBool();
  currDLightFrame = 0;
  currQueueFrame = 0;
  currVisFrame = 0;

  tjLineMarkCheck = (vuint32 *)Z_Calloc((Level->NumLines+1)*sizeof(tjLineMarkCheck[0]));
  tjLineMarkFix = (vuint32 *)Z_Calloc((Level->NumLines+1)*sizeof(tjLineMarkFix[0]));
  tjSubMarkFix = (vuint32 *)Z_Calloc((Level->NumSubsectors+1)*sizeof(tjSubMarkFix[0]));

  PortalDepth = 0;
  PortalUsingStencil = 0;
  //VPortal::ResetFrame();

  BspVisFrame = 1;
  BspVisData = new unsigned[Level->NumSubsectors+1];
  memset(BspVisData, 0, (Level->NumSubsectors+1)*sizeof(BspVisData[0]));
  BspVisSectorData = new unsigned[Level->NumSectors+1];
  memset(BspVisSectorData, 0, (Level->NumSectors+1)*sizeof(BspVisSectorData[0]));

  LightFrameNum = 1; // just to play safe
  LightVis = new unsigned[Level->NumSubsectors+1];
  LightBspVis = new unsigned[Level->NumSubsectors+1];
  memset(LightVis, 0, sizeof(LightVis[0])*(Level->NumSubsectors+1));
  memset(LightBspVis, 0, sizeof(LightBspVis[0])*(Level->NumSubsectors+1));
  //GCon->Logf(NAME_Debug, "*** SUBSECTORS: %d", Level->NumSubsectors);

  SubStaticLights.setLength(Level->NumSubsectors);

  memset((void *)DLights, 0, sizeof(DLights));

  CreatePortalPool();

  InitParticles();
  ClearParticles();

  screenblocks = 0;

  // preload graphics
  // r_precache_textures_override<0: not set; otherwise it is zero
  if (r_precache_textures_override > 0 || (r_precache_textures && r_precache_textures_override != 0) ||
      r_precache_models || r_precache_sprite_textures || r_precache_all_sprite_textures)
  {
    PrecacheLevel();
  }

  ResetVisFrameCount();
  ResetDLightFrameCount();
  ResetUpdateWorldFrame();
  for (auto &&sd : Level->allSides()) sd.rendercount = 0;

  Level->ResetPObjRenderCounts(); // we'll need them

  renderedSectorCounter = 0;
  renderedLineCounter = 0;
  forceDisableShadows = false;

  ColorMap = CM_Default;
  skyheight = 0;
  memset((void *)(&dlinfo[0]), 0, sizeof(dlinfo));
  CurrLightRadius = 0;
  CurrLightInFrustum = false;
  CurrLightBit = 0;
  HasLightIntersection = false;
  LitSurfaceHit = false;
  LitCalcBBox = true;
  CurrLightCalcUnstuck = false;
  //HasBackLit = false;
  doShadows = false;
  //MirrorClipSegs = false;

  VDrawer::LightFadeMult = 1.0f; // for now
  if (Drawer) {
    Drawer->SetMainFBO();
    Drawer->ClearCameraFBOs();
    int bbcount = 0;
    for (auto &&camtexinfo : Level->CameraTextures) {
      VTexture *BaseTex = GTextureManager[camtexinfo.TexNum];
      if (!BaseTex || !BaseTex->bIsCameraTexture) continue;
      VCameraTexture *Tex = (VCameraTexture *)BaseTex;
      Tex->camfboidx = Drawer->GetCameraFBO(camtexinfo.TexNum, max2(1, BaseTex->Width), max2(1, BaseTex->Height));
      vassert(Tex->camfboidx >= 0);
      Tex->NextUpdateTime = 0; // force updating
      ++bbcount;
    }
    if (Level->CameraTextures.length()) GCon->Logf("******* preallocated %d camera FBOs out of %d", bbcount, Level->CameraTextures.length());
  }
}


//==========================================================================
//
//  VRenderLevelShared::~VRenderLevelShared
//
//==========================================================================
VRenderLevelShared::~VRenderLevelShared () {
  VDrawer::LightFadeMult = 1.0f; // restore it
  if (Drawer) {
    Drawer->ClearCameraFBOs();
    Drawer->LevelRendererDestroyed();
  }

  if (Level->CameraTextures.length()) {
    int bcnt = 0;
    for (auto &&camtexinfo : Level->CameraTextures) {
      VTexture *BaseTex = GTextureManager[camtexinfo.TexNum];
      if (!BaseTex || !BaseTex->bIsCameraTexture) continue;
      ++bcnt;
      VCameraTexture *Tex = (VCameraTexture *)BaseTex;
      Tex->camfboidx = -1;
    }
    GCon->Logf("freeing %d camera FBOs out of %d camera textures", bcnt, Level->CameraTextures.length());
  }

  UncacheLevel();

  // free fake floor data
  for (auto &&sector : Level->allSectors()) {
    if (sector.fakefloors) {
      sector.eregions->params = &sector.params; // because it was changed by fake floor
      // delete if it is not a loader fix
      if (!sector.fakefloors->IsCreatedByLoader()) {
        delete sector.fakefloors;
        sector.fakefloors = nullptr;
      }
    }
  }

  delete[] bspVisRadius;
  bspVisRadius = nullptr;

  // clean all subregion decal lists
  // do this with a hack, because the renderer doesn't clone decals, ever
  for (decal_t *dc = Level->subdecalhead; dc; dc = dc->next) {
    dc->sreg = nullptr;
    dc->sregprev = dc->sregnext = nullptr;
  }

  for (auto &&sub : Level->allSubsectors()) {
    for (subregion_t *r = sub.regions; r != nullptr; r = r->next) {
      //r->killAllDecals(Level);
      if (r->realfloor != nullptr) {
        FreeSurfaces(r->realfloor->surfs);
        delete r->realfloor;
        r->realfloor = nullptr;
      }
      if (r->realceil != nullptr) {
        FreeSurfaces(r->realceil->surfs);
        delete r->realceil;
        r->realceil = nullptr;
      }
      if (r->fakefloor != nullptr) {
        FreeSurfaces(r->fakefloor->surfs);
        delete r->fakefloor;
        r->fakefloor = nullptr;
      }
      if (r->fakeceil != nullptr) {
        FreeSurfaces(r->fakeceil->surfs);
        delete r->fakeceil;
        r->fakeceil = nullptr;
      }
    }
    sub.regions = nullptr;

    // remove polyobject floor and ceiling
    /*
    if (sub.HasPObjs()) {
      for (auto &&poit : sub.PObjFirst()) {
        polyobj_t *pobj = poit.pobj();
        if (pobj->region) {
          if (pobj->region->realfloor) {
            FreeSurfaces(pobj->region->realfloor->surfs);
            delete pobj->region->realfloor;
            pobj->region->realfloor = nullptr;
          }
          if (pobj->region->realceil) {
            FreeSurfaces(pobj->region->realceil->surfs);
            delete pobj->region->realceil;
            pobj->region->realceil = nullptr;
          }
          delete pobj->region;
          pobj->region = nullptr;
        }
      }
    }
    */
  }

  // free seg parts
  for (auto &&seg : Level->allSegs()) {
    drawseg_t *ds = seg.drawsegs;
    if (ds) {
      if (ds->top) FreeSegParts(ds->top);
      if (ds->mid) FreeSegParts(ds->mid);
      if (ds->bot) FreeSegParts(ds->bot);
      if (ds->topsky) FreeSegParts(ds->topsky);
      if (ds->extra) FreeSegParts(ds->extra);
      if (ds->HorizonTop) Z_Free(ds->HorizonTop);
      if (ds->HorizonBot) Z_Free(ds->HorizonBot);
      memset((void *)ds, 0, sizeof(drawseg_t));
    }
    seg.drawsegs = nullptr;
  }

  // free allocated wall surface blocks
  for (void *Block = AllocatedWSurfBlocks; Block; ) {
    void *Next = *(void **)Block;
    Z_Free(Block);
    Block = Next;
  }
  AllocatedWSurfBlocks = nullptr;

  // free big blocks
  delete[] AllocatedSubRegions;
  AllocatedSubRegions = nullptr;
  delete[] AllocatedDrawSegs;
  AllocatedDrawSegs = nullptr;
  delete[] AllocatedSegParts;
  AllocatedSegParts = nullptr;
  delete[] SubRegionInfo;
  SubRegionInfo = nullptr;

  delete[] Particles;
  Particles = nullptr;

  delete[] BspVisData;
  BspVisData = nullptr;
  delete[] BspVisSectorData;
  BspVisSectorData = nullptr;

  delete[] LightVis;
  LightVis = nullptr;
  delete[] LightBspVis;
  LightBspVis = nullptr;

  for (int i = 0; i < SideSkies.length(); ++i) {
    delete SideSkies[i];
    SideSkies[i] = nullptr;
  }

  Z_Free(tjLineMarkCheck);
  tjLineMarkCheck = nullptr;
  Z_Free(tjLineMarkFix);
  tjLineMarkFix = nullptr;
  Z_Free(tjSubMarkFix);
  tjSubMarkFix = nullptr;

  KillPortalPool();

  if (Level && Level->Renderer == this) Level->Renderer = nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::IsNodeRendered
//
//==========================================================================
bool VRenderLevelShared::IsNodeRendered (const node_t *node) const noexcept {
  if (!node) return false;
  return (node->visframe == currVisFrame);
}


//==========================================================================
//
//  VRenderLevelShared::IsSubsectorRendered
//
//==========================================================================
bool VRenderLevelShared::IsSubsectorRendered (const subsector_t *sub) const noexcept {
  if (!sub) return false;
  return (sub->VisFrame == currVisFrame);
}


//==========================================================================
//
//  VRenderLevelShared::ResetVisFrameCount
//
//==========================================================================
void VRenderLevelShared::ResetVisFrameCount () noexcept {
  currVisFrame = 1;
  for (auto &&it : Level->allNodes()) it.visframe = 0;
  for (auto &&it : Level->allSubsectors()) it.VisFrame = 0;
}


//==========================================================================
//
//  VRenderLevelShared::ResetDLightFrameCount
//
//==========================================================================
void VRenderLevelShared::ResetDLightFrameCount () noexcept {
  currDLightFrame = 1;
  for (auto &&it : Level->allSubsectors()) {
    it.dlightframe = 0;
    it.dlightbits = 0;
  }
}


//==========================================================================
//
//  VRenderLevelShared::ResetUpdateWorldFrame
//
//==========================================================================
void VRenderLevelShared::ResetUpdateWorldFrame () noexcept {
  updateWorldFrame = 1;
  for (auto &&it : Level->allSubsectors()) it.updateWorldFrame = 0;
  for (auto &&it : Level->allPolyobjects()) it->updateWorldFrame = 0;
  for (auto &&ld : Level->allLines()) ld.updateWorldFrame = 0;
  for (auto &&lt : Lights) lt.litSurfacesValidFrame = 0;
  if (Level->NumLines) {
    if (tjLineMarkCheck) memset((void *)tjLineMarkCheck, 0, Level->NumLines*sizeof(tjLineMarkCheck[0]));
    if (tjLineMarkFix) memset((void *)tjLineMarkFix, 0, Level->NumLines*sizeof(tjLineMarkFix[0]));
    if (tjSubMarkFix) memset((void *)tjSubMarkFix, 0, Level->NumSubsectors*sizeof(tjSubMarkFix[0]));
  }
}


//==========================================================================
//
//  VRenderLevelShared::ClearQueues
//
//==========================================================================
void VRenderLevelShared::ClearQueues () {
  GetCurrentDLS().resetAll();
  IncQueueFrameCount();
}


//==========================================================================
//
//  VRenderLevelShared::GetStaticLightCount
//
//==========================================================================
int VRenderLevelShared::GetStaticLightCount () const noexcept {
  return Lights.length();
}


//==========================================================================
//
//  VRenderLevelShared::GetStaticLight
//
//==========================================================================
VRenderLevelPublic::LightInfo VRenderLevelShared::GetStaticLight (int idx) const noexcept {
  LightInfo res;
  res.origin = Lights[idx].origin;
  res.radius = Lights[idx].radius;
  res.color = Lights[idx].color;
  res.active = Lights[idx].active;
  return res;
}


//==========================================================================
//
//  VRenderLevelShared::GetDynamicLightCount
//
//==========================================================================
int VRenderLevelShared::GetDynamicLightCount () const noexcept {
  return MAX_DLIGHTS;
}


//==========================================================================
//
//  VRenderLevelShared::GetDynamicLight
//
//==========================================================================
VRenderLevelPublic::LightInfo VRenderLevelShared::GetDynamicLight (int idx) const noexcept {
  LightInfo res;
  res.origin = DLights[idx].origin;
  res.radius = DLights[idx].radius;
  res.color = DLights[idx].color;
  res.active = (res.radius > 0);
  return res;
}


//==========================================================================
//
//  R_SetViewSize
//
//  Do not really change anything here, because it might be in the middle
//  of a refresh. The change will take effect next refresh.
//
//==========================================================================
void R_SetViewSize (int blocks) {
  if (blocks > 2) {
    if (screen_size != blocks) {
      screen_size = blocks;
      set_resolution_needed = true;
    }
  }
}


//==========================================================================
//
//  R_ForceViewSizeUpdate
//
//==========================================================================
void R_ForceViewSizeUpdate () {
  set_resolution_needed = true;
}


//==========================================================================
//
//  COMMAND SizeDown
//
//==========================================================================
COMMAND(SizeDown) {
  R_SetViewSize(screenblocks-1);
  GAudio->PlaySound(GSoundManager->GetSoundID("menu/change"),
                    TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
}


//==========================================================================
//
//  COMMAND SizeUp
//
//==========================================================================
COMMAND(SizeUp) {
  R_SetViewSize(screenblocks+1);
  GAudio->PlaySound(GSoundManager->GetSoundID("menu/change"), TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
}


//==========================================================================
//
//  VRenderLevelShared::CalcEffectiveFOV
//
//==========================================================================
float VRenderLevelShared::CalcEffectiveFOV (float fov, const refdef_t &refdef) {
  if (!isFiniteF(fov)) fov = 90.0f;
  fov = clampval(fov, 1.0f, 170.0f);

  float effectiveFOV = fov;
  if (r_vertical_fov) {
    // convert to vertical aspect ratio
    const float centerx = refdef.width*0.5f;

    // for widescreen displays, increase the FOV so that the middle part of the
    // screen that would be visible on a 4:3 display has the requested FOV
    // taken from GZDoom
    const float baseAspect = CalcBaseAspectRatio(r_aspect_ratio); // PixelAspect
    const float centerxwide = centerx*(IsAspectTallerThanWide(baseAspect) ? 1.0f : GetAspectMultiplier(baseAspect)/48.0f);
    if (centerxwide != centerx) {
      // centerxwide is what centerx would be if the display was not widescreen
      effectiveFOV = RAD2DEGF(2.0f*atanf(centerx*tanf(DEG2RADF(effectiveFOV)*0.5f)/centerxwide));
      // just in case
      if (effectiveFOV >= 180.0f) effectiveFOV = 179.5f;
    }
  }

  return effectiveFOV;
}


//==========================================================================
//
//  VRenderLevelShared::SetupRefdefWithFOV
//
//==========================================================================
void VRenderLevelShared::SetupRefdefWithFOV (refdef_t *refdef, float fov) {
  clip_base.setupViewport(refdef->width, refdef->height, fov, (r_dbg_proj_aspect.asBool() ? PixelAspect : 1.0f));
  refdef->fovx = clip_base.fovx;
  refdef->fovy = clip_base.fovy;
}


//==========================================================================
//
//  VRenderLevelShared::ExecuteSetViewSize
//
//==========================================================================
void VRenderLevelShared::ExecuteSetViewSize () {
       if (r_hack_aspect_scale.asFloat() < 0.02f) r_hack_aspect_scale = 0.02f;
  else if (r_hack_aspect_scale.asFloat() > 64.0f) r_hack_aspect_scale = 64.0f;

  set_resolution_needed = false;
  prev_r_dbg_proj_aspect = r_dbg_proj_aspect.asBool();
  prev_r_hack_aspect_scale = r_hack_aspect_scale.asFloat();
  prev_r_hack_psprite_yofs = r_hack_psprite_yofs.asFloat();
  prev_r_light_globvis = r_light_globvis.asFloat();

  // sanitise screen size
  if (allow_small_screen_size) {
    screen_size = clampval(screen_size.asInt(), 3, 13);
  } else {
    screen_size = clampval(screen_size.asInt(), 11, 13);
  }
  screenblocks = screen_size;

  // sanitise aspect ratio
  #ifdef VAVOOM_K8_DEVELOPER
  if (r_aspect_ratio < 0) r_aspect_ratio = 0;
  if (r_aspect_ratio >= (int)ASPECT_COUNT) r_aspect_ratio = 0;
  #else
  if (r_aspect_ratio < 1) r_aspect_ratio = 1;
  if (r_aspect_ratio >= (int)ASPECT_COUNT) r_aspect_ratio = 1;
  #endif

  // sanitise FOV
       if (fov_main < 1.0f) fov_main = 1.0f;
  else if (fov_main > 170.0f) fov_main = 170.0f;

  float fov = fov_main;
  // apply client fov
  if (cl_fov > 1) fov = clampval(cl_fov.asFloat(), 1.0f, 170.0f);
  old_fov = fov;

  if (screenblocks > 10) {
    // no status bar
    refdef.width = ScreenWidth;
    refdef.height = ScreenHeight;
    refdef.y = 0;
  } else if (GGameInfo->NetMode == NM_TitleMap) {
    // no status bar for titlemap
    refdef.width = screenblocks*ScreenWidth/10;
    refdef.height = (screenblocks*ScreenHeight/10);
    refdef.y = (ScreenHeight-refdef.height)>>1;
  } else {
    refdef.width = screenblocks*ScreenWidth/10;
    refdef.height = (screenblocks*(ScreenHeight-SB_RealHeight())/10);
    refdef.y = (ScreenHeight-SB_RealHeight()-refdef.height)>>1;
  }
  refdef.x = (ScreenWidth-refdef.width)>>1;

  RefScrWidth = refdef.width;
  RefScrHeight = refdef.height;

  PixelAspect = CalcAspect(r_aspect_ratio, ScreenWidth, ScreenHeight);
  SetAspectRatioCVars(r_aspect_ratio, ScreenWidth, ScreenHeight);
  prev_aspect_ratio = r_aspect_ratio;

  BaseAspect = CalcBaseAspectRatio(r_aspect_ratio);

  float effectiveFOV = fov;
  float currentFOV = effectiveFOV;

  prev_vertical_fov_flag = r_vertical_fov;
  if (r_vertical_fov) {
    // convert to vertical aspect ratio
    const float centerx = refdef.width*0.5f;

    // for widescreen displays, increase the FOV so that the middle part of the
    // screen that would be visible on a 4:3 display has the requested FOV
    // taken from GZDoom
    const float baseAspect = CalcBaseAspectRatio(r_aspect_ratio); // PixelAspect
    const float centerxwide = centerx*(IsAspectTallerThanWide(baseAspect) ? 1.0f : GetAspectMultiplier(baseAspect)/48.0f);
    if (centerxwide != centerx) {
      // centerxwide is what centerx would be if the display was not widescreen
      effectiveFOV = RAD2DEGF(2.0f*atanf(centerx*tanf(DEG2RADF(effectiveFOV)*0.5f)/centerxwide));
      // just in case
      if (effectiveFOV >= 180.0f) effectiveFOV = 179.5f;
    }

    // GZDoom does this; i don't know why yet
    PSpriteOfsAspect = (!IsAspectTallerThanWide(baseAspect) ? 0.0f : ((4.0f/3.0f)/baseAspect-1.0f)*97.5f);
    if (ScreenHeight+64 > ScreenWidth) {
      //PSpriteOfsAspect = ((4.0f/3.0f)/baseAspect)*97.5f;
      //PSpriteOfsAspect = baseAspect*100.0f;
      //PSpriteOfsAspect = (float)ScreenHeight/(float)ScreenWidth/r_hack_aspect_scale.asFloat()*baseAspect*97.5f;
      PSpriteOfsAspect = (float)ScreenHeight/(float)ScreenWidth*97.5f;
      //PSpriteOfsAspect = baseAspect*97.5f;
      GCon->Logf(NAME_Debug, "*** baseAspect=%f; taller=%d; psofs=%f", baseAspect, (int)IsAspectTallerThanWide(baseAspect), PSpriteOfsAspect);
    }
    // 0.91: 24
    // 0.986: 14
    // 1: 12.2
    // 1.2: 0
    // 2: -42.2
    //PSpriteOfsAspect -= 42.2f;
    if (fabsf(r_hack_aspect_scale.asFloat()-1.2f) > 0.001f) {
      GCon->Logf(NAME_Debug, "!!! baseAspect=%f; IsAspectTallerThanWide=%d; PSpriteOfsAspect=%f", baseAspect, IsAspectTallerThanWide(baseAspect), PSpriteOfsAspect);
      PSpriteOfsAspect -= (r_hack_aspect_scale.asFloat()-1.2f)*(97.5f*0.5f)*baseAspect;
      GCon->Logf(NAME_Debug, "!!!   fixed PSpriteOfsAspect=%f", PSpriteOfsAspect);
    }
    if (r_hack_psprite_yofs.asFloat()) {
      PSpriteOfsAspect += r_hack_psprite_yofs.asFloat();
      GCon->Logf(NAME_Debug, "!!!   final PSpriteOfsAspect=%f", PSpriteOfsAspect);
    }
    ViewCenterX = centerx;
  } else {
    PSpriteOfsAspect = 0.0f;
    ViewCenterX = refdef.width*0.5f;
  }
  EffectiveFOV = effectiveFOV;

  clip_base.setupViewport(refdef.width, refdef.height, effectiveFOV, (r_dbg_proj_aspect.asBool() ? PixelAspect : 1.0f));
  refdef.fovx = clip_base.fovx;
  refdef.fovy = clip_base.fovy;
  refdef.drawworld = true;

  AspectFOVX = refdef.fovx;
  AspectEffectiveFOVX = tanf(DEG2RADF(currentFOV)*0.5f);

  CachedGlobVis = CalcGlobVis();

  /*
  Drawer->ClearScreen(VDrawer::CLEAR_ALL);
  Drawer->LevelRendererDestroyed();
  Drawer->LevelRendererCreated(this);
  //GCon->Logf(NAME_Debug, "ExecuteSetViewSize: screenblocks=%d", screenblocks);
  */
}


//==========================================================================
//
//  R_DrawViewBorder
//
//==========================================================================
void R_DrawViewBorder () {
  if (GGameInfo->NetMode == NM_TitleMap) {
    GClGame->eventDrawViewBorder(VirtualWidth/2-screenblocks*32, (VirtualHeight-screenblocks*480/10)/2, screenblocks*64, screenblocks*VirtualHeight/10);
  } else {
    GClGame->eventDrawViewBorder(VirtualWidth/2-screenblocks*32, (VirtualHeight-sb_height-screenblocks*(VirtualHeight-sb_height)/10)/2, screenblocks*64, screenblocks*(VirtualHeight-sb_height)/10);
  }
}


//==========================================================================
//
//  VRenderLevelShared::TransformFrustum
//
//==========================================================================
void VRenderLevelShared::TransformFrustum () {
  //viewfrustum.setup(clip_base, vieworg, viewangles, false/*no back plane*/, -1.0f/*no forward plane*/);
  bool useFrustumFar = (gl_maxdist > 1.0f);
  if (useFrustumFar && !r_clip_maxdist) {
    useFrustumFar = (Drawer ? Drawer->UseFrustumFarClip() : false);
  }
  Drawer->viewfrustum.setup(clip_base, TFrustumParam(Drawer->vieworg, Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup), true/*create back plane*/,
    (useFrustumFar ? gl_maxdist : -1.0f/*no forward plane*/));
}


//==========================================================================
//
//  PlrInterpAngle
//
//==========================================================================
static void PlrInterpAngle (VLevel *Level, VPlrAngleInterp &ii, float &currangle) {
  if (ii.Duration > 0.0f) {
    const float pdiff = (Level->Time-ii.StartTime)/ii.Duration;
    if (pdiff < 0.0f || pdiff >= 1.0f) {
      //GCon->Logf(NAME_Debug, "pdiff=%g", pdiff);
      ii.Duration = 0.0f;
    } else {
      const float delta = AngleDiff(AngleMod(ii.Prev), AngleMod(currangle));
      //GCon->Logf(NAME_Debug, "pdiff=%g; delta=%g; prev=%g; curr=%g; val=%g", pdiff, delta, ii.Prev, currangle, ii.Prev+delta*pdiff);
      currangle = ii.Prev+delta*pdiff;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::SetupFrame
//
//==========================================================================
void VRenderLevelShared::SetupFrame () {
  // change the view size if needed
  if (screen_size != screenblocks || !screenblocks ||
      set_resolution_needed || old_fov != fov_main || cl_fov >= 1 ||
      r_aspect_ratio != prev_aspect_ratio ||
      r_vertical_fov != prev_vertical_fov_flag ||
      r_dbg_proj_aspect.asBool() != prev_r_dbg_proj_aspect ||
      r_hack_aspect_scale.asFloat() != prev_r_hack_aspect_scale ||
      r_hack_psprite_yofs.asFloat() != prev_r_hack_psprite_yofs ||
      r_light_globvis.asFloat() != prev_r_light_globvis)
  {
    ExecuteSetViewSize();
  }

  ViewEnt = cl->Camera;
  CurrPortal = nullptr;

  Drawer->viewangles = cl->ViewAngles;

  // interpolate pitch and yaw
  if (cl->Camera == cl->MO) {
    PlrInterpAngle(cl->MO->XLevel, cl->ViewYawInterp, Drawer->viewangles.yaw);
    PlrInterpAngle(cl->MO->XLevel, cl->ViewPitchInterp, Drawer->viewangles.pitch);
    PlrInterpAngle(cl->MO->XLevel, cl->ViewRollInterp, Drawer->viewangles.roll);
  }

  if (r_chasecam && r_chase_front) {
    // this is used to see how weapon looks in player's hands
    Drawer->viewangles.yaw = AngleMod(Drawer->viewangles.yaw+180);
    Drawer->viewangles.pitch = -Drawer->viewangles.pitch;
  }
  AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);

  Drawer->viewfrustum.clear(); // why not?

  if (r_chasecam && cl->MO == cl->Camera) {
    //Drawer->vieworg = cl->MO->Origin+TVec(0.0f, 0.0f, 32.0f)-r_chase_dist*viewforward+r_chase_up*viewup+r_chase_right*viewright;
    // for demo replay, make camera always looking forward
    if (cls.demoplayback) {
      Drawer->viewangles.pitch = 0;
      Drawer->viewangles.roll = 0;
      AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
    }
    TVec endcpos = cl->MO->Origin+TVec(0.0f, 0.0f, r_chase_raise)-r_chase_dist*Drawer->viewforward+r_chase_up*Drawer->viewup+r_chase_right*Drawer->viewright;
    // try to move camera as far as we can
    TVec cpos = cl->MO->Origin;
    for (;;) {
      TVec npos = cl->MO->SlideMoveCamera(cpos, endcpos, r_chase_radius);
      float zdiff = fabsf(cpos.z-npos.z);
      cpos = npos;
      if (zdiff < 1.0f) break;
      // try to move up
      npos = cl->MO->SlideMoveCamera(cpos, TVec(cpos.x, cpos.y, endcpos.z), r_chase_radius);
      zdiff = fabsf(cpos.z-npos.z);
      cpos = npos;
      if (zdiff < 1.0f) break;
    }
    // interpolate camera position
    const double cameraITime = r_chase_delay.asFloat();
    if (cameraITime <= 0) {
      prevChaseCamTime = -1;
      prevChaseCamPos = cpos;
    } else {
      const double currTime = Sys_Time();
      const double deltaTime = currTime-prevChaseCamTime;
      if (prevChaseCamTime < 0 || deltaTime < 0 || deltaTime >= cameraITime || cameraITime <= 0) {
        prevChaseCamTime = currTime;
        prevChaseCamPos = cpos;
      } else {
        // some interpolation
        TVec delta = cpos-prevChaseCamPos;
        if (fabsf(delta.x) <= 1 && fabsf(delta.y) <= 1 && fabsf(delta.z) <= 1) {
          prevChaseCamTime = currTime;
          prevChaseCamPos = cpos;
        } else {
          const float dtime = float(deltaTime/cameraITime);
          prevChaseCamPos += delta*dtime;
          prevChaseCamTime = currTime;
        }
      }
    }
    Drawer->vieworg = prevChaseCamPos;
    //Drawer->vieworg = cpos;
  } else {
    prevChaseCamTime = -1;
    Drawer->vieworg = cl->ViewOrg;
  }

  const bool allowFB = !(r_colormap_mode.asInt()&1);
  const bool allowCMap = !(r_colormap_mode.asInt()&2);

  ColorMapFixedLight = false;
  FullbrightThings = 0;
  ExtraLight = (ViewEnt && ViewEnt->Player ? clampval(ViewEnt->Player->ExtraLight*8, 0, 255) : 0);

  FixedLight = 0;
  ColorMap = CM_Default;

  bool doInverseHack = true;
  if (cl->Camera == cl->MO) {
         if (cl->FixedColormap == INVERSECOLORMAP) { if (allowCMap) ColorMap = CM_Inverse; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == INVERSXCOLORMAP) { if (allowCMap) ColorMap = CM_Inverse; doInverseHack = false; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == GOLDCOLORMAP) { if (allowCMap) ColorMap = CM_Gold; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == REDCOLORMAP) { if (allowCMap) ColorMap = CM_Red; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == GREENCOLORMAP) { if (allowCMap) ColorMap = CM_Green; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == MONOCOLORMAP) { if (allowCMap) ColorMap = CM_Mono; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == BEREDCOLORMAP) { if (allowCMap) ColorMap = CM_BeRed; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == BLUECOLORMAP) { if (allowCMap) ColorMap = CM_Blue; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == NOCOLORMAP) { ColorMap = CM_Default; if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap == NOTHINGCOLORMAP) { ColorMap = CM_Default; ExtraLight = min2(254, ExtraLight+clampval(r_cm_extralight_add.asInt(), 0, 255)); }
    else if (cl->FixedColormap >= NUMCOLORMAPS) { if (allowFB) FixedLight = 255; }
    else if (cl->FixedColormap) {
      // lightamp sets this to 1
      bool fixallowed = true;
      if (cl->FixedColormap == 1 && allowCMap) {
        FullbrightThings = 1; // only interesting
        switch (k8ColormapLightAmp.asInt()) {
          case 1: ColorMap = CM_Mono; break;
          case 2: ColorMap = CM_Gold; break;
          case 3: ColorMap = CM_Green; break;
          case 4: ColorMap = CM_Red; break;
          case 5: ColorMap = CM_BeRed; break;
          case 6: ColorMap = CM_Blue; break;
          case 7: ColorMap = CM_Inverse; doInverseHack = false; break;
          case 8: ColorMap = CM_Default; break;
          case 9: ColorMap = CM_Default; fixallowed = false; FixedLight = 0; ExtraLight = min2(254, ExtraLight+clampval(r_cm_extralight_add.asInt(), 0, 255)); break;
        }
      }
      if (allowFB && fixallowed) FixedLight = 255-(cl->FixedColormap<<3);
    }
  }

  // inverse colormap flash effect
  if (cl->ExtraLight == 255) {
    if (allowCMap) ColorMap = CM_Inverse;
    if (allowFB) { ExtraLight = 0; FixedLight = 255; }
  }

  // inverse colormap hack
  if (doInverseHack && ColorMap == CM_Inverse && allowCMap) {
    switch (k8ColormapInverse.asInt()) {
      case 1: ColorMap = CM_Mono; break;
      case 2: ColorMap = CM_Gold; break;
      case 3: ColorMap = CM_Green; break;
      case 4: ColorMap = CM_Red; break;
      case 5: ColorMap = CM_BeRed; break;
      case 6: ColorMap = CM_Blue; break;
      case 7: ColorMap = CM_Inverse; break;
      case 8: ColorMap = CM_Default; break;
      case 9: ColorMap = CM_Default; FixedLight = 0; FullbrightThings = 1/*interesting*/; ExtraLight = min2(254, ExtraLight+clampval(r_cm_extralight_add.asInt(), 0, 255)); break;
    }
  }

  if (!allowFB && ExtraLight > 16) ExtraLight = 16;

  if (r_extralight_mode.asInt() > 0) ExtraLight = 0;

  const int forceL = r_force_sector_light.asInt();
  if (forceL > 0) FixedLight = min2(forceL, 255);

  if (r_level_extra_lighting.asInt() > 0) ExtraLight = clampval(ExtraLight+clampval(r_level_extra_lighting.asInt(), 0, 255), 0, 255);

  if (ColorMap != CM_Default && FixedLight) ColorMapFixedLight = true;

  if (r_fullbright_sprites_mode.asInt()) FullbrightThings = r_fullbright_sprites_mode.asInt();

  Drawer->SetupView(this, &refdef);
  //advanceCacheFrame();
  PortalDepth = 0;
  PortalUsingStencil = 0;
  PortalLevel = 0;
  forceDisableShadows = false;
}


//==========================================================================
//
//  VRenderLevelShared::SetupCameraFrame
//
//==========================================================================
void VRenderLevelShared::SetupCameraFrame (VEntity *Camera, VTexture *Tex, int FOV, refdef_t *rd) {
  rd->width = Tex->GetWidth();
  rd->height = Tex->GetHeight();
  rd->y = 0;
  rd->x = 0;

  PixelAspect = CalcAspect(r_aspect_ratio, rd->width, rd->height);

  clip_base.setupViewport(rd->width, rd->height, FOV, (r_dbg_proj_aspect.asBool() ? PixelAspect : 1.0f));
  rd->fovx = clip_base.fovx;
  rd->fovy = clip_base.fovy;
  rd->drawworld = true;

  ViewEnt = Camera;
  Drawer->viewangles = Camera->Angles;
  AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);

  Drawer->vieworg = Camera->Origin;

  ExtraLight = 0;
  FixedLight = 0;
  ColorMap = CM_Default;
  ColorMapFixedLight = false;

  Drawer->SetupView(this, rd);
  //advanceCacheFrame();
  CurrPortal = nullptr;
  PortalDepth = 0;
  PortalUsingStencil = 0;
  PortalLevel = 0;
  forceDisableShadows = true; // do not render shadows in camera views (for now)
  set_resolution_needed = true;
}


//==========================================================================
//
//  VRenderLevelShared::MarkLeaves
//
//==========================================================================
void VRenderLevelShared::MarkLeaves () {
  //k8: dunno, this is not the best place to do it, but...
  r_viewleaf = Level->PointInSubsector(Drawer->vieworg);

  // we need this for debug automap view
  IncVisFrameCount();
}


//==========================================================================
//
//  R_RenderPlayerView
//
//==========================================================================
void R_RenderPlayerView () {
  GClLevel->Renderer->RenderPlayerView();
}


//==========================================================================
//
//  AddBlend
//
//==========================================================================
static vuint32 AddBlend (vuint32 blend, int cr, int cg, int cb, int ca) noexcept {
  if (ca <= 0) return blend; // no alpha
  const float r1 = float(clampToByte(cr))/255.0;
  const float g1 = float(clampToByte(cg))/255.0;
  const float b1 = float(clampToByte(cb))/255.0;
  const float a1 = float(clampToByte(ca))/255.0;
  const float a0 = float((blend>>24)&0xff)/255.0;
  const float bla = clampval(1.0f-(1.0f-a0)*(1.0f-a1), 0.0f, 1.0f);
  if (bla <= 0.0f) return blend;
  const float r = (float((blend>>16)&0xff)/255.0f*a0*(1.0f-a1)+r1*a1)/bla;
  const float g = (float((blend>>8)&0xff)/255.0f*a0*(1.0f-a1)+g1*a1)/bla;
  const float b = (float(blend&0xff)/255.0f*a0*(1.0f-a1)+b1*a1)/bla;
  const float a = bla;
  blend =
    (clampToByte((int)(a*255.0f))<<24)|
    (clampToByte((int)(r*255.0f))<<16)|
    (clampToByte((int)(g*255.0f))<<8)|
    clampToByte((int)(b*255.0f));
  return blend;
}


//==========================================================================
//
//  VRenderLevelShared::RenderPlayerView
//
//==========================================================================
void VRenderLevelShared::RenderPlayerView () {
  if (!Level->LevelInfo) return;

  if (lastRenderQuality != r_fix_tjunctions.asBool()) {
    lastRenderQuality = r_fix_tjunctions.asBool();
    GCon->Logf(NAME_Debug, "render quality changed to '%s', invalidating all surfaces",
               (lastRenderQuality ? "quality" : "speed"));
    InvaldateAllTJunctions();
  }

  if (IsShadowMapRenderer() || IsShadowVolumeRenderer()) {
    const bool overbright = r_adv_overbright.asBool();
    // it is safe to call it on each frame, it won't do any unneeded work
    if (!Drawer->RecreateFBOs(overbright)) {
      GCon->Log(NAME_Warning, "cannot use overbright, turned it off");
      r_adv_overbright = false;
    }
  } else {
    // lightmapped renderer doesn't need main FP FBO
    // it is safe to call it on each frame, it won't do any unneeded work
    Drawer->RecreateFBOs(false);
  }

  // this swaps FP and non-FP main FBOs if necessary
  Drawer->PrepareMainFBO();

  Drawer->MirrorFlip = false;
  Drawer->MirrorClip = false;

  refdef.drawworld = true;
  refdef.DrawCamera = false;

  // setup frame early, so `glClear()` will have more time to finish

  // update camera textures that were visible in the last frame
  // rendering camera texture sets `NextUpdateTime`
  //GCon->Logf(NAME_Debug, "CAMTEX: %d", Level->CameraTextures.length());
  int updatesLeft = 1;
  for (auto &&camtexinfo : Level->CameraTextures) {
    // game can append new cameras dynamically...
    VTexture *BaseTex = GTextureManager[camtexinfo.TexNum];
    if (!BaseTex || !BaseTex->bIsCameraTexture) continue;
    VCameraTexture *CamTex = (VCameraTexture *)BaseTex;
    if (CamTex->camfboidx < 0) {
      GCon->Logf(NAME_Debug, "new camera texture added, allocating");
      CamTex->camfboidx = Drawer->GetCameraFBO(camtexinfo.TexNum, max2(1, BaseTex->Width), max2(1, BaseTex->Height));
      vassert(CamTex->camfboidx >= 0);
      CamTex->NextUpdateTime = 0; // force updating
    }
    // this updates only cameras with proper `NextUpdateTime`
    if (updatesLeft > 0) {
      if (UpdateCameraTexture(camtexinfo.Camera, camtexinfo.TexNum, camtexinfo.FOV)) {
        // do not update more than one camera texture per frame
        //GCon->Logf(NAME_Debug, "updated camera texture #%d", camtexinfo.TexNum);
        --updatesLeft;
      }
    }
  }

  SetupFrame();

  // this should be always called here
  Drawer->PrepareForPosteffects();

  // prepare draw list stack
  ResetDrawStack();
  ResetPortalPool();
  IncUpdateWorldFrame();
  //Drawer->SetUpdateFrame(updateWorldFrame);

  if (dbg_autoclear_automap) AM_ClearAutomap();

  //FIXME: this is wrong, because fake sectors need to be updated for each camera separately
  r_viewleaf = Level->PointInSubsector(Drawer->vieworg);
  const sec_region_t *sctreg = Level->PointContentsRegion(r_viewleaf->sector, Drawer->vieworg+TVec(0.0f, 0.0f, 1.0f));
  r_viewregion = (sec_region_t *)sctreg;
  // remember it
  const TVec lastorg = Drawer->vieworg;
  subsector_t *playerViewLeaf = r_viewleaf;

  //if (!r_disable_world_update) UpdateFakeSectors(playerViewLeaf);

  GTextureManager.Time = Level->Time;

  BuildPlayerTranslations();

  AnimateSky(host_frametime);
  UpdateParticles(host_frametime);

  //TODO: we can separate `BspVisData` building (and batching surfaces for rendering), and
  //      the actual rendering. this way we'll be able to do better dynlight checks

  PushDlights();

  // reset global colormap, it will be done with the shader
  const int savedColorMap = (r_dbg_force_colormap.asInt() ? r_dbg_force_colormap.asInt() : ColorMap);
  const bool shaderCM = true;
  if (shaderCM) ColorMap = CM_Default;

  if (dbg_clip_dump_added_ranges) GCon->Logf("=== RENDER SCENE: (%f,%f,%f); (yaw=%f; pitch=%f)", Drawer->vieworg.x, Drawer->vieworg.y, Drawer->vieworg.x, Drawer->viewangles.yaw, Drawer->viewangles.pitch);

  //GCon->Log(NAME_Debug, "*** VRenderLevelShared::RenderPlayerView: ENTER ***");
  RenderScene(&refdef, nullptr);
  //GCon->Log(NAME_Debug, "*** VRenderLevelShared::RenderPlayerView: EXIT ***");

  if (dbg_clip_dump_added_ranges) ViewClip.Dump();

  Drawer->RestoreMainFBO();

  // perform bloom effect
  //GCon->Logf(NAME_Debug, "BLOOM: (%d,%d); (%dx%d)", refdef.x, refdef.y, refdef.width, refdef.height);
  Drawer->Posteffect_Bloom(refdef.x, refdef.y, refdef.width, refdef.height);

  const bool drawPSprites = (cl->MO == cl->Camera && GGameInfo->NetMode != NM_TitleMap);

  bool doTonemap = (r_tonemap.asInt() == 1);

  // if we don't have to tonemap psprites, and have no shader colormaps, do tonemapping now
  if (doTonemap && drawPSprites && !r_tonemap_psprites.asBool()) {
    if (!shaderCM || savedColorMap == CM_Default) {
      doTonemap = false;
      Drawer->Posteffect_Tonemap(refdef.x, refdef.y, refdef.width, refdef.height, true);
    }
  }

  // CAS should be not applied to psprites too
  if (r_cas_filter.asInt() > 0) {
    float coeff = r_cas_filter_coeff.asFloat();
    if (isFiniteF(coeff) && coeff >= 0.001f) {
      Drawer->Posteffect_CAS(coeff, refdef.x, refdef.y, refdef.width, refdef.height, true/*save matrices*/);
    }
  }

  // recalc in case recursive scene renderer moved it
  // we need it for psprite rendering
  if (Drawer->vieworg == lastorg) {
    r_viewleaf = playerViewLeaf;
  } else {
    r_viewleaf = Level->PointInSubsector(Drawer->vieworg);
    sctreg = Level->PointContentsRegion(r_viewleaf->sector, Drawer->vieworg+TVec(0.0f, 0.0f, 1.0f));
  }
  r_viewregion = (sec_region_t *)sctreg;

  // draw the psprites on top of everything
  if (drawPSprites) DrawPlayerSprites();

  int sct = 0;
  if (sctreg) {
    if (sctreg->regflags&sec_region_t::RF_BaseRegion) {
      if (r_viewleaf->sector->IsUnderwater()) {
        sct = CONTENTS_BOOMWATER;
      } else {
        const TVec p = Drawer->vieworg+TVec(0.0f, 0.0f, 1.0f);
        if (r_viewleaf->sector->heightsec && r_viewleaf->sector->heightsec->IsUnderwater() &&
            p.z <= r_viewleaf->sector->heightsec->floor.GetPointZClamped(p))
        {
          sct = CONTENTS_BOOMWATER;
        }
      }
    } else if (sctreg->regflags&sec_region_t::RF_NonSolid) {
      sct = sctreg->params->contents;
    }
  }

  if (sct > 0) {
    //GCon->Logf(NAME_Debug, "lightlevel=%u; LightColor=0x%08x; Fade=0x%08x", sctreg->params->lightlevel, sctreg->params->LightColor, sctreg->params->Fade);

    // check for specific lighting
    const unsigned lc = sctreg->params->LightColor&0xffffffu;
    const bool doTint = (lc == 0 || lc == 0xffffffu || !r_underwater_colored_light.asBool());

    if (doTint) {
      vuint32 WaterCShift = 0u;
      switch (sct) {
        case CONTENTS_WATER: WaterCShift = AddBlend(WaterCShift, 130, 80, 50, 128); break;
        case CONTENTS_LAVA: WaterCShift = AddBlend(WaterCShift, 255, 80, 0, 150); break;
        case CONTENTS_NUKAGE: WaterCShift = AddBlend(WaterCShift, 50, 255, 50, 150); break;
        case CONTENTS_SLIME: WaterCShift = AddBlend(WaterCShift, 0, 25, 5, 150); break;
        case CONTENTS_HELLSLIME: WaterCShift = AddBlend(WaterCShift, 255, 80, 0, 150); break;
        case CONTENTS_BLOOD: WaterCShift = AddBlend(WaterCShift, 160, 16, 16, 150); break;
        case CONTENTS_SLUDGE: WaterCShift = AddBlend(WaterCShift, 128, 160, 128, 150); break;
        case CONTENTS_HAZARD: WaterCShift = AddBlend(WaterCShift, 128, 160, 128, 128); break;
        case CONTENTS_BOOMWATER: WaterCShift = AddBlend(WaterCShift, 0, 40, 130, 128); break;
      }
      //GCon->Logf(NAME_Debug, "sct=%d; CShift=0x%08x", sct, CShift);
      if (WaterCShift) Drawer->RenderTint(WaterCShift);
    }

    // apply underwater shader if necessary
    if (r_underwater.asInt() > 0) Drawer->Posteffect_Underwater(Level->Time, refdef.x, refdef.y, refdef.width, refdef.height, false/*don't save matrices*/);
  }

  vuint32 CShift = (cl ? cl->CShift : 0u);
  if (CShift && r_colortint_mode.asInt() == 0) Drawer->RenderTint(CShift);

  // apply colormap
  if (shaderCM) {
    Drawer->Posteffect_ColorMap(savedColorMap, refdef.x, refdef.y, refdef.width, refdef.height);
    // just in case
    ColorMap = savedColorMap;
  }

  // apply tonemap, if necessary
  if (doTonemap) Drawer->Posteffect_Tonemap(refdef.x, refdef.y, refdef.width, refdef.height, false);

  // this should be called even if nothing was applied before
  Drawer->FinishPosteffects();

  Drawer->EndView();
}


//==========================================================================
//
//  VRenderLevelShared::UpdateCameraTexture
//
//  returns `true` if camera texture was updated
//
//==========================================================================
bool VRenderLevelShared::UpdateCameraTexture (VEntity *Camera, int TexNum, int FOV) {
  if (!Camera) return false;

  VTexture *BaseTex = GTextureManager[TexNum];
  if (!BaseTex || !BaseTex->bIsCameraTexture) return false;

  VCameraTexture *Tex = (VCameraTexture *)BaseTex;
  bool forcedUpdate = (Tex->NextUpdateTime == 0.0f);
  if (!Tex->NeedUpdate()) return false;

  refdef_t CameraRefDef;
  CameraRefDef.DrawCamera = true;

  int cfidx = Drawer->GetCameraFBO(TexNum, Tex->GetWidth(), Tex->GetHeight());
  if (cfidx < 0) return false; // alas
  Tex->camfboidx = cfidx;

  //GCon->Logf(NAME_Debug, "  CAMERA; tex=%d; fboidx=%d; forced=%d", TexNum, cfidx, (int)forcedUpdate);

  if (r_allow_cameras) {
    // prepare draw list stack
    // this method is called before rendering everything else, so we can come here unprepared
    // `RenderPlayerView()` will reset everything again later
    ResetDrawStack();
    ResetPortalPool();
    IncUpdateWorldFrame();

    Drawer->SetCameraFBO(cfidx);
    SetupCameraFrame(Camera, Tex, FOV, &CameraRefDef);
    RenderScene(&CameraRefDef, nullptr);
    Drawer->EndView(); // ignore color tint

    //glFlush();
    Tex->CopyImage(); // this sets flags, but doesn't read pixels
  }
  Drawer->SetMainFBO(); // restore main FBO

  return !forcedUpdate;
}


//==========================================================================
//
//  VRenderLevelShared::GetFade
//
//==========================================================================
vuint32 VRenderLevelShared::GetFade (const sec_region_t *reg, bool isFloorCeiling) {
  if (r_fog_test) return 0xff000000|(int(255*r_fog_r)<<16)|(int(255*r_fog_g)<<8)|int(255*r_fog_b);
  if (reg->params->Fade) {
    if (!(reg->regflags&sec_region_t::RF_OnlyVisual)) {
      //if (!(reg->regflags&sec_region_t::RF_NonSolid)) return reg->params->Fade;
      if (Drawer && (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual)) == sec_region_t::RF_NonSolid) {
        if (!isFloorCeiling &&
            Drawer->vieworg.z < reg->eceiling.GetPointZClamped(Drawer->vieworg) &&
            Drawer->vieworg.z > reg->efloor.GetPointZClamped(Drawer->vieworg))
        {
          return (vuint32)reg->params->Fade;
        }
      } else {
        return (vuint32)reg->params->Fade;
      }
    }
  }
  /*
  if (reg->params->Fade) {
    // for non-solid and non-base sectors, apply this fade only if we're inside that sector
    if (Drawer && (reg->regflags&(sec_region_t::RF_BaseRegion|sec_region_t::RF_NonSolid|sec_region_t::RF_OnlyVisual)) == sec_region_t::RF_NonSolid) {
      // check view origin
      if (Drawer->vieworg.z < reg->eceiling.GetPointZClamped(Drawer->vieworg) &&
          Drawer->vieworg.z > reg->efloor.GetPointZClamped(Drawer->vieworg))
      {
        return reg->params->Fade;
      }
    } else {
      return reg->params->Fade;
    }
  }
  */
  if (Level->LevelInfo->OutsideFog && reg->eceiling.splane->pic == skyflatnum) return (vuint32)Level->LevelInfo->OutsideFog;
  if (Level->LevelInfo->Fade) return (vuint32)Level->LevelInfo->Fade;
  if (Level->LevelInfo->FadeTable == NAME_fogmap) return 0xff7f7f7fU;
  if (r_fade_light) return (vuint32)FADE_LIGHT; // simulate light fading using dark fog
  return (vuint32)0;
}


//==========================================================================
//
//  VRenderLevelShared::NukeLightmapCache
//
//==========================================================================
void VRenderLevelShared::NukeLightmapCache () {
}


//==========================================================================
//
//  R_DrawPic
//
//==========================================================================
void R_DrawPic (int x, int y, int handle, float Alpha) {
  if (handle < 0 || Alpha <= 0.0f || !isFiniteF(Alpha)) return;
  if (Alpha > 1.0f) Alpha = 1.0f;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffsetI();
  y -= Tex->GetScaledTOffsetI();
  Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidthI()), fScaleY*(y+Tex->GetScaledHeightI()), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
}


//==========================================================================
//
//  R_DrawPicScaled
//
//==========================================================================
void R_DrawPicScaled (int x, int y, int handle, float scale, float Alpha) {
  if (handle < 0 || Alpha <= 0.0f || !isFiniteF(Alpha) || !isFiniteF(scale) || scale <= 0.0f) return;
  if (Alpha > 1.0f) Alpha = 1.0f;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= (int)((float)Tex->GetScaledSOffsetI()*scale);
  y -= (int)((float)Tex->GetScaledTOffsetI()*scale);
  Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidthI()*scale), fScaleY*(y+Tex->GetScaledHeightI()*scale), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
}


//==========================================================================
//
//  R_DrawPicFloat
//
//==========================================================================
void R_DrawPicFloat (float x, float y, int handle, float Alpha) {
  if (handle < 0) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffsetF();
  y -= Tex->GetScaledTOffsetF();
  Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidthF()), fScaleY*(y+Tex->GetScaledHeightF()), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
}


//==========================================================================
//
//  R_DrawPicPart
//
//==========================================================================
void R_DrawPicPart (int x, int y, float pwdt, float phgt, int handle, float Alpha) {
  R_DrawPicFloatPart(x, y, pwdt, phgt, handle, Alpha);
}


//==========================================================================
//
//  R_DrawPicFloatPart
//
//==========================================================================
void R_DrawPicFloatPart (float x, float y, float pwdt, float phgt, int handle, float Alpha) {
  if (handle < 0 || pwdt <= 0.0f || phgt <= 0.0f || !isFiniteF(pwdt) || !isFiniteF(phgt)) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffsetF();
  y -= Tex->GetScaledTOffsetF();
  //Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidthI()*pwdt), fScaleY*(y+Tex->GetScaledHeightI()*phgt), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
  Drawer->DrawPic(
    fScaleX*x, fScaleY*y,
    fScaleX*(x+Tex->GetScaledWidthF()*pwdt),
    fScaleY*(y+Tex->GetScaledHeightF()*phgt),
    0, 0, Tex->GetWidth()*pwdt, Tex->GetHeight()*phgt,
    Tex, nullptr, Alpha);
}


//==========================================================================
//
//  R_DrawPicPartEx
//
//==========================================================================
void R_DrawPicPartEx (int x, int y, float tx0, float ty0, float tx1, float ty1, int handle, float Alpha) {
  R_DrawPicFloatPartEx(x, y, tx0, ty0, tx1, ty1, handle, Alpha);
}


//==========================================================================
//
//  R_DrawPicFloatPartEx
//
//==========================================================================
void R_DrawPicFloatPartEx (float x, float y, float tx0, float ty0, float tx1, float ty1, int handle, float Alpha) {
  float pwdt = (tx1-tx0);
  float phgt = (ty1-ty0);
  if (handle < 0 || pwdt <= 0.0f || phgt <= 0.0f) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffsetF();
  y -= Tex->GetScaledTOffsetF();
  //Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidthI()*pwdt), fScaleY*(y+Tex->GetScaledHeightI()*phgt), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
  Drawer->DrawPic(
    fScaleX*(x+Tex->GetScaledWidthF()*tx0),
    fScaleY*(y+Tex->GetScaledHeightF()*ty0),
    fScaleX*(x+Tex->GetScaledWidthF()*tx1),
    fScaleY*(y+Tex->GetScaledHeightF()*ty1),
    Tex->GetWidth()*tx0, Tex->GetHeight()*ty0, Tex->GetWidth()*tx1, Tex->GetHeight()*ty1,
    Tex, nullptr, Alpha);
}


//==========================================================================
//
//  R_DrawSpritePatch
//
//==========================================================================
void R_DrawSpritePatch (float x, float y, int sprite, int frame, int rot,
                        int TranslStart, int TranslEnd, int Color, float scale,
                        bool ignoreVScr)
{
  bool flip;
  int lump;

  if (sprite < 0 || sprite >= sprites.length()) return;
  spriteframe_t *sprframe = &sprites.ptr()[sprite].spriteframes[frame&VState::FF_FRAMEMASK];
  flip = sprframe->flip[rot];
  lump = sprframe->lump[rot];
  VTexture *Tex = GTextureManager[lump];
  if (!Tex) return; // just in case

  (void)Tex->GetWidth();

  float x1 = x-Tex->SOffset*scale;
  float y1 = y-Tex->TOffset*scale;
  float x2 = x1+Tex->GetWidth()*scale;
  float y2 = y1+Tex->GetHeight()*scale;

  if (!ignoreVScr) {
    x1 *= fScaleX;
    y1 *= fScaleY;
    x2 *= fScaleX;
    y2 *= fScaleY;
  }

  Drawer->DrawSpriteLump(x1, y1, x2, y2, Tex, R_GetCachedTranslation(R_SetMenuPlayerTrans(TranslStart, TranslEnd, Color), nullptr), flip);
}


enum {
  TType_Normal,
  TType_Sprite,
  TType_PSprite,
  TType_Model,
  TType_Other, // just in case
};

struct SpriteScanInfo {
public:
  TMapNC<vint32, vint32> *texturetype;
  TMapNC<VClass *, bool> classSeen;
  TMapNC<VState *, bool> stateSeen;
  TMapNC<vuint32, bool> spidxSeen;
  TMapNC<vint32, bool> textureIgnore;
  int sprtexcount;
  bool putToIgnore;
  int limit; // <0: no limit
  int ttype;

public:
  VV_DISABLE_COPY(SpriteScanInfo)
  inline SpriteScanInfo (TMapNC<vint32, vint32> &txtype, int alimit) noexcept : texturetype(&txtype), stateSeen(), spidxSeen(), textureIgnore(), sprtexcount(0), putToIgnore(false), limit(alimit), ttype(TType_Normal) {}

  inline void clearStates () { stateSeen.reset(); }
};


//==========================================================================
//
//  ProcessState
//
//==========================================================================
static void ProcessSpriteState (VState *st, SpriteScanInfo &ssi) {
  while (st) {
    if (ssi.stateSeen.has(st)) break;
    ssi.stateSeen.put(st, true);
    // extract sprite and frame
    if (!(st->Frame&VState::FF_KEEPSPRITE)) {
      vuint32 spridx = st->SpriteIndex&0x00ffffff;
      //int sprfrm = st->Frame&VState::FF_FRAMEMASK;
      if (spridx < (vuint32)sprites.length()) {
        if (!ssi.spidxSeen.has(spridx)) {
          // precache all rotations
          ssi.spidxSeen.put(spridx, true);
          const spritedef_t &sfi = sprites.ptr()[spridx];
          if (sfi.numframes > 0) {
            const spriteframe_t *spf = sfi.spriteframes;
            for (int f = sfi.numframes; f--; ++spf) {
              for (int lidx = 0; lidx < 16; ++lidx) {
                int stid = spf->lump[lidx];
                if (stid < 1) continue;
                if (ssi.putToIgnore) { ssi.textureIgnore.put(stid, true); continue; }
                if (ssi.textureIgnore.has(stid)) continue;
                if (ssi.texturetype->has(stid)) continue;
                if (ssi.limit != 0) {
                  ssi.texturetype->put(stid, ssi.ttype);
                  ++ssi.sprtexcount;
                  if (ssi.limit > 0) --ssi.limit;
                }
              }
            }
          }
        }
      }
    }
    ProcessSpriteState(st->NextState, ssi);
    st = st->Next;
  }
}


//==========================================================================
//
//  ProcessSpriteClass
//
//==========================================================================
static void ProcessSpriteClass (VClass *cls, SpriteScanInfo &ssi) {
  if (!cls) return;
  if (ssi.classSeen.has(cls)) return;
  ssi.classSeen.put(cls, true);
  ssi.clearStates();
  ProcessSpriteState(cls->States, ssi);
}


//==========================================================================
//
//  VRenderLevelShared::CollectSpriteTextures
//
//  this is actually private, but meh...
//
//  returns number of new textures
//  do not precache player pawn textures
//
//==========================================================================
int VRenderLevelShared::CollectSpriteTextures (TMapNC<vint32, vint32> &texturetype, int limit, int cstmode) {
  // scan all thinkers, and add sprites from all states, because why not?
  VClass *eexCls = VClass::FindClass("EntityEx");
  if (!eexCls) return 0;

  SpriteScanInfo ssi(texturetype, limit);

  // blood textures
  ssi.ttype = TType_Sprite;
  ssi.putToIgnore = false;
  // precache gore mod sprites
  VClass::ForEachChildOf("K8Gore_BloodBase", [&ssi](VClass *cls) { ProcessSpriteClass(cls, ssi); return FERes::FOREACH_NEXT; });
  VClass::ForEachChildOf("K8Gore_BloodBaseTransient", [&ssi](VClass *cls) { ProcessSpriteClass(cls, ssi); return FERes::FOREACH_NEXT; });
  // precache other blood sprites
  VClass::ForEachChildOf("Blood", [&ssi](VClass *cls) { ProcessSpriteClass(cls, ssi); return FERes::FOREACH_NEXT; });
  VClass *bloodRepl = VClass::FindClass("Blood");
  if (bloodRepl) bloodRepl = bloodRepl->GetReplacement();
  while (bloodRepl && bloodRepl->IsChildOf(eexCls)) {
    ProcessSpriteClass(bloodRepl, ssi);
    bloodRepl = bloodRepl->GetSuperClass();
  }

  if (cstmode == CST_OnlyBlood) return ssi.sprtexcount;

  // psprites
  ssi.putToIgnore = (cstmode == CST_Normal ? !r_precache_weapon_sprite_textures.asBool() : false);
  ssi.ttype = TType_PSprite;
  VClass::ForEachChildOf("Weapon", [&ssi](VClass *cls) { ProcessSpriteClass(cls, ssi); return FERes::FOREACH_NEXT; });

  if (cstmode != CST_Normal) return ssi.sprtexcount;

  // ammo
  ssi.ttype = TType_Sprite;
  ssi.putToIgnore = !r_precache_ammo_sprite_textures.asBool();
  VClass::ForEachChildOf("Ammo", [&ssi](VClass *cls) { ProcessSpriteClass(cls, ssi); return FERes::FOREACH_NEXT; });

  // player
  ssi.putToIgnore = !r_precache_player_sprite_textures.asBool();
  VClass *pawnCls = VClass::FindClass("PlayerPawn");
  if (pawnCls) {
    for (VThinker *th = Level->ThinkerHead; th; th = th->Next) {
      if (th->IsGoingToDie()) continue;
      if (!th->IsA(eexCls)) continue;
      if (!th->IsA(pawnCls)) continue;
      ProcessSpriteClass(th->GetClass(), ssi);
    }
  }

  // other
  ssi.ttype = TType_Sprite;
  ssi.putToIgnore = false;
  for (VThinker *th = Level->ThinkerHead; th; th = th->Next) {
    if (th->IsGoingToDie()) continue;
    if (!th->IsA(eexCls)) continue;
    if (pawnCls && th->IsA(pawnCls)) continue;
    // do not load textures for sprites with 3d models
    VEntity *ee = (VEntity *)th; // safe due to `eexCls` check above
    if (!HasEntityAliasModel(ee)) {
      ProcessSpriteClass(th->GetClass(), ssi);
    }
  }

  return ssi.sprtexcount;
}


//==========================================================================
//
//  CacheTextureCallback
//
//==========================================================================
static void CacheTextureCallback (int id, void *udata) {
  if (id > 0) {
    //VRenderLevelShared *rend = (VRenderLevelShared *)udata;
    TMapNC<vint32, vint32> *tt = (TMapNC<vint32, vint32> *)udata;
    tt->put(id, TType_Normal);
  }
}


//==========================================================================
//
//  VRenderLevelShared::PrecacheLevel
//
//  preloads all relevant graphics for the level
//
//==========================================================================
void VRenderLevelShared::PrecacheLevel () {
  //k8: why?
  if (cls.demoplayback) return;

  NukeLightmapCache();

  if (Drawer) Drawer->ReportGPUMemoryUse("starting level precaching");

  if (r_dbg_disable_all_precaching) return;

  TMapNC<vint32, vint32> texturetype; // key: textureid, value: texturetype

  if (r_precache_textures || r_precache_textures_override > 0) {
    // floors and ceilings
    for (auto &&sec : Level->allSectors()) {
      if (sec.floor.pic > 0) texturetype.put(sec.floor.pic, TType_Normal);
      if (sec.ceiling.pic > 0) texturetype.put(sec.ceiling.pic, TType_Normal);
    }
    // walls
    for (auto &&side : Level->allSides()) {
      R_CheckAnimatedTexture(side.TopTexture, &CacheTextureCallback, &texturetype);
      R_CheckAnimatedTexture(side.MidTexture, &CacheTextureCallback, &texturetype);
      R_CheckAnimatedTexture(side.BottomTexture, &CacheTextureCallback, &texturetype);
      if (side.TopTexture > 0) texturetype.put(side.TopTexture, TType_Normal);
      if (side.MidTexture > 0) texturetype.put(side.MidTexture, TType_Normal);
      if (side.BottomTexture > 0) texturetype.put(side.BottomTexture, TType_Normal);
    }
    // informational message
    const int lvltexcount = texturetype.count();
    if (lvltexcount) GCon->Logf("found %d level textures", lvltexcount);
  }

  // models
  if (r_precache_models) {
    VClass *actorCls = VClass::FindClass("Actor");

    const bool doLods = (r_model_lod_enabled.asBool() || r_model_lod_shadow.asInt() > 0);

    int totalmodels = 0;
    TMapNC<VMeshModel *, bool> mmhash;
    for (VThinker *th = Level->ThinkerHead; th; th = th->Next) {
      if (th->IsGoingToDie() || !th->IsA(actorCls)) continue;
      VEntity *ee = (VEntity *)th; // safe due to `actorCls` check above
      if (!IsAliasModelAllowedFor(ee)) continue;
      VClassModelScript *mscript = RM_FindClassModelByName(GetClassNameForModel(ee));
      if (!mscript) continue;
      VModel *mdl = mscript->Model;
      if (!mdl) continue;
      for (auto &&ScMdl : mdl->Models) {
        for (auto &&SubMdl : ScMdl.SubModels) {
          VMeshModel *mesh = SubMdl.Model;
          if (!mesh->Uploaded) {
            if (!mmhash.putNoReplace(mesh, true)) {
              do {
                if (!mesh->Uploaded) totalmodels += 1;
                mesh = mesh->nextMip;
              } while (doLods && mesh != nullptr);
            }
          }
        }
      }
    }

    if (totalmodels != 0) {
      GCon->Logf("found %d alias model%s", totalmodels, (totalmodels != 1 ? "s" :""));
      R_OSDMsgShowSecondary("UPLOADING MODELS");
      R_PBarReset();
      R_PBarUpdate("Models", 0, totalmodels);
      int emodels = 0;
      for (auto &&kv : mmhash.first()) {
        VMeshModel *mesh = kv.key();
        if (!mesh->Uploaded) {
          mesh->LoadFromWad();
          Drawer->UploadModel(mesh);
          emodels += 1;
          R_PBarUpdate("Models", emodels, totalmodels);
        }
      }
      if (totalmodels != emodels) {
        GCon->Logf(NAME_Debug, "OOPS! model count wrong; emodels=%d; totalmodels=%d (this is harmless)",
                   emodels, totalmodels);
        if (totalmodels < emodels) totalmodels = emodels;
      }
      R_PBarUpdate("Models", totalmodels, totalmodels, true); // final update
    }
  }

  // sprites
  if ((r_precache_sprite_textures || r_precache_all_sprite_textures) && sprites.length() > 0) {
    int sprtexcount = 0;
    int sprlimit = r_precache_max_sprites.asInt();
    if (sprlimit <= 0) sprlimit = -1;
    if (r_precache_all_sprite_textures) {
      sprtexcount = CollectSpriteTextures(texturetype, sprlimit, CST_OnlyBloodAndPSprites);
      if (sprlimit > 0) sprlimit = max2(0, sprlimit-sprtexcount);
      for (auto &&sfi : sprites) {
        if (sfi.numframes == 0) continue;
        const spriteframe_t *spf = sfi.spriteframes;
        for (int f = sfi.numframes; f--; ++spf) {
          for (int lidx = 0; lidx < 16; ++lidx) {
            int stid = spf->lump[lidx];
            if (stid < 1) continue;
            if (texturetype.has(stid)) continue;
            if (sprlimit == 0) break;
            texturetype.put(stid, TType_Sprite);
            ++sprtexcount;
            if (sprlimit > 0) --sprlimit;
          }
        }
        if (sprlimit == 0) break;
      }
    } else {
      sprtexcount = CollectSpriteTextures(texturetype, sprlimit, CST_Normal);
    }
    if (sprtexcount) GCon->Logf("found %d sprite textures", sprtexcount);
  } else {
    int sprtexcount = CollectSpriteTextures(texturetype, -1, CST_OnlyBlood);
    if (sprtexcount) GCon->Logf("found %d blood sprite textures", sprtexcount);
  }

  int maxpbar = texturetype.count(), currpbar = 0;

  R_OSDMsgShowSecondary("PRECACHING TEXTURES");
  R_PBarReset();

  if (maxpbar > 0) {
    GTextureCropMessages = false;
    GCon->Logf("precaching %d textures", maxpbar);
    for (auto &&it : texturetype.first()) {
      const vint32 tid = it.key();
      const vint32 ttype = it.value();
      vassert(tid > 0);
      ++currpbar;
      R_PBarUpdate("Textures", currpbar, maxpbar);
           if (ttype == TType_Sprite) Drawer->PrecacheSpriteTexture(GTextureManager[tid], VDrawer::SpriteType::SP_Normal);
      else if (ttype == TType_PSprite) Drawer->PrecacheSpriteTexture(GTextureManager[tid], VDrawer::SpriteType::SP_PSprite);
      else Drawer->PrecacheTexture(GTextureManager[tid]);
    }
    GTextureCropMessages = true;
  }

  if (Drawer) Drawer->ReportGPUMemoryUse("level precaching complete");
  R_PBarUpdate("Textures", maxpbar, maxpbar, true); // final update
}


//==========================================================================
//
//  VRenderLevelShared::UncacheLevel
//
//==========================================================================
void VRenderLevelShared::UncacheLevel () {
  if (Drawer) Drawer->ReportGPUMemoryUse("starting level uncaching");

  if (r_reupload_level_textures || gl_release_ram_textures_mode.asInt() >= 1) {
    TMapNC<vint32, bool> texturepresent;

    // floors and ceilings
    for (auto &&sec : Level->allSectors()) {
      if (sec.floor.pic > 0) texturepresent.put(sec.floor.pic, true);
      if (sec.ceiling.pic > 0) texturepresent.put(sec.ceiling.pic, true);
    }

    // walls
    for (auto &&side : Level->allSides()) {
      if (side.TopTexture > 0) texturepresent.put(side.TopTexture, true);
      if (side.MidTexture > 0) texturepresent.put(side.MidTexture, true);
      if (side.BottomTexture > 0) texturepresent.put(side.BottomTexture, true);
    }

    int lvltexcount = texturepresent.count();
    if (!lvltexcount) return;

    if (r_reupload_level_textures) {
      GCon->Logf("unloading%s %d level textures", (gl_release_ram_textures_mode.asInt() >= 1 ? " and releasing" : ""), lvltexcount);
    } else {
      GCon->Logf("releasing %d level textures", lvltexcount);
    }
    for (auto &&it : texturepresent.first()) {
      const vint32 tid = it.key();
      vassert(tid > 0);
      VTexture *tex = GTextureManager[tid];
      if (!tex) continue;
      if (r_reupload_level_textures) Drawer->FlushOneTexture(tex);
      if (gl_release_ram_textures_mode.asInt() >= 1) tex->ReleasePixels();
    }
  }

  if (Drawer) Drawer->ReportGPUMemoryUse("level uncaching complete");
}


//==========================================================================
//
//  VRenderLevelShared::GetTranslation
//
//==========================================================================
VTextureTranslation *VRenderLevelShared::GetTranslation (int TransNum) {
  return R_GetCachedTranslation(TransNum, Level);
}


//==========================================================================
//
//  VRenderLevelShared::BuildPlayerTranslations
//
//==========================================================================
void VRenderLevelShared::BuildPlayerTranslations () {
  for (TThinkerIterator<VPlayerReplicationInfo> It(Level); It; ++It) {
    if (It->PlayerNum < 0 || It->PlayerNum >= MAXPLAYERS) continue; // should not happen
    if (!It->TranslStart || !It->TranslEnd) continue;

    VTextureTranslation *Tr = PlayerTranslations[It->PlayerNum];
    if (Tr && Tr->TranslStart == It->TranslStart && Tr->TranslEnd == It->TranslEnd && Tr->Color == It->Color) continue;

    if (!Tr) {
      Tr = new VTextureTranslation;
      PlayerTranslations[It->PlayerNum] = Tr;
    }

    // don't waste time clearing if it's the same range
    if (Tr->TranslStart != It->TranslStart || Tr->TranslEnd != It->TranslEnd) Tr->Clear();

    Tr->BuildPlayerTrans(It->TranslStart, It->TranslEnd, It->Color);
  }
}


//==========================================================================
//
//  VRenderLevelShared::GetLightChainHead
//
//  block number+1 or 0
//
//==========================================================================
vuint32 VRenderLevelShared::GetLightChainHead () {
  return 0;
}


//==========================================================================
//
//  VRenderLevelShared::GetLightChainNext
//
//  block number+1 or 0
//
//==========================================================================
vuint32 VRenderLevelShared::GetLightChainNext (vuint32 /*bnum*/) {
  return 0;
}


//==========================================================================
//
//  VRenderLevelShared::GetLightBlockDirtyArea
//
//==========================================================================
VDirtyArea &VRenderLevelShared::GetLightBlockDirtyArea (vuint32 /*bnum*/) {
  return unusedDirty;
}


//==========================================================================
//
//  VRenderLevelShared::GetLightBlock
//
//==========================================================================
rgba_t *VRenderLevelShared::GetLightBlock (vuint32 /*bnum*/) {
  return nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::GetLightChainFirst
//
//==========================================================================
surfcache_t *VRenderLevelShared::GetLightChainFirst (vuint32 /*bnum*/) {
  return nullptr;
}


//==========================================================================
//
//  VRenderLevelShared::ResetLightmaps
//
//==========================================================================
void VRenderLevelShared::ResetLightmaps (bool /*recalcNow*/) {
}


//==========================================================================
//
//  VRenderLevelShared::IsShadowMapRenderer
//
//==========================================================================
bool VRenderLevelShared::IsShadowMapRenderer () const noexcept {
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::isNeedLightmapCache
//
//==========================================================================
bool VRenderLevelShared::isNeedLightmapCache () const noexcept {
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::saveLightmaps
//
//==========================================================================
void VRenderLevelShared::saveLightmaps (VStream * /*strm*/) {
}


//==========================================================================
//
//  VRenderLevelShared::loadLightmaps
//
//==========================================================================
bool VRenderLevelShared::loadLightmaps (VStream * /*strm*/) {
  return false;
}


//==========================================================================
//
//  VRenderLevelShared::CountSurfacesInChain
//
//==========================================================================
vuint32 VRenderLevelShared::CountSurfacesInChain (const surface_t *s) noexcept {
  vuint32 res = 0;
  for (; s; s = s->next) ++res;
  return res;
}


//==========================================================================
//
//  VRenderLevelShared::CountSegSurfaces
//
//==========================================================================
vuint32 VRenderLevelShared::CountSegSurfacesInChain (const segpart_t *sp) noexcept {
  vuint32 res = 0;
  for (; sp; sp = sp->next) res += CountSurfacesInChain(sp->surfs);
  return res;
}


//==========================================================================
//
//  VRenderLevelShared::CountAllSurfaces
//
//  calculate total number of surfaces
//
//==========================================================================
vuint32 VRenderLevelShared::CountAllSurfaces () const noexcept {
  vuint32 surfCount = 0;
  for (auto &&sub : Level->allSubsectors()) {
    for (subregion_t *r = sub.regions; r != nullptr; r = r->next) {
      if (r->realfloor != nullptr) surfCount += CountSurfacesInChain(r->realfloor->surfs);
      if (r->realceil != nullptr) surfCount += CountSurfacesInChain(r->realceil->surfs);
      if (r->fakefloor != nullptr) surfCount += CountSurfacesInChain(r->fakefloor->surfs);
      if (r->fakeceil != nullptr) surfCount += CountSurfacesInChain(r->fakeceil->surfs);
    }
  }

  for (auto &&seg : Level->allSegs()) {
    drawseg_t *ds = seg.drawsegs;
    if (ds) {
      surfCount += CountSegSurfacesInChain(ds->top);
      surfCount += CountSegSurfacesInChain(ds->mid);
      surfCount += CountSegSurfacesInChain(ds->bot);
      surfCount += CountSegSurfacesInChain(ds->topsky);
      surfCount += CountSegSurfacesInChain(ds->extra);
    }
  }

  return surfCount;
}


//==========================================================================
//
//  VRenderLevelShared::GetNumberOfStaticLights
//
//==========================================================================
int VRenderLevelShared::GetNumberOfStaticLights () {
  return Lights.length();
}


//==========================================================================
//
//  VRenderLevelShared::setupCurrentLight
//
//==========================================================================
void VRenderLevelShared::setupCurrentLight (const TVec &LightPos, const float Radius, const TVec &aconeDir, const float aconeAngle) noexcept {
  //CurrLightNoGeoClip = false; nope, not here
  CurrLightSpot = false;
  CurrLightPos = LightPos;
  CurrLightRadius = Radius;
  if (!isFiniteF(Radius) || Radius < 1.0f) CurrLightRadius = 1.0f;
  CurrLightConeFrustum.clear();
  CurrLightConeDir = aconeDir;
  // is good spotdlight?
  if (aconeDir.isValid() && isFiniteF(aconeAngle) && aconeAngle > 0.0f && aconeAngle < 180.0f) {
    CurrLightConeDir.normaliseInPlace();
    if (CurrLightConeDir.isValid()) {
      CurrLightSpot = true;
      CurrLightConeAngle = aconeAngle;
      // move it back a little, because why not?
      // note that created frustum will have its near plane at the given origin,
      // contrary to normal camera frustums, where near plane is moved forward a little
      CurrLightConeFrustum.setupSimpleDir(CurrLightPos-CurrLightConeDir*0.5f, CurrLightConeDir, clampval(CurrLightConeAngle*2.0f, 1.0f, 179.0f), CurrLightRadius);
    }
  }
}


#include "r_main_automap.cpp"


//==========================================================================
//
//  R_SetMenuPlayerTrans
//
//==========================================================================
int R_SetMenuPlayerTrans (int Start, int End, int Col) {
  if (!Start || !End) return 0;

  VTextureTranslation *Tr = PlayerTranslations[MAXPLAYERS];
  if (Tr && Tr->TranslStart == Start && Tr->TranslEnd == End && Tr->Color == Col) {
    return (TRANSL_Player<<TRANSL_TYPE_SHIFT)+MAXPLAYERS;
  }

  if (!Tr) {
    Tr = new VTextureTranslation();
    PlayerTranslations[MAXPLAYERS] = Tr;
  }

  // don't waste time clearing if it's the same range
  if (Tr->TranslStart != Start || Tr->TranslEnd == End) Tr->Clear();

  Tr->BuildPlayerTrans(Start, End, Col);
  return (TRANSL_Player<<TRANSL_TYPE_SHIFT)+MAXPLAYERS;
}


//==========================================================================
//
//  R_GetCachedTranslation
//
//==========================================================================
VTextureTranslation *R_GetCachedTranslation (int TransNum, VLevel *Level) {
  int Type = TransNum>>TRANSL_TYPE_SHIFT;
  int Index = TransNum&((1<<TRANSL_TYPE_SHIFT)-1);
  VTextureTranslation *Tr = nullptr;
  switch (Type) {
    case TRANSL_Standard:
      if (Index == 7) {
        Tr = &IceTranslation;
      } else {
        if (Index < 0 || Index >= NumTranslationTables) return nullptr;
        Tr = TranslationTables[Index];
      }
      break;
    case TRANSL_Player:
      if (Index < 0 || Index >= MAXPLAYERS+1) return nullptr;
      Tr = PlayerTranslations[Index];
      break;
    case TRANSL_Level:
      if (!Level || Index < 0 || Index >= Level->Translations.length()) return nullptr;
      Tr = Level->Translations[Index];
      break;
    case TRANSL_BodyQueue:
      if (!Level || Index < 0 || Index >= Level->BodyQueueTrans.length()) return nullptr;
      Tr = Level->BodyQueueTrans[Index];
      break;
    case TRANSL_Decorate:
      if (Index < 0 || Index >= DecorateTranslations.length()) return nullptr;
      Tr = DecorateTranslations[Index];
      break;
    case TRANSL_Blood:
      if (Index < 0 || Index >= BloodTranslations.length()) return nullptr;
      Tr = BloodTranslations[Index];
      break;
    default:
      return nullptr;
  }

  if (!Tr) return nullptr;
  return Tr;
}


//==========================================================================
//
//  COMMAND TimeRefresh
//
//  For program optimization
//
//==========================================================================
COMMAND(TimeRefresh) {
  double start, stop, time, RenderTime, UpdateTime;
  float startangle;

  if (!cl) return;
  if (GGameInfo->NetMode == NM_None || GGameInfo->NetMode == NM_Client) return;

  startangle = cl->ViewAngles.yaw;

  RenderTime = 0;
  UpdateTime = 0;
  start = Sys_Time();

  int renderAlloc = 0;
  int renderRealloc = 0;
  int renderFree = 0;

  int renderPeakAlloc = 0;
  int renderPeakRealloc = 0;
  int renderPeakFree = 0;

  for (int i = 0; i < 128; ++i) {
    cl->ViewAngles.yaw = (float)(i)*360.0f/128.0f;

    Drawer->StartUpdate();

#ifdef VAVOOM_CORE_COUNT_ALLOCS
    zone_malloc_call_count = 0;
    zone_realloc_call_count = 0;
    zone_free_call_count = 0;
#endif

    RenderTime -= Sys_Time();
    R_RenderPlayerView();
    RenderTime += Sys_Time();

#ifdef VAVOOM_CORE_COUNT_ALLOCS
    renderAlloc += zone_malloc_call_count;
    renderRealloc += zone_realloc_call_count;
    renderFree += zone_free_call_count;

    if (renderPeakAlloc < zone_malloc_call_count) renderPeakAlloc = zone_malloc_call_count;
    if (renderPeakRealloc < zone_realloc_call_count) renderPeakRealloc = zone_realloc_call_count;
    if (renderPeakFree < zone_free_call_count) renderPeakFree = zone_free_call_count;
#endif

    UpdateTime -= Sys_Time();
    Drawer->Update();
    UpdateTime += Sys_Time();
  }
  stop = Sys_Time();

  time = stop-start;
  GCon->Logf("%f seconds (%f fps)", time, 128/time);
  GCon->Logf("Render time %f, update time %f", RenderTime, UpdateTime);
  GCon->Logf("Render malloc calls: %d", renderAlloc);
  GCon->Logf("Render realloc calls: %d", renderRealloc);
  GCon->Logf("Render free calls: %d", renderFree);
  GCon->Logf("Render peak malloc calls: %d", renderPeakAlloc);
  GCon->Logf("Render peak realloc calls: %d", renderPeakRealloc);
  GCon->Logf("Render peak free calls: %d", renderPeakFree);

  cl->ViewAngles.yaw = startangle;
}


//==========================================================================
//
//  COMMAND dbg_count_all_static_lights
//
//==========================================================================
COMMAND(dbg_count_all_static_lights) {
  if (GClLevel && GClLevel->Renderer) {
    GCon->Logf(NAME_Debug, "static lights count: %d", GClLevel->Renderer->GetNumberOfStaticLights());
  }
}


//==========================================================================
//
//  V_Init
//
//==========================================================================
void V_Init (bool showSplash) {
  int DIdx = -1;
  for (int i = 0; i < DRAWER_MAX; ++i) {
    if (!DrawerList[i]) continue;
    // pick first available as default
    if (DIdx == -1) DIdx = i;
    // check for user driver selection
    if (DrawerList[i]->CmdLineArg && videoDrvName && videoDrvName[0] && VStr::strEquCI(videoDrvName, DrawerList[i]->CmdLineArg)) DIdx = i;
  }
  if (DIdx == -1) Sys_Error("No drawers are available");
  GCon->Logf(NAME_Init, "Selected %s", DrawerList[DIdx]->Description);
  // create drawer
  Drawer = DrawerList[DIdx]->Creator();
  Drawer->Init();
  if (showSplash && cli_DisableSplash <= 0) Drawer->ShowLoadingSplashScreen();
}


//==========================================================================
//
//  V_Shutdown
//
//==========================================================================
void V_Shutdown () {
  if (Drawer) {
    Drawer->Shutdown();
    delete Drawer;
    Drawer = nullptr;
  }
  R_FreeModels();
  for (int i = 0; i < MAXPLAYERS+1; ++i) {
    if (PlayerTranslations[i]) {
      delete PlayerTranslations[i];
      PlayerTranslations[i] = nullptr;
    }
  }
  /*
  for (int i = 0; i < CachedTranslations.length(); ++i) {
    delete CachedTranslations[i];
    CachedTranslations[i] = nullptr;
  }
  CachedTranslations.Clear();
  CachedTranslationsMap.clear();
  */
  R_FreeSkyboxData();
}
