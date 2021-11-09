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
//**
//**  Refresh of things, i.e. objects represented by sprites.
//**
//**  Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn
//**  CLOCKWISE around the axis. This is not the same as the angle, which
//**  increases counter clockwise (protractor). There was a lot of stuff
//**  grabbed wrong, so I changed it...
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../psim/p_entity.h"
#include "../psim/p_player.h"
#include "../screen.h"
#include "../sbar.h"
#include "../client/client.h"
#include "r_local.h"


extern VCvarF fov_main;
extern VCvarF cl_fov;
extern VCvarB gl_pic_filtering;
extern VCvarB r_draw_psprites;
extern VCvarB r_chasecam;
extern VCvarB gl_crop_psprites;
extern VCvarB r_dbg_proj_aspect;

static VCvarI crosshair("crosshair", "2", "Crosshair type (0-2).", CVAR_Archive|CVAR_NoShadow);
static VCvarF crosshair_alpha("crosshair_alpha", "0.6", "Crosshair opacity.", CVAR_Archive|CVAR_NoShadow);
static VCvarF crosshair_scale("crosshair_scale", "1", "Crosshair scale.", CVAR_Archive|CVAR_NoShadow);

static VCvarB crosshair_force("crosshair_force", false, "Force menu selected crosshair (i.e. don't allow mods to set their own)?", CVAR_Archive|CVAR_NoShadow);
static VCvarF crosshair_big_scale("crosshair_big_scale", "0.5", "Scale for 'big' mod crosshairs.", CVAR_Archive|CVAR_NoShadow);
static VCvarB crosshair_big_prefer("crosshair_big_prefer", true, "Prefer 'big' mod crosshairs over small ones?", CVAR_Archive|CVAR_NoShadow);

static VCvarI r_crosshair_yofs("r_crosshair_yofs", "0", "Crosshair y offset (>0: down).", CVAR_Archive|CVAR_NoShadow);

int VRenderLevelShared::prevCrosshairPic = -666;
static int currCrosshairPicHandle = 0;
static TMapNC<int, int> customXHairHandles; // key: id; value: handle (0: invalid; negative: scale by 0.5)


static int cli_WarnSprites = 0;
/*static*/ bool cliRegister_rsprites_args =
  VParsedArgs::RegisterFlagSet("-Wpsprite", "!show some warnings about sprites", &cli_WarnSprites);


//==========================================================================
//
//  showPSpriteWarnings
//
//==========================================================================
static inline bool showPSpriteWarnings () {
  return (cli_WAll > 0 || cli_WarnSprites > 0);
}


