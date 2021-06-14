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
#include "gamedefs.h"
#include "drawer.h"
#include "text.h"
#include "touch.h"
#include "widgets/ui.h"
#include "automap.h"
#include "menu.h"
#include "sbar.h"
#include "chat.h"
#include "psim/p_player.h"
#include "server/server.h"
#include "client/client.h"


extern int screenblocks;

extern VCvarB dbg_world_think_vm_time;
extern VCvarB dbg_world_think_decal_time;

extern double worldThinkTimeVM;
extern double worldThinkTimeDecal;

static double wipeStartedTime = -1.0;
static bool wipeStarted = false;

int ScreenWidth = 0;
int ScreenHeight = 0;
int RealScreenWidth = 0;
int RealScreenHeight = 0;

int VirtualWidth = 640;
int VirtualHeight = 480;

static int lastFSMode = -1;

float fScaleX;
float fScaleY;

int usegamma = 0;

// Table of RGB values in current gamma corection level
static const vuint8 gammatable[5][256] = {
  {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
  17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
  33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
  49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
  65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,
  81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,
  97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,
  113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255},

  {2,4,5,7,8,10,11,12,14,15,16,18,19,20,21,23,24,25,26,27,29,30,31,
  32,33,34,36,37,38,39,40,41,42,44,45,46,47,48,49,50,51,52,54,55,
  56,57,58,59,60,61,62,63,64,65,66,67,69,70,71,72,73,74,75,76,77,
  78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,
  99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,
  115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,129,
  130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,
  146,147,148,148,149,150,151,152,153,154,155,156,157,158,159,160,
  161,162,163,163,164,165,166,167,168,169,170,171,172,173,174,175,
  175,176,177,178,179,180,181,182,183,184,185,186,186,187,188,189,
  190,191,192,193,194,195,196,196,197,198,199,200,201,202,203,204,
  205,205,206,207,208,209,210,211,212,213,214,214,215,216,217,218,
  219,220,221,222,222,223,224,225,226,227,228,229,230,230,231,232,
  233,234,235,236,237,237,238,239,240,241,242,243,244,245,245,246,
  247,248,249,250,251,252,252,253,254,255},

  {4,7,9,11,13,15,17,19,21,22,24,26,27,29,30,32,33,35,36,38,39,40,42,
  43,45,46,47,48,50,51,52,54,55,56,57,59,60,61,62,63,65,66,67,68,69,
  70,72,73,74,75,76,77,78,79,80,82,83,84,85,86,87,88,89,90,91,92,93,
  94,95,96,97,98,100,101,102,103,104,105,106,107,108,109,110,111,112,
  113,114,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,
  129,130,131,132,133,133,134,135,136,137,138,139,140,141,142,143,144,
  144,145,146,147,148,149,150,151,152,153,153,154,155,156,157,158,159,
  160,160,161,162,163,164,165,166,166,167,168,169,170,171,172,172,173,
  174,175,176,177,178,178,179,180,181,182,183,183,184,185,186,187,188,
  188,189,190,191,192,193,193,194,195,196,197,197,198,199,200,201,201,
  202,203,204,205,206,206,207,208,209,210,210,211,212,213,213,214,215,
  216,217,217,218,219,220,221,221,222,223,224,224,225,226,227,228,228,
  229,230,231,231,232,233,234,235,235,236,237,238,238,239,240,241,241,
  242,243,244,244,245,246,247,247,248,249,250,251,251,252,253,254,254,
  255},

  {8,12,16,19,22,24,27,29,31,34,36,38,40,41,43,45,47,49,50,52,53,55,
  57,58,60,61,63,64,65,67,68,70,71,72,74,75,76,77,79,80,81,82,84,85,
  86,87,88,90,91,92,93,94,95,96,98,99,100,101,102,103,104,105,106,107,
  108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,
  125,126,127,128,129,130,131,132,133,134,135,135,136,137,138,139,140,
  141,142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,
  155,156,157,158,159,160,160,161,162,163,164,165,165,166,167,168,169,
  169,170,171,172,173,173,174,175,176,176,177,178,179,180,180,181,182,
  183,183,184,185,186,186,187,188,189,189,190,191,192,192,193,194,195,
  195,196,197,197,198,199,200,200,201,202,202,203,204,205,205,206,207,
  207,208,209,210,210,211,212,212,213,214,214,215,216,216,217,218,219,
  219,220,221,221,222,223,223,224,225,225,226,227,227,228,229,229,230,
  231,231,232,233,233,234,235,235,236,237,237,238,238,239,240,240,241,
  242,242,243,244,244,245,246,246,247,247,248,249,249,250,251,251,252,
  253,253,254,254,255},

  {16,23,28,32,36,39,42,45,48,50,53,55,57,60,62,64,66,68,69,71,73,75,76,
  78,80,81,83,84,86,87,89,90,92,93,94,96,97,98,100,101,102,103,105,106,
  107,108,109,110,112,113,114,115,116,117,118,119,120,121,122,123,124,
  125,126,128,128,129,130,131,132,133,134,135,136,137,138,139,140,141,
  142,143,143,144,145,146,147,148,149,150,150,151,152,153,154,155,155,
  156,157,158,159,159,160,161,162,163,163,164,165,166,166,167,168,169,
  169,170,171,172,172,173,174,175,175,176,177,177,178,179,180,180,181,
  182,182,183,184,184,185,186,187,187,188,189,189,190,191,191,192,193,
  193,194,195,195,196,196,197,198,198,199,200,200,201,202,202,203,203,
  204,205,205,206,207,207,208,208,209,210,210,211,211,212,213,213,214,
  214,215,216,216,217,217,218,219,219,220,220,221,221,222,223,223,224,
  224,225,225,226,227,227,228,228,229,229,230,230,231,232,232,233,233,
  234,234,235,235,236,236,237,237,238,239,239,240,240,241,241,242,242,
  243,243,244,244,245,245,246,246,247,247,248,248,249,249,250,250,251,
  251,252,252,253,254,254,255,255}
};


