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
#include "gl_local.h"


extern VCvarB r_allow_cameras;
extern VCvarB gl_pic_filtering;

static VCvarB gl_recreate_changed_textures("gl_recreate_changed_textures", false, "Destroy and create new OpenGL textures for changed DooM animated ones?", CVAR_Archive|CVAR_NoShadow);
static VCvarB gl_camera_texture_use_readpixels("gl_camera_texture_use_readpixels", true, "Use ReadPixels to update camera textures?", CVAR_Archive|CVAR_NoShadow);

static VCvarI gl_s3tc_mode("gl_s3tc_mode", "0", "Use S3TC texture compression, if supported (0:no; 1:hires; 2:all)?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarB gl_s3tc_dump("gl_s3tc_dump", false, "Show which textures are compressed.", CVAR_PreInit|CVAR_Hidden|CVAR_NoShadow);

#ifndef COMPRESSED_RGB_S3TC_DXT1_EXT
# define COMPRESSED_RGB_S3TC_DXT1_EXT                   0x83F0
#endif
#ifndef COMPRESSED_RGBA_S3TC_DXT1_EXT
# define COMPRESSED_RGBA_S3TC_DXT1_EXT                  0x83F1
#endif
#ifndef COMPRESSED_RGBA_S3TC_DXT3_EXT
# define COMPRESSED_RGBA_S3TC_DXT3_EXT                  0x83F2
#endif
#ifndef COMPRESSED_RGBA_S3TC_DXT5_EXT
# define COMPRESSED_RGBA_S3TC_DXT5_EXT                  0x83F3
#endif


//==========================================================================
//
//  isPOT
//
//==========================================================================
static int isPOT (int val) {
  int answer = 0, origval = val;
  while (val >>= 1) ++answer;
  return ((1<<answer) == origval);
}


//==========================================================================
//
//  VOpenGLDrawer::GenerateLightmapAtlasTextures
//
//==========================================================================
void VOpenGLDrawer::GenerateLightmapAtlasTextures () {
  vassert(!atlasesGenerated);
  glGenTextures(NUM_BLOCK_SURFS, lmap_id);
  for (unsigned f = 0; f < NUM_BLOCK_SURFS; ++f) {
    glBindTexture(GL_TEXTURE_2D, lmap_id[f]);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (anisotropyExists) {
      glTexParameterf(GL_TEXTURE_2D, GLenum(GL_TEXTURE_MAX_ANISOTROPY_EXT), (float)clampval(gl_texture_filter_anisotropic.asInt(), 1, max_anisotropy));
    }
    // create texture image
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  atlasesGenerated = true;
  memset(atlases_updated, 0, sizeof(atlases_updated));
  //GCon->Log(NAME_Debug, "******** VOpenGLDrawer::GenerateLightmapAtlasTextures ********");
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteLightmapAtlases
//
//==========================================================================
void VOpenGLDrawer::DeleteLightmapAtlases () {
  if (atlasesGenerated) {
    glDeleteTextures(NUM_BLOCK_SURFS, lmap_id);
    atlasesGenerated = false;
    memset(lmap_id, 0, sizeof(lmap_id));
    memset(atlases_updated, 0, sizeof(atlases_updated));
    //GCon->Log(NAME_Debug, "******** VOpenGLDrawer::DeleteLightmapAtlases ********");
  }
}


//==========================================================================
//
//  VOpenGLDrawer::FlushTextures
//
//==========================================================================
void VOpenGLDrawer::FlushTextures (bool forced) {
  for (int i = 0; i < GTextureManager.GetNumTextures(); ++i) {
    VTexture *tex = GTextureManager.getIgnoreAnim(i);
    if (tex) {
      if (forced) DeleteTexture(tex); else FlushTexture(tex);
      tex->lastUpdateFrame = 0;
    }
  }
  for (int i = 0; i < GTextureManager.GetNumMapTextures(); ++i) {
    VTexture *tex = GTextureManager.getMapTexIgnoreAnim(i);
    if (tex) {
      if (forced) DeleteTexture(tex); else FlushTexture(tex);
      tex->lastUpdateFrame = 0;
    }
  }
}


//==========================================================================
//
//  VOpenGLDrawer::FlushOneTexture
//
//==========================================================================
void VOpenGLDrawer::FlushOneTexture (VTexture *tex, bool forced) {
  if (!tex) return;
  if (forced) DeleteTexture(tex); else FlushTexture(tex);
  tex->lastUpdateFrame = 0;
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteTextures
//
//  called on drawer shutdown
//
//==========================================================================
void VOpenGLDrawer::DeleteTextures () {
  FlushTextures(true); // forced
  DeleteLightmapAtlases();
  UnloadModels();
}


//==========================================================================
//
//  VOpenGLDrawer::FlushTexture
//
//==========================================================================
void VOpenGLDrawer::FlushTexture (VTexture *Tex) {
  if (!Tex) return;
  if (Tex->DriverHandle) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, (GLuint *)&Tex->DriverHandle);
    Tex->DriverHandle = 0;
    Tex->lastTextureFiltering = -1;
  }
  for (auto &&it : Tex->DriverTranslated) {
    if (it.Handle) {
      glBindTexture(GL_TEXTURE_2D, 0);
      glDeleteTextures(1, (GLuint *)&it.Handle);
    }
  }
  Tex->ResetTranslations();
  Tex->lastUpdateFrame = 0;
  if (Tex->Brightmap) FlushTexture(Tex->Brightmap);
}


//==========================================================================
//
//  VOpenGLDrawer::DeleteTexture
//
//==========================================================================
void VOpenGLDrawer::DeleteTexture (VTexture *Tex) {
  if (!Tex) return;
  if (Tex->DriverHandle) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, (GLuint *)&Tex->DriverHandle);
    Tex->DriverHandle = 0;
    Tex->lastTextureFiltering = -1;
  }
  for (auto &&it : Tex->DriverTranslated) {
    if (it.Handle) {
      glBindTexture(GL_TEXTURE_2D, 0);
      glDeleteTextures(1, (GLuint *)&it.Handle);
    }
  }
  Tex->ClearTranslations();
  Tex->lastUpdateFrame = 0;
  if (Tex->Brightmap) DeleteTexture(Tex->Brightmap);
}


