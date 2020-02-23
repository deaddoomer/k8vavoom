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
//**  Copyright (C) 2018-2020 Ketmar Dark
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


static VCvarB gl_dbg_adv_render_textures_models("gl_dbg_adv_render_textures_models", true, "Render model textures in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_ambient_models("gl_dbg_adv_render_ambient_models", true, "Render model ambient light in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_light_models("gl_dbg_adv_render_light_models", true, "Render model dynamic light in advanced renderer?", 0);
static VCvarB gl_dbg_adv_render_alias_models("gl_dbg_adv_render_alias_models", true, "Render alias models?", 0);
static VCvarB gl_dbg_adv_render_shadow_models("gl_dbg_adv_render_shadow_models", true, "Render model shadow volumes?", 0);
static VCvarB gl_dbg_adv_render_fog_models("gl_dbg_adv_render_fog_models", true, "Render model fog?", 0);


//==========================================================================
//
//  dumpShaderLocs
//
//==========================================================================
static VVA_OKUNUSED void dumpShaderLocs (VOpenGLDrawer *drawer, GLuint prog) {
  char name[1024];
  GLint unicount;
  GLint size;
  GLenum type;
  drawer->p_glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &unicount);
  GCon->Logf("=== active uniforms: %d ===", unicount);
  for (int f = 0; f < unicount; ++f) {
    drawer->p_glGetActiveUniformARB(prog, (unsigned)f, sizeof(name), nullptr, &size, &type, name);
    GCon->Logf("  %d: <%s> (%d : %s)", f, name, size, drawer->glTypeName(type));
  }
  drawer->p_glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTES, &unicount);
  GCon->Logf("=== active attributes: %d ===", unicount);
  for (int f = 0; f < unicount; ++f) {
    drawer->p_glGetActiveAttribARB(prog, (unsigned)f, sizeof(name), nullptr, &size, &type, name);
    GCon->Logf("  %d: <%s> (%d : %s)", f, name, size, drawer->glTypeName(type));
  }
}


//==========================================================================
//
//  AliasSetUpTransform
//
//==========================================================================
static void AliasSetUpTransform (const TVec &modelorg, const TAVec &angles,
                                 const AliasModelTrans &Transform,
                                 VMatrix4 &RotationMatrix)
{
  TVec alias_forward, alias_right, alias_up;
  AngleVectors(angles, alias_forward, alias_right, alias_up);

  // create rotation matrix
  VMatrix4 rotmat = VMatrix4::Identity;
  for (unsigned i = 0; i < 3; ++i) {
    rotmat[i][0] = alias_forward[i];
    rotmat[i][1] = -alias_right[i];
    rotmat[i][2] = alias_up[i];
  }

  // shift it
  rotmat[0][3] = modelorg.x+Transform.Shift.x;
  rotmat[1][3] = modelorg.y+Transform.Shift.y;
  rotmat[2][3] = modelorg.z+Transform.Shift.z;

  //RotationMatrix = rotmat*t3matrix;

  if (Transform.Scale.x != 1.0f || Transform.Scale.y != 1.0f || Transform.Scale.z != 1.0f) {
    // create scale+offset matrix
    VMatrix4 scalemat = VMatrix4::Identity;
    scalemat[0][0] = Transform.Scale.x;
    scalemat[1][1] = Transform.Scale.y;
    scalemat[2][2] = Transform.Scale.z;

    scalemat[0][3] = Transform.Scale.x*Transform.Offset.x;
    scalemat[1][3] = Transform.Scale.y*Transform.Offset.y;
    scalemat[2][3] = Transform.Scale.z*Transform.Offset.z;

    RotationMatrix = scalemat*rotmat;
  } else if (!Transform.Offset.isZero()) {
    // create offset matrix
    VMatrix4 scalemat = VMatrix4::Identity;
    scalemat[0][3] = Transform.Offset.x;
    scalemat[1][3] = Transform.Offset.y;
    scalemat[2][3] = Transform.Offset.z;

    RotationMatrix = scalemat*rotmat;
  } else {
    RotationMatrix = rotmat;
  }
}


