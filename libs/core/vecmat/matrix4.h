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

class VMatrix4 {
public:
  float m[4][4];

private:
  VVA_FORCEINLINE float calcMinor (const size_t r0, const size_t r1, const size_t r2,
                                     const size_t c0, const size_t c1, const size_t c2) const noexcept
  {
    return
      VSUM3(
          m[r0][c0]*VSUM2(m[r1][c1]*m[r2][c2], -(m[r2][c1]*m[r1][c2])),
        -(m[r0][c1]*VSUM2(m[r1][c0]*m[r2][c2], -(m[r2][c0]*m[r1][c2]))),
          m[r0][c2]*VSUM2(m[r1][c0]*m[r2][c1], -(m[r2][c0]*m[r1][c1])));
  }

public:
  static const VMatrix4 Identity;

  VVA_FORCEINLINE VMatrix4 () noexcept {}
  VMatrix4 (float m00, float m01, float m02, float m03,
    float m10, float m11, float m12, float m13,
    float m20, float m21, float m22, float m23,
    float m30, float m31, float m32, float m33) noexcept;
  VVA_FORCEINLINE VMatrix4 (const float *m2) noexcept { memcpy(m, m2, sizeof(m)); }
  VVA_FORCEINLINE VMatrix4 (const VMatrix4 &m2) noexcept { memcpy(m, m2.m, sizeof(m)); }
  VVA_FORCEINLINE VMatrix4 (const TVec &v) noexcept {
    memcpy((void *)m, (const void *)Identity.m, sizeof(m));
    m[0][0] = v.x;
    m[1][1] = v.y;
    m[2][2] = v.z;
  }

  VVA_FORCEINLINE void SetIdentity () noexcept { memcpy((void *)m, (const void *)Identity.m, sizeof(m)); }
  VVA_FORCEINLINE void SetZero () noexcept { memset((void *)m, 0, sizeof(m)); }

  VVA_FORCEINLINE void operator = (const VMatrix4 &m2) noexcept { if (&m2 != this) memcpy(m, m2.m, sizeof(m)); }

  VVA_FORCEINLINE float *operator [] (const int i) noexcept { return m[i]; }
  VVA_FORCEINLINE const float *operator [] (const int i) const noexcept { return m[i]; }

  VVA_CHECKRESULT VVA_FORCEINLINE const TVec &GetRow (const int idx) const noexcept { return *(const TVec *)(m[idx]); }
  //VVA_CHECKRESULT VVA_FORCEINLINE TVec GetRow (const int idx) const noexcept { return *(TVec *)(m[idx]); }
  VVA_CHECKRESULT VVA_FORCEINLINE TVec GetCol (const int idx) const noexcept { return TVec(m[0][idx], m[1][idx], m[2][idx]); }

