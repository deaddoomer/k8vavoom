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
#include "cl_local.h"
#include "drawer.h"
#include "ui/ui.h"
#include "neoui/neoui.h"


extern int screenblocks;

extern VCvarB dbg_world_think_vm_time;
extern VCvarB dbg_world_think_decal_time;

extern double worldThinkTimeVM;
extern double worldThinkTimeDecal;


int ScreenWidth = 0;
int ScreenHeight = 0;

int VirtualWidth = 640;
int VirtualHeight = 480;

static int lastFSMode = -1;

float fScaleX;
float fScaleY;
//float fScaleXI;
//float fScaleYI;

int usegamma = 0;

bool graphics_started = false;

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
static int setwidth;
static int setheight;

static VCvarF menu_darkening("menu_darkening", "0.6", "Screen darkening for active menus.", CVAR_Archive);
static VCvarB draw_pause("draw_pause", true, "Draw \"paused\" text?", CVAR_Archive);
static VCvarB draw_world_timer("draw_world_timer", false, "Draw playing time?", CVAR_Archive);

static VCvarI screen_width("screen_width", "0", "Custom screen width", CVAR_Archive);
static VCvarI screen_height("screen_height", "0", "Custom screen height", CVAR_Archive);
VCvarI screen_fsmode("screen_fsmode", "0", "Video mode: windowed(0), fullscreen scaled(1), fullscreen real(2)", CVAR_Archive);
static VCvarI brightness("brightness", "0", "Brightness.", CVAR_Archive);

static VCvarI draw_fps("draw_fps", "0", "Draw FPS counter (1:FPS; 2:MSECS)?", CVAR_Archive);
static VCvarI draw_fps_posx("draw_fps_posx", "0", "FPS counter position (<0:left; 0:center; >0:right)", CVAR_Archive);
static double fps_start = 0.0;
static double ms = 0.0;
static int fps_frames = 0;
static int show_fps = 0;

