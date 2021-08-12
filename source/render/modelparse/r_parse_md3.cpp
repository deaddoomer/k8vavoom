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
#include "../../gamedefs.h"
#include "../r_local.h"


// ////////////////////////////////////////////////////////////////////////// //
// md3

#pragma pack(push, 1)

struct MD3Header {
  char sign[4];
  vuint32 ver;
  char name[64];
  vuint32 flags;
  vuint32 frameNum;
  vuint32 tagNum;
  vuint32 surfaceNum;
  vuint32 skinNum;
  vuint32 frameOfs;
  vuint32 tagOfs;
  vuint32 surfaceOfs;
  vuint32 eofOfs;

  inline vuint32 get_ver () const noexcept { return LittleULong(ver); }
  inline vuint32 get_flags () const noexcept { return LittleULong(flags); }
  inline vuint32 get_frameNum () const noexcept { return LittleULong(frameNum); }
  inline vuint32 get_tagNum () const noexcept { return LittleULong(tagNum); }
  inline vuint32 get_surfaceNum () const noexcept { return LittleULong(surfaceNum); }
  inline vuint32 get_skinNum () const noexcept { return LittleULong(skinNum); }
  inline vuint32 get_frameOfs () const noexcept { return LittleULong(frameOfs); }
  inline vuint32 get_tagOfs () const noexcept { return LittleULong(tagOfs); }
  inline vuint32 get_surfaceOfs () const noexcept { return LittleULong(surfaceOfs); }
  inline vuint32 get_eofOfs () const noexcept { return LittleULong(eofOfs); }
};

struct MD3Frame {
  float bmin[3];
  float bmax[3];
  float origin[3];
  float radius;
  char name[16];

  inline float get_bmin (const unsigned idx) const noexcept { return LittleFloat(bmin[idx]); }
  inline float get_bmax (const unsigned idx) const noexcept { return LittleFloat(bmax[idx]); }
  inline float get_origin (const unsigned idx) const noexcept { return LittleFloat(origin[idx]); }
  inline float get_radius () const noexcept { return LittleFloat(radius); }
};

struct MD3Tag {
  char name[64];
  float origin[3];
  float axes[3][3];

  inline float get_origin (const unsigned idx) const noexcept { return LittleFloat(origin[idx]); }
  inline float get_axes (const unsigned idx0, const unsigned idx1) const noexcept { return LittleFloat(axes[idx0][idx1]); }
};

struct MD3Shader {
  char name[64];
  vuint32 index;

  inline vuint32 get_index () const noexcept { return LittleULong(index); }
};

struct MD3Tri {
  vuint32 v0, v1, v2;

  inline vuint32 get_v0 () const noexcept { return LittleULong(v0); }
  inline vuint32 get_v1 () const noexcept { return LittleULong(v1); }
  inline vuint32 get_v2 () const noexcept { return LittleULong(v2); }
};

struct MD3ST {
  float s, t;

  inline float get_s () const noexcept { return LittleFloat(s); }
  inline float get_t () const noexcept { return LittleFloat(t); }
};

struct MD3Vertex {
  vint16 x, y, z;
  vuint16 normal;

  inline vint16 get_x () const noexcept { return LittleShort(x); }
  inline vint16 get_y () const noexcept { return LittleShort(y); }
  inline vint16 get_z () const noexcept { return LittleShort(z); }
  inline vuint16 get_normal () const noexcept { return LittleUShort(normal); }
};

struct MD3Surface {
  char sign[4];
  char name[64];
  vuint32 flags;
  vuint32 frameNum;
  vuint32 shaderNum;
  vuint32 vertNum;
  vuint32 triNum;
  vuint32 triOfs;
  vuint32 shaderOfs;
  vuint32 stOfs;
  vuint32 vertOfs;
  vuint32 endOfs;

  inline vuint32 get_flags () const noexcept { return LittleULong(flags); }
  inline vuint32 get_frameNum () const noexcept { return LittleULong(frameNum); }
  inline vuint32 get_shaderNum () const noexcept { return LittleULong(shaderNum); }
  inline vuint32 get_vertNum () const noexcept { return LittleULong(vertNum); }
  inline vuint32 get_triNum () const noexcept { return LittleULong(triNum); }
  inline vuint32 get_triOfs () const noexcept { return LittleULong(triOfs); }
  inline vuint32 get_shaderOfs () const noexcept { return LittleULong(shaderOfs); }
  inline vuint32 get_stOfs () const noexcept { return LittleULong(stOfs); }
  inline vuint32 get_vertOfs () const noexcept { return LittleULong(vertOfs); }
  inline vuint32 get_endOfs () const noexcept { return LittleULong(endOfs); }
};

