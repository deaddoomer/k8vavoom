//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include "gamedefs.h"
#include "cl_local.h"
#include "drawer.h"
#include "ui/ui.h"


void CalcFadetable16 (byte *pal);

extern int screenblocks;


int ScreenWidth = 0;
int ScreenHeight = 0;

int VirtualWidth = 640;
int VirtualHeight = 480;

float fScaleX;
float fScaleY;
float fScaleXI;
float fScaleYI;

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

static VCvarF menu_darkening("menu_darkening", "0.5", "Screen darkening for active menus.", CVAR_Archive);
static VCvarB draw_pause("draw_pause", true, "Draw \"paused\" text?");

static VCvarI screen_width("screen_width", "0", "Custom screen width", CVAR_Archive);
static VCvarI screen_height("screen_height", "0", "Custom screen height", CVAR_Archive);
//static VCvarI screen_bpp("screen_bpp", "0", "Custom screen BPP", CVAR_Archive);
VCvarB screen_windowed("screen_windowed", true, "Use windowed mode?", CVAR_Archive);
static VCvarI brightness("brightness", "0", "Brightness.", CVAR_Archive);

static VCvarI draw_fps("draw_fps", "0", "Draw FPS counter?", CVAR_Archive);
static VCvarI draw_fps_posx("draw_fps_posx", "0", "FPS counter position (<0:left; 0:center; >0:right)", CVAR_Archive);
static double fps_start = 0.0;
static double ms = 0.0;
static int fps_frames = 0;
static int show_fps = 0;

static VCvarB draw_cycles("draw_cycles", false, "Draw cycle counter?", 0); //NOOP


//**************************************************************************
//
//  Screenshots
//
//**************************************************************************
#ifdef VAVOOM_USE_LIBJPG
static VCvarS screenshot_type("screenshot_type", "png", "Screenshot type (png/jpg/tga/pcx).", CVAR_Archive);
#else
static VCvarS screenshot_type("screenshot_type", "png", "Screenshot type (png/tga/pcx).", CVAR_Archive);
#endif

extern void WriteTGA (const VStr &FileName, void *data, int width, int height, int bpp, bool bot2top);
extern void WritePCX (const VStr &FileName, void *data, int width, int height, int bpp, bool bot2top);
extern void WritePNG (const VStr &FileName, const void *Data, int Width, int Height, int Bpp, bool Bot2top);
#ifdef VAVOOM_USE_LIBJPG
extern void WriteJPG (const VStr &FileName, const void *Data, int Width, int Height, int Bpp, bool Bot2top);
#endif