//==========================================================================
//
//  AliasSetUpNormalTransform
//
//==========================================================================
static void AliasSetUpNormalTransform (const TAVec &angles, const TVec &Scale, VMatrix4 &RotationMatrix) {
  TVec alias_forward(0, 0, 0), alias_right(0, 0, 0), alias_up(0, 0, 0);
  AngleVectors(angles, alias_forward, alias_right, alias_up);

  VMatrix4 scalemat = VMatrix4::Identity;
  scalemat[0][0] = Scale.x;
  scalemat[1][1] = Scale.y;
  scalemat[2][2] = Scale.z;

  VMatrix4 rotmat = VMatrix4::Identity;
  for (int i = 0; i < 3; ++i) {
    rotmat[i][0] = alias_forward[i];
    rotmat[i][1] = -alias_right[i];
    rotmat[i][2] = alias_up[i];
  }

  //k8: it seems that the order should be reversed
  //RotationMatrix = rotmat*scalemat;
  RotationMatrix = scalemat*rotmat;

  //k8: wtf?!
  if (fabsf(Scale.x) != fabsf(Scale.y) || fabsf(Scale.x) != fabsf(Scale.z)) {
    // non-uniform scale, do full inverse transpose
    RotationMatrix = RotationMatrix.Inverse().Transpose();
  }
}


//==========================================================================
//
//  VOpenGLDrawer::UploadModel
//
//==========================================================================
void VOpenGLDrawer::UploadModel (VMeshModel *Mdl) {
  if (Mdl->Uploaded) return;

  // create buffer
  p_glGenBuffersARB(1, &Mdl->VertsBuffer);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  int Size = sizeof(VMeshSTVert)*Mdl->STVerts.length()+sizeof(TVec)*Mdl->STVerts.length()*2*Mdl->Frames.length();
  p_glBufferDataARB(GL_ARRAY_BUFFER_ARB, Size, nullptr, GL_STATIC_DRAW_ARB);

  // upload data
  // texture coords array
  p_glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(VMeshSTVert)*Mdl->STVerts.length(), &Mdl->STVerts[0]);
  // vertices array
  p_glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, sizeof(VMeshSTVert)*Mdl->STVerts.length(), sizeof(TVec)*Mdl->AllVerts.length(), &Mdl->AllVerts[0]);
  // normals array
  p_glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, sizeof(VMeshSTVert)*Mdl->STVerts.length()+sizeof(TVec)*Mdl->AllVerts.length(), sizeof(TVec)*Mdl->AllNormals.length(), &Mdl->AllNormals[0]);

  // pre-calculate offsets
  for (int i = 0; i < Mdl->Frames.length(); ++i) {
    Mdl->Frames[i].VertsOffset = sizeof(VMeshSTVert)*Mdl->STVerts.length()+i*sizeof(TVec)*Mdl->STVerts.length();
    Mdl->Frames[i].NormalsOffset = sizeof(VMeshSTVert)*Mdl->STVerts.length()+sizeof(TVec)*Mdl->AllVerts.length()+i*sizeof(TVec)*Mdl->STVerts.length();
  }

  // indexes
  p_glGenBuffersARB(1, &Mdl->IndexBuffer);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

  // vertex indicies
  p_glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 6*Mdl->Tris.length(), &Mdl->Tris[0], GL_STATIC_DRAW_ARB);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
  Mdl->Uploaded = true;
  UploadedModels.Append(Mdl);
}