const vuint8 *getGammaTable (int idx) {
  if (idx < 0) idx = 0; else if (idx > 4) idx = 4;
  return gammatable[idx];
}

static bool setresolutionneeded = false;
static int setwidth = 0;
static int setheight = 0;
static float lastScrScale = 0;

static VCvarB dbg_disable_world_render("dbg_disable_world_render", false, "Disable world rendering?", 0);

static VCvarF menu_darkening("menu_darkening", "0.7", "Screen darkening for active menus.", CVAR_Archive);
static VCvarB draw_pause("draw_pause", true, "Draw \"paused\" text?", CVAR_Archive);

static VCvarB crosshair_topmost("crosshair_topmost", false, "Render crosshair on the top of everything?", CVAR_Archive);

VCvarB r_wipe_enabled("r_wipe_enabled", true, "Is screen wipe effect enabled?", CVAR_Archive);
VCvarI r_wipe_type("r_wipe_type", "0", "Wipe type?", CVAR_Archive);

static VCvarI ui_max_scale("ui_max_scale", "0", "Maximal UI scale (0 means unlimited).", CVAR_Archive);
static VCvarI ui_min_scale("ui_min_scale", "0", "Minimal UI scale (0 means unlimited).", CVAR_Archive);

VCvarF screen_scale("screen_scale", "1", "Screen scaling factor (you can set it to >1 to render screen in lower resolution).", CVAR_Archive);
static VCvarI screen_width("screen_width", "0", "Custom screen width", CVAR_Archive);
static VCvarI screen_height("screen_height", "0", "Custom screen height", CVAR_Archive);
static VCvarI screen_width_internal("screen_width_internal", "0", "Internal (rendering) screen width", CVAR_Rom);
static VCvarI screen_height_internal("screen_height_internal", "0", "Internal (rendering) screen height", CVAR_Rom);
VCvarI screen_fsmode("screen_fsmode", "0", "Video mode: windowed(0), fullscreen scaled(1), fullscreen real(2)", CVAR_Archive);
static VCvarI brightness("brightness", "0", "Brightness.", CVAR_Archive);

static VCvarI draw_fps("draw_fps", "0", "Draw FPS counter (1:FPS; 2:MSECS)?", CVAR_Archive);
static VCvarI draw_fps_posx("draw_fps_posx", "0", "FPS counter position (<0:left; 0:center; >0:right)", CVAR_Archive);
static double fps_start = 0.0;
static double ms = 0.0;
static int fps_frames = 0;
static int show_fps = 0;

VCvarB draw_lag("draw_lag", true, "Draw network lag value?", CVAR_Archive);

static VCvarI draw_gc_stats("draw_gc_stats", "0", "Draw GC stats (0: none; 1: brief; 2: full)?", CVAR_Archive);

static VCvarB draw_cycles("draw_cycles", false, "Draw cycle counter?", 0); //NOOP


//**************************************************************************
//
//  Screenshots
//
//**************************************************************************
#ifdef VAVOOM_DISABLE_STB_IMAGE_JPEG
# ifdef VAVOOM_USE_LIBJPG
#  define VV_SWR_HAS_JPEG
# endif
#else
# define VV_SWR_HAS_JPEG
#endif