//==========================================================================
//
//  ScreenShot_f
//
//==========================================================================
COMMAND(ScreenShot) {
  guard(COMMAND ScreenShot);
  int i;
  int bpp;
  bool bot2top;
  void *data;
  VStr filename;
  char tmpbuf[128];

  if (strlen(screenshot_type) > 8) {
    GCon->Log("Screenshot extension too long");
    return;
  }

  // find a file name to save it to
  VStr BaseDir;
  {
#if !defined(_WIN32)
    const char *HomeDir = getenv("HOME");
    if (HomeDir && HomeDir[0]) {
      BaseDir = VStr(HomeDir)+"/.vavoom";
    } else {
      BaseDir = (fl_savedir.IsNotEmpty() ? fl_savedir : fl_basedir)+"/"+fl_gamedir;
    }
#else
    BaseDir = (fl_savedir.IsNotEmpty() ? fl_savedir : fl_basedir)+"/"+fl_gamedir;
#endif
  }

  for (i = 0; i <= 9999; ++i) {
    snprintf(tmpbuf, sizeof(tmpbuf), "shot%04d.%s", i, (const char*)screenshot_type);
    filename = BaseDir+"/"+tmpbuf;
    if (!Sys_FileExists(filename)) break; // file doesn't exist
  }

  if (i == 10000) {
    GCon->Log("Couldn't create a screenshot file");
    return;
  }

  // save screenshot file
  data = Drawer->ReadScreen(&bpp, &bot2top);
  if (data) {
    bool report = true;
         if (!VStr::ICmp(screenshot_type, "pcx")) WritePCX(filename, data, ScreenWidth, ScreenHeight, bpp, bot2top);
    else if (!VStr::ICmp(screenshot_type, "tga")) WriteTGA(filename, data, ScreenWidth, ScreenHeight, bpp, bot2top);
    else if (!VStr::ICmp(screenshot_type, "png")) WritePNG(filename, data, ScreenWidth, ScreenHeight, bpp, bot2top);
#ifdef VAVOOM_USE_LIBJPG
    else if (!VStr::ICmp(screenshot_type, "jpg")) WriteJPG(filename, data, ScreenWidth, ScreenHeight, bpp, bot2top);
#endif
    else {
      report = false;
      GCon->Log("Bad screenshot type");
      GCon->Log("Supported formats are pcx, tga, png and jpg");
    }
    if (report) GCon->Logf("Saved screenshot to '%s'", *filename);
    Z_Free(data);
  } else {
    GCon->Log("Not enough memory to take a screenshot");
  }

  unguard;
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
  guard(DrawFPS);
  if (draw_fps) {
    double time = Sys_Time();
    ++fps_frames;

    if (time-fps_start > 1.0) {
      show_fps = (int)(fps_frames/(time-fps_start));
      if (draw_fps == 2) ms = 1000.0/fps_frames/(time-fps_start);
      fps_start = time;
      fps_frames = 0;
    }

    T_SetFont(SmallFont);
    if (draw_fps_posx < 0) {
      T_SetAlign(hright, vtop);
      T_DrawText(7*8, 0, va("%d fps", show_fps), CR_UNTRANSLATED);
    } else if (draw_fps_posx == 0) {
      T_SetAlign(hcentre, vtop);
      T_DrawText(VirtualWidth/2, 0, va("%02d fps", show_fps), CR_UNTRANSLATED); //FIXME
    } else {
      T_SetAlign(hright, vtop);
      T_DrawText(VirtualWidth-2, 0, va("%02d fps", show_fps), CR_UNTRANSLATED);
    }

    if (draw_fps == 2) {
      T_SetAlign(hright, vtop);
      T_DrawText(VirtualWidth-2, 12, va("%.2f ms ", ms), CR_UNTRANSLATED);
    }
  }
  unguard;
}


//==========================================================================
//
//  DrawCycles
//
//==========================================================================
/*
static void DrawCycles () {
  guard(DrawCycles);
  if (draw_cycles) {
    T_SetFont(ConFont);
    T_SetAlign(hright, vtop);
    for (int i = 0; i < 16; ++i) {
      T_DrawText(VirtualWidth-2, 32+i*8, va("%d %10u", i, host_cycles[i]), CR_UNTRANSLATED);
      host_cycles[i] = 0;
    }
  }
  unguard;
}
*/


//**************************************************************************
//
//  Resolution change
//
//**************************************************************************

