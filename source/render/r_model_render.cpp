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
  if (!Cls.frameCacheBuilt) {
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
    Cls.frameCacheBuilt = true;
  }
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
static void DrawModel (const VRenderLevelShared::MdlDrawInfo &nfo,
                       ERenderPass Pass, bool isShadowVol)
{
  // some early rejects
  if (!CheckModelEarlyRejects(nfo.ri, Pass)) return;

  VScriptedModelFrame &FDef = nfo.Cls->Frames[nfo.FIdx];
  const int allowedsubmod = FDef.SubModelIndex;
  if (allowedsubmod == -2) return; // this frame is hidden
  const VScriptedModelFrame &NFDef = nfo.Cls->Frames[nfo.NFIdx];
  VScriptModel &ScMdl = nfo.Cls->Model->Models[FDef.ModelIndex];

  bool DoInterp = nfo.Interpolate;
  // cannot interpolate between different models or submodels
  if (DoInterp) {
    if (FDef.ModelIndex != NFDef.ModelIndex ||
        FDef.SubModelIndex != NFDef.SubModelIndex /*||
        NFDef.FrameIndex >= SubMdl.Frames.length()*/)
    {
      DoInterp = false;
    }
  }

  int submodindex = -1;
  for (auto &&SubMdl : ScMdl.SubModels) {
    ++submodindex;
    if (allowedsubmod >= 0 && submodindex != allowedsubmod) continue; // only one submodel allowed
    if (SubMdl.Version != -1 && SubMdl.Version != nfo.Version) continue;
    if (SubMdl.AlphaMul <= 0.0f) continue;

    if (ScMdl.HasAlphaMul && SubMdl.AlphaMul < 1.0f) {
      if (Pass != RPASS_Glass && Pass != RPASS_Normal) continue;
    }

    if (FDef.FrameIndex >= SubMdl.Frames.length()) {
      if (FDef.FrameIndex != 0x7fffffff) {
        GCon->Logf("Bad sub-model frame index %d for model '%s' (class '%s')", FDef.FrameIndex, *ScMdl.Name, *nfo.Cls->Name);
        FDef.FrameIndex = 0x7fffffff;
      }
      continue;
    }

    // can interpolate voxels?
    if (DoInterp && SubMdl.Model->isVoxel && !r_interpolate_frames_voxels) {
      //GCon->Logf("DISABLED interpolation for voxel sub-model frame index %d for model '%s' (class '%s')", FDef.FrameIndex, *ScMdl.Name, *nfo.Cls->Name);
      DoInterp = false;
    }

    // cannot interpolate between different models or submodels
    if (DoInterp) {
      if (NFDef.FrameIndex >= SubMdl.Frames.length()) {
        DoInterp = false;
      }
    }

    VScriptSubModel::VFrame &F = SubMdl.Frames[FDef.FrameIndex];
    VScriptSubModel::VFrame &NF = SubMdl.Frames[DoInterp ? NFDef.FrameIndex : FDef.FrameIndex];

    // alpha
    float Md2Alpha = nfo.ri.alpha;
    if (ScMdl.HasAlphaMul) {
      if (Pass == RPASS_Glass || Pass == RPASS_Normal) Md2Alpha *= SubMdl.AlphaMul;
    }
    if (FDef.AlphaStart != 1.0f || FDef.AlphaEnd != 1.0f) {
      Md2Alpha *= FDef.AlphaStart+(FDef.AlphaEnd-FDef.AlphaStart)*nfo.Inter;
    }
    if (F.AlphaStart != 1.0f || F.AlphaEnd != 1.0f) {
      Md2Alpha *= F.AlphaStart+(F.AlphaEnd-F.AlphaStart)*nfo.Inter;
    }
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
    } else if (SubMdl.SkinAnimSpeed && SubMdl.SkinAnimRange > 1) {
      Md2SkinIdx = int((nfo.Level ? nfo.Level->Time : 0)*SubMdl.SkinAnimSpeed)%SubMdl.SkinAnimRange;
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
    if (DoInterp) {
      Md2NextFrame = NF.Index;
      if ((unsigned)Md2NextFrame >= (unsigned)SubMdl.Model->Frames.length()) {
        if (developer) GCon->Logf(NAME_Dev, "no such next frame %d in model '%s'", Md2NextFrame, *SubMdl.Model->Name);
        Md2NextFrame = (Md2NextFrame <= 0 ? 0 : SubMdl.Model->Frames.length()-1);
        // stop further warnings
        NF.Index = Md2NextFrame;
      }
    }

    // position
    TVec Md2Org = nfo.Org;

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
                 passname, *nfo.Cls->Name,
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

    if (gl_dbg_log_model_rendering) GCon->Logf("     MODEL(%s): class='%s'; alpha=%f; noshadow=%d", passname, *nfo.Cls->Name, Md2Alpha, (int)SubMdl.NoShadow);

    // angle
    TAVec Md2Angle = nfo.Angles;
    // position model
    if (SubMdl.PositionModel) PositionModel(Md2Org, Md2Angle, SubMdl.PositionModel, F.PositionIndex);

    const float smooth_inter = (DoInterp ? SMOOTHSTEP(nfo.Inter) : 0.0f);

    AliasModelTrans Transform = F.Transform;

    if (DoInterp && smooth_inter > 0.0f && &F != &NF) {
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
      Transform.MatTrans.scaleXY(nfo.ScaleX, nfo.ScaleY);
    } else {
      // done in renderer
      Transform.gzScale.x *= nfo.ScaleX;
      Transform.gzScale.y *= nfo.ScaleX;
      Transform.gzScale.z *= nfo.ScaleY;
    }

    if (!nfo.IsViewModel) {
      bool pitchFromMom = false;

      const vuint8 rndVal = (nfo.mobj ? (hashU32(nfo.mobj->ServerUId)>>4)&0xffu : 0);
      /* old code
        if (FDef.AngleStart || FDef.AngleEnd != 1.0f) {
          Md2Angle.yaw = AngleMod(Md2Angle.yaw+FDef.AngleStart+(FDef.AngleEnd-FDef.AngleStart)*Inter);
        }
      */

      Md2Org.z += FDef.zoffset.GetOffset(rndVal);

      Md2Angle.yaw = FDef.angleYaw.GetAngle(Md2Angle.yaw, rndVal);

      if (FDef.AngleStart || FDef.AngleEnd != FDef.AngleStart) {
        Md2Angle.yaw = AngleMod(Md2Angle.yaw+FDef.AngleStart+(FDef.AngleEnd-FDef.AngleStart)*nfo.Inter);
      }

      if (FDef.gzActorRoll == MdlAndle_DontUse) {
        Md2Angle.roll = 0.0f;
      } else {
        Md2Angle.roll = FDef.angleRoll.GetAngle(Md2Angle.roll, rndVal);
      }

      if (!nfo.mobj || FDef.gzActorPitch == MdlAndle_DontUse) {
        Md2Angle.pitch = 0.0f;
      } else {
        if (FDef.gzActorPitch == MdlAndle_FromMomentum && !nfo.mobj->Velocity.isZero()) {
          Md2Angle.pitch = VectorAnglePitch(nfo.mobj->Velocity);
          if (FDef.gzActorPitchInverted) Md2Angle.pitch += 180.0f;
          if (nfo.Cls->isGZDoom) pitchFromMom = true; // gozzo hack
        }
        Md2Angle.pitch = FDef.anglePitch.GetAngle(Md2Angle.pitch, rndVal);
      }

      if (!pitchFromMom && nfo.Level && nfo.mobj) {
        if (r_model_autorotating && FDef.rotateSpeed) {
          Md2Angle.yaw = AngleMod(Md2Angle.yaw+nfo.Level->Time*FDef.rotateSpeed+rndVal*38.6f);
        }

        if (r_model_autobobbing && FDef.bobSpeed) {
          //GCon->Logf("UID: %3u (%s)", (hashU32(mobj->GetUniqueId())&0xff), *mobj->GetClass()->GetFullName());
          const float bobHeight = 4.0f;
          const float zdelta = msin(AngleMod(nfo.Level->Time*FDef.bobSpeed+rndVal*44.5f))*bobHeight;
          Md2Org.z += zdelta+bobHeight;
        }
      }

      // VMatrix4::RotateY: roll
      // VMatrix4::RotateZ: yaw
      // VMatrix4::RotateX: pitch
    }

    // light
    vuint32 Md2Light = nfo.ri.light;
    if (SubMdl.FullBright) Md2Light = 0xffffffff;

    switch (Pass) {
      case RPASS_Normal:
      case RPASS_NonShadow:
      case RPASS_Glass:
      case RPASS_Trans:
        if (true /*IsViewModel || !isShadowVol*/) {
          RenderStyleInfo newri = nfo.ri;
          newri.light = Md2Light;
          newri.alpha = Md2Alpha;
          if (!newri.translucency && Md2Alpha < 1.0f) newri.translucency = RenderStyleInfo::Translucent;
          if (Pass == RPASS_Glass && !newri.translucency) {
            newri.translucency = RenderStyleInfo::Translucent;
          }
          if (Pass == RPASS_NonShadow) {
            if (!nfo.ri.isAdditive() && (nfo.ri.isTranslucent() || Md2Alpha < 1.0f)) break;
          }
          if (gl_dbg_log_model_rendering) GCon->Logf("       RENDER!");
          Drawer->DrawAliasModel(Md2Org, Md2Angle, Transform,
            SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
            nfo.Trans, nfo.ColorMap,
            newri,
            nfo.IsViewModel, smooth_inter, DoInterp, SubMdl.UseDepth,
            SubMdl.AllowTransparency,
            (Pass != RPASS_Glass ? (!nfo.IsViewModel && isShadowVol) : false)); // for advanced renderer, we need to fill z-buffer, but not color buffer
        }
        break;
      case RPASS_Ambient:
        if (!SubMdl.AllowTransparency) {
          Drawer->DrawAliasModelAmbient(Md2Org, Md2Angle, Transform,
            SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
            Md2Light, Md2Alpha, smooth_inter, DoInterp, SubMdl.UseDepth,
            SubMdl.AllowTransparency);
        }
        break;
      case RPASS_ShadowVolumes:
        Drawer->DrawAliasModelShadow(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, smooth_inter, DoInterp,
          nfo.LightPos, nfo.LightRadius);
        break;
      case RPASS_ShadowMaps:
        Drawer->DrawAliasModelShadowMap(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          Md2Alpha, smooth_inter, DoInterp, SubMdl.AllowTransparency);
        break;
      case RPASS_Light:
        Drawer->DrawAliasModelLight(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          Md2Alpha, smooth_inter, DoInterp, SubMdl.AllowTransparency);
        break;
      case RPASS_Textures:
        Drawer->DrawAliasModelTextures(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          nfo.Trans, nfo.ColorMap, nfo.ri, smooth_inter, DoInterp, SubMdl.UseDepth,
          SubMdl.AllowTransparency);
        break;
      case RPASS_Fog:
        Drawer->DrawAliasModelFog(Md2Org, Md2Angle, Transform,
          SubMdl.Model, Md2Frame, Md2NextFrame, SkinTex,
          nfo.ri.fade, Md2Alpha, smooth_inter, DoInterp, SubMdl.AllowTransparency);
        break;
    }
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
  if (Cls.oneForAll) return 0; // guaranteed

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
  if (Cls.oneForAll) return 0; // guaranteed

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
//  VRenderLevelShared::DrawAliasModelFixed
//
//  this is used to draw so-called "fixed model"
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModelFixed (MdlDrawInfo &nfo, VModel *Mdl, ERenderPass Pass) {
  //if (!IsViewModel && !IsAliasModelAllowedFor(mobj)) return false;
  nfo.Cls = Mdl->DefaultClass;

  nfo.FIdx = FindFrame(*nfo.Cls, nfo.Frame, nfo.Inter);
  if (nfo.FIdx == -1) return false;

  const float oldInter = nfo.Inter;
  const bool origInterp = nfo.Interpolate;

  float InterpFrac;
  nfo.NFIdx = FindNextFrame(*nfo.Cls, nfo.FIdx, nfo.NextFrame, oldInter, InterpFrac);
  if (nfo.NFIdx == -1) {
    nfo.NFIdx = nfo.FIdx;
    nfo.Interpolate = false;
    nfo.Inter = 0.0f;
  } else {
    nfo.Inter = InterpFrac;
  }

  nfo.LightPos = CurrLightPos;
  nfo.LightRadius = CurrLightRadius;
  nfo.ColorMap = ColorMap;

  DrawModel(nfo, Pass, IsShadowVolumeRenderer() && !IsShadowMapRenderer());

  nfo.Inter = oldInter;
  nfo.Interpolate = origInterp;
  return true;
}


//==========================================================================
//
//  VRenderLevelShared::DrawAliasModel
//
//  this is used to draw entity models
//
//==========================================================================
bool VRenderLevelShared::DrawAliasModel (MdlDrawInfo &nfo, VName clsName, ERenderPass Pass) {
  if (clsName == NAME_None) return false;
  if (!nfo.IsViewModel && !IsAliasModelAllowedFor(nfo.mobj)) return false;

  nfo.Cls = RM_FindClassModelByName(clsName);
  if (!nfo.Cls) {
    //GCon->Logf(NAME_Debug, "NO VIEW MODEL for class `%s`", *clsName);
    return false;
  }

  nfo.FIdx = FindFrame(*nfo.Cls, nfo.Frame, nfo.Inter);
  if (nfo.FIdx == -1) {
    /*
    GCon->Logf(NAME_Debug, "NO VIEW MODEL for class `%s`: %s", *clsName, *Frame.toString());
    GCon->Logf(NAME_Debug, "  MFI: %s", *mobj->getMFI().toString());
    GCon->Logf(NAME_Debug, "  NEXT MFI: %s", *mobj->getNextMFI().toString());
    */
    //abort();
    return false;
  }

  nfo.LightPos = CurrLightPos;
  nfo.LightRadius = CurrLightRadius;
  nfo.ColorMap = ColorMap;

  #if 0
  GCon->Logf(NAME_Debug, "***FOUND view model for class `%s` (fidx=%d): %s", *clsName, FIdx, *Frame.toString());
  GCon->Logf(NAME_Debug, "  State: %s", *mobj->State->Loc.toStringNoCol());
  GCon->Logf(NAME_Debug, "  MFI: %s", *mobj->getMFI().toString());
  GCon->Logf(NAME_Debug, "  NEXT MFI: %s", *mobj->getNextMFI().toString());
  #endif

  // note that gzdoom-imported modeldef can have more than one model attached to one frame
  // process all attachments -- they should differ by model or submodel indices

  const bool origInterp = nfo.Interpolate;
  const float oldInter = nfo.Inter;

  while (nfo.FIdx >= 0) {
    float InterpFrac;
    nfo.NFIdx = FindNextFrame(*nfo.Cls, nfo.FIdx, nfo.NextFrame, oldInter, InterpFrac);

    if (nfo.NFIdx == -1) {
      nfo.NFIdx = nfo.FIdx;
      nfo.Interpolate = false;
      nfo.Inter = 0.0f;
    } else {
      nfo.Interpolate = origInterp;
      nfo.Inter = InterpFrac;
    }

    #if 0
    GCon->Logf(NAME_Debug, "xxx: %s: midx=%d; smidx=%d; inter=%g (%g); FIdx=%d; NFIdx=%d; Interpolate=%s",
               *Cls->Name, Cls->Frames[FIdx].ModelIndex, Cls->Frames[FIdx].SubModelIndex,
               Inter, InterpFrac, FIdx, NFIdx, (Interpolate ? "tan" : "ona"));
    #endif
    DrawModel(nfo, Pass, IsShadowVolumeRenderer() && !IsShadowMapRenderer());

    // try next one
    const VScriptedModelFrame &cfrm = nfo.Cls->Frames[nfo.FIdx];
    int res = -1;
    if (cfrm.sprite != NAME_None) {
      // by sprite name
      //if (IsViewModel) GCon->Logf(NAME_Debug, "000: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g); nidx=%d", *Cls->Name, *cfrm.sprite, 'A'+cfrm.frame, cfrm.ModelIndex, cfrm.SubModelIndex, Inter, cfrm.Inter, FIdx);
      nfo.FIdx = cfrm.nextSpriteIdx;
      #if 0
      GCon->Logf(NAME_Debug, "001: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g); nidx=%d", *Cls->Name, *cfrm.sprite, 'A'+cfrm.frame, cfrm.ModelIndex, cfrm.SubModelIndex, Inter, cfrm.Inter, FIdx);
      #endif
      while (nfo.FIdx >= 0) {
        const VScriptedModelFrame &nfrm = nfo.Cls->Frames[nfo.FIdx];
        #if 0
        GCon->Logf(NAME_Debug, "  002: %s: sprite=%s %c; midx=%d; smidx=%d; inter=%g (%g) (FIdx=%d)", *Cls->Name, *nfrm.sprite, 'A'+nfrm.frame, nfrm.ModelIndex, nfrm.SubModelIndex, Inter, nfrm.Inter, FIdx);
        #endif
        if (cfrm.ModelIndex != nfrm.ModelIndex || cfrm.SubModelIndex != nfrm.SubModelIndex) {
          if (nfrm.Inter <= oldInter) {
            res = nfo.FIdx;
            // gozzo hack
            if (nfo.Cls->isGZDoom) break;
          } else if (nfrm.Inter > oldInter) {
            break; // the author shouldn't write incorrect defs
          }
        }
        nfo.FIdx = nfrm.nextSpriteIdx;
      }
    } else {
      // by frame index
      nfo.FIdx = cfrm.nextNumberIdx;
      while (nfo.FIdx >= 0) {
        const VScriptedModelFrame &nfrm = nfo.Cls->Frames[nfo.FIdx];
        if (cfrm.ModelIndex != nfrm.ModelIndex || cfrm.SubModelIndex != nfrm.SubModelIndex) {
          if (nfrm.Inter <= oldInter) {
            res = nfo.FIdx;
            // gozzo hack
            if (nfo.Cls->isGZDoom) break;
          } else if (nfrm.Inter > oldInter) {
            break; // the author shouldn't write incorrect defs
          }
        }
        nfo.FIdx = nfrm.nextNumberIdx;
      }
    }
    nfo.FIdx = res;
  }

  nfo.Interpolate = origInterp;
  nfo.Inter = oldInter;
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
//  VRenderLevelShared::IsModelWithGlass
//
//  returns `true` if this model should be queued as translucent
//
//==========================================================================
bool VRenderLevelShared::IsModelWithGlass (VEntity *Ent) {
  if (!Ent || (Ent->EntityFlags&VEntity::EF_FixedModel)) return false;
  VClassModelScript *Cls = RM_FindClassModelByName(GetClassNameForModel(Ent));
  return (Cls && Cls->asTranslucent);
}


//==========================================================================
//
//  VRenderLevelShared::DrawEntityModel
//
//==========================================================================
bool VRenderLevelShared::DrawEntityModel (VEntity *Ent, const RenderStyleInfo &ri,
                                          float Inter, ERenderPass Pass)
{
  //VState *DispState = (Ent->EntityFlags&VEntity::EF_UseDispState ? Ent->DispState : Ent->State);
  //VState *DispState = Ent->State; //FIXME: skipframes

  VModel *Mdl = nullptr;
  VName clsName = NAME_None;

  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    Mdl = FindFixedModelFor(Ent, true); // verbose
    if (!Mdl) return false;
  } else {
    clsName = GetClassNameForModel(Ent);
    if (clsName == NAME_None) return false;
  }

  MdlDrawInfo nfo;
  memset((void *)&nfo, 0, sizeof(nfo));
  nfo.Level = Level;
  nfo.mobj = Ent;
  nfo.Org = Ent->GetDrawOrigin(); // movement interpolation
  nfo.Angles = Ent->/*Angles*/GetModelDrawAngles();
  nfo.ScaleX = Ent->ScaleX;
  nfo.ScaleY = Ent->ScaleY;
  nfo.Frame = Ent->getMFI();
  nfo.NextFrame = Ent->getNextMFI();
  nfo.Trans = GetTranslation(Ent->Translation);
  nfo.Version = Ent->ModelVersion;
  nfo.ri = ri;
  nfo.IsViewModel = false;
  nfo.Inter = Inter;
  nfo.Interpolate = r_interpolate_frames;

  // check if we want to interpolate model frames
  if (Ent->EntityFlags&VEntity::EF_FixedModel) {
    return DrawAliasModelFixed(nfo, Mdl, Pass);
  } else {
    return DrawAliasModel(nfo, clsName, Pass);
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


  VRenderLevelShared::MdlDrawInfo nfo;
  memset((void *)&nfo, 0, sizeof(nfo));
  nfo.Org = Origin;
  nfo.Angles.yaw = Angle;
  nfo.ScaleX = nfo.ScaleY = 1.0f;
  nfo.Cls = Cls;
  nfo.FIdx = FIdx;
  nfo.NFIdx = NFIdx;
  nfo.Inter = InterpFrac;
  nfo.Interpolate = Interpolate;
  nfo.ri.light = 0xffffffffu;
  nfo.ri.fade = 0;
  nfo.ri.alpha = 1.0f;
  nfo.ri.translucency = RenderStyleInfo::Normal;

  DrawModel(nfo, RPASS_Normal, true);

  Drawer->EndView();
  return true;
}


//static VClass *bulbClass = nullptr;


//==========================================================================
//
// R_DrawLightBulb
//
//==========================================================================
void R_DrawLightBulb (const TVec &Org, const TAVec &Angles,
                      vuint32 rgbLight, ERenderPass Pass,
                      bool isShadowVol, float ScaleX, float ScaleY)
{
  /*
  if (!bulbClass) {
    bulbClass = VClass:FindClass("K8DebugLightBulb");
    if (!bulbClass) return;
  }
  */

  VClassModelScript *Cls = RM_FindClassModelByName("K8DebugLightBulb");
  if (!Cls) return;

  VRenderLevelShared::MdlDrawInfo nfo;
  memset((void *)&nfo, 0, sizeof(nfo));

  nfo.Frame.sprite = NAME_None;
  nfo.Frame.frame = 0;
  nfo.Frame.index = 0;
  nfo.Frame.spriteIndex = 0;

  nfo.FIdx = FindFrame(*Cls, nfo.Frame, 0.0f);
  if (nfo.FIdx == -1) return;

  nfo.Org = Org;
  nfo.Angles = Angles;
  nfo.ScaleX = nfo.ScaleY = 1.0f;
  nfo.Cls = Cls;
  nfo.Inter = 0.0f;
  nfo.Interpolate = false;

  rgbLight |= 0xff000000u;

  nfo.ri.light = rgbLight;
  nfo.ri.fade = 0;
  nfo.ri.alpha = 1.0f;
  nfo.ri.translucency = RenderStyleInfo::Normal;

  DrawModel(nfo, Pass, isShadowVol);
}