#ifdef VV_SWR_HAS_JPEG
static VCvarS screenshot_type("screenshot_type", "png", "Screenshot type (png/jpg/tga/pcx).", CVAR_Archive);
#else
static VCvarS screenshot_type("screenshot_type", "png", "Screenshot type (png/tga/pcx).", CVAR_Archive);
#endif

extern void WriteTGA (VStr FileName, void *data, int width, int height, int bpp, bool bot2top);
extern void WritePCX (VStr FileName, void *data, int width, int height, int bpp, bool bot2top);
extern void WritePNG (VStr FileName, const void *Data, int Width, int Height, int Bpp, bool Bot2top);
#ifdef VV_SWR_HAS_JPEG
extern void WriteJPG (VStr FileName, const void *Data, int Width, int Height, int Bpp, bool Bot2top);
#endif


//==========================================================================
//
//  Screenshot command
//
//==========================================================================
COMMAND(Screenshot) {
  int i;
  int bpp;
  bool bot2top;
  void *data;
  VStr filename;
  char tmpbuf[128];

  VStr ssname;
  if (Args.length() >= 2) {
    ssname = Args[1].FixFileSlashes();
    if (ssname.indexOf('/') >= 0) GCon->Log(NAME_Error, "screenshot file name cannot contain a path");
    while (!ssname.isEmpty() && ssname.endsWith(".")) ssname.chopRight(1);
  }

  VStr sst;
  if (!ssname.isEmpty()) sst = ssname.ExtractFileExtension().toLowerCase();
  if (sst.isEmpty()) sst = screenshot_type.asStr().toLowerCase();
  if (sst.length() > 0 && sst[0] == '.') sst.chopLeft(1);
  if (sst.strEqu("jpeg")) sst = "jpg";
  if (sst.isEmpty()) { GCon->Log(NAME_Error, "Empty screenshot type"); return; }
  if (sst.length() > 3) { GCon->Log(NAME_Error, "Screenshot type too long"); return; }

  // find a file name to save it to
  VStr BaseDir = FL_GetScreenshotsDir();
  if (BaseDir.isEmpty()) {
    GCon->Logf(NAME_Error, "Invalid engine screenshot directory");
    return;
  }

  if (ssname.isEmpty()) {
    for (i = 0; i <= 9999; ++i) {
      snprintf(tmpbuf, sizeof(tmpbuf), "shot%04d.%s", i, *sst);
      filename = BaseDir+"/"+tmpbuf;
      if (!Sys_FileExists(filename)) break; // file doesn't exist
    }

    if (i == 10000) {
      GCon->Log(NAME_Error, "Couldn't create a screenshot file");
      return;
    }
  } else {
    filename = BaseDir.appendPath(ssname);
  }

  // save screenshot file
  data = Drawer->ReadScreen(&bpp, &bot2top);
  if (data) {
    bool report = true;
    // type is already lowercased
         if (sst.strEqu("pcx")) WritePCX(filename, data, ScreenWidth, ScreenHeight, bpp, bot2top);
    else if (sst.strEqu("tga")) WriteTGA(filename, data, ScreenWidth, ScreenHeight, bpp, bot2top);
    else if (sst.strEqu("png")) WritePNG(filename, data, ScreenWidth, ScreenHeight, bpp, bot2top);
#ifdef VV_SWR_HAS_JPEG
    else if (sst.strEqu("jpg")) WriteJPG(filename, data, ScreenWidth, ScreenHeight, bpp, bot2top);
#endif
    else {
      report = false;
      GCon->Log(NAME_Error, "Bad screenshot type");
#ifdef VV_SWR_HAS_JPEG
      GCon->Log(NAME_Error, "Supported formats are pcx, tga, png, jpg");
#else
      GCon->Log(NAME_Error, "Supported formats are pcx, tga, png");
#endif
    }
    if (report) GCon->Logf("Saved screenshot to '%s'", *filename);
    Z_Free(data);
  } else {
    GCon->Log(NAME_Error, "Not enough memory to take a screenshot");
  }
}


//**************************************************************************
//
//  Misc drawing stuff
//
//**************************************************************************

//==========================================================================
//
//  DrawFPS
//
//==========================================================================
#ifdef CLIENT
static RunningAverageExp vmAverage;
static RunningAverageExp decalAverage;
static bool stripeRendered;

static void DrawDebugTimesStripe () {
  if (stripeRendered) return;
  stripeRendered = true;
  const float stripeAlpha = 0.666f;
  T_SetFont(ConFont);
  int sXPos = VirtualWidth;
  int sYPos = T_FontHeight();
  GRoot->ToDrawerCoords(sXPos, sYPos);
  Drawer->ShadeRect(0, 0, sXPos, sYPos, stripeAlpha);
}
#endif


