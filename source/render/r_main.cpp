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
//**  Rendering main loop and setup functions, utility functions (BSP,
//**  geometry, trigonometry). See tables.c, too.
//**
//**************************************************************************
//#define RADVLIGHT_GRID_OPTIMIZER

#include "gamedefs.h"
#include "r_local.h"

//#define VAVOOM_DEBUG_PORTAL_POOL

extern int light_reset_surface_cache; // in r_light_reg.cpp


void R_FreeSkyboxData ();


int screenblocks = 0;

TVec vieworg(0, 0, 0);
TVec viewforward(0, 0, 0);
TVec viewright(0, 0, 0);
TVec viewup(0, 0, 0);
TAVec viewangles(0, 0, 0);

TFrustum view_frustum;

int r_visframecount = 0;

VCvarB r_chasecam("r_chasecam", false, "Chasecam mode.", CVAR_Archive);
VCvarF r_chase_dist("r_chase_dist", "32.0", "Chasecam distance.", CVAR_Archive);
VCvarF r_chase_up("r_chase_up", "128.0", "Chasecam position: up.", CVAR_Archive);
VCvarF r_chase_right("r_chase_right", "0", "Chasecam position: right.", CVAR_Archive);
VCvarI r_chase_front("r_chase_front", "0", "Chasecam position: front.", CVAR_Archive);

VCvarI r_fog("r_fog", "0", "Fog mode (0:GL_LINEAR; 1:GL_LINEAR; 2:GL_EXP; 3:GL_EXP2; add 4 to get \"nicer\" fog).");
VCvarB r_fog_test("r_fog_test", false, "Is fog testing enabled?");
VCvarF r_fog_r("r_fog_r", "0.5", "Fog color: red component.");
VCvarF r_fog_g("r_fog_g", "0.5", "Fog color: green component.");
VCvarF r_fog_b("r_fog_b", "0.5", "Fog color: blue component.");
VCvarF r_fog_start("r_fog_start", "1.0", "Fog start distance.");
VCvarF r_fog_end("r_fog_end", "2048.0", "Fog end distance.");
VCvarF r_fog_density("r_fog_density", "0.5", "Fog density.");

VCvarI aspect_ratio("r_aspect_ratio", "0", "Aspect ratio correction mode ([0..3]: normal/4:3/16:9/16:10).", CVAR_Archive);
VCvarB r_interpolate_frames("r_interpolate_frames", true, "Use frame interpolation for smoother rendering?", CVAR_Archive);
VCvarB r_vsync("r_vsync", true, "VSync mode.", CVAR_Archive);
VCvarB r_vsync_adaptive("r_vsync_adaptive", true, "Use adaptive VSync mode.", CVAR_Archive);
VCvarB r_fade_light("r_fade_light", "0", "Fade lights?", CVAR_Archive);
VCvarF r_fade_factor("r_fade_factor", "7", "Fade actor lights?", CVAR_Archive);
VCvarF r_sky_bright_factor("r_sky_bright_factor", "1", "Skybright actor factor.", CVAR_Archive);

VCvarF r_lights_radius("r_lights_radius", "2048", "Maximum light radius.", CVAR_Archive);
//static VCvarB r_lights_cast_many_rays("r_lights_cast_many_rays", false, "Cast more rays to better check light visibility (usually doesn't make visuals any better)?", CVAR_Archive);

static VCvarF r_hud_fullscreen_alpha("r_hud_fullscreen_alpha", "0.44", "Alpha for fullscreen HUD", CVAR_Archive);

extern VCvarB r_dynamic_clip_more;

VDrawer *Drawer;

refdef_t refdef;

float PixelAspect;

bool MirrorFlip = false;
bool MirrorClip = false;


static FDrawerDesc *DrawerList[DRAWER_MAX];

VCvarI screen_size("screen_size", "11", "Screen size.", CVAR_Archive);
bool set_resolutioon_needed = true;

// angles in the SCREENWIDTH wide window
VCvarF fov("fov", "90", "Field of vision.");

// base planes to create fov-based frustum
TClipBase clip_base;

// translation tables
VTextureTranslation *PlayerTranslations[MAXPLAYERS+1];
static TArray<VTextureTranslation*> CachedTranslations;

static VCvarB r_precache_textures("r_precache_textures", true, "Precache level textures?", CVAR_Archive);
static VCvarI r_level_renderer("r_level_renderer", "1", "Level renderer type (0:auto; 1:normal; 2:advanced).", CVAR_Archive);