#pragma pack(pop)


static inline /*VVA_OKUNUSED*/ TVec md3vert (const MD3Vertex *v) { return TVec(v->get_x()/64.0f, v->get_y()/64.0f, v->get_z()/64.0f); }

static inline /*VVA_OKUNUSED*/ TVec md3vertNormal (const MD3Vertex *v) {
  const float lat = ((v->get_normal()>>8)&0xff)*(2*M_PI)/255.0f;
  const float lng = (v->get_normal()&0xff)*(2*M_PI)/255.0f;
  return TVec(cosf(lat)*sinf(lng), sinf(lat)*sinf(lng), cosf(lng));
}


//==========================================================================
//
//  VMeshModel::Load_MD3
//
//  FIXME: use `DataSize` for checks!
//
//==========================================================================
void VMeshModel::Load_MD3 (const vuint8 *Data, int DataSize) {
  (void)DataSize;
  this->Uploaded = false;
  this->VertsBuffer = 0;
  this->IndexBuffer = 0;

  const MD3Header *pmodel = (const MD3Header *)Data;

  if (pmodel->get_ver() != IDMD3_VERSION) Sys_Error("model '%s' has wrong version number (%u should be %i)", *this->Name, pmodel->get_ver(), IDMD3_VERSION);
  if (pmodel->get_frameNum() < 1 || pmodel->get_frameNum() > 1024) Sys_Error("model '%s' has invalid numebr of frames: %u", *this->Name, pmodel->get_frameNum());
  if (pmodel->get_surfaceNum() < 1) Sys_Error("model '%s' has no meshes", *this->Name);
  if ((unsigned)this->MeshIndex >= pmodel->get_surfaceNum()) Sys_Error("model '%s' has no mesh with index %d", *this->Name, this->MeshIndex);
  //if (pmodel->get_surfaceNum() > 1) GCon->Logf(NAME_Warning, "model '%s' has more than one mesh (%u); ignoring extra meshes", *this->Name, pmodel->get_surfaceNum());

  const MD3Frame *pframe = (const MD3Frame *)((const vuint8 *)pmodel+pmodel->get_frameOfs());
  const MD3Surface *pmesh = (const MD3Surface *)(Data+pmodel->get_surfaceOfs());

  // skip to relevant mesh
  for (int f = 0; f < this->MeshIndex; ++f) {
    if (memcmp(pmesh->sign, "IDP3", 4) != 0) Sys_Error("model '%s' has invalid mesh signature", *this->Name);

    if (/*pmesh->get_shaderNum() < 0 ||*/ pmesh->get_shaderNum() > 1024) Sys_Error("model '%s' has invalid number of shaders: %u", *this->Name, pmesh->get_shaderNum());
    if (pmesh->get_frameNum() != pmodel->get_frameNum()) Sys_Error("model '%s' has mismatched number of frames in mesh", *this->Name);
    //if (pmesh->get_vertNum() < 1) Sys_Error("model '%s' has no vertices", *this->Name);
    if (pmesh->get_vertNum() < 1) GCon->Logf(NAME_Error, "model '%s' has no vertices", *this->Name);
    if (pmesh->get_vertNum() > MAXALIASVERTS) Sys_Error("model '%s' has too many vertices", *this->Name);
    //if (pmesh->get_triNum() < 1) Sys_Error("model '%s' has no triangles", *this->Name);
    if (pmesh->get_triNum() < 1) GCon->Logf(NAME_Error, "model '%s' has no triangles", *this->Name);
    if (pmesh->get_triNum() > 65536) Sys_Error("model '%s' has too many triangles", *this->Name);

    pmesh = (const MD3Surface *)((const vuint8 *)pmesh+pmesh->get_endOfs());
  }

  if (r_models_verbose_loading) GCon->Logf(NAME_Debug, "*** MD3 model '%s': loading mesh #%d", *this->Name, this->MeshIndex);

  // copy shader data
  const MD3Shader *pshader = (const MD3Shader *)((const vuint8 *)pmesh+pmesh->get_shaderOfs());
  for (unsigned i = 0; i < pmesh->get_shaderNum(); ++i) {
    VStr name = getStrZ(pshader[i].name, 64).toLowerCase();
    // prepend model path
    if (!name.isEmpty()) name = this->Name.ExtractFilePath()+name.ExtractFileBaseName();
    //GCon->Logf("SKIN: %s", *name);
    VMeshModel::SkinInfo &si = this->Skins.alloc();
    si.fileName = *name;
    si.textureId = -1;
    si.shade = -1;
  }

  // copy S and T (texture coordinates)
  const MD3ST *pstverts = (const MD3ST *)((const vuint8 *)pmesh+pmesh->get_stOfs());
  this->STVerts.setLength((int)pmesh->get_vertNum());
  for (unsigned i = 0; i < pmesh->get_vertNum(); ++i) {
    if (r_models_verbose_loading) GCon->Logf(NAME_Debug, "MD3 model '%s' (mesh #%d): st#%u: s=%g; t=%g", *this->Name, this->MeshIndex, i, pstverts[i].get_s(), pstverts[i].get_t());
    this->STVerts[i].S = pstverts[i].get_s();
    this->STVerts[i].T = pstverts[i].get_t();
  }

  // copy triangles, create edges
  const MD3Tri *ptri = (const MD3Tri *)((const vuint8 *)pmesh+pmesh->get_triOfs());
  TArray<VTempEdge> Edges;
  this->Tris.setLength(pmesh->get_triNum());
  for (unsigned i = 0; i < pmesh->get_triNum(); ++i) {
    if (ptri[i].get_v0() >= pmesh->get_vertNum() || ptri[i].get_v1() >= pmesh->get_vertNum() || ptri[i].get_v2() >= pmesh->get_vertNum()) Sys_Error("model '%s' has invalid vertex index in triangle #%u", *this->Name, i);
    this->Tris[i].VertIndex[0] = ptri[i].get_v0();
    this->Tris[i].VertIndex[1] = ptri[i].get_v1();
    this->Tris[i].VertIndex[2] = ptri[i].get_v2();
    for (unsigned j = 0; j < 3; ++j) {
      AddEdge(Edges, this->Tris[i].VertIndex[j], this->Tris[i].VertIndex[(j+1)%3], i);
    }
  }

  // copy vertices
  const MD3Vertex *pverts = (const MD3Vertex *)((const vuint8 *)pmesh+pmesh->get_vertOfs());
  this->AllVerts.setLength(pmesh->get_vertNum()*pmodel->get_frameNum());
  this->AllNormals.setLength(pmesh->get_vertNum()*pmodel->get_frameNum());
  for (unsigned i = 0; i < pmesh->get_vertNum()*pmodel->get_frameNum(); ++i) {
    this->AllVerts[i] = md3vert(pverts+i);
    this->AllNormals[i] = md3vertNormal(pverts+i);
  }

  if (AllVerts.length() == 0) {
    AllVerts.append(TVec(0.0f, 0.0f, 0.0f));
    AllVerts.append(TVec(0.0f, 0.0f, 0.0f));
    AllVerts.append(TVec(0.0f, 0.0f, 0.0f));
    AllNormals.append(TVec(0.0f, 0.0f, 1.0f));
    AllNormals.append(TVec(0.0f, 0.0f, 1.0f));
    AllNormals.append(TVec(0.0f, 0.0f, 1.0f));
  }

  // frames
  bool hadError = false;
  bool showError = true;

  // if we have only one frame, and that frame has invalid triangles, just rebuild it
  TArray<vuint8> validTri;
  if (pmodel->get_frameNum() == 1) {
    validTri.setLength((int)pmesh->get_triNum());
    if (pmesh->get_triNum()) memset(validTri.ptr(), 0, pmesh->get_triNum());
  }

  this->Frames.setLength(pmodel->get_frameNum());
  this->AllPlanes.setLength(pmodel->get_frameNum()*pmesh->get_triNum());

  if (AllPlanes.length() == 0) {
    TPlane pl;
    pl.normal = TVec(0.0f, 0.0f, 1.0f);
    pl.dist = 0;
    AllPlanes.append(pl);
  }

  int triIgnored = 0;
  for (unsigned i = 0; i < pmodel->get_frameNum(); ++i, ++pframe) {
    VMeshFrame &Frame = this->Frames[i];
    Frame.Name = getStrZ(pframe->name, 16);
    Frame.Scale = TVec(1.0f, 1.0f, 1.0f);
    Frame.Origin = TVec(pframe->get_origin(0), pframe->get_origin(1), pframe->get_origin(2));
    Frame.Verts = &this->AllVerts[i*pmesh->get_vertNum()];
    Frame.Normals = &this->AllNormals[i*pmesh->get_vertNum()];
    Frame.Planes = &this->AllPlanes[i*pmesh->get_triNum()];
    Frame.VertsOffset = 0;
    Frame.NormalsOffset = 0;
    Frame.TriCount = pmesh->get_triNum();

    // process triangles
    for (unsigned j = 0; j < pmesh->get_triNum(); ++j) {
      TVec PlaneNormal;
      TVec v3(0, 0, 0);
      bool reported = false, hacked = false;
      for (int vnn = 0; vnn < 3; ++vnn) {
        TVec v1 = Frame.Verts[this->Tris[j].VertIndex[(vnn+0)%3]];
        TVec v2 = Frame.Verts[this->Tris[j].VertIndex[(vnn+1)%3]];
             v3 = Frame.Verts[this->Tris[j].VertIndex[(vnn+2)%3]];

        TVec d1 = v2-v3;
        TVec d2 = v1-v3;
        PlaneNormal = CrossProduct(d1, d2);
        if (PlaneNormal.lengthSquared() == 0.0f) {
          //k8:hack!
          if (mdl_report_errors && !reported) {
            GCon->Logf("Alias model '%s' has degenerate triangle %d; v1=(%f,%f,%f), v2=(%f,%f,%f); v3=(%f,%f,%f); d1=(%f,%f,%f); d2=(%f,%f,%f); cross=(%f,%f,%f)",
              *this->Name, j, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z, d1.x, d1.y, d1.z, d2.x, d2.y, d2.z, PlaneNormal.x, PlaneNormal.y, PlaneNormal.z);
          }
          PlaneNormal = TVec(0.0f, 0.0f, 1.0f);
          reported = true;
        } else {
          hacked = (vnn != 0);
          break;
        }
      }
      if (mdl_report_errors && reported) {
        if (hacked) GCon->Log("  hacked around"); else { GCon->Log("  CANNOT FIX"); PlaneNormal = TVec(0, 0, 1); }
      }
      hadError = hadError || reported;
      PlaneNormal = Normalise(PlaneNormal);
      const float PlaneDist = DotProduct(PlaneNormal, v3);
      Frame.Planes[j].Set(PlaneNormal, PlaneDist);
      if (reported) {
        ++triIgnored;
        if (mdl_report_errors) GCon->Logf("  triangle #%u is ignored", j);
        if (validTri.length()) validTri[j] = 0;
      } else {
        if (validTri.length()) validTri[j] = 1;
      }
    }
  }

  if (pmodel->get_frameNum() == 1 && validTri.length()) {
    // rebuild triangle indicies, why not
    #if 0
    if (hadError) {
      VMeshFrame &Frame = this->Frames[0];
      TArray<VMeshTri> NewTris; // vetex indicies
      Frame.TriCount = 0;
      for (unsigned j = 0; j < pmesh->get_triNum(); ++j) {
        if (validTri[j]) {
          NewTris.append(this->Tris[j]);
          ++Frame.TriCount;
        }
      }
      if (Frame.TriCount == 0) Sys_Error("model %s has no valid triangles", *this->Name);
      // replace index array
      this->Tris.setLength(NewTris.length());
      memcpy(this->Tris.ptr(), NewTris.ptr(), NewTris.length()*sizeof(VMeshTri));
      pmesh->get_triNum() = Frame.TriCount;
      if (showError) {
        GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u! model rebuilt.", *this->Name, triIgnored, pmesh->get_triNum());
      }
      // rebuild edges
      this->Edges.setLength(0);
      for (unsigned i = 0; i < pmesh->get_triNum(); ++i) {
        for (unsigned j = 0; j < 3; ++j) {
          //AddEdge(Edges, this->Tris[i].VertIndex[j], ptri[i].vertindex[j], this->Tris[i].VertIndex[(j+1)%3], ptri[i].vertindex[(j+1)%3], i);
          AddEdge(Edges, this->Tris[i].VertIndex[j], this->Tris[i].VertIndex[(j+1)%3], i);
        }
      }
    }
    #endif
  } else {
    if (hadError && showError) {
      GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u!", *this->Name, triIgnored, pmesh->get_triNum());
    }
  }

  // if there were some errors, disable shadows for this model, it is probably broken anyway
  this->HadErrors = hadError;

  // store edges
  CopyEdgesTo(this->Edges, Edges);

  this->loaded = true;
}