//==========================================================================
//
//  VOpenGLDrawer::UnloadModels
//
//==========================================================================
void VOpenGLDrawer::UnloadModels () {
  for (int i = 0; i < UploadedModels.length(); ++i) {
    p_glDeleteBuffersARB(1, &UploadedModels[i]->VertsBuffer);
    p_glDeleteBuffersARB(1, &UploadedModels[i]->IndexBuffer);
    UploadedModels[i]->Uploaded = false;
  }
  UploadedModels.Clear();
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModel
//
//  renders alias model for lightmapped renderer
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModel (const TVec &origin, const TAVec &angles,
                                    const AliasModelTrans &Transform,
                                    VMeshModel *Mdl, int frame, int nextframe,
                                    VTexture *Skin, VTextureTranslation *Trans, int CMap,
                                    const RenderStyleInfo &ri,
                                    bool is_view_model, float Inter, bool Interpolate,
                                    bool ForceDepthUse, bool AllowTransparency, bool onlyDepth)
{
  if (is_view_model) {
    // hack the depth range to prevent view model from poking into walls
    if (CanUseRevZ()) glDepthRange(0.7f, 1.0f); else glDepthRange(0.0f, 0.3f);
  }

  if (!gl_dbg_adv_render_alias_models) return;

  UploadModel(Mdl);

  SetPicModel(Skin, Trans, CMap, (ri.isShaded() ? ri.stencilColor : 0u));
  if (ri.isShaded()) AllowTransparency = true;

  //glEnable(GL_ALPHA_TEST);
  glShadeModel(GL_SMOOTH);
  //glAlphaFunc(GL_GREATER, 0.0f);
  GLEnableBlend();

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Transform, RotationMatrix);

  if (!ri.isStenciled()) {
    SurfModel.Activate();
    SurfModel.SetTexture(0);
    SurfModel.SetModelToWorldMat(RotationMatrix);
    SurfModel.SetFogFade(ri.fade, ri.alpha);
    SurfModel.SetInAlpha(ri.alpha < 1.0f ? ri.alpha : 1.0f);
    SurfModel.SetAllowTransparency(AllowTransparency);
    SurfModel.SetInter(Inter);
    SurfModel.UploadChangedUniforms();
  } else {
    SurfModelStencil.Activate();
    SurfModelStencil.SetTexture(0);
    SurfModelStencil.SetModelToWorldMat(RotationMatrix);
    SurfModelStencil.SetFogFade(ri.fade, ri.alpha);
    SurfModelStencil.SetInAlpha(ri.alpha < 1.0f ? ri.alpha : 1.0f);
    SurfModelStencil.SetAllowTransparency(AllowTransparency);
    SurfModelStencil.SetInter(Inter);
    SurfModelStencil.SetStencilColor(((ri.stencilColor>>16)&255)/255.0f, ((ri.stencilColor>>8)&255)/255.0f, (ri.stencilColor&255)/255.0f);
    SurfModelStencil.UploadChangedUniforms();
  }

  PushDepthMask();

  //if (ri.isShaded()) GCon->Logf(NAME_Debug, "!!!!! %s (0x%08x)", *Mdl->Name, ri.stencilColor);
  SetupBlending(ri);

  {
    VMeshFrame *FrameDesc = &Mdl->Frames[frame];
    VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

    p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

    p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
    p_glEnableVertexAttribArrayARB(0);

    if (!ri.isStenciled()) {
      p_glVertexAttribPointerARB(SurfModel.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
      p_glEnableVertexAttribArrayARB(SurfModel.loc_Vert2);

      p_glVertexAttribPointerARB(SurfModel.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
      p_glEnableVertexAttribArrayARB(SurfModel.loc_TexCoord);

      SurfModel.SetLightValAttr(((ri.light>>16)&255)/255.0f, ((ri.light>>8)&255)/255.0f, (ri.light&255)/255.0f, ri.alpha);
    } else {
      p_glVertexAttribPointerARB(SurfModelStencil.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
      p_glEnableVertexAttribArrayARB(SurfModelStencil.loc_Vert2);

      p_glVertexAttribPointerARB(SurfModelStencil.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
      p_glEnableVertexAttribArrayARB(SurfModelStencil.loc_TexCoord);

      SurfModelStencil.SetLightValAttr(((ri.light>>16)&255)/255.0f, ((ri.light>>8)&255)/255.0f, (ri.light&255)/255.0f, ri.alpha);
    }

    p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

    if ((ri.alpha < 1.0f && !ForceDepthUse) || AllowTransparency) { //k8: dunno. really.
      glDepthMask(GL_FALSE);
    }

    p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);

    p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

    p_glDisableVertexAttribArrayARB(0);
    if (!ri.isStenciled()) {
      p_glDisableVertexAttribArrayARB(SurfModel.loc_Vert2);
      p_glDisableVertexAttribArrayARB(SurfModel.loc_TexCoord);
    } else {
      p_glDisableVertexAttribArrayARB(SurfModelStencil.loc_Vert2);
      p_glDisableVertexAttribArrayARB(SurfModelStencil.loc_TexCoord);
    }
    p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
  }

  glShadeModel(GL_FLAT);
  //glAlphaFunc(GL_GREATER, getAlphaThreshold());
  //glDisable(GL_ALPHA_TEST);
  //if (ri.isAdditive()) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  RestoreBlending(ri);

  if (is_view_model) glDepthRange(0.0f, 1.0f);

  if (onlyDepth) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  PopDepthMask();
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsAmbientPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsAmbientPass () {
  glEnable(GL_ALPHA_TEST);
  glShadeModel(GL_SMOOTH);
  glAlphaFunc(GL_GREATER, 0.0f);
  GLEnableBlend();
  glDepthMask(GL_TRUE);
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsAmbientPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsAmbientPass () {
  glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glShadeModel(GL_FLAT);
  glDisable(GL_ALPHA_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelAmbient
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelAmbient (const TVec &origin, const TAVec &angles,
                                           const AliasModelTrans &Transform,
                                           VMeshModel *Mdl, int frame, int nextframe,
                                           VTexture *Skin, vuint32 light, float Alpha,
                                           float Inter, bool Interpolate,
                                           bool ForceDepth, bool AllowTransparency)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  SetPicModel(Skin, nullptr, CM_Default);

  if (!gl_dbg_adv_render_ambient_models) return;

  UploadModel(Mdl);

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Transform, RotationMatrix);

  ShadowsModelAmbient.Activate();
  ShadowsModelAmbient.SetTexture(0);
  ShadowsModelAmbient.SetInter(Inter);
  ShadowsModelAmbient.SetLight(((light>>16)&255)/255.0f, ((light>>8)&255)/255.0f, (light&255)/255.0f, Alpha);
  ShadowsModelAmbient.SetModelToWorldMat(RotationMatrix);

  ShadowsModelAmbient.SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f);
  ShadowsModelAmbient.SetAllowTransparency(AllowTransparency);

  ShadowsModelAmbient.UploadChangedUniforms();

  if (Alpha < 1.0f && !ForceDepth) glDepthMask(GL_FALSE);

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);

  p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_Vert2);

  p_glVertexAttribPointerARB(ShadowsModelAmbient.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  p_glEnableVertexAttribArrayARB(ShadowsModelAmbient.loc_TexCoord);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(0);
  p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_Vert2);
  p_glDisableVertexAttribArrayARB(ShadowsModelAmbient.loc_TexCoord);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

  if (Alpha < 1.0f && !ForceDepth) glDepthMask(GL_TRUE);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsLightPass
//
//  always called after `BeginLightPass()`
//
//==========================================================================
void VOpenGLDrawer::BeginModelsLightPass (const TVec &LightPos, float Radius, float LightMin, vuint32 Color, const TVec &aconeDir, const float aconeAngle) {
  if (spotLight) {
    ShadowsModelLightSpot.Activate();
    ShadowsModelLightSpot.SetTexture(0);
    ShadowsModelLightSpot.SetLightPos(LightPos);
    ShadowsModelLightSpot.SetLightRadius(Radius);
    ShadowsModelLightSpot.SetLightMin(LightMin);
    ShadowsModelLightSpot.SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f);
    ShadowsModelLightSpot.SetConeDirection(coneDir);
    ShadowsModelLightSpot.SetConeAngle(coneAngle);
  } else {
    ShadowsModelLight.Activate();
    ShadowsModelLight.SetTexture(0);
    ShadowsModelLight.SetLightPos(LightPos);
    ShadowsModelLight.SetLightRadius(Radius);
    ShadowsModelLight.SetLightMin(LightMin);
    ShadowsModelLight.SetLightColor(((Color>>16)&255)/255.0f, ((Color>>8)&255)/255.0f, (Color&255)/255.0f);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsLightPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsLightPass () {
}


#define DO_DRAW_AMDL_LIGHT(shad_)  do { \
  (shad_).SetInter(Inter); \
  (shad_).SetModelToWorldMat(RotationMatrix); \
  (shad_).SetNormalToWorldMat(NormalMat[0]); \
 \
  (shad_).SetInAlpha(Alpha < 1.0f ? Alpha : 1.0f); \
  (shad_).SetAllowTransparency(AllowTransparency); \
  /*(shad_).SetViewOrigin(vieworg);*/ \
  (shad_).UploadChangedUniforms(); \
 \
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer); \
 \
  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset); \
  p_glEnableVertexAttribArrayARB(0); \
 \
  p_glVertexAttribPointerARB((shad_).loc_VertNormal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->NormalsOffset); \
  p_glEnableVertexAttribArrayARB((shad_).loc_VertNormal); \
 \
  p_glVertexAttribPointerARB((shad_).loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset); \
  p_glEnableVertexAttribArrayARB((shad_).loc_Vert2); \
 \
  p_glVertexAttribPointerARB((shad_).loc_Vert2Normal, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->NormalsOffset); \
  p_glEnableVertexAttribArrayARB((shad_).loc_Vert2Normal); \
 \
  p_glVertexAttribPointerARB((shad_).loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0); \
  p_glEnableVertexAttribArrayARB((shad_).loc_TexCoord); \
 \
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer); \
  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0); \
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0); \
 \
  p_glDisableVertexAttribArrayARB(0); \
  p_glDisableVertexAttribArrayARB((shad_).loc_VertNormal); \
  p_glDisableVertexAttribArrayARB((shad_).loc_Vert2); \
  p_glDisableVertexAttribArrayARB((shad_).loc_Vert2Normal); \
  p_glDisableVertexAttribArrayARB((shad_).loc_TexCoord); \
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0); \
} while (0)


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelLight
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelLight (const TVec &origin, const TAVec &angles,
                                         const AliasModelTrans &Transform,
                                         VMeshModel *Mdl, int frame, int nextframe,
                                         VTexture *Skin, float Alpha, float Inter,
                                         bool Interpolate, bool AllowTransparency)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Transform, RotationMatrix);
  VMatrix4 normalmatrix;
  AliasSetUpNormalTransform(angles, Transform.Scale, normalmatrix);

  float NormalMat[3][3];
  NormalMat[0][0] = normalmatrix[0][0];
  NormalMat[0][1] = normalmatrix[0][1];
  NormalMat[0][2] = normalmatrix[0][2];
  NormalMat[1][0] = normalmatrix[1][0];
  NormalMat[1][1] = normalmatrix[1][1];
  NormalMat[1][2] = normalmatrix[1][2];
  NormalMat[2][0] = normalmatrix[2][0];
  NormalMat[2][1] = normalmatrix[2][1];
  NormalMat[2][2] = normalmatrix[2][2];

  SetPicModel(Skin, nullptr, CM_Default);

  if (!gl_dbg_adv_render_light_models) return;

  UploadModel(Mdl);

  if (spotLight) {
    DO_DRAW_AMDL_LIGHT(ShadowsModelLightSpot);
  } else {
    DO_DRAW_AMDL_LIGHT(ShadowsModelLight);
  }
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsShadowsPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsShadowsPass (TVec &LightPos, float LightRadius) {
  ShadowsModelShadow.Activate();
  ShadowsModelShadow.SetLightPos(LightPos);
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsShadowsPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsShadowsPass () {
}


#define outv(idx, offs) do { \
  ShadowsModelShadow.SetOffsetAttr(offs); \
  glArrayElement(index ## idx); \
} while (0)

//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelShadow
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelShadow (const TVec &origin, const TAVec &angles,
                                          const AliasModelTrans &Transform,
                                          VMeshModel *Mdl, int frame, int nextframe,
                                          float Inter, bool Interpolate,
                                          const TVec &LightPos, float LightRadius)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  if (!gl_dbg_adv_render_shadow_models) return;

  UploadModel(Mdl);

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Transform, RotationMatrix);

  //VMatrix4 InvRotationMatrix = RotationMatrix.Inverse();
  VMatrix4 InvRotationMatrix = RotationMatrix;
  InvRotationMatrix.invert();
  //TVec LocalLightPos = InvRotationMatrix.Transform(LightPos);
  TVec LocalLightPos = LightPos*InvRotationMatrix;
  //TVec LocalLightPos = RotationMatrix*(LightPos-origin)+LightPos;

  static vuint8 *psPool = nullptr;
  static int psPoolSize = 0;

  if (psPoolSize < Mdl->Tris.length()) {
    psPoolSize = (Mdl->Tris.length()|0xfff)+1;
    psPool = (vuint8 *)Z_Realloc(psPool, psPoolSize*sizeof(vuint8));
  }

  vuint8 *PlaneSides = psPool;

  VMeshFrame *PlanesFrame = (Inter >= 0.5f ? NextFrameDesc : FrameDesc);
  TPlane *P = PlanesFrame->Planes;
  int xcount = 0;
  for (int i = 0; i < Mdl->Tris.length(); ++i, ++P) {
    // planes facing to the light
    const float pdist = DotProduct(LocalLightPos, P->normal)-P->dist;
    PlaneSides[i] = (pdist > 0.0f && pdist < LightRadius);
    xcount += PlaneSides[i];
  }
  if (xcount == 0) {
    //GCon->Logf("WTF?! xcount=%d", xcount);
    return;
  }

  ShadowsModelShadow.SetInter(Inter);
  ShadowsModelShadow.SetModelToWorldMat(RotationMatrix);
  ShadowsModelShadow.UploadChangedUniforms();

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);
  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);
  p_glVertexAttribPointerARB(ShadowsModelShadow.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelShadow.loc_Vert2);

  // caps
  if (!usingZPass && !gl_dbg_use_zpass) {
    currentActiveShader->UploadChangedUniforms();
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < Mdl->Tris.length(); ++i) {
      if (PlaneSides[i]) {
        ShadowsModelShadow.SetOffsetAttr(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[0]);
        ShadowsModelShadow.SetOffsetAttr(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[1]);
        ShadowsModelShadow.SetOffsetAttr(1.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[2]);
      }
    }

    for (int i = 0; i < Mdl->Tris.length(); ++i) {
      if (PlaneSides[i]) {
        ShadowsModelShadow.SetOffsetAttr(0.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[2]);
        ShadowsModelShadow.SetOffsetAttr(0.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[1]);
        ShadowsModelShadow.SetOffsetAttr(0.0f);
        glArrayElement(Mdl->Tris[i].VertIndex[0]);
      }
    }
    glEnd();
  }

  for (int i = 0; i < Mdl->Edges.length(); ++i) {
    // edges with no matching pair are drawn only if corresponding triangle
    // is facing light, other are drawn if facing light changes
    if ((Mdl->Edges[i].Tri2 == -1 && PlaneSides[Mdl->Edges[i].Tri1]) ||
        (Mdl->Edges[i].Tri2 != -1 && PlaneSides[Mdl->Edges[i].Tri1] != PlaneSides[Mdl->Edges[i].Tri2]))
    {
      int index1 = Mdl->Edges[i].Vert1;
      int index2 = Mdl->Edges[i].Vert2;

      currentActiveShader->UploadChangedUniforms();
      glBegin(GL_TRIANGLE_STRIP);
      if (PlaneSides[Mdl->Edges[i].Tri1]) {
        outv(1, 1.0f);
        outv(1, 0.0f);
        outv(2, 1.0f);
        outv(2, 0.0f);
      } else {
        outv(2, 1.0f);
        outv(2, 0.0f);
        outv(1, 1.0f);
        outv(1, 0.0f);
      }
      glEnd();
    }
  }

  p_glDisableVertexAttribArrayARB(0);
  p_glDisableVertexAttribArrayARB(ShadowsModelShadow.loc_Vert2);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