static VCvarB draw_gc_stats("draw_gc_stats", false, "Draw GC stats?", CVAR_Archive);
static VCvarI draw_gc_stats_posx("draw_gc_stats_posx", "0", "GC stats counter position (<0:left; 0:center; >0:right)", CVAR_Archive);

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
//  ScreenShot_f
//
//==========================================================================
COMMAND(ScreenShot) {
  int i;
  int bpp;
  bool bot2top;
  void *data;
  VStr filename;
  char tmpbuf[128];

  VStr sst = screenshot_type.asStr().toLowerCase();
  if (sst.length() > 0 && sst[0] == '.') sst.chopLeft(1);
  if (sst.strEqu("jpeg")) sst = "jpg";
  if (sst.isEmpty()) { GCon->Log(NAME_Error, "Empty screenshot type"); return; }
  if (sst.length() > 3) { GCon->Log(NAME_Error, "Screenshot type too long"); return; }

  // find a file name to save it to
  VStr BaseDir = FL_GetScreenshotsDir();
  if (BaseDir.isEmpty()) return;

  for (i = 0; i <= 9999; ++i) {
    snprintf(tmpbuf, sizeof(tmpbuf), "shot%04d.%s", i, *sst);
    filename = BaseDir+"/"+tmpbuf;
    if (!Sys_FileExists(filename)) break; // file doesn't exist
  }

  if (i == 10000) {
    GCon->Log(NAME_Error, "Couldn't create a screenshot file");
    return;
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
static void DrawFPS () {
  const float FilterFadeoff = 0.1f; // 10%
  static float curFilterValue = 0;

  int ypos = 0;

  if (draw_gc_stats) {
    const VObject::GCStats &stats = VObject::GetGCStats();
    T_SetFont(ConFont);
    int xpos;
    if (draw_gc_stats_posx < 0) {
      T_SetAlign(hleft, vtop);
      xpos = 4;
    } else if (draw_gc_stats_posx == 0) {
      T_SetAlign(hcentre, vtop);
      xpos = VirtualWidth/2;
    } else {
      T_SetAlign(hright, vtop);
      xpos = VirtualWidth-2;
    }
    if (Sys_Time()-stats.lastCollectTime > 1) VObject::ResetGCStatsLastCollected();
    T_DrawText(xpos, ypos, va("obj:[\034U%3d\034-/\034U%3d\034-]  array:[\034U%5d\034-/\034U%5d\034-/\034U%d\034-]; \034U%2d\034- msec",
      stats.lastCollected, stats.alive, stats.firstFree, stats.poolSize, stats.poolAllocated, (int)(stats.lastCollectDuration*1000+0.5)), CR_DARKBROWN);
    ypos += 9;
  }

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
    if (draw_fps_posx < 0) {
      T_SetAlign(hleft, vtop);
      xpos = 4;
    } else if (draw_fps_posx == 0) {
      T_SetAlign(hcentre, vtop);
      xpos = VirtualWidth/2;
    } else {
      T_SetAlign(hright, vtop);
      xpos = VirtualWidth-2;
    }
    T_DrawText(xpos, ypos, va("%02d fps", show_fps), CR_DARKBROWN);

    if (draw_fps == 2) {
      T_SetAlign(hright, vtop);
      T_DrawText(VirtualWidth-2, ypos, va("%.2f ms", ms), CR_DARKBROWN);
    }

    ypos += 9;
  }

  if (worldThinkTimeVM < 0) curFilterValue = 0;
  if (worldThinkTimeVM >= 0) curFilterValue = FilterFadeoff*worldThinkTimeVM+(1.0f-FilterFadeoff)*curFilterValue;

  if ((dbg_world_think_vm_time && worldThinkTimeVM >= 0) || (dbg_world_think_decal_time && worldThinkTimeDecal >= 0)) {
    T_SetFont(ConFont);
    T_SetAlign(hleft, vtop);
    int xpos = 4;

    if (dbg_world_think_vm_time && worldThinkTimeVM < 0) worldThinkTimeVM = 0;
    if (dbg_world_think_decal_time && worldThinkTimeDecal < 0) worldThinkTimeDecal = 0;

    if (dbg_world_think_vm_time) { T_DrawText(xpos, ypos, va("VM:%d", (int)(curFilterValue*1000+0.5)), CR_DARKBROWN); ypos += 9; }
    if (dbg_world_think_decal_time) { T_DrawText(xpos, ypos, va("DECALS:%d", (int)(worldThinkTimeDecal*1000+0.5)), CR_DARKBROWN); ypos += 9; }
  }
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

  Drawer->DeinitResolution();

  // changing resolution
  if (!Drawer->SetResolution(width, height, screen_fsmode)) {
    GCon->Logf("Failed to set resolution %dx%d", width, height);
    if (ScreenWidth && lastFSMode >= 0) {
      if (!Drawer->SetResolution(ScreenWidth, ScreenHeight, lastFSMode)) {
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

  screen_width = ScreenWidth;
  screen_height = ScreenHeight;

  VirtualWidth = (ScreenWidth >= 1280 ? ScreenWidth/2 : 640);
  VirtualHeight = (ScreenHeight >= 960 ? ScreenHeight/2 : 480);
  if (ScreenWidth/3 >= 640 && ScreenHeight/3 >= 480) {
    VirtualWidth = ScreenWidth/3;
    VirtualHeight = ScreenHeight/3;
  }

  if (ScreenHeight > 480 && ScreenHeight < 960) VirtualHeight = ScreenHeight/2;

  //VirtualWidth = ScreenWidth;
  //VirtualHeight = ScreenHeight;

  fScaleX = (float)ScreenWidth/(float)VirtualWidth;
  fScaleY = (float)ScreenHeight/(float)VirtualHeight;

  //fScaleXI = (float)VirtualWidth/(float)ScreenWidth;
  //fScaleYI = (float)VirtualHeight/(float)ScreenHeight;

  //GCon->Logf("***SCALE0: %g, %g; scr:%dx%d; vscr:%dx%d", fScaleX, fScaleY, ScreenWidth, ScreenHeight, VirtualWidth, VirtualHeight);
  if (GRoot) GRoot->RefreshScale();
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
  } else if (!screen_width || screen_width != ScreenWidth || screen_height != ScreenHeight) {
    ChangeResolution(screen_width, screen_height);
    res_changed = true;
  }

  if (res_changed) {
    Drawer->InitResolution();
    if (Drawer->RendLev) Drawer->RendLev->PrecacheLevel();
    if (GRoot) GRoot->RefreshScale();
    // recalculate view size and other data
    R_SetViewSize(screenblocks);
  }
  graphics_started = true;
}


//==========================================================================
//
//  SetResolution_f
//
//==========================================================================
COMMAND(SetResolution) {
  if (Args.Num() == 3) {
    setwidth = superatoi(*Args[1]);
    setheight = superatoi(*Args[2]);
    setresolutionneeded = true;
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
  setwidth = ScreenWidth;
  setheight = ScreenHeight;
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
//  SCR_Update
//
//==========================================================================
void SCR_Update (bool fullUpdate) {
  CheckResolutionChange();

  if (!fullUpdate) return;

  bool updateStarted = false;
  bool allowClear = true;

  // do buffered drawing
  if (cl && cls.signon && cl->MO && !GClGame->InIntermission()) {
    if (!GLevel || GLevel->TicTime >= serverStartRenderFramesTic) {
      //k8: always render level, so automap will be updated in all cases
      updateStarted = true;
      Drawer->StartUpdate();
      if (automapactive <= 0 || am_always_update) R_RenderPlayerView();
      Drawer->Setup2D(); // restore 2D projection
      if (automapactive) AM_Drawer();
      if (GGameInfo->NetMode != NM_TitleMap) {
        if (!automapactive && draw_world_timer) AM_DrawWorldTimer();
        CT_Drawer();
        SB_Drawer();
      }
    } else {
      //GCon->Logf("render: tic=%d; starttic=%d", GLevel->TicTime, serverStartRenderFramesTic);
      //return; // skip all rendering
      // k8: nope, we still need to render console
      allowClear = false;
    }
  }

  if (!updateStarted) {
    Drawer->StartUpdate(allowClear);
    Drawer->Setup2D(); // setup 2D projection
  }

  // draw user interface
  GRoot->DrawWidgets();
  // menu drawing
  MN_Drawer();
  // console drawing
  C_Drawer();
  // various on-screen statistics
  DrawFPS();

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
  int xpos = (int)((float)ScreenWidth*260.0f/320.0f);
  int ypos = (int)((float)ScreenHeight*68.0f/200.0f);
  Drawer->StartUpdate(false); // don't clear
  R_DrawPic(xpos, ypos, pt);
  Drawer->Update();
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
  //fScaleXI = (float)VirtualWidth/(float)ScreenWidth;
  //fScaleYI = (float)VirtualHeight/(float)ScreenHeight;
  if (GRoot) GRoot->RefreshScale();
  //GCon->Logf("***SCALE1: %g, %g; scr:%dx%d; vscr:%dx%d", fScaleX, fScaleY, ScreenWidth, ScreenHeight, VirtualWidth, VirtualHeight);
}