static void DrawFPS () {
  #ifdef CLIENT
  const bool isClient = (GGameInfo->NetMode == NM_Client && cl && cl->Net);

       if (worldThinkTimeVM < 0) vmAverage.setValue(0);
  else if (worldThinkTimeVM >= 0) vmAverage.update(worldThinkTimeVM);

       if (worldThinkTimeDecal < 0) decalAverage.setValue(0);
  else if (worldThinkTimeDecal >= 0) decalAverage.update(worldThinkTimeDecal);

  int ypos = 0;
  stripeRendered = false;

  // VM/Decal times
  /*
  if (!isClient) {
    if ((dbg_world_think_vm_time && worldThinkTimeVM >= 0) || (dbg_world_think_decal_time && worldThinkTimeDecal >= 0)) {
      DrawDebugTimesStripe();
      T_SetFont(ConFont);
      T_SetAlign(hleft, vtop);
      int xpos = VirtualWidth/2;
      xpos += xpos/2;
      //ypos = GRoot->GetHeight()-64;

      if (dbg_world_think_decal_time && dbg_world_think_vm_time) {
        T_DrawText(xpos, ypos, va("VM:%d | DECALS:%d", (int)(vmAverage.getValue()*1000+0.5), (int)(decalAverage.getValue()*1000+0.5)), CR_DARKBROWN);
      } else {
        if (dbg_world_think_decal_time) T_DrawText(xpos, ypos, va("DECALS:%d", (int)(decalAverage.getValue()*1000+0.5)), CR_DARKBROWN);
        if (dbg_world_think_vm_time) T_DrawText(xpos, ypos, va("VM:%d", (int)(vmAverage.getValue()*1000+0.5)), CR_DARKBROWN);
      }
    }
  }
  */

  // GC stats
  if (draw_gc_stats > 0) {
    const VObject::GCStats &stats = VObject::GetGCStats();
    DrawDebugTimesStripe();
    T_SetFont(ConFont);
    int xpos;
    T_SetAlign(hcenter, vtop);
    xpos = VirtualWidth/2;
    xpos += xpos/3;
    if (Sys_Time()-stats.lastCollectTime > 1) VObject::ResetGCStatsLastCollected();

    VStr ss = va("[\034U%3d\034-/\034U%4d\034-/\034U%3d\034-]", stats.lastCollected, stats.alive, (int)(stats.lastCollectDuration*1000+0.5));
    if ((dbg_world_think_vm_time && worldThinkTimeVM >= 0) || (dbg_world_think_decal_time && worldThinkTimeDecal >= 0)) {
      if (dbg_world_think_decal_time && dbg_world_think_vm_time) {
        ss += va(" [VM:\034U%d\034- | DC:\034U%d\034-]", (int)(vmAverage.getValue()*1000+0.5), (int)(decalAverage.getValue()*1000+0.5));
      } else {
        if (dbg_world_think_decal_time) ss += va(" [DC:\034U%d\034-]", (int)(decalAverage.getValue()*1000+0.5));
        if (dbg_world_think_vm_time) ss += va(" [VM:\034U%d\034-]", (int)(vmAverage.getValue()*1000+0.5));
      }
    }

    T_DrawText(xpos, ypos, ss, CR_DARKBROWN);
    /*
    T_DrawText(xpos, ypos, va("OBJ:[\034U%3d\034-/\034U%3d\034-]  ARRAY:[\034U%5d\034-/\034U%5d\034-/\034U%d\034-]; \034U%2d\034- MSEC GC",
      stats.lastCollected, stats.alive, stats.firstFree, stats.poolSize, stats.poolAllocated, (int)(stats.lastCollectDuration*1000+0.5)), CR_DARKBROWN);
    */

    //ypos += T_FontHeight();
    if (xpos > 4) {
      T_SetAlign(hleft, vtop);
      T_DrawText(7*T_TextWidth("W"), ypos, va("[T\034U%5d\034-/S\034U%5d\034-|N\034U%4d\034-/F\034U%3d\034-]", dbgEntityTickTotal, dbgEntityTickSimple, dbgEntityTickNoTick, dbgEntityTickTotal-(dbgEntityTickSimple+dbgEntityTickNoTick)), CR_DARKBROWN);
    }
  }

  // FPS
  if (draw_fps) {
    double time = Sys_Time();
    ++fps_frames;

    if (time-fps_start > 1.0) {
      show_fps = (int)(fps_frames/(time-fps_start)+0.5);
      if (draw_fps == 2) ms = 1000.0/fps_frames/(time-fps_start);
      fps_start = time;
      fps_frames = 0;
    }

    T_SetFont(SmallFont);
    int xpos;
    if (stripeRendered) {
      T_SetFont(ConFont);
      T_SetAlign(hleft, vtop);
      xpos = 0;
    } else if (draw_fps_posx < 0) {
      T_SetAlign(hleft, vtop);
      xpos = 4;
    } else if (draw_fps_posx == 0) {
      T_SetAlign(hcenter, vtop);
      xpos = VirtualWidth/2;
    } else {
      T_SetAlign(hright, vtop);
      xpos = VirtualWidth-2;
    }
    if (stripeRendered) {
      T_DrawText(xpos, ypos, va("FPS:%02d", show_fps), CR_DARKBROWN);
    } else {
      T_DrawText(xpos, ypos, va("%02d FPS", show_fps), CR_DARKBROWN);
    }

    if (!stripeRendered && draw_fps == 2) {
      T_SetAlign(hright, vtop);
      T_DrawText(VirtualWidth-2, ypos, va("%.2f MSEC", ms), CR_DARKBROWN);
    }

    ypos += 9;
  }

  // network lag
  if (isClient && draw_lag) {
    T_SetFont(ConFont);
    T_SetAlign(hright, vtop);
    int xpos = GRoot->GetWidth()-4;
    int lypos = GRoot->GetHeight()-64;

    const int nlag = clampval(CL_GetNetLag(), 0, 999);
    T_DrawText(xpos, lypos, va("(%d CHANS) LAG:%3d", CL_GetNumberOfChannels(), nlag), (Host_IsDangerousTimeout() ? CR_RED : CR_DARKBROWN));
    //lypos -= T_FontHeight();

    // draw lag chart
    const int ChartHeight = 32;
    lypos -= 4;

    int sXPos = xpos;
    int sYPos = lypos;
    GRoot->ToDrawerCoords(sXPos, sYPos);
    sXPos -= NETLAG_CHART_ITEMS;

    Drawer->ShadeRect(sXPos, sYPos, sXPos+NETLAG_CHART_ITEMS, sYPos-ChartHeight, 0.666f);
    Drawer->StartAutomap(true); // as overlay
    unsigned pos = (NetLagChartPos+1)%NETLAG_CHART_ITEMS;
    for (int xx = 0; xx < NETLAG_CHART_ITEMS; ++xx) {
      //const int hgt = min2(500, NetLagChart[pos])*ChartHeight/500;
      const int hgt = NetLagChart[pos]*ChartHeight/1000;
      Drawer->DrawLineAM(sXPos+xx, sYPos, 0xff00cf00u, sXPos+xx, sYPos-hgt-1, 0xff005f00u);
      pos = (pos+1)%NETLAG_CHART_ITEMS;
    }
    Drawer->EndAutomap();
  }
  #endif
}