  VVA_CHECKRESULT VMatrix4 operator * (const VMatrix4 &mt) const noexcept {
    return VMatrix4(
      VSUM4(m[0][0]*mt.m[0][0],m[1][0]*mt.m[0][1],m[2][0]*mt.m[0][2],m[3][0]*mt.m[0][3]),VSUM4(m[0][1]*mt.m[0][0],m[1][1]*mt.m[0][1],m[2][1]*mt.m[0][2],m[3][1]*mt.m[0][3]),VSUM4(m[0][2]*mt.m[0][0],m[1][2]*mt.m[0][1],m[2][2]*mt.m[0][2],m[3][2]*mt.m[0][3]),VSUM4(m[0][3]*mt.m[0][0],m[1][3]*mt.m[0][1],m[2][3]*mt.m[0][2],m[3][3]*mt.m[0][3]),
      VSUM4(m[0][0]*mt.m[1][0],m[1][0]*mt.m[1][1],m[2][0]*mt.m[1][2],m[3][0]*mt.m[1][3]),VSUM4(m[0][1]*mt.m[1][0],m[1][1]*mt.m[1][1],m[2][1]*mt.m[1][2],m[3][1]*mt.m[1][3]),VSUM4(m[0][2]*mt.m[1][0],m[1][2]*mt.m[1][1],m[2][2]*mt.m[1][2],m[3][2]*mt.m[1][3]),VSUM4(m[0][3]*mt.m[1][0],m[1][3]*mt.m[1][1],m[2][3]*mt.m[1][2],m[3][3]*mt.m[1][3]),
      VSUM4(m[0][0]*mt.m[2][0],m[1][0]*mt.m[2][1],m[2][0]*mt.m[2][2],m[3][0]*mt.m[2][3]),VSUM4(m[0][1]*mt.m[2][0],m[1][1]*mt.m[2][1],m[2][1]*mt.m[2][2],m[3][1]*mt.m[2][3]),VSUM4(m[0][2]*mt.m[2][0],m[1][2]*mt.m[2][1],m[2][2]*mt.m[2][2],m[3][2]*mt.m[2][3]),VSUM4(m[0][3]*mt.m[2][0],m[1][3]*mt.m[2][1],m[2][3]*mt.m[2][2],m[3][3]*mt.m[2][3]),
      VSUM4(m[0][0]*mt.m[3][0],m[1][0]*mt.m[3][1],m[2][0]*mt.m[3][2],m[3][0]*mt.m[3][3]),VSUM4(m[0][1]*mt.m[3][0],m[1][1]*mt.m[3][1],m[2][1]*mt.m[3][2],m[3][1]*mt.m[3][3]),VSUM4(m[0][2]*mt.m[3][0],m[1][2]*mt.m[3][1],m[2][2]*mt.m[3][2],m[3][2]*mt.m[3][3]),VSUM4(m[0][3]*mt.m[3][0],m[1][3]*mt.m[3][1],m[2][3]*mt.m[3][2],m[3][3]*mt.m[3][3])
    );
  }

  VMatrix4 &operator *= (const VMatrix4 &mt) noexcept {
    const VMatrix4 res(
      VSUM4(m[0][0]*mt.m[0][0],m[1][0]*mt.m[0][1],m[2][0]*mt.m[0][2],m[3][0]*mt.m[0][3]),VSUM4(m[0][1]*mt.m[0][0],m[1][1]*mt.m[0][1],m[2][1]*mt.m[0][2],m[3][1]*mt.m[0][3]),VSUM4(m[0][2]*mt.m[0][0],m[1][2]*mt.m[0][1],m[2][2]*mt.m[0][2],m[3][2]*mt.m[0][3]),VSUM4(m[0][3]*mt.m[0][0],m[1][3]*mt.m[0][1],m[2][3]*mt.m[0][2],m[3][3]*mt.m[0][3]),
      VSUM4(m[0][0]*mt.m[1][0],m[1][0]*mt.m[1][1],m[2][0]*mt.m[1][2],m[3][0]*mt.m[1][3]),VSUM4(m[0][1]*mt.m[1][0],m[1][1]*mt.m[1][1],m[2][1]*mt.m[1][2],m[3][1]*mt.m[1][3]),VSUM4(m[0][2]*mt.m[1][0],m[1][2]*mt.m[1][1],m[2][2]*mt.m[1][2],m[3][2]*mt.m[1][3]),VSUM4(m[0][3]*mt.m[1][0],m[1][3]*mt.m[1][1],m[2][3]*mt.m[1][2],m[3][3]*mt.m[1][3]),
      VSUM4(m[0][0]*mt.m[2][0],m[1][0]*mt.m[2][1],m[2][0]*mt.m[2][2],m[3][0]*mt.m[2][3]),VSUM4(m[0][1]*mt.m[2][0],m[1][1]*mt.m[2][1],m[2][1]*mt.m[2][2],m[3][1]*mt.m[2][3]),VSUM4(m[0][2]*mt.m[2][0],m[1][2]*mt.m[2][1],m[2][2]*mt.m[2][2],m[3][2]*mt.m[2][3]),VSUM4(m[0][3]*mt.m[2][0],m[1][3]*mt.m[2][1],m[2][3]*mt.m[2][2],m[3][3]*mt.m[2][3]),
      VSUM4(m[0][0]*mt.m[3][0],m[1][0]*mt.m[3][1],m[2][0]*mt.m[3][2],m[3][0]*mt.m[3][3]),VSUM4(m[0][1]*mt.m[3][0],m[1][1]*mt.m[3][1],m[2][1]*mt.m[3][2],m[3][1]*mt.m[3][3]),VSUM4(m[0][2]*mt.m[3][0],m[1][2]*mt.m[3][1],m[2][2]*mt.m[3][2],m[3][2]*mt.m[3][3]),VSUM4(m[0][3]*mt.m[3][0],m[1][3]*mt.m[3][1],m[2][3]*mt.m[3][2],m[3][3]*mt.m[3][3])
    );
    memcpy(m, res.m, sizeof(m));
    return *this;
  }