//==========================================================================
//
//  VOpenGLDrawer::PrecacheTexture
//
//==========================================================================
void VOpenGLDrawer::PrecacheTexture (VTexture *Tex) {
  if (!Tex) return;
  if (Tex->bIsCameraTexture) return;
  ResetTextureUpdateFrames();
  SetCommonTexture(Tex, 0);
  //if (Tex->Brightmap) SetBrightmapTexture(Tex->Brightmap);
  if (Tex->Brightmap && Tex->Brightmap->Width > 1 && Tex->Brightmap->Height > 1) {
    (void)SetTextureLump(SetTexType::TT_Brightmap, Tex->Brightmap, nullptr, 0, 0u);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::PrecacheSpriteTexture
//
//==========================================================================
void VOpenGLDrawer::PrecacheSpriteTexture (VTexture *Tex, SpriteType sptype) {
  if (!Tex) return;
  if (Tex->bIsCameraTexture) return;
  ResetTextureUpdateFrames();
  switch (sptype) {
    case SpriteType::SP_Normal: (void)SetTextureLump(SetTexType::TT_Sprite, Tex, nullptr, 0); break;
    case SpriteType::SP_PSprite: (void)SetTextureLump(SetTexType::TT_PSprite, Tex, nullptr, 0); break;
  }
  if (Tex->Brightmap && Tex->Brightmap->Width > 1 && Tex->Brightmap->Height > 1) {
    (void)SetTextureLump((sptype == SpriteType::SP_PSprite ? SetTexType::TT_PSpriteBrightmap: SetTexType::TT_SpriteBrightmap), Tex->Brightmap, nullptr, 0, 0u);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::SetBrightmapTexture
//
//==========================================================================
void VOpenGLDrawer::SetBrightmapTexture (VTexture *Tex) {
  if (!Tex || /*Tex->Type == TEXTYPE_Null ||*/ Tex->Width < 1 || Tex->Height < 1) return;
  if (Tex->bIsCameraTexture) return;
  //SetCommonTexture(Tex, 0); // default colormap
  (void)SetTextureLump(SetTexType::TT_Brightmap, Tex, nullptr, 0, 0u);
  // `asModel` sets GL_REPEAT
  SetupTextureFiltering(Tex, (r_brightmaps_filter ? 4 : 0), TexWrapRepeat); // trilinear or none
}


//==========================================================================
//
//  VOpenGLDrawer::SetSpriteBrightmapTexture
//
//==========================================================================
void VOpenGLDrawer::SetSpriteBrightmapTexture (SpriteType sptype, VTexture *Tex) {
  if (!Tex || Tex->Width < 1 || Tex->Height < 1) return;
  if (Tex->bIsCameraTexture) return;
  //SetCommonTexture(Tex, 0); // default colormap
  (void)SetTextureLump((sptype == SpriteType::SP_PSprite ? SetTexType::TT_PSpriteBrightmap: SetTexType::TT_SpriteBrightmap), Tex, nullptr, 0, 0u);
  // `asModel` sets GL_REPEAT
  SetupTextureFiltering(Tex, (r_brightmaps_filter ? 4 : 0), TexWrapRepeat); // trilinear or none
}


//==========================================================================
//
//  VOpenGLDrawer::SetCommonTexture
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetCommonTexture (VTexture *Tex, int CMap, vuint32 ShadeColor) {
  if (!Tex) Sys_Error("cannot set null texture");
  // camera textures are special
  const bool res = SetTextureLump(SetTexType::TT_Common, Tex, nullptr, CMap, ShadeColor);
  SetOrForceTextureFiltering(res, Tex, texture_filter);
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::SetDecalTexture
//
//  returns `false` if non-main texture was bound
//  bloodsplats will be converted to pure red
//
//==========================================================================
bool VOpenGLDrawer::SetDecalTexture (VTexture *Tex, VTextureTranslation *Translation, int CMap, bool isBloodSplat) {
  if (!Tex) Sys_Error("cannot set null texture");
  //GCon->Logf(NAME_Debug, "DECAL TEXTURE '%s'", *Tex->Name);
  const bool res = SetTextureLump((isBloodSplat ? SetTexType::TT_BloodDecal : SetTexType::TT_Decal), Tex, Translation, CMap, /*ShadeColor*/0);
  SetOrForceTextureFiltering(res, Tex, texture_filter, TexWrapRepeat);
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::SetPic
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetPic (VTexture *Tex, VTextureTranslation *Trans, int CMap, vuint32 ShadeColor) {
  const bool res = SetTextureLump(SetTexType::TT_Pic, Tex, Trans, CMap, ShadeColor);
  const int oldAniso = gl_texture_filter_anisotropic.asInt();
  gl_texture_filter_anisotropic.Set(1);
  SetOrForceTextureFiltering(res, Tex, (gl_pic_filtering ? 3 : 0), TexWrapClamp);
  gl_texture_filter_anisotropic.Set(oldAniso);
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::SetPicModel
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetPicModel (VTexture *Tex, VTextureTranslation *Trans, int CMap, vuint32 ShadeColor) {
  const bool res = SetTextureLump(SetTexType::TT_Model, Tex, Trans, CMap, ShadeColor);
  SetOrForceTextureFiltering(res, Tex, model_filter, TexWrapRepeat);
  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::SetTextureLump
//
//  returns `false` if non-main texture was bound
//
//==========================================================================
bool VOpenGLDrawer::SetTextureLump (SetTexType ttype, VTexture *Tex, VTextureTranslation *Translation, int CMap, vuint32 ShadeColor) {
  vassert(Tex);
  bool res = true;
  if (mInitialized) {
    if (ShadeColor) {
      Translation = nullptr; // just in case
      ShadeColor |= 0xff000000u; // normalise it
    }
    bool needUp = false;
    if (Tex->lastUpdateFrame != updateFrame) {
      // returns 0 if not, positive if only data need to be updated, or
      // negative to recreate texture
      const int upmode = Tex->CheckModified();
      //GCon->Logf(NAME_Debug, "texture '%s' needs update! (%u : %u)", *Tex->Name, Tex->lastUpdateFrame, updateFrame);
      if (upmode) {
        if (upmode < 0 || gl_recreate_changed_textures) {
          //GCon->Logf(NAME_Debug, "texture '%s' needs update! (flushing)", *Tex->Name);
          FlushTexture(Tex);
        }
        needUp = true;
      }
    }
    Tex->lastUpdateFrame = updateFrame;
    if (Translation || CMap || ShadeColor) {
      // color translation, color map, or stenciled
      // find translation, and mark it as recently used
      //FIXME!
      res = false;
      VTexture::VTransData *TData = (ShadeColor ? Tex->FindDriverShaded(ShadeColor, CMap, true) : Tex->FindDriverTrans(Translation, CMap, true));
      if (TData) {
        if (needUp || !TData->Handle || Tex->bIsCameraTexture) {
          GenerateTexture(ttype, Tex, &TData->Handle, Translation, CMap, needUp, ShadeColor);
        } else {
          glBindTexture(GL_TEXTURE_2D, TData->Handle);
        }
      } else {
        // drop some old translations
        const vint32 ctime = Tex->GetTranslationCTime();
        if (ctime >= 0) {
          for (int f = 0; f < Tex->DriverTranslated.length(); ++f) {
            TData = &Tex->DriverTranslated[f];
            const vint32 ttime = TData->lastUseTime;
            // keep it for 100 seconds, and then drop it
            if (ttime >= ctime || ctime-ttime < 35*100) continue;
            // drop it
            if (TData->Handle) glDeleteTextures(1, (GLuint *)&TData->Handle);
            Tex->ClearTranslationAt(f);
            Tex->DriverTranslated.removeAt(f);
            --f;
          }
        }
        // new translation (insert it at the top of the list, why not)
        VTexture::VTransData newtrans;
        //newtrans.wipe();
        newtrans.Handle = 0;
        newtrans.Trans = Translation;
        newtrans.ColorMap = CMap;
        newtrans.ShadeColor = ShadeColor;
        if (ctime >= 0) newtrans.lastUseTime = ctime;
        Tex->DriverTranslated.insert(0, newtrans);
        TData = &Tex->DriverTranslated[0];
        GenerateTexture(ttype, Tex, &TData->Handle, Translation, CMap, needUp, ShadeColor);
      }
    } else if (!Tex->DriverHandle || needUp || Tex->bIsCameraTexture) {
      // generate and bind new texture
      GenerateTexture(ttype, Tex, &Tex->DriverHandle, nullptr, 0, needUp, ShadeColor);
    } else {
      // existing normal texture
      glBindTexture(GL_TEXTURE_2D, Tex->DriverHandle);
    }
  }

  const int tex_w = max2(1, Tex->GetWidth());
  const int tex_h = max2(1, Tex->GetHeight());
  tex_iw = 1.0f/(float)tex_w;
  tex_ih = 1.0f/(float)tex_h;

  float tex_w_sc = Tex->GetScaledWidthF();
  float tex_h_sc = Tex->GetScaledHeightF();
  if (!isFiniteF(tex_w_sc) || tex_w_sc <= 0.0f) tex_w_sc = 1.0f;
  if (!isFiniteF(tex_h_sc) || tex_h_sc <= 0.0f) tex_h_sc = 1.0f;
  tex_iw_sc = 1.0f/tex_w_sc;
  tex_ih_sc = 1.0f/tex_h_sc;

  tex_scale_x = (Tex->SScale > 0.0f ? Tex->SScale : 1.0f);
  tex_scale_y = (Tex->TScale > 0.0f ? Tex->TScale : 1.0f);

  return res;
}


//==========================================================================
//
//  VOpenGLDrawer::GenerateTexture
//
//  TODO: remove some old translations if they weren't used for a long time
//
//==========================================================================
void VOpenGLDrawer::GenerateTexture (SetTexType ttype, VTexture *Tex, GLuint *pHandle, VTextureTranslation *Translation, int CMap, bool needUpdate, vuint32 ShadeColor) {
  // special handling for camera textures
  bool isCamTexture = Tex->bIsCameraTexture;

  if (isCamTexture) {
    VCameraTexture *CamTex = (VCameraTexture *)Tex;
    if (r_allow_cameras) {
      CamTex->bUsedInFrame = true;
      GLuint tid = GetCameraFBOTextureId(CamTex->camfboidx);
      if (!tid || Translation || CMap || currMainFBO == CamTex->camfboidx || gl_camera_texture_use_readpixels) {
        // game can append new cameras dynamically, and FBO can be unready yet
        if (CamTex->camfboidx >= 0) {
          // copy texture pixels for translations (or if user requested it)
          if (!CamTex->bPixelsLoaded) {
            // read FBO data
            //vassert(CamTex->camfboidx >= 0);
            if (CamTex->camfboidx < cameraFBOList.length()) {
              CamTex->bPixelsLoaded = true;
              CameraFBOInfo *cfi = cameraFBOList[CamTex->camfboidx];
              rgba_t *px = (rgba_t *)Tex->GetPixels();
              ReadFBOPixels(&cfi->fbo, Tex->Width, Tex->Height, px);
            } else {
              GCon->Logf(NAME_Error, "THE THING THAT SHOULD NOT BE: camera FBO array is too small! (want %d, size is %d)", CamTex->camfboidx+1, cameraFBOList.length());
            }
          }
        } else {
          //GCon->Logf(NAME_Debug, "trying to select unintialized camera texture (this is harmeless)");
        }
        isCamTexture = false;
      }
    } else {
      isCamTexture = false;
    }
  }

  // flag can be changed here, so recheck
  if (isCamTexture) {
    // handle camera texture
    VCameraTexture *CamTex = (VCameraTexture *)Tex;
    //FlushTexture(Tex); // remove all created textures
    GCon->Logf(NAME_Debug, "***** camtex; fboindex=%d; len=%d", CamTex->camfboidx, cameraFBOList.length());
    GLuint tid = GetCameraFBOTextureId(CamTex->camfboidx);
    vassert(tid);
    glBindTexture(GL_TEXTURE_2D, tid);
    #if 0
    GLint oldbindtex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldbindtex);
    GCon->Logf(NAME_Debug, "set camera texture; fboidx=%d; fbotid=%u; tid=%d; curtid=%d; size=%dx%d; ofs=(%d,%d); scale=(%g,%g)", CamTex->camfboidx, cameraFBOList[CamTex->camfboidx]->fbo.getColorTid(), tid, oldbindtex, Tex->Width, Tex->Height, Tex->SOffset, Tex->TOffset, Tex->SScale, Tex->TScale);
    #endif
  } else {
    bool forceRelease = false;

    // handle non-camera texture
    if (!*pHandle) glGenTextures(1, pHandle);
    glBindTexture(GL_TEXTURE_2D, *pHandle);
    p_glObjectLabelVA(GL_TEXTURE, *pHandle, "Texture '%s'", *Tex->Name);
    // this hangs!
    //GCon->Logf(NAME_Debug, "texture '%s'; tid=%d", *Tex->Name, *pHandle);
    //fprintf(stderr, "*** texture '%s'; tid=%d\n", *Tex->Name, *pHandle);

    // try to load high resolution version
    int hitype = (Tex->IsDynamicTexture() ? UpTexNoCompress : (Tex->hiresRepTex ? UpTexHiRes : UpTexNormal));
    VTexture *SrcTex = Tex;
    if (!Tex->bIsCameraTexture) {
      VTexture *hitex = Tex->GetHighResolutionTexture();
      if (hitex && hitex->Type != TEXTYPE_Null) {
        SrcTex = hitex;
        hitype = (hitex->IsDynamicTexture() ? UpTexNoCompress : UpTexHiRes);
      }
    } else {
      hitype = UpTexNoCompress;
    }

    // force texture update if we need to crop it
    // note that brightmap textures will be cropped by main texture crop call
    const bool isSpriteBM = (/*ttype == TT_Brightmap ||*/ ttype == TT_SpriteBrightmap || ttype == TT_PSpriteBrightmap);
    bool doCrop = false;
    switch (ttype) {
      case TT_Sprite: case TT_Shadow: doCrop = gl_crop_sprites.asBool(); break;
      case TT_PSprite: doCrop = gl_crop_psprites.asBool(); break;
      default: break;
    }

    if (SrcTex->Type == TEXTYPE_Null) {
      // morons
      GCon->Logf(NAME_Warning, "something is VERY wrong with textures in this mod (trying to upload null texture '%s')", *SrcTex->Name);
      if (SrcTex->SourceLump >= 0) GCon->Logf(NAME_Warning, "...source lump: %s", *W_FullLumpName(SrcTex->SourceLump));
      rgba_t dummy[4];
      memset((void *)dummy, 0, sizeof(dummy));
      VTexture::checkerFillRGBA((vuint8 *)dummy, 2, 2);
      UploadTexture(2, 2, dummy, false, -1, UpTexNoCompress); // no fringe filtering
    } else if (Translation && CMap) {
      // both colormap and translation
      rgba_t tmppal[256];
      const vuint8 *TrTab = Translation->GetTable();
      const rgba_t *CMPal = ColorMaps[CMap].GetPalette();
      for (int i = 0; i < 256; ++i) tmppal[i] = CMPal[TrTab[i]];
      if (doCrop && !isSpriteBM) SrcTex->CropTexture(); // do not crop brightmaps, it is already done by the main texture cropper
      if (ttype == SetTexType::TT_Decal || ttype == SetTexType::TT_BloodDecal) {
        SrcTex->ReleasePixels();
        SrcTex->Shade(0xff0000); // shade to red
        (void)SrcTex->GetPixels(); // shaded
        forceRelease = true;
      } else {
        if (needUpdate) (void)SrcTex->GetPixels(); // this updates warp and other textures
      }
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), tmppal, SrcTex->SourceLump, hitype);
    } else if (Translation) {
      // only translation
      //GCon->Logf("uploading translated texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      //for (int f = 0; f < 256; ++f) GCon->Logf("  %3d: r:g:b=%02x:%02x:%02x", f, Translation->GetPalette()[f].r, Translation->GetPalette()[f].g, Translation->GetPalette()[f].b);
      if (doCrop && !isSpriteBM) SrcTex->CropTexture(); // do not crop brightmaps, it is already done by the main texture cropper
      if (ttype == SetTexType::TT_Decal || ttype == SetTexType::TT_BloodDecal) {
        SrcTex->ReleasePixels();
        SrcTex->Shade(0xff0000); // shade to red
        (void)SrcTex->GetPixels(); // shaded
        forceRelease = true;
      } else {
        if (needUpdate) (void)SrcTex->GetPixels(); // this updates warp and other textures
      }
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), Translation->GetPalette(), SrcTex->SourceLump, hitype);
    } else if (CMap) {
      // only colormap
      //GCon->Logf(NAME_Dev, "uploading colormapped texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      if (doCrop && !isSpriteBM) SrcTex->CropTexture(); // do not crop brightmaps, it is already done by the main texture cropper
      if (ttype == SetTexType::TT_Decal || ttype == SetTexType::TT_BloodDecal) {
        SrcTex->ReleasePixels();
        SrcTex->Shade(0xff0000); // shade to red
        (void)SrcTex->GetPixels(); // shaded
        forceRelease = true;
      } else {
        if (needUpdate) (void)SrcTex->GetPixels(); // this updates warp and other textures
      }
      UploadTexture8A(SrcTex->GetWidth(), SrcTex->GetHeight(), SrcTex->GetPixels8A(), ColorMaps[CMap].GetPalette(), SrcTex->SourceLump, hitype);
    } else if (ShadeColor) {
      // shade (and possible colormap)
      //GLog.Logf(NAME_Debug, "*** SHADE: tex='%s'; color=0x%08x", *SrcTex->Name, ShadeColor);
      if (doCrop && !isSpriteBM) SrcTex->CropTexture(); // do not crop brightmaps, it is already done by the main texture cropper
      rgba_t *shadedPixels = SrcTex->CreateShadedPixels(ShadeColor, (CMap ? ColorMaps[CMap].GetPalette() : nullptr));
      UploadTexture(SrcTex->GetWidth(), SrcTex->GetHeight(), shadedPixels, false/*remove fringe*/, -1, hitype);
      SrcTex->FreeShadedPixels(shadedPixels);
    } else {
      // normal uploading
      if (doCrop && !isSpriteBM && SrcTex->SourceLump >= 0) SrcTex->CropTexture(); // do not crop brightmaps, it is already done by the main texture cropper
      //if (SrcTex->SourceLump < 0) fprintf(stderr, "*** uploading texture '%s' (%dx%d)\n", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      vuint8 *block = SrcTex->GetPixels();
      //if (SrcTex->SourceLump >= 0) GCon->Logf(NAME_Debug, "uploading normal texture '%s' (%dx%d)", *SrcTex->Name, SrcTex->GetWidth(), SrcTex->GetHeight());
      if (SrcTex->Format == TEXFMT_8 || SrcTex->Format == TEXFMT_8Pal) {
        UploadTexture8(SrcTex->GetWidth(), SrcTex->GetHeight(), block, SrcTex->GetPalette(), SrcTex->SourceLump, hitype);
      } else {
        UploadTexture(SrcTex->GetWidth(), SrcTex->GetHeight(), (rgba_t *)block, (SrcTex->isTransparent() || SrcTex->isTranslucent()), SrcTex->SourceLump, hitype);
      }
    }

    // do not release cropped textures, we may need their pixels in the future
    //FIXME: make them "releaseable"
    if (forceRelease) {
      SrcTex->ReleasePixels();
    } else if (!doCrop) {
      const int relmode =
        ttype == TT_Sprite || ttype == TT_SpriteBrightmap /*||
        ttype == TT_PSprite || ttype == TT_PSpriteBrightmap*/ ?
          gl_release_ram_textures_mode_sprite.asInt() : gl_release_ram_textures_mode.asInt();
      if (SrcTex && !SrcTex->IsDynamicTexture() && SrcTex->SourceLump >= 0 && (SrcTex->IsHugeTexture() || relmode >= 2)) {
        //if (SrcTex->IsHugeTexture()) GCon->Logf(NAME_Debug, "freeing \"huge\" texture '%s' (%s) (%dx%d)", *SrcTex->Name, *W_FullLumpName(SrcTex->SourceLump), SrcTex->Width, SrcTex->Height);
        SrcTex->ReleasePixels();
      }
    }
  }

  if (isCamTexture) {
    Tex->MarkGCUnused();
  } else if (ttype == TT_Common || ttype == TT_Sprite /*|| ttype == TT_PSprite*/) {
    if (Tex->HasPixels()) Tex->MarkGCUsed(CurrentTime); else Tex->MarkGCUnused();
  } else {
    Tex->MarkGCUnused();
  }

  // set up texture wrapping
  const GLint rep = (Tex->isWrapRepeat() ? GL_REPEAT : ClampToEdge);
  //GCon->Logf(NAME_Debug, "repeat for '%s' (%s): %s (isWrapRepeat=%d)", *Tex->Name, VTexture::TexTypeToStr(Tex->Type), (rep == GL_REPEAT ? "tan" : "ona"), (int)Tex->isWrapRepeat());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, rep);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, rep);
  Tex->initFilteringAsModelOnly(rep == GL_REPEAT);
  // other parameters will be set by the caller
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture8
//
//==========================================================================
void VOpenGLDrawer::UploadTexture8 (int Width, int Height, const vuint8 *Data, const rgba_t *Pal, int /*SourceLump*/, int hitype) {
  // this is single-threaded, so why not?
  int w = (Width > 0 ? Width : 2);
  int h = (Height > 0 ? Height : 2);
  static rgba_t *databuf = nullptr;
  static size_t databufSize = 0;
  if (databufSize < (size_t)(w*h*4)) {
    databuf = (rgba_t *)Z_Realloc(databuf, w*h*4);
    databufSize = (size_t)(w*h*4);
  }
  rgba_t *NewData = databuf;
  bool doFringeRemove = false;
  if (Width > 0 && Height > 0) {
    for (int i = 0; i < Width*Height; ++i, ++Data, ++NewData) {
      *NewData = (*Data ? Pal[*Data] : rgba_t::Transparent());
      if (!doFringeRemove && NewData->a != 255) doFringeRemove = true;
    }
    //if (doFringeRemove) VTexture::FilterFringe(databuf, w, h);
  } else {
    if (w && h) memset((void *)NewData, 0, w*h*4);
  }
  UploadTexture(w, h, databuf, doFringeRemove, -1, hitype);
  //Z_Free(NewData);
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture8A
//
//==========================================================================
void VOpenGLDrawer::UploadTexture8A (int Width, int Height, const pala_t *Data, const rgba_t *Pal, int /*SourceLump*/, int hitype) {
  // this is single-threaded, so why not?
  int w = (Width > 0 ? Width : 2);
  int h = (Height > 0 ? Height : 2);
  static rgba_t *databuf = nullptr;
  static size_t databufSize = 0;
  if (databufSize < (size_t)(w*h*4)) {
    databuf = (rgba_t *)Z_Realloc(databuf, w*h*4);
    databufSize = (size_t)(w*h*4);
  }
  rgba_t *NewData = databuf;
  //rgba_t *NewData = (rgba_t *)Z_Calloc(Width*Height*4);
  bool doFringeRemove = false;
  if (Width > 0 && Height > 0) {
    for (int i = 0; i < Width*Height; ++i, ++Data, ++NewData) {
      *NewData = Pal[Data->idx];
      NewData->a = Data->a;
      if (!doFringeRemove && NewData->a != 255) doFringeRemove = true;
    }
    //if (doFringeRemove) VTexture::FilterFringe(databuf, w, h);
  } else {
    if (w && h) memset((void *)NewData, 0, w*h*4);
  }
  UploadTexture(w, h, databuf, doFringeRemove, -1, hitype);
  //Z_Free(NewData);
}


//==========================================================================
//
//  VOpenGLDrawer::UploadTexture
//
//==========================================================================
void VOpenGLDrawer::UploadTexture (int width, int height, const rgba_t *data, bool doFringeRemove, int SourceLump, int hitype) {
  if (width < 1 || height < 1) Sys_Error("WARNING: fucked texture (w=%d; h=%d)", width, height);
  if (!data) Sys_Error("WARNING: fucked texture (w=%d; h=%d, no data)", width, height);

  int w, h;

  if (hasNPOT) {
    w = width;
    h = height;
  } else {
    w = ToPowerOf2(width);
    h = ToPowerOf2(height);
  }

  if (w > maxTexSize) w = maxTexSize;
  if (h > maxTexSize) h = maxTexSize;

  // get two temporary buffers
  if (tmpImgBufSize < w*h*4) {
    tmpImgBufSize = ((w*h*4)|0xffff)+1;
    tmpImgBuf0 = (vuint8 *)Z_Realloc(tmpImgBuf0, tmpImgBufSize);
    tmpImgBuf1 = (vuint8 *)Z_Realloc(tmpImgBuf1, tmpImgBufSize);
  }

  vuint8 *image = tmpImgBuf0;
  vuint8 *pmimage = tmpImgBuf1;

  if (w != width || h != height) {
    // smooth transparent edges
    if (width <= maxTexSize && height <= maxTexSize) {
      if (width && height) memcpy(pmimage, data, width*height*4);
      if (doFringeRemove) VTexture::SmoothEdges(pmimage, width, height);
      // must rescale image to get "top" mipmap texture image
      VTexture::ResampleTexture(width, height, pmimage, w, h, image, multisampling_sample);
    } else {
      VTexture::ResampleTexture(width, height, (const vuint8 *)data, w, h, image, multisampling_sample);
    }
  } else {
    if (w && h) memcpy(image, data, w*h*4);
    if (doFringeRemove) VTexture::SmoothEdges(image, w, h);
    //if (doFringeRemove) VTexture::FilterFringe((rgba_t *)image, w, h);
    //VTexture::PremultiplyImage((rgba_t *)image, w, h);
  }

  VTexture::AdjustGamma((rgba_t *)image, w*h);

  GLint internal = GL_RGBA8;
  int cmode = (HaveS3TC && hitype != UpTexNoCompress ? gl_s3tc_mode.asInt() : 0);
  if (cmode == 1 && hitype != UpTexHiRes) cmode = 0;
  // don't bother with small textures anyway
  if (cmode > 0 && width > 256 && height > 256 && isPOT(width) && isPOT(height) && !gl_use_srgb.asBool()) {
    // determine texture format:
    // COMPRESSED_RGB_S3TC_DXT1_EXT  -- RGB
    // COMPRESSED_RGBA_S3TC_DXT1_EXT -- RGBA with 1-bit alpha
    // COMPRESSED_RGBA_S3TC_DXT3_EXT -- RGBA with 4-bit alpha
    // COMPRESSED_RGBA_S3TC_DXT5_EXT -- RGBA with 8-bit interpolated alpha
    bool hasAlpha = false;
    bool hasMoreAlpha = false;
    const rgba_t *s = (const rgba_t *)image;
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x, ++s) {
             if (s->a == 0) hasAlpha = true;
        else if (s->a < 255) { hasMoreAlpha = true; break; }
      }
    }
         if (hasMoreAlpha) internal = COMPRESSED_RGBA_S3TC_DXT3_EXT;
    else if (hasAlpha) internal = COMPRESSED_RGBA_S3TC_DXT1_EXT;
    else internal = COMPRESSED_RGB_S3TC_DXT1_EXT;
    if (gl_s3tc_dump) GCon->Logf(NAME_Debug, "S3TC(%s): 0x%04X  size: %dx%d", *W_FullLumpName(SourceLump), internal, width, height);
  } else {
    //if (gl_s3tc_dump) GCon->Logf(NAME_Debug, "NOS3TC(%s): size: %dx%d", *W_FullLumpName(SourceLump), width, height);
  }

  if (internal == GL_RGBA8 && gl_use_srgb.asBool()) internal = GL_SRGB8_ALPHA8;

  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
  glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
  if (p_glGenerateMipmap) p_glGenerateMipmap(GL_TEXTURE_2D);
}