//**************************************************************************
//
// Resolution change
//
//**************************************************************************

//==========================================================================
//
//  ChangeResolution
//
//==========================================================================
static void ChangeResolution (int InWidth, int InHeight) {
  int width = InWidth;
  int height = InHeight;
  //bool win = false;
  //if (screen_fsmode > 0) win = true;

  if (Drawer->RendLev) Drawer->RendLev->UncacheLevel();
  Drawer->DeinitResolution();

  // changing resolution
  if (!Drawer->SetResolution(width, height, screen_fsmode)) {
    GCon->Logf("Failed to set resolution %dx%d", width, height);
    if (RealScreenWidth && lastFSMode >= 0) {
      if (!Drawer->SetResolution(RealScreenWidth, RealScreenHeight, lastFSMode)) {
        Sys_Error("ChangeResolution: failed to restore resolution");
      } else {
        GCon->Log("Restoring previous resolution");
      }
    } else {
      if (!Drawer->SetResolution(0, 0, 0)) {
        Sys_Error("ChangeResolution: Failed to set default resolution");
      } else {
        GCon->Log("Setting default resolution");
        lastFSMode = 0;
      }
    }
  } else {
    lastFSMode = screen_fsmode;
  }
  //GCon->Logf("%dx%d.", ScreenWidth, ScreenHeight);

  screen_width = RealScreenWidth;
  screen_height = RealScreenHeight;

  screen_width_internal = ScreenWidth;
  screen_height_internal = ScreenHeight;

  lastScrScale = max2(1.0f, screen_scale.asFloat());

  VirtualWidth = ScreenWidth;
  VirtualHeight = ScreenHeight;
  // calc scale
  int scale = 1;
  //GCon->Logf(NAME_Debug, "000: %dx%d", VirtualWidth, VirtualHeight);
  while (VirtualWidth/scale >= 640 && VirtualHeight/scale >= 480) ++scale;
  if (scale > 1) --scale;
  if (ui_max_scale.asInt() > 0 && scale > ui_max_scale.asInt()) scale = ui_max_scale.asInt();
  if (ui_min_scale.asInt() > 0 && scale < ui_min_scale.asInt()) scale = ui_min_scale.asInt();
  VirtualWidth /= scale;
  VirtualHeight /= scale;

  fScaleX = (float)ScreenWidth/(float)VirtualWidth;
  fScaleY = (float)ScreenHeight/(float)VirtualHeight;

  if (GRoot) GRoot->RefreshScale();

  // don't forget to call `GRoot->RefreshScale()`!
  //GCon->Logf("***SCALE0: %g, %g; scr:%dx%d; vscr:%dx%d", fScaleX, fScaleY, ScreenWidth, ScreenHeight, VirtualWidth, VirtualHeight);
  // level precaching will be called by the caller
}