  // unary minus
  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 operator - (void) const noexcept {
    return VMatrix4(
      -m[0][0], -m[0][1], -m[0][2], -m[0][3],
      -m[1][0], -m[1][1], -m[1][2], -m[1][3],
      -m[2][0], -m[2][1], -m[2][2], -m[2][3],
      -m[3][0], -m[3][1], -m[3][2], -m[3][3]
    );
  }

  // unary plus (does nothing)
  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 operator + (void) const noexcept { return VMatrix4(*this); }


  // this is for camera matrices
  VVA_CHECKRESULT VVA_FORCEINLINE TVec getUpVector () const noexcept { return TVec(m[0][1], m[1][1], m[2][1]); }
  VVA_CHECKRESULT VVA_FORCEINLINE TVec getRightVector () const noexcept { return TVec(m[0][0], m[1][0], m[2][0]); }
  VVA_CHECKRESULT VVA_FORCEINLINE TVec getForwardVector () const noexcept { return TVec(m[0][2], m[1][2], m[2][2]); }


  VVA_CHECKRESULT VVA_FORCEINLINE float Determinant () const noexcept {
    return
      VSUM4(
        m[0][0]*calcMinor(1, 2, 3, 1, 2, 3),
      -(m[0][1]*calcMinor(1, 2, 3, 0, 2, 3)),
        m[0][2]*calcMinor(1, 2, 3, 0, 1, 3),
      -(m[0][3]*calcMinor(1, 2, 3, 0, 1, 2)));
  }


  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 Transpose () const noexcept {
    VMatrix4 res;
    for (unsigned i = 0; i < 4; ++i) {
      for (unsigned j = 0; j < 4; ++j) {
        res.m[j][i] = m[i][j];
      }
    }
    return res;
  }

  // more matrix operations (not used in k8vavoom, but nice to have)
  // this is for OpenGL coordinate system
  static VVA_CHECKRESULT VMatrix4 RotateX (float angle) noexcept;
  static VVA_CHECKRESULT VMatrix4 RotateY (float angle) noexcept;
  static VVA_CHECKRESULT VMatrix4 RotateZ (float angle) noexcept;

  static VVA_CHECKRESULT VMatrix4 Translate (const TVec &v) noexcept;
  static VVA_CHECKRESULT VMatrix4 TranslateNeg (const TVec &v) noexcept;

  static VVA_CHECKRESULT VMatrix4 Scale (const TVec &v) noexcept;
  static VVA_CHECKRESULT VMatrix4 Rotate (const TVec &v) noexcept; // x, y and z are angles; does x, then y, then z

  // for camera; x is pitch (up/down); y is yaw (left/right); z is roll (tilt)
  static VVA_CHECKRESULT VMatrix4 RotateZXY (const TVec &v) noexcept;
  static VVA_CHECKRESULT VMatrix4 RotateZXY (const TAVec &v) noexcept;


  // ignore translation column
  VVA_CHECKRESULT VVA_FORCEINLINE TVec RotateVector (const TVec &V) const noexcept {
    return TVec(
      VSUM3(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z),
      VSUM3(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z),
      VSUM3(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z));
  }

