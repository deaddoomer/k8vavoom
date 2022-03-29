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
#include "../gamedefs.h"
#include "../psim/p_entity.h"
#include "../psim/p_player.h"
#include "../client/client.h"
#include "r_local.h"


#define SMOOTHSTEP(x) ((x)*(x)*(3.0f-2.0f*(x)))


extern VCvarF gl_alpha_threshold;

static inline float getAlphaThreshold () {
  float res = gl_alpha_threshold;
  if (res < 0.0f) res = 0.0f; else if (res > 1.0f) res = 1.0f;
  return res;
}

static VCvarB gl_dbg_log_model_rendering("gl_dbg_log_model_rendering", false, "Some debug log.", CVAR_PreInit|CVAR_NoShadow);

static VCvarB r_model_autorotating("r_model_autorotating", true, "Allow model autorotation?", CVAR_Archive);
static VCvarB r_model_autobobbing("r_model_autobobbing", true, "Allow model autobobbing?", CVAR_Archive);
static VCvarB r_model_ignore_missing_textures("r_model_ignore_missing_textures", false, "Do not render models with missing textures (if `false`, renders with checkerboards)?", CVAR_Archive|CVAR_NoShadow);


static TMap<VStr, VModel *> GFixedModelMap;


//==========================================================================
//
//  RM_FreeModelRenderer
//
//==========================================================================
void RM_FreeModelRenderer () {
  GFixedModelMap.clear();
}


//==========================================================================
//
//  SprNameFrameToInt
//
//==========================================================================
static inline vuint32 SprNameFrameToInt (VName name, int frame) {
  return (vuint32)name.GetIndex()|((vuint32)frame<<19);
}


//==========================================================================
//
//  PositionModel
//
//==========================================================================
static void PositionModel (TVec &Origin, TAVec &Angles, VMeshModel *wpmodel, int InFrame) {
  wpmodel->LoadFromWad();
  unsigned frame = (unsigned)InFrame;
  if (frame >= (unsigned)wpmodel->Frames.length()) frame = 0;
  TVec p[3];
  /*k8: dunno
  mtriangle_t *ptris = (mtriangle_t *)((vuint8 *)pmdl+pmdl->ofstris);
  mframe_t *pframe = (mframe_t *)((vuint8 *)pmdl+pmdl->ofsframes+frame*pmdl->framesize);
  trivertx_t *pverts = (trivertx_t *)(pframe+1);
  for (int vi = 0; vi < 3; ++vi) {
    p[vi].x = pverts[ptris[0].vertindex[vi]].v[0]*pframe->scale[0]+pframe->scale_origin[0];
    p[vi].y = pverts[ptris[0].vertindex[vi]].v[1]*pframe->scale[1]+pframe->scale_origin[1];
    p[vi].z = pverts[ptris[0].vertindex[vi]].v[2]*pframe->scale[2]+pframe->scale_origin[2];
  }
  */
  const VMeshFrame &frm = wpmodel->Frames[(int)frame];
  for (int vi = 0; vi < 3; ++vi) {
    p[vi].x = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].x;
    p[vi].y = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].y;
    p[vi].z = frm.Verts[wpmodel->Tris[0].VertIndex[vi]].z;
  }
  TVec md_forward(0, 0, 0), md_left(0, 0, 0), md_up(0, 0, 0);
  AngleVectors(Angles, md_forward, md_left, md_up);
  md_left = -md_left;
  Origin += md_forward*p[0].x+md_left*p[0].y+md_up*p[0].z;
  TAVec wangles = VectorAngles(p[1]-p[0]);
  Angles.yaw = AngleMod(Angles.yaw+wangles.yaw);
  Angles.pitch = AngleMod(Angles.pitch+wangles.pitch);
}


//==========================================================================
//
//  UpdateClassFrameCache
//
//==========================================================================
static void UpdateClassFrameCache (VClassModelScript &Cls) {
  if (!Cls.CacheBuilt) {
    for (int i = 0; i < Cls.Frames.length(); ++i) {
      VScriptedModelFrame &frm = Cls.Frames[i];
      frm.nextSpriteIdx = frm.nextNumberIdx = -1;
      if (frm.disabled) continue;
      // sprite cache
      if (frm.sprite != NAME_None) {
        //FIXME: sanity checks
        vassert(frm.frame >= 0 && frm.frame < 4096);
        vassert(frm.sprite.GetIndex() > 0 && frm.sprite.GetIndex() < 524288);
        vuint32 nfi = SprNameFrameToInt(frm.sprite, frm.frame);
        if (!Cls.SprFrameMap.has(nfi)) {
          // new one
          Cls.SprFrameMap.put(nfi, i);
          //GCon->Logf(NAME_Debug, "*NEW sprite frame for '%s': %s %c (%d)", *Cls.Name, *frm.sprite, 'A'+frm.frame, i);
        } else {
          // add to list
          int idx = *Cls.SprFrameMap.get(nfi);
          while (Cls.Frames[idx].nextSpriteIdx != -1) idx = Cls.Frames[idx].nextSpriteIdx;
          Cls.Frames[idx].nextSpriteIdx = i;
          //GCon->Logf(NAME_Debug, "*  more sprite frames for '%s': %s %c (%d)", *Cls.Name, *frm.sprite, 'A'+frm.frame, i);
        }
      }
      // frame number cache
      if (frm.Number >= 0) {
        if (!Cls.NumFrameMap.has(frm.Number)) {
          // new one
          Cls.NumFrameMap.put(frm.Number, i);
        } else {
          // add to list
          int idx = *Cls.NumFrameMap.get(frm.Number);
          while (Cls.Frames[idx].nextNumberIdx != -1) idx = Cls.Frames[idx].nextNumberIdx;
          Cls.Frames[idx].nextNumberIdx = i;
        }
      }
    }
    Cls.CacheBuilt = true;
  }
}


#define FINDFRAME_CHECK_FRM  \
  const VScriptedModelFrame &frm = Cls.Frames[idx]; \
  if (res < 0 || (Cls.Frames[res].ModelIndex == frm.ModelIndex && Cls.Frames[res].SubModelIndex == frm.SubModelIndex)) { \
    if (frm.Inter <= Inter && frm.Inter > bestInter) { res = idx; bestInter = frm.Inter; } \
  }


