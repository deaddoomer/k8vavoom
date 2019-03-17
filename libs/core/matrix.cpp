//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
#include "core.h"


const VMatrix4 VMatrix4::Identity(
  1, 0, 0, 0,
  0, 1, 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1
);


//==========================================================================
//
//  VMatrix4::VMatrix4
//
//==========================================================================
VMatrix4::VMatrix4 (float m00, float m01, float m02, float m03,
  float m10, float m11, float m12, float m13,
  float m20, float m21, float m22, float m23,
  float m30, float m31, float m32, float m33)
{
  m[0][0] = m00;
  m[0][1] = m01;
  m[0][2] = m02;
  m[0][3] = m03;
  m[1][0] = m10;
  m[1][1] = m11;
  m[1][2] = m12;
  m[1][3] = m13;
  m[2][0] = m20;
  m[2][1] = m21;
  m[2][2] = m22;
  m[2][3] = m23;
  m[3][0] = m30;
  m[3][1] = m31;
  m[3][2] = m32;
  m[3][3] = m33;
}


//==========================================================================
//
//  VMatrix4::getRow
//
//==========================================================================
TVec VMatrix4::getRow (int idx) const {
  if (idx < 0 || idx > 3) return TVec(0, 0, 0);
  return TVec(m[0][idx], m[1][idx], m[2][idx]);
}


//==========================================================================
//
//  VMatrix4::getCol
//
//==========================================================================
TVec VMatrix4::getCol (int idx) const {
  if (idx < 0 || idx > 3) return TVec(0, 0, 0);
  return TVec(m[idx][0], m[idx][1], m[idx][2]);
}


//==========================================================================
//
//  operator VMatrix4 * VMatrix4
//
//==========================================================================
VMatrix4 VMatrix4::operator * (const VMatrix4 &in2) const {
  VMatrix4 out;
  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned j = 0; j < 4; ++j) {
      out[i][j] = VSUM4(m[i][0]*in2.m[0][j], m[i][1]*in2.m[1][j], m[i][2]*in2.m[2][j], m[i][3]*in2.m[3][j]);
    }
  }
  return out;
}


//==========================================================================
//
//  MINOR
//
//==========================================================================
inline static float MINOR (const VMatrix4 &m,
  const size_t r0, const size_t r1, const size_t r2,
  const size_t c0, const size_t c1, const size_t c2)
{
  return
    VSUM3(
        m[r0][c0]*VSUM2(m[r1][c1]*m[r2][c2], -(m[r2][c1]*m[r1][c2])),
      -(m[r0][c1]*VSUM2(m[r1][c0]*m[r2][c2], -(m[r2][c0]*m[r1][c2]))),
        m[r0][c2]*VSUM2(m[r1][c0]*m[r2][c1], -(m[r2][c0]*m[r1][c1])));
  ;
}


//==========================================================================
//
//  VMatrix4::Determinant
//
//==========================================================================
float VMatrix4::Determinant () const {
  return
    VSUM4(
      m[0][0]*MINOR(*this, 1, 2, 3, 1, 2, 3),
    -(m[0][1]*MINOR(*this, 1, 2, 3, 0, 2, 3)),
      m[0][2]*MINOR(*this, 1, 2, 3, 0, 1, 3),
    -(m[0][3]*MINOR(*this, 1, 2, 3, 0, 1, 2)))
  ;
}