//==========================================================================
//
//  VRenderLevelShared::RenderPSprite
//
//==========================================================================
void VRenderLevelShared::RenderPSprite (float SX, float SY, const VAliasModelFrameInfo &mfi, float PSP_DIST, const RenderStyleInfo &ri) {
  spritedef_t *sprdef;
  spriteframe_t *sprframe;
  int lump;
  bool flip;

  if (ri.alpha < 0.004f) return; // no reason to render it, it is invisible
  //if (ri.alpha > 1.0f) ri.alpha = 1.0f;

  // decide which patch to use
  if ((unsigned)mfi.spriteIndex >= (unsigned)sprites.length()) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite number %d", mfi.spriteIndex);
    }
    return;
  }
  sprdef = &sprites.ptr()[mfi.spriteIndex];

  if (mfi.frame >= sprdef->numframes) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite frame %d : %d (max is %d)", mfi.spriteIndex, mfi.frame, sprdef->numframes);
    }
    return;
  }
  sprframe = &sprdef->spriteframes[mfi.frame];

  lump = sprframe->lump[0];
  if (lump < 0) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "R_ProjectSprite: invalid sprite texture id %d in frame %d : %d", lump, mfi.spriteIndex, mfi.frame);
    }
    return;
  }
  flip = sprframe->flip[0];

  VTexture *Tex = GTextureManager[lump];
  if (!Tex) {
    if (showPSpriteWarnings()) {
      GCon->Logf(NAME_Warning, "RenderPSprite: invalid sprite texture id %d in frame %d : %d (the thing that should not be)", lump, mfi.spriteIndex, mfi.frame);
    }
    return;
  }

  // need to call cropper here, because if the texture is not cached yet, its dimensions are wrong
  if (gl_crop_psprites.asBool()) Tex->CropTexture();

  const int TexWidth = Tex->GetWidth();
  const int TexHeight = Tex->GetHeight();
  const int TexSOffset = Tex->SOffset;
  const int TexTOffset = Tex->TOffset;

  const float scaleX = max2(0.001f, Tex->SScale);
  const float scaleY = max2(0.001f, Tex->TScale);

  const float invScaleX = 1.0f/scaleX;
  const float invScaleY = 1.0f/scaleY;

  const float PSP_DISTI = 1.0f/PSP_DIST;

  TVec vo = Drawer->vieworg;
  TVec vfwd = Drawer->viewforward;
  TVec vright = Drawer->viewright;
  TVec vup = Drawer->viewup;
  //FIXME: doesn't work
  if (!r_dbg_proj_aspect.asBool()) {
    //vo.z *= PixelAspect;
    TAVec aa = Drawer->viewangles;
    aa.pitch *= PixelAspect;
    AngleVectors(aa, vfwd, vright, vup);
  }

  TVec sprorigin = vo+PSP_DIST*vfwd;

  float sprx = 160.0f-SX+TexSOffset*invScaleX;
  float spry = 100.0f-SY+TexTOffset*invScaleY;

  spry -= cl->PSpriteSY;
  spry -= PSpriteOfsAspect;

  //GCon->Logf(NAME_Debug, "PSPRITE: '%s'; size=(%d,%d); ofs=(%d,%d); scale=(%g,%g); sxy=(%g,%g); sprxy=(%g,%g)", *Tex->Name, TexWidth, TexHeight, TexSOffset, TexTOffset, Tex->SScale, Tex->TScale, SX, SY, sprx, spry);

  //k8: this is not right, but meh...
  /*
  if (fov > 90) {
    // this moves sprx to the left screen edge
    //sprx += (AspectEffectiveFOVX-1.0f)*160.0f;
  }
  */

  float ourfov = clampval(fov_main.asFloat(), 1.0f, 170.0f);
  // apply client fov
  if (cl_fov > 1) ourfov = clampval(cl_fov.asFloat(), 1.0f, 170.0f);

  //k8: don't even ask me!
  const float sxymul = (1.0f+(ourfov != 90 ? AspectEffectiveFOVX-1.0f : 0.0f))/160.0f;

  // horizontal
  TVec start = sprorigin-(sprx*PSP_DIST*sxymul)*vright;
  TVec end = start+(TexWidth*invScaleX*PSP_DIST*sxymul)*vright;

  TVec topdelta = (spry*PSP_DIST*sxymul)*vup;
  TVec botdelta = topdelta-(TexHeight*invScaleY*PSP_DIST*sxymul)*vup;

  // this puts psprite at the fixed screen position (only for horizontal FOV)
  if (r_dbg_proj_aspect.asBool()) {
    if (!r_vertical_fov) {
      topdelta /= PixelAspect;
      botdelta /= PixelAspect;
    }
  }

  TVec dv[4];
  dv[0] = start+botdelta;
  dv[1] = start+topdelta;
  dv[2] = end+topdelta;
  dv[3] = end+botdelta;

  TVec saxis(0, 0, 0);
  TVec taxis(0, 0, 0);
  TVec texorg(0, 0, 0);

  // texture scale
  const float axismul = 1.0f/160.0f/sxymul;

  const float saxmul = 160.0f*axismul;
  if (flip) {
    saxis = -(vright*saxmul*PSP_DISTI);
    texorg = dv[2];
  } else {
    saxis = vright*saxmul*PSP_DISTI;
    texorg = dv[1];
  }

  taxis = -(vup*100.0f*axismul*(320.0f/200.0f)*PSP_DISTI);

  saxis *= scaleX;
  taxis *= scaleY;

  Drawer->PushDepthMaskSlow();
  Drawer->GLDisableDepthWriteSlow();
  Drawer->GLDisableDepthTestSlow();
  Drawer->DrawSpritePolygon((Level ? Level->Time : 0.0f), dv, GTextureManager[lump], ri,
    nullptr, ColorMap, -vfwd,
    dv[0].dot(-vfwd), saxis, taxis, texorg, VDrawer::SpriteType::SP_PSprite);
  Drawer->PopDepthMaskSlow();
  Drawer->GLEnableDepthTestSlow();
}