//==========================================================================
//
//  FindFrame
//
//  returns first frame found
//  note that there can be several frames for one sprite!
//
//==========================================================================
static int FindFrame (VClassModelScript &Cls, const VAliasModelFrameInfo &Frame, float Inter) {
  UpdateClassFrameCache(Cls);

  // try cached frames
  if (Cls.OneForAll) return 0; // guaranteed

  // we have to check both index and frame here, because we don't know which was defined
  // i can preprocess this, but meh, i guess that hashtable+chain is fast enough

  int res = -1;
  float bestInter = -9999.9f;

  //FIXME: reduce pasta!
  if (Frame.sprite != NAME_None && Frame.frame >= 0 && Frame.frame < 4096) {
    // by sprite name
    int *idxp = Cls.SprFrameMap.get(SprNameFrameToInt(Frame.sprite, Frame.frame));
    if (idxp) {
      int idx = *idxp;
      while (idx >= 0) {
        FINDFRAME_CHECK_FRM
        idx = frm.nextSpriteIdx;
      }
      if (res >= 0) return res;
    }
  }

  if (Frame.index >= 0) {
    // by index
    int *idxp = Cls.NumFrameMap.get(Frame.index);
    if (idxp) {
      int idx = *idxp;
      while (idx >= 0) {
        FINDFRAME_CHECK_FRM
        idx = frm.nextNumberIdx;
      }
    }
  }

  return res;
}


//==========================================================================
//
//  FindFrameGZ
//
//  returns first frame found
//  note that there can be several frames for one sprite!
//
//==========================================================================
static int FindFrameGZ (VClassModelScript &Cls, const VAliasModelFrameInfo &Frame,
                        int midx, int smidx)
{
  UpdateClassFrameCache(Cls);

  // try cached frames
  if (Cls.OneForAll) return 0; // guaranteed

  // we have to check both index and frame here, because we don't know which was defined
  // i can preprocess this, but meh, i guess that hashtable+chain is fast enough

  int res = -1;

  //FIXME: reduce pasta!
  if (Frame.sprite != NAME_None && Frame.frame >= 0 && Frame.frame < 4096) {
    // by sprite name
    int *idxp = Cls.SprFrameMap.get(SprNameFrameToInt(Frame.sprite, Frame.frame));
    if (idxp) {
      int idx = *idxp;
      while (idx >= 0) {
        const VScriptedModelFrame &frm = Cls.Frames[idx];
        if (frm.ModelIndex == midx && frm.SubModelIndex == smidx) {
          res = idx;
          break;
        }
        idx = frm.nextSpriteIdx;
      }
      if (res >= 0) return res;
      return *idxp;
    }
  }

  if (Frame.index >= 0) {
    // by index
    int *idxp = Cls.NumFrameMap.get(Frame.index);
    if (idxp) {
      int idx = *idxp;
      while (idx >= 0) {
        const VScriptedModelFrame &frm = Cls.Frames[idx];
        if (frm.ModelIndex == midx && frm.SubModelIndex == smidx) {
          res = idx;
          break;
        }
        idx = frm.nextNumberIdx;
      }
      if (res >= 0) return res;
      return *idxp;
    }
  }

  return 0;
}


#define FINDNEXTFRAME_CHECK_FRM  \
  const VScriptedModelFrame &nfrm = Cls.Frames[nidx]; \
  if (FDef.ModelIndex == nfrm.ModelIndex && FDef.SubModelIndex == nfrm.SubModelIndex && \
      (Cls.isGZDoom || (nfrm.Inter >= FDef.Inter && nfrm.Inter < bestInter))) \
  { \
    res = nidx; \
    if (Cls.isGZDoom) break; \
    bestInter = nfrm.Inter; \
  }


//==========================================================================
//
//  FindNextFrame
//
//==========================================================================
static int FindNextFrame (VClassModelScript &Cls, int FIdx, const VAliasModelFrameInfo &Frame,
                          float Inter, float &InterpFrac)
{
  if (FIdx < 0) { InterpFrac = 0.0f; return -1; } // just in case
  UpdateClassFrameCache(Cls);
  if (Inter < 0.0f) Inter = 0.0f; // just in case

  const VScriptedModelFrame &FDef = Cls.Frames[FIdx];

  int res = -1;
  if (FDef.Inter < 1.0f) {
    // previous code was using `FIdx+1`, and it was wrong
    // just in case, check for valid `Inter`
    // walk the list

    // doesn't finish time slice
    float bestInter = 9999.9f;

    if (FDef.sprite != NAME_None) {
      // by sprite name
      int nidx = FDef.nextSpriteIdx;
      while (nidx >= 0) {
        FINDNEXTFRAME_CHECK_FRM
        nidx = nfrm.nextSpriteIdx;
      }
    } else {
      // by frame index
      int nidx = FDef.nextNumberIdx;
      while (nidx >= 0) {
        FINDNEXTFRAME_CHECK_FRM
        nidx = nfrm.nextNumberIdx;
      }
    }
  }


  // found interframe?
  if (res >= 0) {
    const VScriptedModelFrame &nfrm = Cls.Frames[res];
    if (nfrm.Inter <= FDef.Inter) {
      InterpFrac = 1.0f;
    } else {
      float frc = (Inter-FDef.Inter)/(nfrm.Inter-FDef.Inter);
      if (!isFiniteF(frc)) frc = 1.0f; // just in case
      InterpFrac = frc;
    }
    return res;
  }

  // gozzo hack
  if (Cls.isGZDoom) {
    res = FindFrameGZ(Cls, Frame, FDef.ModelIndex, FDef.SubModelIndex);
    if (res >= 0) {
      InterpFrac = Inter;
      return res;
    }
  }

  // no interframe models found, get normal frame
  InterpFrac = (FDef.Inter >= 0.0f && FDef.Inter < 1.0f ? (Inter-FDef.Inter)/(1.0f-FDef.Inter) : 1.0f);
  return FindFrame(Cls, Frame, 0.0f);
}


//==========================================================================
//
//  lerpvec
//
//==========================================================================
static inline void lerpvec (TVec &a, const TVec b, const float t) noexcept {
  a.x += (b.x-a.x)*t;
  a.y += (b.y-a.y)*t;
  a.z += (b.z-a.z)*t;
}