//==========================================================================
//
//  CheckResolutionChange
//
//==========================================================================
static void CheckResolutionChange () {
  bool res_changed = false;

  if (brightness != usegamma) {
    usegamma = brightness;
    if (usegamma < 0) {
      usegamma = 0;
      brightness = usegamma;
    }
    if (usegamma > 4) {
      usegamma = 4;
      brightness = usegamma;
    }
  }

  if (setresolutionneeded) {
    ChangeResolution(setwidth, setheight);
    setresolutionneeded = false;
    res_changed = true;
  } else if (!screen_width || screen_width != RealScreenWidth || screen_height != RealScreenHeight || lastScrScale != max2(1.0f, screen_scale.asFloat())) {
    ChangeResolution(screen_width, screen_height);
    res_changed = true;
  }

  if (res_changed) {
    Drawer->InitResolution();
    //R_OSDMsgReset(OSD_MapLoading);
    if (Drawer->RendLev) Drawer->RendLev->PrecacheLevel();
    if (GRoot) GRoot->RefreshScale();
    // post "resolution changed" event
    event_t ev;
    ev.clear();
    ev.type = ev_broadcast;
    ev.data1 = ev_resolution;
    VObject::PostEvent(ev);
    // recalculate view size and other data
    //R_SetViewSize(screenblocks);
    if (Drawer->RendLev) R_ForceViewSizeUpdate();
    //GCon->Logf(NAME_Debug, "RES: real=(%dx%d); scr=(%dx%d)", RealScreenWidth, RealScreenHeight, ScreenWidth, ScreenHeight);
  }
}


//==========================================================================
//
//  SetResolution_f
//
//==========================================================================
COMMAND(SetResolution) {
  if (Args.length() == 3) {
    int w = VStr::atoi(*Args[1]);
    int h = VStr::atoi(*Args[2]);
    if (w >= 320 && h >= 200 && w <= 8192 && h <= 8192) {
      setwidth = w;
      setheight = h;
      setresolutionneeded = true;
    } else {
      GCon->Logf(NAME_Error, "SetResolution: invalid resolution (%sx%s)", *Args[1], *Args[2]);
    }
  } else {
    GCon->Log("SetResolution <width> <height> -- change resolution");
  }
}


//==========================================================================
//
//  COMMAND vid_restart
//
//==========================================================================
COMMAND(vid_restart) {
  setwidth = RealScreenWidth;
  setheight = RealScreenHeight;
  setresolutionneeded = true;
}


//**************************************************************************
//
//  General (public) stuff
//
//**************************************************************************

//==========================================================================
//
//  SCR_Init
//
//==========================================================================
void SCR_Init () {
}


//==========================================================================
//
//  SCR_SignalWipeStart
//
//==========================================================================
void SCR_SignalWipeStart () {
  if (!r_wipe_enabled || !GGameInfo || !GGameInfo->IsWipeAllowed()) {
    clWipeTimer = -1.0f;
  } else {
    clWipeTimer = 0.0f;
  }
  wipeStartedTime = -1.0;
  wipeStarted = false;
}