int r_precache_textures_override = -1;


// ////////////////////////////////////////////////////////////////////////// //
// pool allocator for portal data
// ////////////////////////////////////////////////////////////////////////// //
VRenderLevelShared::PPNode *VRenderLevelShared::pphead = nullptr;
VRenderLevelShared::PPNode *VRenderLevelShared::ppcurr = nullptr;
int VRenderLevelShared::ppMinNodeSize = 0;


//==========================================================================
//
//  CalcAspect
//
//==========================================================================
static float CalcAspect (int aspectRatio, int scrwdt, int scrhgt) {
  switch (aspectRatio) {
    default:
    case 0: // original aspect ratio
      //return ((float)scrhgt*320.0f)/((float)scrwdt*200.0f);
      return 1.2f; // original vanilla pixels are 20% taller than wide
    case 1: // 4:3 aspect ratio
      return ((float)scrhgt*4.0f)/((float)scrwdt*3.0f);
    case 2: // 16:9 aspect ratio
      return ((float)scrhgt*16.0f)/((float)scrwdt*9.0f);
    case 3: // 16:9 aspect ratio
      return ((float)scrhgt*16.0f)/((float)scrwdt*10.0f);
  }
}


//==========================================================================
//
//  GetAspectRatio
//
//==========================================================================
float R_GetAspectRatio () {
  return CalcAspect(aspect_ratio, ScreenWidth, ScreenHeight);
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
  guard(R_Init);
  R_InitSkyBoxes();
  R_InitModels();
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
  unguard;
}