//==========================================================================
//
//  CheckModelEarlyRejects
//
//==========================================================================
static inline bool CheckModelEarlyRejects (const RenderStyleInfo &ri, ERenderPass Pass) {
  switch (Pass) {
    case RPASS_Normal: // lightmapped renderer is using this
      break;
    case RPASS_Ambient:
      if (ri.isAdditive()) return false;
      if (ri.isTranslucent() && ri.stencilColor) return false; // shaded too
      break;
    case RPASS_ShadowVolumes:
    case RPASS_ShadowMaps:
      if (ri.isTranslucent()) return false;
      break;
    case RPASS_Textures:
      if (ri.isAdditive() || ri.isShaded()) return false;
      break;
    case RPASS_Light:
      if (ri.isAdditive() || ri.isShaded()) return false;
      //if (ri.isTranslucent() && ri.stencilColor) return; // shaded too
      break;
    case RPASS_Fog:
      //FIXME
      if (ri.isAdditive() || ri.isShaded()) return false;
      //if (ri.stencilColor) return;
      break;
    case RPASS_NonShadow: // non-lightmapped renderers are using this for translucent models
      if (!ri.isAdditive() && !ri.isShaded()) return false;
      break;
    case RPASS_Glass:
      if (ri.isTranslucent()) return false;
      break;
    case RPASS_Trans:
      //if (!ri.isTranslucent()) return false;
      break;
  }
  return true;
}