//==========================================================================
//
//  VRenderLevelShared::RenderViewModel
//
//  FIXME: this doesn't work with "----" and "####" view states
//
//==========================================================================
bool VRenderLevelShared::RenderViewModel (VViewState *VSt, float SX, float SY, const RenderStyleInfo &ri) {
  if (!r_models_view) return false;
  if (!R_HaveClassModelByName(VSt->State->Outer->Name)) return false;

  VMatrix4 oldProjMat;
  TClipBase old_clip_base = clip_base;

  // remporarily set FOV to 90
  const bool restoreFOV = (fov_main.asFloat() != 90.0f || cl_fov.asFloat() >= 1.0f);

  if (restoreFOV) {
    refdef_t newrd = refdef;
    const float fov90 = CalcEffectiveFOV(90.0f, newrd);
    SetupRefdefWithFOV(&newrd, fov90);

    VMatrix4 newProjMat;
    Drawer->CalcProjectionMatrix(newProjMat, /*this,*/ &newrd);

    Drawer->GetProjectionMatrix(oldProjMat);
    Drawer->SetProjectionMatrix(newProjMat);
  }

  TVec origin = Drawer->vieworg+(SX-1.0f)*Drawer->viewright/8.0f-(SY-32.0f)*Drawer->viewup/6.0f;

  float TimeFrac = 0;
  if (VSt->State->Time > 0) {
    TimeFrac = 1.0f-(VSt->StateTime/VSt->State->Time);
    TimeFrac = midval(0.0f, TimeFrac, 1.0f);
  }

  const bool res = DrawAliasModel(nullptr, VSt->State->Outer->Name, origin, cl->ViewAngles, 1.0f, 1.0f,
    VSt->State->getMFI(), (VSt->State->NextState ? VSt->State->NextState->getMFI() : VSt->State->getMFI()),
    nullptr, 0, ri, true, TimeFrac, r_interpolate_frames,
    RPASS_Normal);

  if (restoreFOV) {
    // restore original FOV (just in case)
    Drawer->SetProjectionMatrix(oldProjMat);
    clip_base = old_clip_base;
  }

  return res;
}


//==========================================================================
//
//  getVSOffset
//
//==========================================================================
static inline float getVSOffset (const float ofs, const float stofs) noexcept {
  if (stofs <= -10000.0f) return stofs+10000.0f;
  if (stofs >=  10000.0f) return stofs-10000.0f;
  return ofs;
}