//==========================================================================
//
//  R_Start
//
//==========================================================================
void R_Start (VLevel *ALevel) {
  guard(R_Start);
  switch (r_level_renderer) {
    case 1:
      ALevel->RenderData = new VRenderLevel(ALevel);
      break;
    case 2:
      ALevel->RenderData = new VAdvancedRenderLevel(ALevel);
      break;
    default:
      if (Drawer->SupportsAdvancedRendering()) {
        //ALevel->RenderData = new VAdvancedRenderLevel(ALevel);
        GCon->Logf("Your GPU supports Advanced Renderer, but it is slow and unfinished, so i won't use it.");
      }
      r_level_renderer = 1; // so it will be saved on exit
      ALevel->RenderData = new VRenderLevel(ALevel);
      break;
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::VRenderLevelShared
//
//==========================================================================
VRenderLevelShared::VRenderLevelShared (VLevel *ALevel)
  : VRenderLevelDrawer()
  , Level(ALevel)
  , ViewEnt(nullptr)
  , MirrorLevel(0)
  , PortalLevel(0)
  , VisSize(0)
  , BspVis(nullptr)
  , BspVisThing(nullptr)
  , r_viewleaf(nullptr)
  , r_oldviewleaf(nullptr)
  , old_fov(90.0f)
  , prev_aspect_ratio(0)
  , ExtraLight(0)
  , FixedLight(0)
  , Particles(0)
  , ActiveParticles(0)
  , FreeParticles(0)
  , CurrentSky1Texture(-1)
  , CurrentSky2Texture(-1)
  , CurrentDoubleSky(false)
  , CurrentLightning(false)
  , trans_sprites(nullptr)
  , traspUsed(0)
  , traspSize(0)
  , traspFirst(0)
  , free_wsurfs(nullptr)
  , AllocatedWSurfBlocks(nullptr)
  , AllocatedSubRegions(nullptr)
  , AllocatedDrawSegs(nullptr)
  , AllocatedSegParts(nullptr)
  , cacheframecount(0)
  , showCreateWorldSurfProgress(false)
  , updateWorldCheckVisFrame(false)
  , updateWorldFrame(0)
  , bspVisRadius(nullptr)
  , bspVisRadiusFrame(0)
{
  guard(VRenderLevelShared::VRenderLevelShared);

  memset(light_block, 0, sizeof(light_block));
  memset(block_changed, 0, sizeof(block_changed));
  memset(light_chain, 0, sizeof(light_chain));
  memset(add_block, 0, sizeof(add_block));
  memset(add_changed, 0, sizeof(add_changed));
  memset(add_chain, 0, sizeof(add_chain));
  SimpleSurfsHead = nullptr;
  SimpleSurfsTail = nullptr;
  SkyPortalsHead = nullptr;
  SkyPortalsTail = nullptr;
  HorizonPortalsHead = nullptr;
  HorizonPortalsTail = nullptr;
  PortalDepth = 0;
  //VPortal::ResetFrame();

  VisSize = (Level->NumSubsectors+7)>>3;
  BspVis = new vuint8[VisSize];
  BspVisThing = new vuint8[VisSize];

  lastDLightView = TVec(-1e9, -1e9, -1e9);
  lastDLightViewSub = nullptr;

  memset(DLights, 0, sizeof(DLights));

  CreatePortalPool();

  InitParticles();
  ClearParticles();

  screenblocks = 0;

  // preload graphics
  if (r_precache_textures_override != 0) {
    if (r_precache_textures || r_precache_textures_override > 0) PrecacheLevel();
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::~VRenderLevelShared
//
//==========================================================================
VRenderLevelShared::~VRenderLevelShared () {
  delete[] bspVisRadius;
  bspVisRadius = nullptr;

  // free fake floor data
  for (int i = 0; i < Level->NumSectors; ++i) {
    if (Level->Sectors[i].fakefloors) {
      delete Level->Sectors[i].fakefloors;
      Level->Sectors[i].fakefloors = nullptr;
    }
  }

  for (int i = 0; i < Level->NumSubsectors; ++i) {
    for (subregion_t *r = Level->Subsectors[i].regions; r != nullptr; r = r->next) {
      if (r->floor != nullptr) {
        FreeSurfaces(r->floor->surfs);
        delete r->floor;
        r->floor = nullptr;
      }
      if (r->ceil != nullptr) {
        FreeSurfaces(r->ceil->surfs);
        delete r->ceil;
        r->ceil = nullptr;
      }
    }
  }

  // free seg parts
  for (int i = 0; i < Level->NumSegs; ++i) {
    for (drawseg_t *ds = Level->Segs[i].drawsegs; ds; ds = ds->next) {
      FreeSegParts(ds->top);
      FreeSegParts(ds->mid);
      FreeSegParts(ds->bot);
      FreeSegParts(ds->topsky);
      FreeSegParts(ds->extra);
      if (ds->HorizonTop) Z_Free(ds->HorizonTop);
      if (ds->HorizonBot) Z_Free(ds->HorizonBot);
    }
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

  delete[] Particles;
  Particles = nullptr;
  delete[] BspVis;
  BspVis = nullptr;
  delete[] BspVisThing;
  BspVisThing = nullptr;

  for (int i = 0; i < SideSkies.Num(); ++i) {
    delete SideSkies[i];
    SideSkies[i] = nullptr;
  }

  KillPortalPool();

  // free translucent sprite list
  if (trans_sprites) Z_Free(trans_sprites);
  trans_sprites = nullptr;
  traspUsed = 0;
  traspSize = 0;
  traspFirst = 0;
}


//==========================================================================
//
//  VRenderLevelShared::RadiusCastRay
//
//==========================================================================
bool VRenderLevelShared::RadiusCastRay (const TVec &org, const TVec &dest, float radius, bool advanced) {
#if 0
  // BSP tracing
  float dsq = length2DSquared(org-dest);
  if (dsq <= 1) return true;
  linetrace_t Trace;
  bool canHit = !!Level->TraceLine(Trace, org, dest, SPF_NOBLOCKSIGHT);
  if (canHit) return true;
  if (!advanced || radius < 12) return false;
  // check some more rays
  if (r_lights_cast_many_rays) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        if ((dy|dx) == 0) continue;
        TVec np = org;
        np.x += radius*(0.73f*dx);
        np.y += radius*(0.73f*dy);
        canHit = !!Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT);
        if (canHit) return true;
      }
    }
  } else {
    // check only "head" and "feet"
    TVec np = org;
    np.y += radius*0.73f;
    if (Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT)) return true;
    np = org;
    np.y -= radius*0.73f;
    if (Level->TraceLine(Trace, np, dest, SPF_NOBLOCKSIGHT)) return true;
  }
  return false;
#else
 // blockmap tracing
 return Level->CastCanSee(org, dest, (advanced ? radius : 0));
#endif
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
  guard(R_SetViewSize);
  if (blocks > 2) screen_size = blocks;
  set_resolutioon_needed = true;
  unguard;
}