//==========================================================================
//
//  DrawModel
//
//  FIXME: make this faster -- stop looping, cache data!
//
//==========================================================================
static void DrawModel (VLevel *Level, VEntity *mobj, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY, VClassModelScript &Cls, int FIdx, int NFIdx,
  VTextureTranslation *Trans, int ColorMap, int Version,
  const RenderStyleInfo &ri,
  bool IsViewModel, float Inter,
  bool Interpolate, const TVec &LightPos, float LightRadius, ERenderPass Pass, bool isShadowVol)
{
  // some early rejects
  if (!CheckModelEarlyRejects(ri, Pass)) return;

  VScriptedModelFrame &FDef = Cls.Frames[FIdx];
  const int allowedsubmod = FDef.SubModelIndex;
  if (allowedsubmod == -2) return; // this frame is hidden
  VScriptedModelFrame &NFDef = Cls.Frames[NFIdx];
  VScriptModel &ScMdl = Cls.Model->Models[FDef.ModelIndex];

  int submodindex = -1;
  for (auto &&SubMdl : ScMdl.SubModels) {
    ++submodindex;
    if (allowedsubmod >= 0 && submodindex != allowedsubmod) continue; // only one submodel allowed
    if (SubMdl.Version != -1 && SubMdl.Version != Version) continue;
    if (SubMdl.AlphaMul <= 0.0f) continue;

    if (ScMdl.HasAlphaMul && SubMdl.AlphaMul < 1.0f) {
      if (Pass != RPASS_Glass && Pass != RPASS_Normal) continue;
    }

    if (FDef.FrameIndex >= SubMdl.Frames.length()) {
      if (FDef.FrameIndex != 0x7fffffff) {
        GCon->Logf("Bad sub-model frame index %d for model '%s' (class '%s')", FDef.FrameIndex, *ScMdl.Name, *Cls.Name);
        FDef.FrameIndex = 0x7fffffff;
      }
      continue;
    }

    // cannot interpolate between different models or submodels
    if (Interpolate) {
      if (FDef.ModelIndex != NFDef.ModelIndex ||
          FDef.SubModelIndex != NFDef.SubModelIndex ||
          NFDef.FrameIndex >= SubMdl.Frames.length())
      {
        Interpolate = false;
      }
    }

    VScriptSubModel::VFrame &F = SubMdl.Frames[FDef.FrameIndex];
    VScriptSubModel::VFrame &NF = SubMdl.Frames[Interpolate ? NFDef.FrameIndex : FDef.FrameIndex];

    // alpha
    float Md2Alpha = ri.alpha;
    if (ScMdl.HasAlphaMul) {
      if (Pass == RPASS_Glass || Pass == RPASS_Normal) Md2Alpha *= SubMdl.AlphaMul;
    }
    if (FDef.AlphaStart != 1.0f || FDef.AlphaEnd != 1.0f) Md2Alpha *= FDef.AlphaStart+(FDef.AlphaEnd-FDef.AlphaStart)*Inter;
    if (F.AlphaStart != 1.0f || F.AlphaEnd != 1.0f) Md2Alpha *= F.AlphaStart+(F.AlphaEnd-F.AlphaStart)*Inter;
    if (Md2Alpha <= 0.01f) continue;
    if (Md2Alpha > 1.0f) Md2Alpha = 1.0f;
    //Md2Alpha = 0.8f;

    // alpha-blended things should be rendered with "non-shadow" pass
    // `RPASS_Normal` means "lightmapped renderer", it always does it right
    if (Pass != RPASS_Normal) {
      // translucent?
      if (Pass == RPASS_Glass) {
        // render only translucent models here
        if (Md2Alpha >= 1.0f) continue;
      } else if (Pass == RPASS_Trans) {
        //if (Md2Alpha >= 1.0f) continue;
      } else {
        if (Pass != RPASS_NonShadow && Md2Alpha < 1.0f) continue;
      }
    }

    // locate the proper data
    SubMdl.Model->LoadFromWad();
    //FIXME: this should be done earilier
    //!if (SubMdl.Model->HadErrors) SubMdl.NoShadow = true;

    // skin animations
    int Md2SkinIdx = 0;
    if (F.SkinIndex >= 0) {
      Md2SkinIdx = F.SkinIndex;
    } else if (SubMdl.SkinAnimSpeed) {
      Md2SkinIdx = int((Level ? Level->Time : 0)*SubMdl.SkinAnimSpeed)%SubMdl.SkinAnimRange;
    }
    if (Md2SkinIdx < 0) Md2SkinIdx = 0; // just in case

    // get the proper skin texture ID
    int SkinID;
    if (SubMdl.Skins.length()) {
      // skins defined in definition file override all skins in MD2 file
      if (Md2SkinIdx < 0 || Md2SkinIdx >= SubMdl.Skins.length()) {
        if (SubMdl.Skins.length() == 0) Sys_Error("model '%s' has no skins", *SubMdl.Model->Name);
        //if (SubMdl.SkinShades.length() == 0) Sys_Error("model '%s' has no skin shades", *SubMdl.Model->Name);
        Md2SkinIdx = 0;
      }
      SkinID = SubMdl.Skins[Md2SkinIdx].textureId;
      if (SkinID < 0) {
        SkinID = GTextureManager.AddFileTextureShaded(SubMdl.Skins[Md2SkinIdx].fileName, TEXTYPE_Skin, SubMdl.Skins[Md2SkinIdx].shade);
        SubMdl.Skins[Md2SkinIdx].textureId = SkinID;
      }
    } else {
      if (SubMdl.Model->Skins.length() == 0) Sys_Error("model '%s' has no skins", *SubMdl.Model->Name);
      Md2SkinIdx = Md2SkinIdx%SubMdl.Model->Skins.length();
      if (Md2SkinIdx < 0) Md2SkinIdx = (Md2SkinIdx+SubMdl.Model->Skins.length())%SubMdl.Model->Skins.length();
      SkinID = SubMdl.Model->Skins[Md2SkinIdx].textureId;
      if (SkinID < 0) {
        //SkinID = GTextureManager.AddFileTexture(SubMdl.Model->Skins[Md2SkinIdx%SubMdl.Model->Skins.length()], TEXTYPE_Skin);
        SkinID = GTextureManager.AddFileTextureShaded(SubMdl.Model->Skins[Md2SkinIdx].fileName, TEXTYPE_Skin, SubMdl.Model->Skins[Md2SkinIdx].shade);
        SubMdl.Model->Skins[Md2SkinIdx].textureId = SkinID;
      }
    }
    if (SkinID < 0) SkinID = GTextureManager.DefaultTexture;

    // check for missing texture
    if (SkinID == GTextureManager.DefaultTexture) {
      if (r_model_ignore_missing_textures) return;
    }

    VTexture *SkinTex = GTextureManager(SkinID);
    if (!SkinTex || SkinTex->Type == TEXTYPE_Null) return; // do not render models without textures

    // get and verify frame number
    int Md2Frame = F.Index;
    if ((unsigned)Md2Frame >= (unsigned)SubMdl.Model->Frames.length()) {
      if (developer) GCon->Logf(NAME_Dev, "no such frame %d in model '%s'", Md2Frame, *SubMdl.Model->Name);
      Md2Frame = (Md2Frame <= 0 ? 0 : SubMdl.Model->Frames.length()-1);
      // stop further warnings
      F.Index = Md2Frame;
    }

    // get and verify next frame number
    int Md2NextFrame = Md2Frame;
    if (Interpolate) {
      Md2NextFrame = NF.Index;
      if ((unsigned)Md2NextFrame >= (unsigned)SubMdl.Model->Frames.length()) {
        if (developer) GCon->Logf(NAME_Dev, "no such next frame %d in model '%s'", Md2NextFrame, *SubMdl.Model->Name);
        Md2NextFrame = (Md2NextFrame <= 0 ? 0 : SubMdl.Model->Frames.length()-1);
        // stop further warnings
        NF.Index = Md2NextFrame;
      }
    }

    // position
    TVec Md2Org = Org;

    const char *passname = nullptr;
    if (gl_dbg_log_model_rendering) {
      switch (Pass) {
        case RPASS_Normal: passname = "normal"; break;
        case RPASS_Ambient: passname = "ambient"; break;
        case RPASS_ShadowVolumes: passname = "shadow"; break;
        case RPASS_ShadowMaps: passname = "shadowmaps"; break;
        case RPASS_Light: passname = "light"; break;
        case RPASS_Textures: passname = "texture"; break;
        case RPASS_Fog: passname = "fog"; break;
        case RPASS_NonShadow: passname = "nonshadow"; break;
        case RPASS_Glass: passname = "glass"; break;
        case RPASS_Trans: passname = "trans"; break;
        default: Sys_Error("WTF?!");
      }
      GCon->Logf("000: MODEL(%s): class='%s'; model='%s':%d alpha=%f; noshadow=%d; usedepth=%d",
                 passname, *Cls.Name,
                 *SubMdl.Model->Name, submodindex,
                 Md2Alpha, (int)SubMdl.NoShadow, (int)SubMdl.UseDepth);
    }

    switch (Pass) {
      case RPASS_Normal:
        break;
      case RPASS_Ambient:
        //if (ri.isTranslucent() && ri.stencilColor) continue; // already checked
        //if (ri.isAdditive()) continue; // already checked
        break;
      case RPASS_ShadowVolumes:
        if (Md2Alpha < 1.0f || SubMdl.NoShadow || SubMdl.Model->HadErrors) continue;
        break;
      case RPASS_ShadowMaps:
        if (Md2Alpha < 1.0f || SubMdl.NoShadow) continue;
        //if (ri.isTranslucent() && ri.stencilColor) continue; // already checked
        //if (ri.isAdditive()) continue; // already checked
        break;
      case RPASS_Textures:
        //if (Md2Alpha <= getAlphaThreshold() && !ri.isAdditive()) continue; // already checked
        //if (ri.isAdditive()) continue; // already checked
        break;
      case RPASS_Light:
        if (Md2Alpha <= getAlphaThreshold() /*|| SubMdl.NoShadow*/) continue;
        //if (ri.isTranslucent() && ri.stencilColor) continue; // no need to
        //if (ri.isAdditive()) continue; // already checked
        break;
      case RPASS_Fog:
        /*
        // noshadow model is rendered as "noshadow", so it doesn't need fog
        if (Md2Alpha <= getAlphaThreshold() || SubMdl.NoShadow) {
          //if (gl_dbg_log_model_rendering) GCon->Logf("  SKIP FOG FOR MODEL(%s): class='%s'; alpha=%f; noshadow=%d", passname, *Cls.Name, Md2Alpha, (int)SubMdl.NoShadow);
          continue;
        }
        */
        break;
      case RPASS_NonShadow:
        //if (Md2Alpha >= 1.0f && !Additive && !SubMdl.NoShadow) continue;
        //!if (Md2Alpha < 1.0f || ri.isAdditive() /*|| SubMdl.NoShadow*/) continue;
        //if (Md2Alpha >= 1.0f && !ri.isAdditive() /*|| SubMdl.NoShadow*/) continue;
        //if (ri.isTranslucent() && ri.stencilColor) continue;
        //
        //if (!ri.isAdditive()) continue; // already checked
        break;
      case RPASS_Glass:
      case RPASS_Trans:
        break;
    }

    if (gl_dbg_log_model_rendering) GCon->Logf("     MODEL(%s): class='%s'; alpha=%f; noshadow=%d", passname, *Cls.Name, Md2Alpha, (int)SubMdl.NoShadow);

    // angle
    TAVec Md2Angle = Angles;
    // position model
    if (SubMdl.PositionModel) PositionModel(Md2Org, Md2Angle, SubMdl.PositionModel, F.PositionIndex);

    const float smooth_inter = (Interpolate ? SMOOTHSTEP(Inter) : 0.0f);

    AliasModelTrans Transform = F.Transform;

    if (Interpolate && smooth_inter > 0.0f && &F != &NF) {
      if (smooth_inter >= 1.0f) {
        Transform = NF.Transform;
      } else if (Transform.MatTrans != NF.Transform.MatTrans) {
        Transform.DecTrans = Transform.DecTrans.interpolate(NF.Transform.DecTrans, smooth_inter);
        Transform.recompose();
        //FIXME: what to do with RotCenter here?
        // gzdoom-style scale/offset?
        if (Transform.gzdoom || NF.Transform.gzdoom) {
          lerpvec(Transform.gzPreScaleOfs, NF.Transform.gzPreScaleOfs, smooth_inter);
          lerpvec(Transform.gzScale, NF.Transform.gzScale, smooth_inter);
          Transform.gzdoom = true;
        }
        //Transform.TransRot = Transform.MatTrans.getAngles();
      }
    }

    if (!Transform.gzdoom) {
      Transform.MatTrans.scaleXY(ScaleX, ScaleY);
    } else {
      // done in renderer
      Transform.gzScale.x *= ScaleX;
      Transform.gzScale.y *= ScaleX;
      Transform.gzScale.z *= ScaleY;
    }

    if (!IsViewModel) {
      bool pitchFromMom = false;

      const vuint8 rndVal = (mobj ? (hashU32(mobj->ServerUId)>>4)&0xffu : 0);
      /* old code
        if (FDef.AngleStart || FDef.AngleEnd != 1.0f) {
          Md2Angle.yaw = AngleMod(Md2Angle.yaw+FDef.AngleStart+(FDef.AngleEnd-FDef.AngleStart)*Inter);
        }
      */

      Md2Org.z += FDef.zoffset.GetOffset(rndVal);

      Md2Angle.yaw = FDef.angleYaw.GetAngle(Md2Angle.yaw, rndVal);

      if (FDef.AngleStart || FDef.AngleEnd != FDef.AngleStart) {
        Md2Angle.yaw = AngleMod(Md2Angle.yaw+FDef.AngleStart+(FDef.AngleEnd-FDef.AngleStart)*Inter);
      }

      if (FDef.gzActorRoll == MdlAndle_DontUse) {
        Md2Angle.roll = 0.0f;
      } else {
        Md2Angle.roll = FDef.angleRoll.GetAngle(Md2Angle.roll, rndVal);
      }

      if (!mobj || FDef.gzActorPitch == MdlAndle_DontUse) {
        Md2Angle.pitch = 0.0f;
      } else {
        if (FDef.gzActorPitch == MdlAndle_FromMomentum && !mobj->Velocity.isZero()) {
          Md2Angle.pitch = VectorAnglePitch(mobj->Velocity);
          if (FDef.gzActorPitchInverted) Md2Angle.pitch += 180.0f;
          if (Cls.isGZDoom) pitchFromMom = true; // gozzo hack
        }
        Md2Angle.pitch = FDef.anglePitch.GetAngle(Md2Angle.pitch, rndVal);
      }

      if (!pitchFromMom && Level && mobj) {
        if (r_model_autorotating && FDef.rotateSpeed) {
          Md2Angle.yaw = AngleMod(Md2Angle.yaw+Level->Time*FDef.rotateSpeed+rndVal*38.6f);
        }

        if (r_model_autobobbing && FDef.bobSpeed) {
          //GCon->Logf("UID: %3u (%s)", (hashU32(mobj->GetUniqueId())&0xff), *mobj->GetClass()->GetFullName());
          const float bobHeight = 4.0f;
          const float zdelta = msin(AngleMod(Level->Time*FDef.bobSpeed+rndVal*44.5f))*bobHeight;
          Md2Org.z += zdelta+bobHeight;
        }
      }

      // VMatrix4::RotateY: roll
      // VMatrix4::RotateZ: yaw
      // VMatrix4::RotateX: pitch
    }

    // light
    vuint32 Md2Light = ri.light;
    if (SubMdl.FullBright) Md2Light = 0xffffffff;

    switch (Pass) {
      case RPASS_Normal:
      case RPASS_NonShadow:
      case RPASS_Glass:
      case RPASS_Trans:
        if (true /*IsViewModel || !isShadowVol*/) {
          RenderStyleInfo newri = ri;
          newri.light = Md2Light;
          newri.alpha = Md2Alpha;
          if (!newri.translucency && Md2Alpha < 1.0f) newri.translucency = RenderStyleInfo::Translucent;
          if (Pass == RPASS_Glass && !newri.translucency) {
            newri.translucency = RenderStyleInfo::Translucent;
          }
          if (Pass == RPASS_NonShadow) {
            if (!ri.isAdditive() && (ri.isTranslucent() || Md2Alpha < 1.0f)) break;
          }
          if (gl_dbg_log_model_rendering) GCon->Logf("       RENDER!");
          Drawer->DrawAliasModel(Md2Org, Md2Angle, Transform,
            SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
            Trans, ColorMap,
            newri,
            IsViewModel, smooth_inter, Interpolate, SubMdl.UseDepth,
            SubMdl.AllowTransparency,
            (Pass != RPASS_Glass ? (!IsViewModel && isShadowVol) : false)); // for advanced renderer, we need to fill z-buffer, but not color buffer
        }
        break;
      case RPASS_Ambient:
        if (!SubMdl.AllowTransparency) {
          Drawer->DrawAliasModelAmbient(Md2Org, Md2Angle, Transform,
            SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
            Md2Light, Md2Alpha, smooth_inter, Interpolate, SubMdl.UseDepth,
            SubMdl.AllowTransparency);
        }
        break;
      case RPASS_ShadowVolumes:
        Drawer->DrawAliasModelShadow(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, smooth_inter, Interpolate,
          LightPos, LightRadius);
        break;
      case RPASS_ShadowMaps:
        Drawer->DrawAliasModelShadowMap(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
      case RPASS_Light:
        Drawer->DrawAliasModelLight(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
      case RPASS_Textures:
        Drawer->DrawAliasModelTextures(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          Trans, ColorMap, ri, smooth_inter, Interpolate, SubMdl.UseDepth,
          SubMdl.AllowTransparency);
        break;
      case RPASS_Fog:
        Drawer->DrawAliasModelFog(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          ri.fade, Md2Alpha, smooth_inter, Interpolate, SubMdl.AllowTransparency);
        break;
    }
  }
}


//==========================================================================
//
//  VRenderLevelShared::GetClassNameForModel
//
//==========================================================================
VName VRenderLevelShared::GetClassNameForModel (VEntity *mobj) noexcept {
  return
    mobj && mobj->State ?
      (r_models_strict ? mobj->GetClass()->Name : mobj->State->Outer->Name) :
      NAME_None;
}


//==========================================================================
//
//  VRenderLevelShared::IsAliasModelAllowedFor
//
//==========================================================================
bool VRenderLevelShared::IsAliasModelAllowedFor (VEntity *Ent) {
  if (!Ent || !Ent->State || Ent->IsGoingToDie() || !r_models) return false;
  switch (Ent->Classify()) {
    case VEntity::EType::ET_Unknown: return r_models_other;
    case VEntity::EType::ET_Player: return r_models_players;
    case VEntity::EType::ET_Missile: return r_models_missiles;
    case VEntity::EType::ET_Corpse: return r_models_corpses;
    case VEntity::EType::ET_Monster: return r_models_monsters;
    case VEntity::EType::ET_Decoration: return r_models_decorations;
    case VEntity::EType::ET_Pickup: return r_models_pickups;
    default: abort();
  }
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::IsShadowAllowedFor
//
//==========================================================================
bool VRenderLevelShared::IsShadowAllowedFor (VEntity *Ent) {
  if (!Ent || !Ent->State || Ent->IsGoingToDie() || !r_models || !r_model_shadows) return false;
  if (cl && Ent == cl->Camera && !r_camera_player_shadows) return false;
  switch (Ent->Classify()) {
    case VEntity::EType::ET_Unknown: return r_shadows_other;
    case VEntity::EType::ET_Player: return r_shadows_players;
    case VEntity::EType::ET_Missile: return r_shadows_missiles;
    case VEntity::EType::ET_Corpse: return r_shadows_corpses;
    case VEntity::EType::ET_Monster: return r_shadows_monsters;
    case VEntity::EType::ET_Decoration: return r_shadows_decorations;
    case VEntity::EType::ET_Pickup: return r_shadows_pickups;
    default: abort();
  }
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::HasEntityAliasModel
//
//==========================================================================
bool VRenderLevelShared::HasEntityAliasModel (VEntity *Ent) const {
  return (IsAliasModelAllowedFor(Ent) && RM_FindClassModelByName(GetClassNameForModel(Ent)));
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//  this is used to draw so-called "fixed model"
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (VEntity *mobj, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY, VModel *Mdl,
  const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
  VTextureTranslation *Trans, int Version,
  const RenderStyleInfo &ri,
  bool IsViewModel, float Inter, bool Interpolate,
  ERenderPass Pass)
{
  //if (!IsViewModel && !IsAliasModelAllowedFor(mobj)) return false;
  int FIdx = FindFrame(*Mdl->DefaultClass, Frame, Inter);
  if (FIdx == -1) return false;
  float InterpFrac;
  int NFIdx = FindNextFrame(*Mdl->DefaultClass, FIdx, NextFrame, Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = FIdx;
    Interpolate = false;
  }
  DrawModel(Level, mobj, Org, Angles, ScaleX, ScaleY, *Mdl->DefaultClass, FIdx,
    NFIdx, Trans, ColorMap, Version, ri, /*Light, Fade, Alpha, Additive,*/
    IsViewModel, InterpFrac, Interpolate, CurrLightPos, CurrLightRadius,
    Pass, IsShadowVolumeRenderer() && !IsShadowMapRenderer());
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//  this is used to draw entity models
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (VEntity *mobj, VName clsName, const TVec &Org, const TAVec &Angles,
  float ScaleX, float ScaleY,
  const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame, //old:VState *State, VState *NextState,
  VTextureTranslation *Trans, int Version,
  const RenderStyleInfo &ri,
  bool IsViewModel, float Inter, bool Interpolate,
  ERenderPass Pass)
{
  if (clsName == NAME_None) return false;
  if (!IsViewModel && !IsAliasModelAllowedFor(mobj)) return false;

  VClassModelScript *Cls = RM_FindClassModelByName(clsName);
  if (!Cls) {
    //GCon->Logf(NAME_Debug, "NO VIEW MODEL for class `%s`", *clsName);
    return false;
  }

  int FIdx = FindFrame(*Cls, Frame, Inter);
  if (FIdx == -1) {
    /*
    GCon->Logf(NAME_Debug, "NO VIEW MODEL for class `%s`: %s", *clsName, *Frame.toString());
    GCon->Logf(NAME_Debug, "  MFI: %s", *mobj->getMFI().toString());
    GCon->Logf(NAME_Debug, "  NEXT MFI: %s", *mobj->getNextMFI().toString());
    */
    //abort();
    return false;
  }

  #if 0
  GCon->Logf(NAME_Debug, "***FOUND view model for class `%s` (fidx=%d): %s", *clsName, FIdx, *Frame.toString());
  GCon->Logf(NAME_Debug, "  State: %s", *mobj->State->Loc.toStringNoCol());
  GCon->Logf(NAME_Debug, "  MFI: %s", *mobj->getMFI().toString());
  GCon->Logf(NAME_Debug, "  NEXT MFI: %s", *mobj->getNextMFI().toString());
  #endif

  // note that gzdoom-imported modeldef can have more than one model attached to one frame
  // process all attachments -- they should differ by model or submodel indices

  const bool origInterp = Interpolate;
  while (FIdx >= 0) {
    float InterpFrac;
    int NFIdx = FindNextFrame(*Cls, FIdx, NextFrame, Inter, InterpFrac);
    if (NFIdx == -1) {
      NFIdx = FIdx;
      Interpolate = false;
    } else {
      Interpolate = origInterp;
    }

    #if 0
    GCon->Logf(NAME_Debug, "xxx: %s: midx=%d; smidx=%d; inter=%g (%g); FIdx=%d; NFIdx=%d; Interpolate=%s",
               *Cls->Name, Cls->Frames[FIdx].ModelIndex, Cls->Frames[FIdx].SubModelIndex,
               Inter, InterpFrac, FIdx, NFIdx, (Interpolate ? "tan" : "ona"));
    #endif
    DrawModel(Level, mobj, Org, Angles, ScaleX, ScaleY, *Cls, FIdx, NFIdx, Trans,
      ColorMap, Version, ri, IsViewModel,
      InterpFrac, Interpolate, CurrLightPos, CurrLightRadius, Pass,
      IsShadowVolumeRenderer() && !IsShadowMapRenderer());

    // try next one
    const VScriptedModelFrame &cfrm = Cls->Frames[FIdx];
    int res = -1;
    if (cfrm.sprite != NAME_None) {
      // by sprite name
      //if (IsViewModel) GCon->Logf(NAME_Debug, "000: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g); nidx=%d", *Cls->Name, *cfrm.sprite, 'A'+cfrm.frame, cfrm.ModelIndex, cfrm.SubModelIndex, Inter, cfrm.Inter, FIdx);
      FIdx = cfrm.nextSpriteIdx;
      #if 0
      GCon->Logf(NAME_Debug, "001: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g); nidx=%d", *Cls->Name, *cfrm.sprite, 'A'+cfrm.frame, cfrm.ModelIndex, cfrm.SubModelIndex, Inter, cfrm.Inter, FIdx);
      #endif
      while (FIdx >= 0) {
        const VScriptedModelFrame &nfrm = Cls->Frames[FIdx];
        #if 0
        GCon->Logf(NAME_Debug, "  002: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g) (FIdx=%d)", *Cls->Name, *nfrm.sprite, 'A'+nfrm.frame, nfrm.ModelIndex, nfrm.SubModelIndex, Inter, nfrm.Inter, FIdx);
        #endif
        if (cfrm.ModelIndex != nfrm.ModelIndex || cfrm.SubModelIndex != nfrm.SubModelIndex) {
          if (nfrm.Inter <= Inter) {
            res = FIdx;
            // gozzo hack
            if (Cls->isGZDoom) break;
          } else if (nfrm.Inter > Inter) {
            break; // the author shouldn't write incorrect defs
          }
        }
        FIdx = nfrm.nextSpriteIdx;
      }
    } else {
      // by frame index
      FIdx = cfrm.nextNumberIdx;
      while (FIdx >= 0) {
        const VScriptedModelFrame &nfrm = Cls->Frames[FIdx];
        if (cfrm.ModelIndex != nfrm.ModelIndex || cfrm.SubModelIndex != nfrm.SubModelIndex) {
          if (nfrm.Inter <= Inter) {
            res = FIdx;
            // gozzo hack
            if (Cls->isGZDoom) break;
          } else if (nfrm.Inter > Inter) {
            break; // the author shouldn't write incorrect defs
          }
        }
        FIdx = nfrm.nextNumberIdx;
      }
    }
    FIdx = res;
  }

  return true;
}


//==========================================================================
//
//  FindFixedModelFor
//
//==========================================================================
static VModel *FindFixedModelFor (VEntity *Ent, bool verbose) {
  vassert(Ent);
  auto mpp = GFixedModelMap.get(Ent->FixedModelName);
  if (mpp) return *mpp;
  // first time
  VStr fname = VStr("models/")+Ent->FixedModelName;
  if (!FL_FileExists(fname)) {
    if (verbose) GCon->Logf(NAME_Warning, "Can't find alias model '%s'", *Ent->FixedModelName);
    GFixedModelMap.put(Ent->FixedModelName, nullptr);
    return nullptr;
  } else {
    VModel *mdl = Mod_FindName(fname);
    GFixedModelMap.put(Ent->FixedModelName, mdl);
    return mdl;
  }
}


//==========================================================================
//
//  VRenderLevelShared::IsTranslucentEntityModel
//
//  returns `true` if this model should be queued as translucent
//
//==========================================================================
bool VRenderLevelShared::IsTranslucentEntityModel (VEntity *Ent, const RenderStyleInfo &ri,
                                                   float Inter)
{
  if (!Ent || (Ent->EntityFlags&VEntity::EF_FixedModel)) return false;

  VClassModelScript *Cls = RM_FindClassModelByName(VRenderLevelShared::GetClassNameForModel(Ent));
  if (!Cls) return false;

  return Cls->asTranslucent;
}


//==========================================================================
//
//  VRenderLevelShared::DrawEntityModel
//
//==========================================================================
bool VRenderLevelShared::DrawEntityModel (VEntity *Ent, const RenderStyleInfo &ri, float Inter, ERenderPass Pass) {
  //VState *DispState = (Ent->EntityFlags&VEntity::EF_UseDispState ? Ent->DispState : Ent->State);
  //VState *DispState = Ent->State; //FIXME: skipframes

  // movement interpolation
  TVec sprorigin = Ent->GetDrawOrigin();

  // check if we want to interpolate model frames
  const bool Interpolate = r_interpolate_frames;
  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    VModel *Mdl = FindFixedModelFor(Ent, true); // verbose
    if (!Mdl) return false;
    return DrawAliasModel(Ent, sprorigin,
      Ent->/*Angles*/GetModelDrawAngles(), Ent->ScaleX, Ent->ScaleY, Mdl,
      Ent->getMFI(), Ent->getNextMFI(),
      GetTranslation(Ent->Translation),
      Ent->ModelVersion, ri, false, Inter,
      Interpolate, Pass);
  } else {
    return DrawAliasModel(Ent, GetClassNameForModel(Ent), sprorigin,
      Ent->/*Angles*/GetModelDrawAngles(), Ent->ScaleX, Ent->ScaleY,
      Ent->getMFI(), Ent->getNextMFI(),
      GetTranslation(Ent->Translation), Ent->ModelVersion,
      ri, false, Inter, Interpolate, Pass);
  }
}


//==========================================================================
//
//  VRenderLevelShared::CheckAliasModelFrame
//
//==========================================================================
bool VRenderLevelShared::CheckAliasModelFrame (VEntity *Ent, float Inter) {
  if (!Ent->State) return false;
  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    VModel *Mdl = FindFixedModelFor(Ent, false); // silent
    if (!Mdl) return false;
    return FindFrame(*Mdl->DefaultClass, Ent->getMFI(), Inter) != -1;
  } else {
    //VClassModelScript *Cls = FindClassModelByName(Ent->State->Outer->Name);
    VClassModelScript *Cls = RM_FindClassModelByName(GetClassNameForModel(Ent));
    if (!Cls) return false;
    return (FindFrame(*Cls, Ent->getMFI(), Inter) != -1);
  }
}


//==========================================================================
//
//  R_DrawModelFrame
//
//  used only in UI, for model selector
//
//==========================================================================
void R_DrawModelFrame (const TVec &Origin, float Angle, VModel *Model,
  int Frame, int NextFrame,
  //const VAliasModelFrameInfo &Frame, const VAliasModelFrameInfo &NextFrame,
  const char *Skin, int TranslStart,
  int TranslEnd, int Color, float Inter)
{
  (void)Origin;
  (void)Angle;
  (void)Model;
  (void)Frame;
  (void)NextFrame;
  (void)Skin;
  (void)TranslStart;
  (void)TranslEnd;
  (void)Color;
  (void)Inter;

  //FIXME!
  /*
  bool Interpolate = true;
  int FIdx = FindFrame(*Model->DefaultClass, Frame, Inter);
  if (FIdx == -1) return;

  float InterpFrac;
  int NFIdx = FindNextFrame(*Model->DefaultClass, FIdx, NextFrame, Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = 0;
    Interpolate = false;
  }

  Drawer->viewangles.yaw = 180;
  Drawer->viewangles.pitch = 0;
  Drawer->viewangles.roll = 0;
  AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
  Drawer->vieworg = TVec(0, 0, 0);

  refdef_t rd;

  rd.x = 0;
  rd.y = 0;
  rd.width = Drawer->getWidth();
  rd.height = Drawer->getHeight();
  rd.fovx = tan(DEG2RADF(90)*0.5f);
  rd.fovy = rd.fovx*3.0f/4.0f;
  rd.drawworld = false;
  rd.DrawCamera = false;

  Drawer->SetupView(nullptr, &rd);
  Drawer->SetupViewOrg();

  TAVec Angles;
  Angles.yaw = Angle;
  Angles.pitch = 0;
  Angles.roll = 0;

  DrawModel(nullptr, nullptr, Origin, Angles, 1.0f, 1.0f, *Model->DefaultClass, FIdx,
    NFIdx, R_GetCachedTranslation(R_SetMenuPlayerTrans(TranslStart,
    TranslEnd, Color), nullptr), 0, 0, 0xffffffff, 0, 1.0f, false, false,
    InterpFrac, Interpolate, TVec(), 0, RPASS_Normal, true); // force draw

  Drawer->EndView();
  */
}


//==========================================================================
//
//  R_DrawStateModelFrame
//
//  called from UI widget only
//
//==========================================================================
bool R_DrawStateModelFrame (VState *State, VState *NextState, float Inter,
                            const TVec &Origin, float Angle)
{
  bool Interpolate = true;
  if (!State) return false;
  VClassModelScript *Cls = RM_FindClassModelByName(State->Outer->Name);
  if (!Cls) return false;
  int FIdx = FindFrame(*Cls, State->getMFI(), Inter);
  if (FIdx == -1) return false;
  float InterpFrac;
  int NFIdx = FindNextFrame(*Cls, FIdx, (NextState ? NextState->getMFI() : State->getMFI()), Inter, InterpFrac);
  if (NFIdx == -1) {
    NFIdx = 0;
    Interpolate = false;
  }

  Drawer->viewangles.yaw = 180;
  Drawer->viewangles.pitch = 0;
  Drawer->viewangles.roll = 0;
  AngleVectors(Drawer->viewangles, Drawer->viewforward, Drawer->viewright, Drawer->viewup);
  Drawer->vieworg = TVec(0, 0, 0);

  refdef_t rd;

  rd.x = 0;
  rd.y = 0;
  rd.width = Drawer->getWidth();
  rd.height = Drawer->getHeight();
  TClipBase::CalcFovXY(&rd.fovx, &rd.fovy, rd.width, rd.height, 90.0f, PixelAspect);
  rd.drawworld = false;
  rd.DrawCamera = false;

  Drawer->SetupView(nullptr, &rd);
  Drawer->SetupViewOrg();

  TAVec Angles;
  Angles.yaw = Angle;
  Angles.pitch = 0;
  Angles.roll = 0;

  RenderStyleInfo ri;
  ri.light = 0xffffffffu;
  ri.fade = 0;
  ri.alpha = 1.0f;
  ri.translucency = RenderStyleInfo::Normal;
  DrawModel(nullptr, nullptr, Origin, Angles, 1.0f, 1.0f, *Cls, FIdx, NFIdx, nullptr, 0, 0,
    ri, //0xffffffff, 0, 1.0f, false,
    false, InterpFrac, Interpolate, TVec(), 0, RPASS_Normal, true); // force draw

  Drawer->EndView();
  return true;
}


//static VClass *bulbClass = nullptr;


//==========================================================================
//
// R_DrawLightBulb
//
//==========================================================================
void R_DrawLightBulb (const TVec &Org, const TAVec &Angles, vuint32 rgbLight, ERenderPass Pass, bool isShadowVol, float ScaleX, float ScaleY) {
  /*
  if (!bulbClass) {
    bulbClass = VClass:FindClass("K8DebugLightBulb");
    if (!bulbClass) return;
  }
  */

  VClassModelScript *Cls = RM_FindClassModelByName("K8DebugLightBulb");
  if (!Cls) return;

  VAliasModelFrameInfo Frame;
  Frame.sprite = NAME_None;
  Frame.frame = 0;
  Frame.index = 0;
  Frame.spriteIndex = 0;

  int FIdx = FindFrame(*Cls, Frame, 0.0f);
  if (FIdx == -1) return;

  rgbLight |= 0xff000000u;

  RenderStyleInfo ri;
  ri.light = rgbLight;
  ri.fade = 0;
  ri.alpha = 1.0f;
  ri.translucency = RenderStyleInfo::Normal;

  DrawModel(nullptr, nullptr, Org, Angles, ScaleX, ScaleY, *Cls, FIdx, FIdx, nullptr/*translation*/,
    0/*colormap*/, 0/*version*/, ri, false/*isviewmodel*/,
    0.0f/*interpfrac*/, false/*interpolate*/, TVec()/*currlightpos*/, 0/*currlightradius*/,
    Pass, isShadowVol);
}