#undef outv


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsTexturesPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsTexturesPass () {
  PushDepthMask();
  GLEnableBlend();
  glDisable(GL_ALPHA_TEST);
  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_EQUAL);
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsTexturesPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsTexturesPass () {
  PopDepthMask();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelTextures
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelTextures (const TVec &origin, const TAVec &angles,
                                            const AliasModelTrans &Transform,
                                            VMeshModel *Mdl, int frame, int nextframe,
                                            VTexture *Skin, VTextureTranslation *Trans, int CMap,
                                            const RenderStyleInfo &ri, float Inter,
                                            bool Interpolate, bool ForceDepth, bool AllowTransparency)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  SetPicModel(Skin, Trans, CMap);

  if (!gl_dbg_adv_render_textures_models) return;

  UploadModel(Mdl);

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Transform, RotationMatrix);

  if (!ri.isStenciled()) {
    ShadowsModelTextures.Activate();
    ShadowsModelTextures.SetTexture(0);
    ShadowsModelTextures.SetInter(Inter);
    ShadowsModelTextures.SetModelToWorldMat(RotationMatrix);

    ShadowsModelTextures.SetInAlpha(ri.alpha < 1.0f ? ri.alpha : 1.0f);
    ShadowsModelTextures.SetAllowTransparency(AllowTransparency);

    ShadowsModelTextures.UploadChangedUniforms();
  } else {
    ShadowsModelTexturesStencil.Activate();
    ShadowsModelTexturesStencil.SetTexture(0);
    ShadowsModelTexturesStencil.SetInter(Inter);
    ShadowsModelTexturesStencil.SetModelToWorldMat(RotationMatrix);

    ShadowsModelTexturesStencil.SetInAlpha(ri.alpha < 1.0f ? ri.alpha : 1.0f);
    ShadowsModelTexturesStencil.SetAllowTransparency(AllowTransparency);

    ShadowsModelTexturesStencil.UploadChangedUniforms();
  }

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);

  if (!ri.isStenciled()) {
    p_glVertexAttribPointerARB(ShadowsModelTextures.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
    p_glEnableVertexAttribArrayARB(ShadowsModelTextures.loc_Vert2);

    p_glVertexAttribPointerARB(ShadowsModelTextures.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
    p_glEnableVertexAttribArrayARB(ShadowsModelTextures.loc_TexCoord);
  } else {
    p_glVertexAttribPointerARB(ShadowsModelTexturesStencil.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
    p_glEnableVertexAttribArrayARB(ShadowsModelTexturesStencil.loc_Vert2);

    p_glVertexAttribPointerARB(ShadowsModelTexturesStencil.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
    p_glEnableVertexAttribArrayARB(ShadowsModelTexturesStencil.loc_TexCoord);
  }

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);

  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(0);
  if (!ri.isStenciled()) {
    p_glDisableVertexAttribArrayARB(ShadowsModelTextures.loc_Vert2);
    p_glDisableVertexAttribArrayARB(ShadowsModelTextures.loc_TexCoord);
  } else {
    p_glDisableVertexAttribArrayARB(ShadowsModelTexturesStencil.loc_Vert2);
    p_glDisableVertexAttribArrayARB(ShadowsModelTexturesStencil.loc_TexCoord);
  }
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}


//==========================================================================
//
//  VOpenGLDrawer::BeginModelsFogPass
//
//==========================================================================
void VOpenGLDrawer::BeginModelsFogPass () {
  PushDepthMask();
  glDisable(GL_ALPHA_TEST);
  //glAlphaFunc(GL_GREATER, 0.0f);
  GLEnableBlend();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // fog is not premultiplied
  glDepthMask(GL_FALSE);
  glDepthFunc(GL_EQUAL);
}


//==========================================================================
//
//  VOpenGLDrawer::EndModelsFogPass
//
//==========================================================================
void VOpenGLDrawer::EndModelsFogPass () {
  PopDepthMask();
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  //glAlphaFunc(GL_GREATER, getAlphaThreshold());
  glShadeModel(GL_FLAT);
  //glDisable(GL_ALPHA_TEST);
}


//==========================================================================
//
//  VOpenGLDrawer::DrawAliasModelFog
//
//==========================================================================
void VOpenGLDrawer::DrawAliasModelFog (const TVec &origin, const TAVec &angles,
                                       const AliasModelTrans &Transform,
                                       VMeshModel *Mdl, int frame, int nextframe,
                                       VTexture *Skin, vuint32 Fade, float Alpha, float Inter,
                                       bool Interpolate, bool AllowTransparency)
{
  VMeshFrame *FrameDesc = &Mdl->Frames[frame];
  VMeshFrame *NextFrameDesc = &Mdl->Frames[nextframe];

  if (!gl_dbg_adv_render_fog_models) return;

  UploadModel(Mdl);

  SetPicModel(Skin, nullptr, CM_Default);

  GLint oldDepthMask;
  glGetIntegerv(GL_DEPTH_WRITEMASK, &oldDepthMask);

  VMatrix4 RotationMatrix;
  AliasSetUpTransform(origin, angles, Transform, RotationMatrix);

  ShadowsModelFog.Activate();
  ShadowsModelFog.SetTexture(0);
  ShadowsModelFog.SetInter(Inter);
  ShadowsModelFog.SetModelToWorldMat(RotationMatrix);
  ShadowsModelFog.SetFogFade(Fade, Alpha);
  ShadowsModelFog.SetAllowTransparency(AllowTransparency);
  ShadowsModelFog.UploadChangedUniforms();

  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, Mdl->VertsBuffer);

  p_glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)FrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(0);

  p_glVertexAttribPointerARB(ShadowsModelFog.loc_Vert2, 3, GL_FLOAT, GL_FALSE, 0, (void *)(size_t)NextFrameDesc->VertsOffset);
  p_glEnableVertexAttribArrayARB(ShadowsModelFog.loc_Vert2);

  p_glVertexAttribPointerARB(ShadowsModelFog.loc_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);
  p_glEnableVertexAttribArrayARB(ShadowsModelFog.loc_TexCoord);

  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, Mdl->IndexBuffer);
  p_glDrawRangeElements(GL_TRIANGLES, 0, Mdl->STVerts.length()-1, Mdl->Tris.length()*3, GL_UNSIGNED_SHORT, 0);
  p_glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

  p_glDisableVertexAttribArrayARB(0);
  p_glDisableVertexAttribArrayARB(ShadowsModelFog.loc_Vert2);
  p_glDisableVertexAttribArrayARB(ShadowsModelFog.loc_TexCoord);
  p_glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}