//==========================================================================
//
//  COMMAND SizeDown
//
//==========================================================================
COMMAND(SizeDown) {
  R_SetViewSize(screenblocks-1);
  GAudio->PlaySound(GSoundManager->GetSoundID("menu/change"), TVec(0, 0, 0), TVec(0, 0, 0), 0, 0, 1, 0, false);
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
//  VRenderLevelShared::ExecuteSetViewSize
//
//==========================================================================
void VRenderLevelShared::ExecuteSetViewSize () {
  guard(VRenderLevelShared::ExecuteSetViewSize);
  set_resolutioon_needed = false;
       if (screen_size < 3) screen_size = 3;
  else if (screen_size > 11) screen_size = 11;
  screenblocks = screen_size;

       if (fov < 5.0f) fov = 5.0f;
  else if (fov > 175.0f) fov = 175.0f;
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

  PixelAspect = CalcAspect(aspect_ratio, ScreenWidth, ScreenHeight);
  prev_aspect_ratio = aspect_ratio;

  clip_base.setupViewport(refdef.width, refdef.height, fov, PixelAspect);
  refdef.fovx = clip_base.fovx;
  refdef.fovy = clip_base.fovy;
  refdef.drawworld = true;
  unguard;
}


//==========================================================================
//
//  R_DrawViewBorder
//
//==========================================================================
void R_DrawViewBorder () {
  guard(R_DrawViewBorder);
  if (GGameInfo->NetMode == NM_TitleMap) {
    GClGame->eventDrawViewBorder(VirtualWidth/2-screenblocks*32, (VirtualHeight-screenblocks*480/10)/2, screenblocks*64, screenblocks*VirtualHeight/10);
  } else {
    GClGame->eventDrawViewBorder(VirtualWidth/2-screenblocks*32, (VirtualHeight-sb_height-screenblocks*(VirtualHeight-sb_height)/10)/2, screenblocks*64, screenblocks*(VirtualHeight-sb_height)/10);
  }
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::TransformFrustum
//
//==========================================================================
void VRenderLevelShared::TransformFrustum () {
  //view_frustum.setup(clip_base, vieworg, viewangles, false/*no back plane*/, -1.0f/*no forward plane*/);
  view_frustum.setup(clip_base, TFrustumParam(vieworg, viewangles, viewforward, viewright, viewup), true/*create back plane*/, -1.0f/*no forward plane*/);
}


//==========================================================================
//
//  VRenderLevelShared::SetupFrame
//
//==========================================================================
void VRenderLevelShared::SetupFrame () {
  guard(VRenderLevelShared::SetupFrame);
  // change the view size if needed
  if (screen_size != screenblocks || !screenblocks ||
      set_resolutioon_needed || old_fov != fov ||
      aspect_ratio != prev_aspect_ratio)
  {
    ExecuteSetViewSize();
  }

  ViewEnt = cl->Camera;
  viewangles = cl->ViewAngles;
  if (r_chasecam && r_chase_front) {
    // this is used to see how weapon looks in player's hands
    viewangles.yaw = AngleMod(viewangles.yaw+180);
    viewangles.pitch = -viewangles.pitch;
  }
  AngleVectors(viewangles, viewforward, viewright, viewup);

  view_frustum.clear(); // why not?

  if (r_chasecam && cl->MO == cl->Camera) {
    vieworg = cl->MO->Origin+TVec(0.0f, 0.0f, 32.0f)-r_chase_dist*viewforward+r_chase_up*viewup+r_chase_right*viewright;
  } else {
    vieworg = cl->ViewOrg;
  }

  ExtraLight = (ViewEnt && ViewEnt->Player ? ViewEnt->Player->ExtraLight*8 : 0);
  if (cl->Camera == cl->MO) {
    ColourMap = CM_Default;
         if (cl->FixedColourmap == INVERSECOLOURMAP) { ColourMap = CM_Inverse; FixedLight = 255; }
    else if (cl->FixedColourmap == GOLDCOLOURMAP) { ColourMap = CM_Gold; FixedLight = 255; }
    else if (cl->FixedColourmap == REDCOLOURMAP) { ColourMap = CM_Red; FixedLight = 255; }
    else if (cl->FixedColourmap == GREENCOLOURMAP) { ColourMap = CM_Green; FixedLight = 255; }
    else if (cl->FixedColourmap >= NUMCOLOURMAPS) { FixedLight = 255; }
    else if (cl->FixedColourmap) { FixedLight = 255-(cl->FixedColourmap<<3); }
    else { FixedLight = 0; }
  } else {
    FixedLight = 0;
    ColourMap = 0;
  }
  // inverse colourmap flash effect
  if (cl->ExtraLight == 255) {
    ExtraLight = 0;
    ColourMap = CM_Inverse;
    FixedLight = 255;
  }

  Drawer->SetupView(this, &refdef);
  ++cacheframecount;
  PortalDepth = 0;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::SetupCameraFrame
//
//==========================================================================
void VRenderLevelShared::SetupCameraFrame (VEntity *Camera, VTexture *Tex, int FOV, refdef_t *rd) {
  guard(VRenderLevelShared::SetupCameraFrame);
  rd->width = Tex->GetWidth();
  rd->height = Tex->GetHeight();
  rd->y = 0;
  rd->x = 0;

  PixelAspect = CalcAspect(aspect_ratio, rd->width, rd->height);

  clip_base.setupViewport(rd->width, rd->height, FOV, PixelAspect);
  rd->fovx = clip_base.fovx;
  rd->fovy = clip_base.fovy;
  rd->drawworld = true;

  ViewEnt = Camera;
  viewangles = Camera->Angles;
  AngleVectors(viewangles, viewforward, viewright, viewup);

  vieworg = Camera->Origin;

  ExtraLight = 0;
  FixedLight = 0;
  ColourMap = 0;

  Drawer->SetupView(this, rd);
  cacheframecount++;
  PortalDepth = 0;
  set_resolutioon_needed = true;
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::MarkLeaves
//
//==========================================================================
void VRenderLevelShared::MarkLeaves () {
  // no need to do anything if we are still in the same subsector
  if (r_oldviewleaf == r_viewleaf) return;

  r_oldviewleaf = r_viewleaf;
  if (!Level->HasPVS()) return;

  if ((++r_visframecount) == 0x7fffffff) {
    r_visframecount = 1;
    for (unsigned nidx = 0; nidx < (unsigned)Level->NumNodes; ++nidx) {
      Level->Nodes[nidx].VisFrame = 0;
    }
    for (unsigned nidx = 0; nidx < (unsigned)Level->NumSubsectors; ++nidx) {
      Level->Subsectors[nidx].VisFrame = 0;
    }
  }
  const int currvisframe = r_visframecount;

  const vuint8 *vis = Level->LeafPVS(r_viewleaf);
  subsector_t *sub = &Level->Subsectors[0];

#if 0
  {
    const unsigned ssleft = (unsigned)Level->NumSubsectors;
    for (unsigned i = 0; i < ssleft; ++i, ++sub) {
      if (vis[i>>3]&(1<<(i&7))) {
        sub->VisFrame = currvisframe;
        node_t *node = sub->parent;
        while (node) {
          if (node->VisFrame == currvisframe) break;
          node->VisFrame = currvisframe;
          node = node->parent;
        }
      }
    }
  }
#else
  {
    unsigned ssleft = (unsigned)Level->NumSubsectors;
    if (!ssleft) return; // just in case
    // process by 8 subsectors
    while (ssleft >= 8) {
      ssleft -= 8;
      vuint8 cvb = *vis++;
      if (!cvb) {
        // everything is invisible, skip 8 subsectors
        sub += 8;
      } else {
        // something is visible
        for (unsigned bc = 8; bc--; cvb >>= 1, ++sub) {
          if (cvb&1) {
            sub->VisFrame = currvisframe;
            node_t *node = sub->parent;
            while (node && node->VisFrame != currvisframe) {
              node->VisFrame = currvisframe;
              node = node->parent;
            }
          }
        }
      }
    }
    // process last byte
    if (ssleft) {
      vuint8 cvb = *vis;
      if (cvb) {
        while (ssleft--) {
          if (cvb&1) {
            sub->VisFrame = currvisframe;
            node_t *node = sub->parent;
            while (node && node->VisFrame != currvisframe) {
              node->VisFrame = currvisframe;
              node = node->parent;
            }
          }
          if ((cvb >>= 1) == 0) break;
          ++sub;
        }
      }
    }
    /*
    for (unsigned i = 0; i < ssleft; ++i, ++sub) {
      if (vis[i>>3]&(1<<(i&7))) {
        sub->VisFrame = currvisframe;
        node_t *node = sub->parent;
        while (node) {
          if (node->VisFrame == currvisframe) break;
          node->VisFrame = currvisframe;
          node = node->parent;
        }
      }
    }
    */
  }
  /*
  else {
    // eh, we have no PVS, so just mark it all
    // we won't check for visframe ever if level has no PVS, so do nothing here
    subsector_t *sub = &Level->Subsectors[0];
    for (int i = Level->NumSubsectors-1; i >= 0; --i, ++sub) {
      sub->VisFrame = currvisframe;
      node_t *node = sub->parent;
      while (node) {
        if (node->VisFrame == currvisframe) break;
        node->VisFrame = currvisframe;
        node = node->parent;
      }
    }
  }
  */
#endif
}


//==========================================================================
//
//  R_RenderPlayerView
//
//==========================================================================
void R_RenderPlayerView () {
  guard(R_RenderPlayerView);
  GClLevel->RenderData->RenderPlayerView();
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::RenderPlayerView
//
//==========================================================================
void VRenderLevelShared::RenderPlayerView () {
  guard(VRenderLevelShared::RenderPlayerView);
  if (!Level->LevelInfo) return;

  int renderattempts = 2;
  bool didIt = false;

  if (updateWorldFrame == 0x7fffffff) {
    for (unsigned f = 0; f < (unsigned)Level->NumSubsectors; ++f) Level->Subsectors[f].updateWorldFrame = 0;
    updateWorldFrame = 1;
  } else {
    ++updateWorldFrame;
  }

again:
  lastDLightView = TVec(-1e9, -1e9, -1e9);
  lastDLightViewSub = nullptr;

  GTextureManager.Time = Level->Time;

  BuildPlayerTranslations();

  if (!didIt) {
    didIt = true;
    AnimateSky(host_frametime);
    UpdateParticles(host_frametime);
  }

  PushDlights();

  // update camera textures that were visible in last frame
  for (int i = 0; i < Level->CameraTextures.Num(); ++i) {
    UpdateCameraTexture(Level->CameraTextures[i].Camera, Level->CameraTextures[i].TexNum, Level->CameraTextures[i].FOV);
  }

  SetupFrame();

  double stt = -Sys_Time();
  RenderScene(&refdef, nullptr);
  stt += Sys_Time();
  if (times_render_highlevel) GCon->Logf("render scene time: %f", stt);
  if (light_reset_surface_cache != 0) {
    light_reset_surface_cache = 0;
    if (--renderattempts <= 0) Host_Error("*** Surface cache overflow, cannot repair");
    GCon->Logf("*** Surface cache overflow, starting it all again, %d attempt%s left", renderattempts, (renderattempts != 1 ? "s" : ""));
    GentlyFlushAllCaches();
    goto again;
  }

  // draw the psprites on top of everything
  if (/*fov <= 90.0f &&*/ cl->MO == cl->Camera && GGameInfo->NetMode != NM_TitleMap) DrawPlayerSprites();

  Drawer->EndView();

  // draw crosshair
  if (cl->MO == cl->Camera && GGameInfo->NetMode != NM_TitleMap) DrawCrosshair();
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::UpdateCameraTexture
//
//==========================================================================
void VRenderLevelShared::UpdateCameraTexture (VEntity *Camera, int TexNum, int FOV) {
  guard(VRenderLevelShared::UpdateCameraTexture);
  if (!Camera) return;

  if (!GTextureManager[TexNum]->bIsCameraTexture) return;

  VCameraTexture *Tex = (VCameraTexture*)GTextureManager[TexNum];
  if (!Tex->bNeedsUpdate) return;

  refdef_t CameraRefDef;
  CameraRefDef.DrawCamera = true;

  SetupCameraFrame(Camera, Tex, FOV, &CameraRefDef);

  RenderScene(&CameraRefDef, nullptr);

  Drawer->EndView();

  Tex->CopyImage();
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::GetFade
//
//==========================================================================
vuint32 VRenderLevelShared::GetFade (sec_region_t *Reg) {
  if (r_fog_test) return 0xff000000|(int(255*r_fog_r)<<16)|(int(255*r_fog_g)<<8)|int(255*r_fog_b);
  if (Reg->params->Fade) return Reg->params->Fade;
  if (Level->LevelInfo->OutsideFog && Reg->ceiling->pic == skyflatnum) return Level->LevelInfo->OutsideFog;
  if (Level->LevelInfo->Fade) return Level->LevelInfo->Fade;
  if (Level->LevelInfo->FadeTable == NAME_fogmap) return 0xff7f7f7fU;
  if (r_fade_light) return FADE_LIGHT; // simulate light fading using dark fog
  return 0;
}


//==========================================================================
//
//  R_DrawPic
//
//==========================================================================
void R_DrawPic (int x, int y, int handle, float Alpha) {
  if (handle < 0) return;
  VTexture *Tex = GTextureManager(handle);
  if (!Tex || Tex->Type == TEXTYPE_Null) return;
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()), fScaleY*(y+Tex->GetScaledHeight()), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
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
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()), fScaleY*(y+Tex->GetScaledHeight()), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
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
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  //Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()*pwdt), fScaleY*(y+Tex->GetScaledHeight()*phgt), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
  Drawer->DrawPic(
    fScaleX*x, fScaleY*y,
    fScaleX*(x+Tex->GetScaledWidth()*pwdt),
    fScaleY*(y+Tex->GetScaledHeight()*phgt),
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
  x -= Tex->GetScaledSOffset();
  y -= Tex->GetScaledTOffset();
  //Drawer->DrawPic(fScaleX*x, fScaleY*y, fScaleX*(x+Tex->GetScaledWidth()*pwdt), fScaleY*(y+Tex->GetScaledHeight()*phgt), 0, 0, Tex->GetWidth(), Tex->GetHeight(), Tex, nullptr, Alpha);
  Drawer->DrawPic(
    fScaleX*(x+Tex->GetScaledWidth()*tx0),
    fScaleY*(y+Tex->GetScaledHeight()*ty0),
    fScaleX*(x+Tex->GetScaledWidth()*tx1),
    fScaleY*(y+Tex->GetScaledHeight()*ty1),
    Tex->GetWidth()*tx0, Tex->GetHeight()*ty0, Tex->GetWidth()*tx1, Tex->GetHeight()*ty1,
    Tex, nullptr, Alpha);
}


//==========================================================================
//
//  VRenderLevelShared::PrecacheLevel
//
//  Preloads all relevant graphics for the level.
//
//==========================================================================
void VRenderLevelShared::PrecacheLevel () {
  guard(VRenderLevelShared::PrecacheLevel);

  if (cls.demoplayback) return;

  TArray<bool> texturepresent;
  int maxtex = GTextureManager.GetNumTextures();
  if (maxtex < 2) return;

  texturepresent.setLength(maxtex);
  for (int f = maxtex-1; f >= 0; --f) texturepresent[f] = false;

  for (int f = 0; f < Level->NumSectors; ++f) {
    if (Level->Sectors[f].floor.pic > 0 && Level->Sectors[f].floor.pic < maxtex) texturepresent[Level->Sectors[f].floor.pic] = true;
    if (Level->Sectors[f].ceiling.pic > 0 && Level->Sectors[f].ceiling.pic < maxtex) texturepresent[Level->Sectors[f].ceiling.pic] = true;
  }

  for (int f = 0; f < Level->NumSides; ++f) {
    if (Level->Sides[f].TopTexture > 0 && Level->Sides[f].TopTexture < maxtex) texturepresent[Level->Sides[f].TopTexture] = true;
    if (Level->Sides[f].MidTexture > 0 && Level->Sides[f].MidTexture < maxtex) texturepresent[Level->Sides[f].MidTexture] = true;
    if (Level->Sides[f].BottomTexture > 0 && Level->Sides[f].BottomTexture < maxtex) texturepresent[Level->Sides[f].BottomTexture] = true;
  }

  R_LdrMsgShowSecondary("PRECACHING TEXTURES...");
  R_PBarReset();

  // precache textures
  for (int f = 1; f < maxtex; ++f) {
    if (texturepresent[f]) {
      R_PBarUpdate("Textures", f, maxtex);
      Drawer->PrecacheTexture(GTextureManager[f]);
    }
  }
  R_PBarUpdate("Textures", maxtex, maxtex, true); // final update

  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::GetTranslation
//
//==========================================================================
VTextureTranslation *VRenderLevelShared::GetTranslation (int TransNum) {
  guard(VRenderLevelShared::GetTranslation);
  return R_GetCachedTranslation(TransNum, Level);
  unguard;
}


//==========================================================================
//
//  VRenderLevelShared::BuildPlayerTranslations
//
//==========================================================================
void VRenderLevelShared::BuildPlayerTranslations () {
  guard(VRenderLevelShared::BuildPlayerTranslations);
  for (TThinkerIterator<VPlayerReplicationInfo> It(Level); It; ++It) {
    if (It->PlayerNum < 0 || It->PlayerNum >= MAXPLAYERS) continue; // should not happen
    if (!It->TranslStart || !It->TranslEnd) continue;

    VTextureTranslation *Tr = PlayerTranslations[It->PlayerNum];
    if (Tr && Tr->TranslStart == It->TranslStart && Tr->TranslEnd == It->TranslEnd && Tr->Colour == It->Colour) continue;

    if (!Tr) {
      Tr = new VTextureTranslation;
      PlayerTranslations[It->PlayerNum] = Tr;
    }

    // don't waste time clearing if it's the same range
    if (Tr->TranslStart != It->TranslStart || Tr->TranslEnd != It->TranslEnd) Tr->Clear();

    Tr->BuildPlayerTrans(It->TranslStart, It->TranslEnd, It->Colour);
  }
  unguard;
}


//==========================================================================
//
//  R_SetMenuPlayerTrans
//
//==========================================================================
int R_SetMenuPlayerTrans (int Start, int End, int Col) {
  guard(R_SetMenuPlayerTrans);
  if (!Start || !End) return 0;

  VTextureTranslation *Tr = PlayerTranslations[MAXPLAYERS];
  if (Tr && Tr->TranslStart == Start && Tr->TranslEnd == End && Tr->Colour == Col) {
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
  unguard;
}


//==========================================================================
//
//  R_GetCachedTranslation
//
//==========================================================================
VTextureTranslation *R_GetCachedTranslation (int TransNum, VLevel *Level) {
  guard(R_GetCachedTranslation);
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
      if (!Level || Index < 0 || Index >= Level->Translations.Num()) return nullptr;
      Tr = Level->Translations[Index];
      break;
    case TRANSL_BodyQueue:
      if (!Level || Index < 0 || Index >= Level->BodyQueueTrans.Num()) return nullptr;
      Tr = Level->BodyQueueTrans[Index];
      break;
    case TRANSL_Decorate:
      if (Index < 0 || Index >= DecorateTranslations.Num()) return nullptr;
      Tr = DecorateTranslations[Index];
      break;
    case TRANSL_Blood:
      if (Index < 0 || Index >= BloodTranslations.Num()) return nullptr;
      Tr = BloodTranslations[Index];
      break;
    default:
      return nullptr;
  }

  if (!Tr) return nullptr;

  for (int i = 0; i < CachedTranslations.Num(); ++i) {
    VTextureTranslation *Check = CachedTranslations[i];
    if (Check->Crc != Tr->Crc) continue;
    if (memcmp(Check->Palette, Tr->Palette, sizeof(Tr->Palette))) continue;
    return Check;
  }

  VTextureTranslation *Copy = new VTextureTranslation;
  *Copy = *Tr;
  CachedTranslations.Append(Copy);
  return Copy;
  unguard;
}


//==========================================================================
//
//  COMMAND TimeRefresh
//
//  For program optimization
//
//==========================================================================
COMMAND(TimeRefresh) {
  guard(COMMAND TimeRefresh);
  double start, stop, time, RenderTime, UpdateTime;
  float startangle;

  if (!cl) return;

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

    zone_malloc_call_count = 0;
    zone_realloc_call_count = 0;
    zone_free_call_count = 0;

    RenderTime -= Sys_Time();
    R_RenderPlayerView();
    RenderTime += Sys_Time();

    renderAlloc += zone_malloc_call_count;
    renderRealloc += zone_realloc_call_count;
    renderFree += zone_free_call_count;

    if (renderPeakAlloc < zone_malloc_call_count) renderPeakAlloc = zone_malloc_call_count;
    if (renderPeakRealloc < zone_realloc_call_count) renderPeakRealloc = zone_realloc_call_count;
    if (renderPeakFree < zone_free_call_count) renderPeakFree = zone_free_call_count;

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
  unguard;
}


//==========================================================================
//
//  V_Init
//
//==========================================================================
void V_Init () {
  guard(V_Init);
  int DIdx = -1;
  for (int i = 0; i < DRAWER_MAX; ++i) {
    if (!DrawerList[i]) continue;
    // pick first available as default
    if (DIdx == -1) DIdx = i;
    // check for user driver selection
    if (DrawerList[i]->CmdLineArg && GArgs.CheckParm(DrawerList[i]->CmdLineArg)) DIdx = i;
  }
  if (DIdx == -1) Sys_Error("No drawers are available");
  GCon->Logf(NAME_Init, "Selected %s", DrawerList[DIdx]->Description);
  // create drawer
  Drawer = DrawerList[DIdx]->Creator();
  Drawer->Init();
  unguard;
}


//==========================================================================
//
//  V_Shutdown
//
//==========================================================================
void V_Shutdown () {
  guard(V_Shutdown);
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
  for (int i = 0; i < CachedTranslations.Num(); ++i) {
    delete CachedTranslations[i];
    CachedTranslations[i] = nullptr;
  }
  CachedTranslations.Clear();
  R_FreeSkyboxData();
  unguard;
}