//==========================================================================
//
//  SCR_Update
//
//==========================================================================
void SCR_Update (bool fullUpdate) {
  CheckResolutionChange();

  if (Drawer) Drawer->IncUpdateFrame();

  // disable tty logs for network games
  if (GGameInfo->NetMode >= NM_DedicatedServer) C_DisableTTYLogs(); else C_EnableTTYLogs();

  if (!fullUpdate) return;

  if (clWipeTimer >= 0.0f && wipeStartedTime < 0.0) {
    //GCon->Logf(NAME_Debug, "PrepareWipe(): clWipeTimer=%g; wipeStartedTime=%g; wipeStarted=%d", clWipeTimer, wipeStartedTime, (int)wipeStarted);
    Drawer->PrepareWipe();
    wipeStartedTime = Sys_Time();
  }

  Drawer->ResetCrosshair();

  bool updateStarted = false;
  bool allowClear = true;
  bool allowWipeStart = true;
  bool drawOther = true;

  //GCon->Logf(NAME_Debug, "cl=%p; signon=%d; MO=%p; ingame=%d (iphase:%d); clWipeTimer=%g; TicTime=%d; srft=%d", cl, cls.signon, (cl ? cl->MO : nullptr), CL_IsInGame(), CL_IntermissionPhase(), clWipeTimer, (GLevel ? GLevel->TicTime : -1), serverStartRenderFramesTic);

  // if the map forced "map end" on the very first ticks...
  if (GGameInfo->NetMode != NM_Client && cl && cls.signon && cl->MO && CL_IntermissionPhase() && clWipeTimer >= 0.0f && GLevel && GLevel->TicTime < serverStartRenderFramesTic) {
    //GCon->Logf(NAME_Debug, "*************************");
    clWipeTimer = -1.0f;
    Drawer->RenderWipe(-1.0f);
  }

  // do buffered drawing
  if (cl && cls.signon && cl->MO && /*!GClGame->InIntermission()*/CL_IsInGame()) {
    if (GGameInfo->NetMode == NM_Client && !cl->Level) {
      allowClear = false;
      allowWipeStart = false;
      drawOther = false;
    } else if (!GLevel || GLevel->TicTime >= serverStartRenderFramesTic) {
      //k8: always render level, so automap will be updated in all cases
      updateStarted = true;
      Drawer->StartUpdate();
      if (!CL_GotNetOrigin()) {
        Drawer->ClearScreen(VDrawer::CLEAR_ALL);
      } else if (am_always_update || clWipeTimer >= 0.0f || AM_IsOverlay()) {
        if (dbg_disable_world_render) {
          Drawer->ClearScreen(VDrawer::CLEAR_ALL);
        } else {
          //if (clWipeTimer >= 0.0f) GCon->Logf(NAME_Debug, "R_RenderPlayerView(): clWipeTimer=%g; wipeStartedTime=%g; wipeStarted=%d; Time=%g; TicTime=%d", clWipeTimer, wipeStartedTime, (int)wipeStarted, GLevel->Time, GLevel->TicTime);
          R_RenderPlayerView();
          // draw crosshair
          if (cl && cl->MO && cl->MO == cl->Camera && GGameInfo->NetMode != NM_TitleMap) {
            Drawer->WantCrosshair();
            if (!crosshair_topmost) Drawer->DrawCrosshair();
          }
        }
      } else {
        Drawer->ClearScreen(VDrawer::CLEAR_ALL);
      }
      Drawer->Setup2D(); // restore 2D projection
      if (GGameInfo->NetMode != NM_TitleMap) {
        AM_Drawer();
        CT_Drawer();
        SB_Drawer();
        //if (cl && cl->MO && cl->MO == cl->Camera) AM_DrawAtWidget(GRoot, cl->MO->Origin.x, cl->MO->Origin.y, 2.0f, 0.0f, 1.0f);
      }
    } else {
      //GCon->Logf("render: tic=%d; starttic=%d", GLevel->TicTime, serverStartRenderFramesTic);
      //return; // skip all rendering
      // k8: nope, we still need to render console
      allowClear = false;
      allowWipeStart = false;
    }
  } else if (GGameInfo->NetMode == NM_Client && cl && cl->Net && !cls.signon && /*!GClGame->InIntermission()*/CL_IsInGame()) {
    allowClear = false;
    allowWipeStart = false;
    drawOther = false;
  }

  if (!updateStarted) {
    Drawer->StartUpdate();
    if (allowClear) Drawer->ClearScreen();
    Drawer->Setup2D(); // setup 2D projection
    if (clWipeTimer >= 0.0f && wipeStartedTime > 0.0) Drawer->RenderWipe(-1.0f);
  }

  if (drawOther) {
    // draw user interface
    GRoot->DrawWidgets();

    // console drawing
    C_Drawer();
    // various on-screen statistics
    DrawFPS();

    // so it will be always visible
    Drawer->DrawCrosshair();

    if (clWipeTimer >= 0.0f && (!GLevel || GLevel->TicTime >= serverStartRenderFramesTic)) {
      // fix wipe timer
      const double ctt = Sys_Time();
      if (allowWipeStart) {
        //GCon->Logf(NAME_Debug, "wiperender: clWipeTimer=%g; wipeStartedTime=%g; wipeStarted=%d", clWipeTimer, wipeStartedTime, (int)wipeStarted);
        if (!wipeStarted) { wipeStarted = true; wipeStartedTime = ctt; }
        clWipeTimer = (float)(ctt-wipeStartedTime);
        // render wipe
        if (clWipeTimer >= 0.0f) {
          if (!Drawer->RenderWipe(clWipeTimer)) clWipeTimer = -1.0f;
        }
      } else {
        Drawer->RenderWipe(-1.0f);
      }
    } else if (clWipeTimer >= 0.0f) {
      Drawer->RenderWipe(-1.0f);
    }

    if ((wipeStarted || (!r_wipe_enabled && updateStarted)) && (!GLevel || GLevel->TicTime >= serverStartRenderFramesTic) && clWipeTimer < 0.0f) {
      MN_CheckStartupWarning();
    }
  } else if (GGameInfo->NetMode == NM_Client && cl && cl->Net && !cls.signon && /*!GClGame->InIntermission()*/CL_IsInGame()) {
    T_SetFont(SmallFont);
    T_SetAlign(hleft, vtop);
    const int y = 8+cls.gotmap*8;
    // slightly off vcenter
    switch (cls.gotmap) {
      case 0: T_DrawText(4, y, "getting network data (map)", CR_TAN); break;
      case 1: T_DrawText(4, y, "getting network data (world)", CR_TAN); break;
      case 2: T_DrawText(4, y, "getting network data (spawning)", CR_TAN); break;
      default: T_DrawText(4, y, "getting network data (something)", CR_TAN); break;
    }
  }

#ifdef ANDROID
  // draw touchscreen controls
  Touch_Draw();
#endif

  // page flip or blit buffer
  Drawer->Update();
}


