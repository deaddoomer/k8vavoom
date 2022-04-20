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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
//**  Switches, buttons. Two-state animation. Exits.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../sound/sound.h"
#include "p_levelinfo.h"
#include "p_entity.h"
#include "../utils/ntvalueioex.h"


#define BUTTONTIME  (1.0f) /* 1 second */


enum EBWhere {
  SWITCH_Top,
  SWITCH_Middle,
  SWITCH_Bottom
};


static TMapNC<int, TSwitch *> switchTextures;
static bool switchTexturesInited = false;


//==========================================================================
//
//  InitSwitchTextureCache
//
//==========================================================================
void InitSwitchTextureCache () noexcept {
  if (!switchTexturesInited) {
    switchTexturesInited = true;
    //GCon->Logf(NAME_Debug, "initializing switch texture cache...");
    for (TSwitch *sw : Switches) {
      if (sw->Tex > 0) switchTextures.put(sw->Tex, sw);
    }
  }
}


//**************************************************************************
//
// change the texture of a wall switch to its opposite
//
//**************************************************************************
class VThinkButton : public VThinker {
  DECLARE_CLASS(VThinkButton, VThinker, 0)
  NO_DEFAULT_CONSTRUCTOR(VThinkButton)

  vint32 Side;
  vuint8 Where;
  vint32 SwitchDef;
  vint32 Frame;
  float Timer;
  VName DefaultSound;
  vuint8 UseAgain; // boolean
  vint32 tbversion; // v1 stores more data
  VTextureID SwitchDefTexture;
  TVec ActivateSoundOrg;
  enum {
    Flag_SoundOrgSet = 1u<<0,
  };
  vuint32 Flags;

  virtual void SerialiseOther (VStream &) override;
  virtual void Tick (float) override;
  bool AdvanceFrame ();
};


IMPLEMENT_CLASS(V, ThinkButton)


//==========================================================================
//
//  VLevelInfo::IsSwitchTexture
//
//==========================================================================
bool VLevelInfo::IsSwitchTexture (int texid) {
  if (texid <= 0) return false;
  InitSwitchTextureCache();
  /*
  for (TSwitch *sw : Switches) {
    if (sw->Tex > 0 && sw->Tex == texid) return true;
  }
  return false;
  */
  auto pp = switchTextures.get(texid);
  return !!pp;
}


//==========================================================================
//
//  CalcSoundOrigin
//
//==========================================================================
static bool CalcSoundOrigin (TVec *outsndorg, const line_t *line, VEntity *activator, const sector_t *sec) {
  if (!line) {
    if (outsndorg) *outsndorg = TVec(INFINITY, INFINITY, INFINITY);
    return false;
  }
  // for polyobjects, use polyobject anchor
  //FIXME: start polyobject sequence instead?
  const polyobj_t *po = line->pobj();
  if (po) {
    if (outsndorg) *outsndorg = po->startSpot;
    return true;
  }
  // use center of the line, and activator/sector z
  TVec sndorg = ((*line->v1)+(*line->v2))*0.5f; // center of the line
  if (activator) {
    // use activator z position
    sndorg.z = activator->Origin.z+max2(0.0f, activator->Height*0.5f);
  } else if (sec) {
    // use sector floor
    sndorg.z = (sec->floor.minz+sec->floor.maxz)*0.5f+8.0f;
  } else {
    sndorg.z = 0.0f; // just in case
  }
  if (outsndorg) *outsndorg = sndorg;
  return true;
}