//==========================================================================
//
//  ChangeResolution
//
//==========================================================================
static void ChangeResolution (int InWidth, int InHeight) {
  guard(ChangeResolution);
  int width = InWidth;
  int height = InHeight;
  bool win = false;
  if (screen_windowed > 0) win = true;

  // Changing resolution
  if (!Drawer->SetResolution(width, height, win)) {
    GCon->Logf("Failed to set resolution %dx%d", width, height);
    if (ScreenWidth) {
      if (!Drawer->SetResolution(ScreenWidth, ScreenHeight, win)) {
        Sys_Error("ChangeResolution: failed to restore resolution");
      } else {
        GCon->Log("Restoring previous resolution");
      }
    } else {
      if (!Drawer->SetResolution(0, 0, win)) {
        Sys_Error("ChangeResolution: Failed to set default resolution");
      } else {
        GCon->Log("Setting default resolution");
      }
    }
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
  //VirtualWidth = 640;
  //VirtualWidth = 800;

  fScaleX = (float)ScreenWidth / (float)VirtualWidth;
  fScaleY = (float)ScreenHeight / (float)VirtualHeight;

  fScaleXI = (float)VirtualWidth / (float)ScreenWidth;
  fScaleYI = (float)VirtualHeight / (float)ScreenHeight;

  //GCon->Logf("***SCALE0: %g, %g; scr:%dx%d; vscr:%dx%d", fScaleX, fScaleY, ScreenWidth, ScreenHeight, VirtualWidth, VirtualHeight);
  if (GRoot) GRoot->RefreshScale();

  unguard;
}


//==========================================================================
//
//  CheckResolutionChange
//
//==========================================================================
static void CheckResolutionChange () {
  guard(CheckResolutionChange);
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
    // recalculate view size and other data
    R_SetViewSize(screenblocks);
  }
  graphics_started = true;
  unguard;
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
void SCR_Update () {
  guard(SCR_Update);
  CheckResolutionChange();

  Drawer->StartUpdate();

  // do buffered drawing
  if (cl && cls.signon && cl->MO) {
    switch (GClGame->intermission) {
      case 0:
        if (automapactive <= 0) R_RenderPlayerView();
        if (automapactive) AM_Drawer();
        if (GGameInfo->NetMode != NM_TitleMap) {
          CT_Drawer();
          SB_Drawer();
        }
        break;
    }
  }

  // draw user interface
  GRoot->DrawWidgets();

  // menu drawing
  MN_Drawer();

  // console drawing
  C_Drawer();

  DrawFPS();
  //DrawCycles();

  Drawer->Update(); // page flip or blit buffer
  unguard;
}


//==========================================================================
//
// Draw_TeleportIcon
//
//==========================================================================
void Draw_TeleportIcon () {
  /*
  guard(Draw_TeleportIcon);
  if (W_CheckNumForName(NAME_teleicon) >= 0) {
    Drawer->BeginDirectUpdate();
    R_DrawPic(260, 68, GTextureManager.AddPatch(NAME_teleicon, TEXTYPE_Pic));
    Drawer->EndDirectUpdate();
  }
  unguard;
  */
}


//==========================================================================
//
// Draw_SaveIcon
//
//==========================================================================
void Draw_SaveIcon () {
  /*
  guard(Draw_SaveIcon);
  if (W_CheckNumForName(NAME_saveicon) >= 0) {
    Drawer->BeginDirectUpdate();
    R_DrawPic(260, 68, GTextureManager.AddPatch(NAME_saveicon, TEXTYPE_Pic));
    Drawer->EndDirectUpdate();
  }
  unguard;
  */
}


//==========================================================================
//
// Draw_LoadIcon
//
//==========================================================================
void Draw_LoadIcon () {
  /*
  guard(Draw_LoadIcon);
  if (W_CheckNumForName(NAME_loadicon) >= 0) {
    Drawer->BeginDirectUpdate();
    R_DrawPic(260, 68, GTextureManager.AddPatch(NAME_loadicon, TEXTYPE_Pic));
    Drawer->EndDirectUpdate();
  }
  unguard;
  */
}


//==========================================================================
//
//  SCR_SetVirtualScreen
//
//==========================================================================
void SCR_SetVirtualScreen (int Width, int Height) {
  guard(SCR_SetVirtualScreen);
  VirtualWidth = Width;
  VirtualHeight = Height;
  fScaleX = (float)ScreenWidth/(float)VirtualWidth;
  fScaleY = (float)ScreenHeight/(float)VirtualHeight;
  fScaleXI = (float)VirtualWidth/(float)ScreenWidth;
  fScaleYI = (float)VirtualHeight/(float)ScreenHeight;
  if (GRoot) GRoot->RefreshScale();
  //GCon->Logf("***SCALE1: %g, %g; scr:%dx%d; vscr:%dx%d", fScaleX, fScaleY, ScreenWidth, ScreenHeight, VirtualWidth, VirtualHeight);
  unguard;
}