//==========================================================================
//
//  VMatrix4::Inverse
//
//==========================================================================
VMatrix4 VMatrix4::Inverse () const {
  const float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
  const float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
  const float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
  const float m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

  float v0 = VSUM2(m20*m31, -(m21*m30));
  float v1 = VSUM2(m20*m32, -(m22*m30));
  float v2 = VSUM2(m20*m33, -(m23*m30));
  float v3 = VSUM2(m21*m32, -(m22*m31));
  float v4 = VSUM2(m21*m33, -(m23*m31));
  float v5 = VSUM2(m22*m33, -(m23*m32));

  const float t00 = +VSUM3(v5*m11, -(v4*m12), v3*m13);
  const float t10 = -VSUM3(v5*m10, -(v2*m12), v1*m13);
  const float t20 = +VSUM3(v4*m10, -(v2*m11), v0*m13);
  const float t30 = -VSUM3(v3*m10, -(v1*m11), v0*m12);

  float invDet = 1.0f/VSUM4(t00*m00, t10*m01, t20*m02, t30*m03);
  if (!isFiniteF(invDet)) invDet = 0.0f;

  const float d00 = t00*invDet;
  const float d10 = t10*invDet;
  const float d20 = t20*invDet;
  const float d30 = t30*invDet;

  const float d01 = -VSUM3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d11 = +VSUM3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d21 = -VSUM3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d31 = +VSUM3(v3*m00, -(v1*m01), v0*m02)*invDet;

  v0 = VSUM2(m10*m31, -(m11*m30));
  v1 = VSUM2(m10*m32, -(m12*m30));
  v2 = VSUM2(m10*m33, -(m13*m30));
  v3 = VSUM2(m11*m32, -(m12*m31));
  v4 = VSUM2(m11*m33, -(m13*m31));
  v5 = VSUM2(m12*m33, -(m13*m32));

  const float d02 = +VSUM3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d12 = -VSUM3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d22 = +VSUM3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d32 = -VSUM3(v3*m00, -(v1*m01), v0*m02)*invDet;

  v0 = VSUM2(m21*m10, -(m20*m11));
  v1 = VSUM2(m22*m10, -(m20*m12));
  v2 = VSUM2(m23*m10, -(m20*m13));
  v3 = VSUM2(m22*m11, -(m21*m12));
  v4 = VSUM2(m23*m11, -(m21*m13));
  v5 = VSUM2(m23*m12, -(m22*m13));

  const float d03 = -VSUM3(v5*m01, -(v4*m02), v3*m03)*invDet;
  const float d13 = +VSUM3(v5*m00, -(v2*m02), v1*m03)*invDet;
  const float d23 = -VSUM3(v4*m00, -(v2*m01), v0*m03)*invDet;
  const float d33 = +VSUM3(v3*m00, -(v1*m01), v0*m02)*invDet;

  return VMatrix4(
    d00, d01, d02, d03,
    d10, d11, d12, d13,
    d20, d21, d22, d23,
    d30, d31, d32, d33);
}


//==========================================================================
//
//  VMatrix4::Transpose
//
//==========================================================================
VMatrix4 VMatrix4::Transpose () const {
  VMatrix4 Out;
  for (unsigned i = 0; i < 4; ++i) {
    for (unsigned j = 0; j < 4; ++j) {
      Out[j][i] = m[i][j];
    }
  }
  return Out;
}