//==========================================================================
//
//  VRenderLevelShared::DrawPlayerSprites
//
//==========================================================================
void VRenderLevelShared::DrawPlayerSprites () {
  if (!r_draw_psprites || r_chasecam) return;
  if (!cl || !cl->MO) return;

  // BadApple.wad hack
  if (Level->IsBadApple()) return;

  int RendStyle = STYLE_Normal;
  float Alpha = 1.0f;
  cl->MO->eventGetViewEntRenderParams(Alpha, RendStyle);

  RenderStyleInfo ri;
  if (!CalculateRenderStyleInfo(ri, RendStyle, Alpha)) return;

  const int oldRM = r_light_mode.asInt();
  r_light_mode.ForceSet(0); // switch to real ambient lighting

  int ltxr = 0, ltxg = 0, ltxb = 0;
  {
    // check if we have any light at player's origin (rough), and owned by player
    const dlight_t *dl = DLights;
    for (int dlcount = MAX_DLIGHTS; dlcount--; ++dl) {
      if (dl->die < Level->Time || dl->radius < 1.0f) continue;
      //if (!dl->Owner || dl->Owner->IsGoingToDie() || !dl->Owner->IsA(eclass)) continue;
      //VEntity *e = (VEntity *)dl->Owner;
      VEntity *e = Level->GetEntityBySUId(dl->ownerUId);
      if (!e) continue;
      /*
      if (dl->ownerUId) {
        auto ownpp = suid2ent.get(dl->ownerUId);
        if (!ownpp) continue;
        e = *ownpp;
      } else {
        continue;
      }
      */
      if (!e->IsPlayer()) continue;
      if (e != cl->MO) continue;
      if ((e->Origin-dl->origin).length() > dl->radius*0.75f) continue;
      ltxr += (dl->color>>16)&0xff;
      ltxg += (dl->color>>8)&0xff;
      ltxb += dl->color&0xff;
    }
  }

  RenderStyleInfo mdri = ri;

  // calculate interpolation
  const float currSX = cl->ViewStateSX;
  const float currSY = cl->ViewStateSY;

  float SX = currSX;
  float SY = currSY;

  const float dur = cl->PSpriteWeaponLoweringDuration;
  if (dur > 0.0f) {
    float stt = cl->PSpriteWeaponLoweringStartTime;
    float currt = Level->Time;
    float t = currt-stt;
    float prevSY = cl->PSpriteWeaponLowerPrev;
    if (t >= 0.0f && t < dur) {
      const float ydelta = fabsf(prevSY-currSY)*(t/dur);
      //GCon->Logf("prev=%f; end=%f; curr=%f; dur=%f; t=%f; mul=%f; ydelta=%f", cl->PSpriteWeaponLowerPrev, currSY, prevSY+ydelta, dur, t, t/dur, ydelta);
      if (prevSY < currSY) {
        prevSY += ydelta;
        //GCon->Logf("DOWN: prev=%f; end=%f; curr=%f; dur=%f; t=%f; mul=%f; ydelta=%f", cl->PSpriteWeaponLowerPrev, currSY, prevSY, dur, t, t/dur, ydelta);
        if (prevSY >= currSY) {
          prevSY = currSY;
          cl->PSpriteWeaponLoweringDuration = 0.0f;
        }
      } else {
        prevSY -= ydelta;
        //GCon->Logf("UP: prev=%f; end=%f; curr=%f; dur=%f; t=%f; mul=%f; ydelta=%f", cl->PSpriteWeaponLowerPrev, currSY, prevSY, dur, t, t/dur, ydelta);
        if (prevSY <= currSY) {
          prevSY = currSY;
          cl->PSpriteWeaponLoweringDuration = 0.0f;
        }
      }
    } else {
      prevSY = currSY;
      cl->PSpriteWeaponLoweringDuration = 0.0f;
    }
    //cl->ViewStates[i].SY = prevSY;
    SY = prevSY;
  }

       if (cl->ViewStateOfsX <= -10000.0f) SX += cl->ViewStateOfsX+10000.0f;
  else if (cl->ViewStateOfsX >= 10000.0f) SX += cl->ViewStateOfsX-10000.0f;
  else SX += cl->ViewStateOfsX;
  // fuck you, gshitcc
       if (cl->ViewStateOfsY <= -10000.0f) SY += cl->ViewStateOfsY+10000.0f;
  else if (cl->ViewStateOfsY >= 10000.0f) SY += cl->ViewStateOfsY-10000.0f;
  else SY += cl->ViewStateOfsY;

  //GCon->Logf(NAME_Debug, "weapon offset:(%g,%g):(%g,%g)  flash offset:(%g,%g)", cl->ViewStateOfsX, cl->ViewStateOfsY, SX, SY, cl->ViewStates[PS_FLASH].OfsX, cl->ViewStates[PS_FLASH].OfsY);

  SX += cl->ViewStateBobOfsX;
  SY += cl->ViewStateBobOfsY;

  // calculate base light and fade
  vuint32 baselight;
  if (RendStyle == STYLE_Fuzzy) {
    baselight = 0;
  } else {
    if (ltxr|ltxg|ltxb) {
      baselight = (0xff000000u)|(((vuint32)clampToByte(ltxr))<<16)|(((vuint32)clampToByte(ltxg))<<8)|((vuint32)clampToByte(ltxb));
    } else {
      //const TVec lpos = Drawer->vieworg-TVec(0.0f, 0.0f, cl->MO->Height);
      const TVec lpos = cl->MO->Origin;
      baselight = LightPoint(nullptr, lpos, cl->MO->Radius, cl->MO->Height, r_viewleaf);
    }
  }

  const vuint32 basefase = GetFade(Level->PointRegionLight(r_viewleaf, cl->ViewOrg));

  // draw all active psprites
  for (int ii = 0; ii < NUMPSPRITES; ++ii) {
    const int i = VPSpriteRenderOrder[ii];

    VState *vst = cl->ViewStates[i].State;
    if (!vst) continue;
    //GCon->Logf(NAME_Debug, "PSPRITE #%d is %d: %s", ii, i, *vst->Loc.toStringNoCol());

    const vuint32 light = (RendStyle != STYLE_Fuzzy && (vst->Frame&VState::FF_FULLBRIGHT) ? 0xffffffff : baselight);

    ri.light = ri.seclight = light;
    ri.fade = basefase;

    mdri.light = mdri.seclight = light;
    mdri.fade = ri.fade;

    //GCon->Logf(NAME_Debug, "PSPRITE #%d is %d: sx=%g; sy=%g; ovlsx=%g; ovlsy=%g; %s", ii, i, cl->ViewStates[i].OvlOfsX, cl->ViewStates[i].OvlOfsY, SX, SY, *vst->Loc.toStringNoCol());

    const float nSX = (i != PS_WEAPON ? getVSOffset(SX, cl->ViewStates[i].OvlOfsX) : SX);
    const float nSY = (i != PS_WEAPON ? getVSOffset(SY, cl->ViewStates[i].OvlOfsY) : SY);

    if (!RenderViewModel(&cl->ViewStates[i], nSX, nSY, mdri)) {
      RenderPSprite(nSX, nSY, cl->getMFI(i), 3.0f/*NUMPSPRITES-ii*/, ri);
    }
  }

  r_light_mode.ForceSet(oldRM); // restore lighting mode
}