//==========================================================================
//
//  DrawSomeIcon
//
//==========================================================================
static void DrawSomeIcon (VName icname) {
  if (icname == NAME_None) return;
  if (!Drawer || !Drawer->IsInited()) return;
  int pt = GTextureManager.AddPatch(icname, TEXTYPE_Pic, true);
  if (pt <= 0) return;
  picinfo_t info;
  GTextureManager.GetTextureInfo(pt, &info);
  if (info.width < 1 || info.height < 1) return;
  #if 0
  int xpos = (int)((float)ScreenWidth*260.0f/320.0f);
  int ypos = (int)((float)ScreenHeight*68.0f/200.0f);
  Drawer->StartUpdate();
  R_DrawPic(xpos, ypos, pt);
  Drawer->Update();
  #else
  const int oldVW = VirtualWidth;
  const int oldVH = VirtualHeight;
  VirtualWidth = ScreenWidth;
  VirtualHeight = ScreenHeight;
  fScaleX = fScaleY = 1.0f;
  if (GRoot) GRoot->RefreshScale();
  Drawer->StartUpdate();
  R_DrawPic(VirtualWidth-info.width, VirtualHeight-info.height, pt, 0.4f);
  Drawer->Update();
  VirtualWidth = oldVW;
  VirtualHeight = oldVH;
  fScaleX = (float)ScreenWidth/(float)VirtualWidth;
  fScaleY = (float)ScreenHeight/(float)VirtualHeight;
  if (GRoot) GRoot->RefreshScale();
  #endif
}


//==========================================================================
//
//  Draw_TeleportIcon
//
//==========================================================================
void Draw_TeleportIcon () {
  DrawSomeIcon(NAME_teleicon);
}


//==========================================================================
//
// Draw_SaveIcon
//
//==========================================================================
void Draw_SaveIcon () {
  DrawSomeIcon(NAME_saveicon);
}


//==========================================================================
//
// Draw_LoadIcon
//
//==========================================================================
void Draw_LoadIcon () {
  DrawSomeIcon(NAME_loadicon);
}


//==========================================================================
//
//  SCR_SetVirtualScreen
//
//==========================================================================
void SCR_SetVirtualScreen (int Width, int Height) {
  VirtualWidth = Width;
  VirtualHeight = Height;
  fScaleX = (float)ScreenWidth/(float)VirtualWidth;
  fScaleY = (float)ScreenHeight/(float)VirtualHeight;
  if (GRoot) GRoot->RefreshScale();
  //GCon->Logf("***SCALE1: %g, %g; scr:%dx%d; vscr:%dx%d", fScaleX, fScaleY, ScreenWidth, ScreenHeight, VirtualWidth, VirtualHeight);
}