  // ignore rotation part
  VVA_CHECKRESULT VVA_FORCEINLINE TVec TranslateVector (const TVec &V) const noexcept { return TVec(VSUM2(V.x, m[3][0]), VSUM2(V.y, m[3][1]), VSUM2(V.z, m[3][2])); }


  // inversions
  // general inversion
  VVA_CHECKRESULT VMatrix4 Inverse () const noexcept;
  VMatrix4 &InverseInPlace () noexcept;

  // WARNING: this assumes uniform scaling
  VVA_FORCEINLINE VVA_CHECKRESULT VMatrix4 InverseFast () const noexcept { return invertedFast(); }

  // partially ;-) taken from DarkPlaces
  // WARNING: this assumes uniform scaling
  VVA_CHECKRESULT VMatrix4 invertedFast () const noexcept;
  VMatrix4 &invertFast () noexcept;

  // compute the inverse of 4x4 Euclidean transformation matrix
  //
  // Euclidean transformation is translation, rotation, and reflection.
  // With Euclidean transform, only the position and orientation of the object
  // will be changed. Euclidean transform does not change the shape of an object
  // (no scaling). Length and angle are prereserved.
  //
  // Use inverseAffine() if the matrix has scale and shear transformation.
  VMatrix4 &invertEuclidean () noexcept;

  // compute the inverse of a 4x4 affine transformation matrix
  //
  // Affine transformations are generalizations of Euclidean transformations.
  // Affine transformation includes translation, rotation, reflection, scaling,
  // and shearing. Length and angle are NOT preserved.
  VMatrix4 &invertAffine () noexcept;

  // compute the inverse of a general 4x4 matrix using Cramer's Rule
  // if cannot find inverse, it returns indentity matrix
  VMatrix4 &invertGeneral () noexcept;

  // general matrix inversion
  VMatrix4 &invert () noexcept;

  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 inverted () const noexcept { VMatrix4 res(*this); res.invert(); return res; }