//==========================================================================
//
//  VRenderLevelShared::RenderCrosshair
//
//==========================================================================
void VRenderLevelShared::RenderCrosshair () {
  // do not show crosshair if player is dead
  if (!cl || !cl->MO || cl->MO->Health <= 0) return;
  int ch = crosshair.asInt();

  // BadApple.wad hack
  if (Level->IsBadApple()) return;

  //GCon->Logf(NAME_Debug, "***xhair*** prevCrosshairPic=%d; currCrosshairPicHandle=%d; pch=%d", prevCrosshairPic, currCrosshairPicHandle, (cl->Crosshair > 0 ? cl->Crosshair+1024 : 0));

  // crosshair override
  if (!crosshair_force.asBool() && cl->Crosshair > 0) {
    if (prevCrosshairPic != cl->Crosshair+1024) {
      int xpich = 0; // handle
      // try to load a custom crosshair
      auto xhp = customXHairHandles.get(cl->Crosshair);
      if (xhp) {
        xpich = *xhp;
      } else {
        if (crosshair_big_prefer.asBool()) {
          xpich = GTextureManager.AddPatch(VName(va("xhairb%d", cl->Crosshair), VName::AddLower8), TEXTYPE_Pic, true, true); //silent, as crosshair
          if (xpich < 0) xpich = 0; else xpich = -xpich;
        }
        if (!xpich) {
          xpich = GTextureManager.AddPatch(VName(va("xhairs%d", cl->Crosshair), VName::AddLower8), TEXTYPE_Pic, true, true); //silent, as crosshair
          if (xpich < 0) xpich = 0;
        }
        if (!xpich && !crosshair_big_prefer.asBool()) {
          xpich = GTextureManager.AddPatch(VName(va("xhairb%d", cl->Crosshair), VName::AddLower8), TEXTYPE_Pic, true, true); //silent, as crosshair
          if (xpich < 0) xpich = 0; else xpich = -xpich;
        }
        customXHairHandles.put(cl->Crosshair, xpich);
        //GCon->Logf(NAME_Debug, "custom xhair %d (handle=%d)", cl->Crosshair, xpich);
      }
      // have pic?
      if (xpich) {
        ch = cl->Crosshair+1024;
        prevCrosshairPic = ch;
        currCrosshairPicHandle = xpich;
        //GCon->Logf(NAME_Debug, "custom xhair %d (handle=%d)", ch, xpich);
      }
    } else {
      ch = cl->Crosshair+1024;
    }
  }

  if (ch <= 0) return;

  float scale = crosshair_scale.asFloat();
  if (!isFiniteF(scale)) scale = 0.0f;
  if (scale <= 0.0f) return;

  float alpha;
  if (ch > 1024) {
    alpha = 1.0f;
  } else {
    alpha = crosshair_alpha.asFloat();
    if (!isFiniteF(alpha)) alpha = 0.0f;
    if (alpha <= 0.0f) return;
  }

  if (!currCrosshairPicHandle || prevCrosshairPic != ch) {
    prevCrosshairPic = ch;
    if (ch < 1024) {
      currCrosshairPicHandle = GTextureManager.AddPatch(VName(va("croshai%x", ch), VName::AddLower8), TEXTYPE_Pic, true); //silent
      if (currCrosshairPicHandle < 0) currCrosshairPicHandle = 0;
    } else {
      currCrosshairPicHandle = 0;
    }
    //GCon->Logf(NAME_Debug, "standard xhair %d (handle=%d)", ch, currCrosshairPicHandle);
  }

  if (currCrosshairPicHandle) {
    int hh = currCrosshairPicHandle;
    if (hh < 0) {
      hh = -hh;
      const float bss = crosshair_big_scale.asFloat();
      if (isFiniteF(bss) && bss > 0.0f) scale *= bss;
    }
    if (alpha > 1.0f) alpha = 1.0f;
    int cy = (screenblocks < 11 ? (VirtualHeight-sb_height)/2 : VirtualHeight/2);
    cy += r_crosshair_yofs;
    bool oldflt = gl_pic_filtering;
    gl_pic_filtering = false;
    R_DrawPicScaled(VirtualWidth/2, cy, hh, scale, alpha);
    gl_pic_filtering = oldflt;
  }

  //GCon->Logf(NAME_Debug, "***DONE xhair*** prevCrosshairPic=%d; currCrosshairPicHandle=%d; pch=%d", prevCrosshairPic, currCrosshairPicHandle, (cl->Crosshair > 0 ? cl->Crosshair+1024 : 0));
}