//==========================================================================
//
//  VMatrix4::CombineAndExtractFrustum
//
//==========================================================================
void VMatrix4::CombineAndExtractFrustum (const VMatrix4 &model, const VMatrix4 &proj, TPlane planes[6]) {
  const float *projm = (const float *)proj.m;
  const float *modlm = (const float *)model.m;
  float clip[16];

  // combine the two matrices (multiply projection by modelview)
  clip[ 0] = VSUM4(modlm[ 0]*projm[ 0], modlm[ 1]*projm[ 4], modlm[ 2]*projm[ 8], modlm[ 3]*projm[12]);
  clip[ 1] = VSUM4(modlm[ 0]*projm[ 1], modlm[ 1]*projm[ 5], modlm[ 2]*projm[ 9], modlm[ 3]*projm[13]);
  clip[ 2] = VSUM4(modlm[ 0]*projm[ 2], modlm[ 1]*projm[ 6], modlm[ 2]*projm[10], modlm[ 3]*projm[14]);
  clip[ 3] = VSUM4(modlm[ 0]*projm[ 3], modlm[ 1]*projm[ 7], modlm[ 2]*projm[11], modlm[ 3]*projm[15]);

  clip[ 4] = VSUM4(modlm[ 4]*projm[ 0], modlm[ 5]*projm[ 4], modlm[ 6]*projm[ 8], modlm[ 7]*projm[12]);
  clip[ 5] = VSUM4(modlm[ 4]*projm[ 1], modlm[ 5]*projm[ 5], modlm[ 6]*projm[ 9], modlm[ 7]*projm[13]);
  clip[ 6] = VSUM4(modlm[ 4]*projm[ 2], modlm[ 5]*projm[ 6], modlm[ 6]*projm[10], modlm[ 7]*projm[14]);
  clip[ 7] = VSUM4(modlm[ 4]*projm[ 3], modlm[ 5]*projm[ 7], modlm[ 6]*projm[11], modlm[ 7]*projm[15]);

  clip[ 8] = VSUM4(modlm[ 8]*projm[ 0], modlm[ 9]*projm[ 4], modlm[10]*projm[ 8], modlm[11]*projm[12]);
  clip[ 9] = VSUM4(modlm[ 8]*projm[ 1], modlm[ 9]*projm[ 5], modlm[10]*projm[ 9], modlm[11]*projm[13]);
  clip[10] = VSUM4(modlm[ 8]*projm[ 2], modlm[ 9]*projm[ 6], modlm[10]*projm[10], modlm[11]*projm[14]);
  clip[11] = VSUM4(modlm[ 8]*projm[ 3], modlm[ 9]*projm[ 7], modlm[10]*projm[11], modlm[11]*projm[15]);

  clip[12] = VSUM4(modlm[12]*projm[ 0], modlm[13]*projm[ 4], modlm[14]*projm[ 8], modlm[15]*projm[12]);
  clip[13] = VSUM4(modlm[12]*projm[ 1], modlm[13]*projm[ 5], modlm[14]*projm[ 9], modlm[15]*projm[13]);
  clip[14] = VSUM4(modlm[12]*projm[ 2], modlm[13]*projm[ 6], modlm[14]*projm[10], modlm[15]*projm[14]);
  clip[15] = VSUM4(modlm[12]*projm[ 3], modlm[13]*projm[ 7], modlm[14]*projm[11], modlm[15]*projm[15]);

  planes[TFrustum::Right].SetAndNormalise(TVec(VSUM2(clip[3], -clip[0]), VSUM2(clip[7], -clip[4]), VSUM2(clip[11], -clip[8])), -VSUM2(clip[15], -clip[12]));
  planes[TFrustum::Left].SetAndNormalise(TVec(VSUM2(clip[3], clip[0]), VSUM2(clip[7], clip[4]), VSUM2(clip[11], clip[8])), -VSUM2(clip[15], clip[12]));
  planes[TFrustum::Bottom].SetAndNormalise(TVec(VSUM2(clip[3], clip[1]), VSUM2(clip[7], clip[5]), VSUM2(clip[11], clip[9])), -VSUM2(clip[15], clip[13]));
  planes[TFrustum::Top].SetAndNormalise(TVec(VSUM2(clip[3], -clip[1]), VSUM2(clip[7], -clip[5]), VSUM2(clip[11], -clip[9])), -VSUM2(clip[15], -clip[13]));
  planes[TFrustum::Far].SetAndNormalise(TVec(VSUM2(clip[3], -clip[2]), VSUM2(clip[7], -clip[6]), VSUM2(clip[11], -clip[10])), -VSUM2(clip[15], -clip[14]));
  planes[TFrustum::Near].SetAndNormalise(TVec(VSUM2(clip[3], clip[2]), VSUM2(clip[7], clip[6]), VSUM2(clip[11], clip[10])), -VSUM2(clip[15], clip[14]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumLeft
//
//  combine the two matrices (multiply projection by modelview)
//
//==========================================================================
void VMatrix4::ModelProjectCombine (const VMatrix4 &model, const VMatrix4 &proj) {
  const float *projm = (const float *)proj.m;
  const float *modlm = (const float *)model.m;
  float *clip = (float *)m;

  // combine the two matrices (multiply projection by modelview)
  clip[ 0] = VSUM4(modlm[ 0]*projm[ 0], modlm[ 1]*projm[ 4], modlm[ 2]*projm[ 8], modlm[ 3]*projm[12]);
  clip[ 1] = VSUM4(modlm[ 0]*projm[ 1], modlm[ 1]*projm[ 5], modlm[ 2]*projm[ 9], modlm[ 3]*projm[13]);
  clip[ 2] = VSUM4(modlm[ 0]*projm[ 2], modlm[ 1]*projm[ 6], modlm[ 2]*projm[10], modlm[ 3]*projm[14]);
  clip[ 3] = VSUM4(modlm[ 0]*projm[ 3], modlm[ 1]*projm[ 7], modlm[ 2]*projm[11], modlm[ 3]*projm[15]);

  clip[ 4] = VSUM4(modlm[ 4]*projm[ 0], modlm[ 5]*projm[ 4], modlm[ 6]*projm[ 8], modlm[ 7]*projm[12]);
  clip[ 5] = VSUM4(modlm[ 4]*projm[ 1], modlm[ 5]*projm[ 5], modlm[ 6]*projm[ 9], modlm[ 7]*projm[13]);
  clip[ 6] = VSUM4(modlm[ 4]*projm[ 2], modlm[ 5]*projm[ 6], modlm[ 6]*projm[10], modlm[ 7]*projm[14]);
  clip[ 7] = VSUM4(modlm[ 4]*projm[ 3], modlm[ 5]*projm[ 7], modlm[ 6]*projm[11], modlm[ 7]*projm[15]);

  clip[ 8] = VSUM4(modlm[ 8]*projm[ 0], modlm[ 9]*projm[ 4], modlm[10]*projm[ 8], modlm[11]*projm[12]);
  clip[ 9] = VSUM4(modlm[ 8]*projm[ 1], modlm[ 9]*projm[ 5], modlm[10]*projm[ 9], modlm[11]*projm[13]);
  clip[10] = VSUM4(modlm[ 8]*projm[ 2], modlm[ 9]*projm[ 6], modlm[10]*projm[10], modlm[11]*projm[14]);
  clip[11] = VSUM4(modlm[ 8]*projm[ 3], modlm[ 9]*projm[ 7], modlm[10]*projm[11], modlm[11]*projm[15]);

  clip[12] = VSUM4(modlm[12]*projm[ 0], modlm[13]*projm[ 4], modlm[14]*projm[ 8], modlm[15]*projm[12]);
  clip[13] = VSUM4(modlm[12]*projm[ 1], modlm[13]*projm[ 5], modlm[14]*projm[ 9], modlm[15]*projm[13]);
  clip[14] = VSUM4(modlm[12]*projm[ 2], modlm[13]*projm[ 6], modlm[14]*projm[10], modlm[15]*projm[14]);
  clip[15] = VSUM4(modlm[12]*projm[ 3], modlm[13]*projm[ 7], modlm[14]*projm[11], modlm[15]*projm[15]);
}


//==========================================================================
//
//  VMatrix4::ExtractFrustum
//
//==========================================================================
void VMatrix4::ExtractFrustum (TPlane planes[6]) const {
  const float *clip = (const float *)m;
  planes[TFrustum::Right].SetAndNormalise(TVec(VSUM2(clip[3], -clip[0]), VSUM2(clip[7], -clip[4]), VSUM2(clip[11], -clip[8])), -VSUM2(clip[15], -clip[12]));
  planes[TFrustum::Left].SetAndNormalise(TVec(VSUM2(clip[3], clip[0]), VSUM2(clip[7], clip[4]), VSUM2(clip[11], clip[8])), -VSUM2(clip[15], clip[12]));
  planes[TFrustum::Bottom].SetAndNormalise(TVec(VSUM2(clip[3], clip[1]), VSUM2(clip[7], clip[5]), VSUM2(clip[11], clip[9])), -VSUM2(clip[15], clip[13]));
  planes[TFrustum::Top].SetAndNormalise(TVec(VSUM2(clip[3], -clip[1]), VSUM2(clip[7], -clip[5]), VSUM2(clip[11], -clip[9])), -VSUM2(clip[15], -clip[13]));
  planes[TFrustum::Far].SetAndNormalise(TVec(VSUM2(clip[3], -clip[2]), VSUM2(clip[7], -clip[6]), VSUM2(clip[11], -clip[10])), -VSUM2(clip[15], -clip[14]));
  planes[TFrustum::Near].SetAndNormalise(TVec(VSUM2(clip[3], clip[2]), VSUM2(clip[7], clip[6]), VSUM2(clip[11], clip[10])), -VSUM2(clip[15], clip[14]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumLeft
//
//==========================================================================
void VMatrix4::ExtractFrustumLeft (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], clip[0]), VSUM2(clip[7], clip[4]), VSUM2(clip[11], clip[8])), -VSUM2(clip[15], clip[12]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumRight
//
//==========================================================================
void VMatrix4::ExtractFrustumRight (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], -clip[0]), VSUM2(clip[7], -clip[4]), VSUM2(clip[11], -clip[8])), -VSUM2(clip[15], -clip[12]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumTop
//
//==========================================================================
void VMatrix4::ExtractFrustumTop (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], -clip[1]), VSUM2(clip[7], -clip[5]), VSUM2(clip[11], -clip[9])), -VSUM2(clip[15], -clip[13]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumBottom
//
//==========================================================================
void VMatrix4::ExtractFrustumBottom (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], clip[1]), VSUM2(clip[7], clip[5]), VSUM2(clip[11], clip[9])), -VSUM2(clip[15], clip[13]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumFar
//
//==========================================================================
void VMatrix4::ExtractFrustumFar (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], -clip[2]), VSUM2(clip[7], -clip[6]), VSUM2(clip[11], -clip[10])), -VSUM2(clip[15], -clip[14]));
}


//==========================================================================
//
//  VMatrix4::ExtractFrustumNear
//
//==========================================================================
void VMatrix4::ExtractFrustumNear (TPlane &plane) const {
  const float *clip = (const float *)m;
  plane.SetAndNormalise(TVec(VSUM2(clip[3], clip[2]), VSUM2(clip[7], clip[6]), VSUM2(clip[11], clip[10])), -VSUM2(clip[15], clip[14]));
}