  // vector transformations
  VVA_CHECKRESULT VVA_FORCEINLINE TVec Transform (const TVec &V) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]),
      VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]),
      VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]));
  }

  VVA_CHECKRESULT VVA_FORCEINLINE TVec Transform (const TVec &V, const float w) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]*w),
      VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]*w),
      VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]*w));
  }

  // this can be used to transform with OpenGL model/projection matrices
  VVA_CHECKRESULT VVA_FORCEINLINE TVec Transform2 (const TVec &V) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]),
      VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]),
      VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]));
  }

  // this can be used to transform with OpenGL model/projection matrices
  VVA_CHECKRESULT VVA_FORCEINLINE TVec Transform2OnlyXY (const TVec &V) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]),
      VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]),
      V.z); // meh
  }

  // this can be used to transform with OpenGL model/projection matrices
  VVA_CHECKRESULT VVA_FORCEINLINE float Transform2OnlyZ (const TVec &V) const noexcept {
    return VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]);
  }

  // this can be used to transform with OpenGL model/projection matrices
  VVA_CHECKRESULT VVA_FORCEINLINE TVec Transform2 (const TVec &V, const float w) const noexcept {
    return TVec(
      VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]*w),
      VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]*w),
      VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]*w));
  }

  // returns `w`
  VVA_FORCEINLINE float TransformInPlace (TVec &V) const noexcept {
    const float newx = VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]);
    const float newy = VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]);
    const float newz = VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]);
    const float neww = VSUM4(m[3][0]*V.x, m[3][1]*V.y, m[3][2]*V.z, m[3][3]);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }

  // returns `w`
  VVA_FORCEINLINE float TransformInPlace (TVec &V, float w) const noexcept {
    const float newx = VSUM4(m[0][0]*V.x, m[0][1]*V.y, m[0][2]*V.z, m[0][3]*w);
    const float newy = VSUM4(m[1][0]*V.x, m[1][1]*V.y, m[1][2]*V.z, m[1][3]*w);
    const float newz = VSUM4(m[2][0]*V.x, m[2][1]*V.y, m[2][2]*V.z, m[2][3]*w);
    const float neww = VSUM4(m[3][0]*V.x, m[3][1]*V.y, m[3][2]*V.z, m[3][3]*w);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }

  // returns `w`
  // this can be used to transform with OpenGL model/projection matrices
  VVA_FORCEINLINE float Transform2InPlace (TVec &V) const noexcept {
    const float newx = VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]);
    const float newy = VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]);
    const float newz = VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]);
    const float neww = VSUM4(m[0][3]*V.x, m[1][3]*V.y, m[2][3]*V.z, m[3][3]);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }

  // returns `w`
  // this can be used to transform with OpenGL model/projection matrices
  VVA_FORCEINLINE float Transform2InPlace (TVec &V, const float w) const noexcept {
    const float newx = VSUM4(m[0][0]*V.x, m[1][0]*V.y, m[2][0]*V.z, m[3][0]*w);
    const float newy = VSUM4(m[0][1]*V.x, m[1][1]*V.y, m[2][1]*V.z, m[3][1]*w);
    const float newz = VSUM4(m[0][2]*V.x, m[1][2]*V.y, m[2][2]*V.z, m[3][2]*w);
    const float neww = VSUM4(m[0][3]*V.x, m[1][3]*V.y, m[2][3]*V.z, m[3][3]*w);
    V.x = newx;
    V.y = newy;
    V.z = newz;
    return neww;
  }


  // combine the two matrices (multiply projection by modelview)
  // destroys (ignores) current matrix
  void ModelProjectCombine (const VMatrix4 &model, const VMatrix4 &proj) noexcept;


  // frustum operations
  static void CombineAndExtractFrustum (const VMatrix4 &model, const VMatrix4 &proj, TPlane planes[6]) noexcept;

  // this expects result of `ModelProjectCombine()`
  void ExtractFrustum (TPlane planes[6]) const noexcept;

  // the following expects result of `ModelProjectCombine()`
  void ExtractFrustumLeft (TPlane &plane) const noexcept;
  void ExtractFrustumRight (TPlane &plane) const noexcept;
  void ExtractFrustumTop (TPlane &plane) const noexcept;
  void ExtractFrustumBottom (TPlane &plane) const noexcept;
  void ExtractFrustumFar (TPlane &plane) const noexcept;
  void ExtractFrustumNear (TPlane &plane) const noexcept;

  // same as `glFrustum()`
  static VMatrix4 Frustum (float left, float right, float bottom, float top, float nearVal, float farVal) noexcept;
  // same as `glOrtho()`
  static VMatrix4 Ortho (float left, float right, float bottom, float top, float nearVal, float farVal) noexcept;
  // same as `gluPerspective()`
  // sets the frustum to perspective mode
  // fovY   - Field of vision in degrees in the y direction
  // aspect - Aspect ratio of the viewport
  // zNear  - The near clipping distance
  // zFar   - The far clipping distance
  static VVA_CHECKRESULT VMatrix4 Perspective (float fovY, float aspect, float zNear, float zFar) noexcept;

  static VVA_CHECKRESULT VMatrix4 LookAtFucked (const TVec &eye, const TVec &center, const TVec &up) noexcept;

  // for [-1..1]
  static VVA_CHECKRESULT VMatrix4 ProjectionNegOne (float fovY, float aspect, float zNear, float zFar) noexcept;
  // for [0..1]
  static VVA_CHECKRESULT VMatrix4 ProjectionZeroOne (float fovY, float aspect, float zNear, float zFar) noexcept;

  static VVA_CHECKRESULT VMatrix4 LookAtGLM (const TVec &eye, const TVec &center, const TVec &up) noexcept;

  // does `gluLookAt()`
  VVA_CHECKRESULT VMatrix4 lookAt (const TVec &eye, const TVec &center, const TVec &up) const noexcept;

  // does `gluLookAt()`
  static VVA_FORCEINLINE VVA_CHECKRESULT VMatrix4 LookAt (const TVec &eye, const TVec &center, const TVec &up) noexcept { VMatrix4 m; m.SetIdentity(); return m.lookAt(eye, center, up); }

  // rotate matrix to face along the target direction
  // this function will clear the previous rotation and scale, but it will keep the previous translation
  // it is for rotating object to look at the target, NOT for camera
  VMatrix4 &lookingAt (const TVec &target) noexcept;
  VMatrix4 &lookingAt (const TVec &target, const TVec &upVec) noexcept;

  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 lookAt (const TVec &target) const noexcept { auto res = VMatrix4(*this); return res.lookingAt(target); }
  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 lookAt (const TVec &target, const TVec &upVec) const noexcept { auto res = VMatrix4(*this); return res.lookingAt(target, upVec); }

  VMatrix4 &rotate (float angle, const TVec &axis) noexcept;
  VMatrix4 &rotateX (float angle) noexcept;
  VMatrix4 &rotateY (float angle) noexcept;
  VMatrix4 &rotateZ (float angle) noexcept;

  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 rotated (float angle, const TVec &axis) const noexcept { auto res = VMatrix4(*this); return res.rotate(angle, axis); }
  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 rotatedX (float angle) const noexcept { auto res = VMatrix4(*this); return res.rotateX(angle); }
  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 rotatedY (float angle) const noexcept { auto res = VMatrix4(*this); return res.rotateY(angle); }
  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 rotatedZ (float angle) const noexcept { auto res = VMatrix4(*this); return res.rotateZ(angle); }

  // retrieve angles in degree from rotation matrix, M = Rx*Ry*Rz, in degrees
  // Rx: rotation about X-axis, pitch
  // Ry: rotation about Y-axis, yaw (heading)
  // Rz: rotation about Z-axis, roll
  VVA_CHECKRESULT TAVec getAngles () const noexcept;

  VVA_CHECKRESULT VVA_FORCEINLINE VMatrix4 transposed () const noexcept {
    return VMatrix4(
      m[0][0], m[1][0], m[2][0], m[3][0],
      m[0][1], m[1][1], m[2][1], m[3][1],
      m[0][2], m[1][2], m[2][2], m[3][2],
      m[0][3], m[1][3], m[2][3], m[3][3]
    );
  }

  // blends two matrices together, at a given percentage (range is [0..1]), blend==0: m2 is ignored
  // WARNING! it won't sanitize `blend`
  VVA_CHECKRESULT VMatrix4 blended (const VMatrix4 &m2, float blend) const noexcept;

  void toQuaternion (float quat[4]) const noexcept;
};