//==========================================================================
//
//  VLevelInfo::ChangeSwitchTexture
//
//  Function that changes wall texture.
//  Tell it if switch is ok to use again (1=yes, it's a button).
//
//==========================================================================
bool VLevelInfo::ChangeSwitchTexture (line_t *line, VEntity *activator, int sidenum, bool useAgain, VName DefaultSound, bool &Quest, const TVec *org) {
  Quest = false;
  if (sidenum < 0 || sidenum >= XLevel->NumSides) return false;
  InitSwitchTextureCache();

  TSwitch **swpp = nullptr;
  EBWhere where = SWITCH_Top;
  do {
    // top?
    swpp = switchTextures.get(XLevel->Sides[sidenum].TopTexture);
    if (swpp) {
      where = SWITCH_Top;
      XLevel->Sides[sidenum].TopTexture = (*swpp)->Frames[0].Texture;
      break;
    }
    // middle?
    swpp = switchTextures.get(XLevel->Sides[sidenum].MidTexture);
    if (swpp) {
      where = SWITCH_Middle;
      XLevel->Sides[sidenum].MidTexture = (*swpp)->Frames[0].Texture;
      break;
    }
    // bottom?
    swpp = switchTextures.get(XLevel->Sides[sidenum].BottomTexture);
    if (swpp) {
      where = SWITCH_Bottom;
      XLevel->Sides[sidenum].BottomTexture = (*swpp)->Frames[0].Texture;
      break;
    }
  } while (0);

  if (!swpp) return false;

  TSwitch *sw = *swpp;
  vassert(sw);

  TVec outsndorg(INFINITY, INFINITY, INFINITY);
  bool calcSoundOrigin = true;

  bool PlaySound = true;
  if (useAgain || sw->NumFrames > 1) {
    PlaySound = StartButton(line, activator, sidenum, where, sw->InternalIndex, DefaultSound, useAgain, outsndorg);
    calcSoundOrigin = false;
  }

  if (PlaySound) {
    if (calcSoundOrigin) (void)CalcSoundOrigin(&outsndorg, line, activator, XLevel->Sides[sidenum].Sector);
    sector_t *sec = XLevel->Sides[sidenum].Sector;
    const int sndid = (sw->Sound ? sw->Sound : GSoundManager->GetSoundID(DefaultSound));
    if (org) {
      SectorStartSound(sec, sndid, 0/*channel*/, 1.0f/*volume*/, 1.0f/*attenuation*/, org);
    } else if (isFiniteF(outsndorg.x)) {
      SectorStartSound(sec, sndid, 0/*channel*/, 1.0f/*volume*/, 1.0f/*attenuation*/, &outsndorg);
    } else {
      SectorStartSound(sec, sndid, 0/*channel*/, 1.0f/*volume*/, 1.0f/*attenuation*/);
    }
  }

  Quest = sw->Quest;
  return true;
}


//==========================================================================
//
//  VLevelInfo::StartButton
//
//  start a button counting down till it turns off
//  FIXME: make this faster!
//
//==========================================================================
bool VLevelInfo::StartButton (line_t *line, VEntity *activator, int sidenum, vuint8 w, int SwitchDef, VName DefaultSound, bool UseAgain, TVec &outsndorg) {
  outsndorg.x = outsndorg.y = outsndorg.z = INFINITY;
  if (sidenum < 0 || sidenum >= XLevel->NumSides) return false;

  // see if button is already pressed
  for (TThinkerIterator<VThinkButton> Btn(XLevel); Btn; ++Btn) {
    if (Btn->Side == sidenum) {
      // force advancing to the next frame
      Btn->Timer = 0.001f;
      if (Btn->Flags&VThinkButton::Flag_SoundOrgSet) outsndorg = Btn->ActivateSoundOrg;
      return false;
    }
  }

  VThinkButton *But = (VThinkButton *)XLevel->SpawnThinker(VThinkButton::StaticClass());
  if (!But) return false;
  if (But->IsA(VThinkButton::StaticClass())) {
    But->Side = sidenum;
    But->Where = w;
    But->SwitchDef = SwitchDef;
    But->Frame = -1;
    But->DefaultSound = DefaultSound;
    But->UseAgain = (UseAgain ? 1 : 0);
    But->tbversion = 1;
    But->SwitchDefTexture = Switches[SwitchDef]->Tex;
    But->AdvanceFrame();
    TVec sndorg;
    if (CalcSoundOrigin(&sndorg, line, activator, XLevel->Sides[sidenum].Sector)) {
      But->Flags = VThinkButton::Flag_SoundOrgSet;
      #if 0
      GCon->Logf(NAME_Debug, "%s: sndorg=(%g,%g,%g); sectorsndorg=(%g,%g,%g)",
        (activator ? activator->GetClass()->GetName() : "<none>"),
        sndorg.x, sndorg.y, sndorg.z,
        XLevel->Sides[sidenum].Sector->soundorg.x, XLevel->Sides[sidenum].Sector->soundorg.y, XLevel->Sides[sidenum].Sector->soundorg.z);
      #endif
      But->ActivateSoundOrg = sndorg;
      outsndorg = sndorg;
    } else {
      But->Flags = 0u;
    }
    return true;
  } else {
    But->DestroyThinker(); // just in case
    return false;
  }
}