// ////////////////////////////////////////////////////////////////////////// //
// use this to get vector transformed by matrix
// i.e. you should multiply matrix by vector to apply matrix transformations
// WARNING: multiplying vector by matrix gives something completely different
static VVA_FORCEINLINE VVA_OKUNUSED TVec operator * (const VMatrix4 &mt, const TVec &v) noexcept {
  return TVec(
    VSUM4(mt.m[0][0]*v.x, mt.m[1][0]*v.y, mt.m[2][0]*v.z, mt.m[3][0]),
    VSUM4(mt.m[0][1]*v.x, mt.m[1][1]*v.y, mt.m[2][1]*v.z, mt.m[3][1]),
    VSUM4(mt.m[0][2]*v.x, mt.m[1][2]*v.y, mt.m[2][2]*v.z, mt.m[3][2]));
}


static VVA_FORCEINLINE VVA_OKUNUSED TVec operator * (const TVec &v, const VMatrix4 &mt) noexcept {
  return TVec(
    VSUM4(mt.m[0][0]*v.x, mt.m[0][1]*v.y, mt.m[0][2]*v.z, mt.m[0][3]),
    VSUM4(mt.m[1][0]*v.x, mt.m[1][1]*v.y, mt.m[1][2]*v.z, mt.m[1][3]),
    VSUM4(mt.m[2][0]*v.x, mt.m[2][1]*v.y, mt.m[2][2]*v.z, mt.m[2][3]));
}