//==========================================================================
//
//  VThinkButton::SerialiseOther
//
//==========================================================================
void VThinkButton::SerialiseOther (VStream &Strm) {
  Super::SerialiseOther(Strm);
  if (tbversion == 1) {
    VNTValueIOEx vio(&Strm);
    vio.io(VName("switch.texture"), SwitchDefTexture);
    if (Strm.IsLoading()) {
      bool found = false;
      for (int idx = Switches.length()-1; idx >= 0; --idx) {
        TSwitch *sw = Switches[idx];
        if (sw->Tex == SwitchDefTexture) {
          found = true;
          if (idx != SwitchDef) {
            GCon->Logf(NAME_Warning, "switch index changed from %d to %d (this may break the game!)", SwitchDef, idx);
          } else {
            //GCon->Logf("*** switch index %d found!", SwitchDef);
          }
          SwitchDef = idx;
          break;
        }
      }
      if (!found) {
        GCon->Logf(NAME_Error, "switch index for old index %d not found (this WILL break the game!)", SwitchDef);
        SwitchDef = -1;
      }
    }
  } else {
    GCon->Log(NAME_Warning, "*** old switch data found in save, this may break the game!");
  }
}


//==========================================================================
//
//  VThinkButton::Tick
//
//==========================================================================
void VThinkButton::Tick (float DeltaTime) {
  if (DeltaTime <= 0.0f) return;
  // do buttons
  Timer -= DeltaTime;
  if (Timer <= 0.0f) {
    if (SwitchDef >= 0 && SwitchDef < Switches.length()) {
      TSwitch *Def = Switches[SwitchDef];
      if (Frame == Def->NumFrames-1) {
        SwitchDef = Def->PairIndex;
        Def = Switches[Def->PairIndex];
        Frame = -1;
        sector_t *sec = XLevel->Sides[Side].Sector;
        const int sndid = (Def->Sound ? Def->Sound : GSoundManager->GetSoundID(DefaultSound));
        if (Flags&Flag_SoundOrgSet) {
          // make "deactivate" sound at the exact same place as "activate" sound
          Level->SectorStartSound(sec, sndid, 0/*channel*/, 1.0f/*volume*/, 1.0f/*attenuation*/, &ActivateSoundOrg);
        } else {
          Level->SectorStartSound(sec, sndid, 0/*channel*/, 1.0f/*volume*/, 1.0f/*attenuation*/);
        }
        UseAgain = 0;
      }

      bool KillMe = AdvanceFrame();
      if (Side >= 0 && Side < XLevel->NumSides) {
        if (Where == SWITCH_Middle) {
          XLevel->Sides[Side].MidTexture = Def->Frames[Frame].Texture;
        } else if (Where == SWITCH_Bottom) {
          XLevel->Sides[Side].BottomTexture = Def->Frames[Frame].Texture;
        } else {
          // texture_top
          XLevel->Sides[Side].TopTexture = Def->Frames[Frame].Texture;
        }
      }
      if (KillMe) DestroyThinker();
    } else {
      UseAgain = 0;
      DestroyThinker();
    }
  }
}


//==========================================================================
//
//  VThinkButton::AdvanceFrame
//
//==========================================================================
bool VThinkButton::AdvanceFrame () {
  ++Frame;
  bool Ret = false;
  TSwitch *Def = Switches[SwitchDef];
  if (Frame == Def->NumFrames-1) {
    if (UseAgain) {
      Timer = BUTTONTIME;
    } else {
      Ret = true;
    }
  } else {
    if (Def->Frames[Frame].RandomRange) {
      Timer = (Def->Frames[Frame].BaseTime+FRandom()*Def->Frames[Frame].RandomRange)/35.0f;
    } else {
      Timer = Def->Frames[Frame].BaseTime/35.0f;
    }
  }
  return Ret;
}
