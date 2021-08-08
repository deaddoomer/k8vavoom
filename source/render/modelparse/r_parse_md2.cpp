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
// md2
#pragma pack(push, 1)

struct MD2Header {
  vuint32 ident;
  vuint32 version;
  vuint32 skinwidth;
  vuint32 skinheight;
  vuint32 framesize;
  vuint32 numskins;
  vuint32 numverts;
  vuint32 numstverts;
  vuint32 numtris;
  vuint32 numcmds;
  vuint32 numframes;
  vuint32 ofsskins;
  vuint32 ofsstverts;
  vuint32 ofstris;
  vuint32 ofsframes;
  vuint32 ofscmds;
  vuint32 ofsend;

  inline vuint32 get_ident () const noexcept { return LittleULong(ident); }
  inline vuint32 get_version () const noexcept { return LittleULong(version); }
  inline vuint32 get_skinwidth () const noexcept { return LittleULong(skinwidth); }
  inline vuint32 get_skinheight () const noexcept { return LittleULong(skinheight); }
  inline vuint32 get_framesize () const noexcept { return LittleULong(framesize); }
  inline vuint32 get_numskins () const noexcept { return LittleULong(numskins); }
  inline vuint32 get_numverts () const noexcept { return LittleULong(numverts); }
  inline vuint32 get_numstverts () const noexcept { return LittleULong(numstverts); }
  inline vuint32 get_numtris () const noexcept { return LittleULong(numtris); }
  inline vuint32 get_numcmds () const noexcept { return LittleULong(numcmds); }
  inline vuint32 get_numframes () const noexcept { return LittleULong(numframes); }
  inline vuint32 get_ofsskins () const noexcept { return LittleULong(ofsskins); }
  inline vuint32 get_ofsstverts () const noexcept { return LittleULong(ofsstverts); }
  inline vuint32 get_ofstris () const noexcept { return LittleULong(ofstris); }
  inline vuint32 get_ofsframes () const noexcept { return LittleULong(ofsframes); }
  inline vuint32 get_ofscmds () const noexcept { return LittleULong(ofscmds); }
  inline vuint32 get_ofsend () const noexcept { return LittleULong(ofsend); }
};

struct MD2Skin {
  char name[64];
};

struct MD2STVert {
  vint16 s;
  vint16 t;

  inline vint16 get_s () const noexcept { return LittleShort(s); }
  inline vint16 get_t () const noexcept { return LittleShort(t); }
};

struct MD2Triangle {
  vuint16 vertindex[3];
  vuint16 stvertindex[3];

  inline vuint16 get_vertindex (const unsigned idx) const noexcept { return LittleUShort(vertindex[idx]); }
  inline vuint16 get_stvertindex (const unsigned idx) const noexcept { return LittleUShort(stvertindex[idx]); }
};

struct MD2Frame {
  float scale[3];
  float scale_origin[3];
  char name[16];

  inline float get_scale (const unsigned idx) const noexcept { return LittleFloat(scale[idx]); }
  inline float get_scale_origin (const unsigned idx) const noexcept { return LittleFloat(scale_origin[idx]); }
};

struct MD2Vertex {
  vuint8 v[3];
  vuint8 lightnormalindex;
};

#pragma pack(pop)


enum { NUMVERTEXNORMALS = 162 };
static const float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "md2_normals.h"
};


//==========================================================================
//
//  VMeshModel::LoadMD2Frames
//
//  return `true` if model was succesfully found and parsed, or
//  false if model wasn't found or in invalid format
//  WARNING: don't clear `names` array!
//
//==========================================================================
bool VMeshModel::LoadMD2Frames (VStr mdpath, TArray<VStr> &names) {
  // load the file
  VStream *strm = FL_OpenFileRead(mdpath);
  if (!strm) return false;

  TArray<vuint8> data;
  data.setLength(strm->TotalSize());
  if (data.length() < 4) {
    VStream::Destroy(strm);
    return false;
  }
  strm->Serialise(data.ptr(), data.length());
  bool wasError = strm->IsError();
  VStream::Destroy(strm);
  if (wasError) return false;

  // is this MD2 model?
  if (LittleLong(*(const vuint32 *)data.ptr()) != IDPOLY2HEADER) return false;

  const MD2Header *pmodel = (const MD2Header *)data.ptr();

  if (pmodel->get_version() != IDMD2_VERSION) return false;
  if (pmodel->get_numverts() <= 0 || pmodel->get_numverts() > MAXALIASVERTS) return false;
  if (pmodel->get_numstverts() <= 0 || pmodel->get_numstverts() > MAXALIASSTVERTS) return false;
  if (pmodel->get_numtris() <= 0 || pmodel->get_numtris() > 65536) return false;
  if (pmodel->get_numskins() > 1024) return false;
  if (pmodel->get_numframes() < 1 || pmodel->get_numframes() > 1024) return false;

  const MD2Frame *pframe = (const MD2Frame *)((const vuint8 *)pmodel+pmodel->get_ofsframes());

  for (unsigned i = 0; i < pmodel->get_numframes(); ++i) {
    VStr frname = getStrZ(pframe->name, 16);
    names.append(frname);
    pframe = (const MD2Frame *)((const vuint8 *)pframe+pmodel->get_framesize());
  }

  return true;
}


//==========================================================================
//
//  VMeshModel::Load_MD2
//
//  FIXME: use `DataSize` for checks!
//
//==========================================================================
void VMeshModel::Load_MD2 (const vuint8 *Data, int DataSize) {
  (void)DataSize;

  const MD2Header *pmodel = (const MD2Header *)Data;
  this->Uploaded = false;
  this->VertsBuffer = 0;
  this->IndexBuffer = 0;

  if (pmodel->get_version() != IDMD2_VERSION) Sys_Error("model '%s' has wrong version number (%i should be %i)", *this->Name, pmodel->get_version(), IDMD2_VERSION);
  if (pmodel->get_numverts() <= 0) Sys_Error("model '%s' has no vertices", *this->Name);
  if (pmodel->get_numverts() > MAXALIASVERTS) Sys_Error("model '%s' has too many vertices", *this->Name);
  if (pmodel->get_numstverts() <= 0) Sys_Error("model '%s' has no texture vertices", *this->Name);
  if (pmodel->get_numstverts() > MAXALIASSTVERTS) Sys_Error("model '%s' has too many texture vertices", *this->Name);
  if (pmodel->get_numtris() <= 0) Sys_Error("model '%s' has no triangles", *this->Name);
  if (pmodel->get_numtris() > 65536) Sys_Error("model '%s' has too many triangles", *this->Name);
  //if (pmodel->get_skinwidth()&0x03) Sys_Error("Mod_LoadAliasModel: skinwidth not multiple of 4");
  if (/*pmodel->get_numskins() < 0 ||*/ pmodel->get_numskins() > 1024) Sys_Error("model '%s' has invalid number of skins: %u", *this->Name, pmodel->get_numskins());
  if (pmodel->get_numframes() < 1 || pmodel->get_numframes() > 1024) Sys_Error("model '%s' has invalid numebr of frames: %u", *this->Name, pmodel->get_numframes());

  // base s and t vertices
  const MD2STVert *pstverts = (const MD2STVert *)((const vuint8 *)pmodel+pmodel->get_ofsstverts());

  // triangles
  //k8: this tried to collapse same vertices, but meh
  TArray<TVertMap> VertMap;
  TArray<VTempEdge> Edges;
  this->Tris.SetNum(pmodel->get_numtris());
  const MD2Triangle *ptri = (const MD2Triangle *)((const vuint8 *)pmodel+pmodel->get_ofstris());
  for (unsigned i = 0; i < pmodel->get_numtris(); ++i) {
    for (unsigned j = 0; j < 3; ++j) {
      this->Tris[i].VertIndex[j] = VertMap.length();
      TVertMap &v = VertMap.alloc();
      v.VertIndex = ptri[i].get_vertindex(j);
      v.STIndex = ptri[i].get_stvertindex(j);
    }
    for (unsigned j = 0; j < 3; ++j) {
      AddEdge(Edges, this->Tris[i].VertIndex[j], this->Tris[i].VertIndex[(j+1)%3], i);
    }
  }

  // calculate remapped ST verts
  this->STVerts.SetNum(VertMap.length());
  for (int i = 0; i < VertMap.length(); ++i) {
    this->STVerts[i].S = (float)pstverts[VertMap[i].STIndex].get_s()/(float)pmodel->get_skinwidth();
    this->STVerts[i].T = (float)pstverts[VertMap[i].STIndex].get_t()/(float)pmodel->get_skinheight();
  }

  // frames
  bool hadError = false;
  bool showError = true;

  // if we have only one frame, and that frame has invalid triangles, just rebuild it
  TArray<vuint8> validTri;
  if (pmodel->get_numframes() == 1) {
    validTri.setLength((int)pmodel->get_numtris());
    if (pmodel->get_numtris()) memset(validTri.ptr(), 0, pmodel->get_numtris());
  }

  this->Frames.SetNum(pmodel->get_numframes());
  this->AllVerts.SetNum(pmodel->get_numframes()*VertMap.length());
  this->AllNormals.SetNum(pmodel->get_numframes()*VertMap.length());
  this->AllPlanes.SetNum(pmodel->get_numframes()*pmodel->get_numtris());
  const MD2Frame *pframe = (const MD2Frame *)((const vuint8 *)pmodel+pmodel->get_ofsframes());

  int triIgnored = 0;
  for (unsigned i = 0; i < pmodel->get_numframes(); ++i) {
    VMeshFrame &Frame = this->Frames[i];
    Frame.Name = getStrZ(pframe->name, 16);
    Frame.Scale = TVec(pframe->get_scale(0), pframe->get_scale(1), pframe->get_scale(2));
    Frame.Origin = TVec(pframe->get_scale_origin(0), pframe->get_scale_origin(1), pframe->get_scale_origin(2));
    Frame.Verts = &this->AllVerts[i*VertMap.length()];
    Frame.Normals = &this->AllNormals[i*VertMap.length()];
    Frame.Planes = &this->AllPlanes[i*pmodel->get_numtris()];
    Frame.VertsOffset = 0;
    Frame.NormalsOffset = 0;
    Frame.TriCount = pmodel->get_numtris();
    //Frame.ValidTris.setLength((int)pmodel->get_numtris());

    const MD2Vertex *Verts = (const MD2Vertex *)(pframe+1);
    for (int j = 0; j < VertMap.length(); ++j) {
      const MD2Vertex &Vert = Verts[VertMap[j].VertIndex];
      Frame.Verts[j].x = Vert.v[0]*pframe->get_scale(0)+pframe->get_scale_origin(0);
      Frame.Verts[j].y = Vert.v[1]*pframe->get_scale(1)+pframe->get_scale_origin(1);
      Frame.Verts[j].z = Vert.v[2]*pframe->get_scale(2)+pframe->get_scale_origin(2);
      Frame.Normals[j] = r_avertexnormals[Vert.lightnormalindex];
    }

    for (unsigned j = 0; j < pmodel->get_numtris(); ++j) {
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
        if (lengthSquared(PlaneNormal) == 0.0f) {
          //k8:hack!
          if (mdl_report_errors && !reported) {
            GCon->Logf("Alias model '%s' has degenerate triangle %d; v1=(%f,%f,%f), v2=(%f,%f,%f); v3=(%f,%f,%f); d1=(%f,%f,%f); d2=(%f,%f,%f); cross=(%f,%f,%f)",
              *this->Name, j, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z, v3.x, v3.y, v3.z, d1.x, d1.y, d1.z, d2.x, d2.y, d2.z, PlaneNormal.x, PlaneNormal.y, PlaneNormal.z);
          }
          reported = true;
          PlaneNormal = TVec(0.0f, 0.0f, 1.0f);
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
    pframe = (const MD2Frame *)((const vuint8 *)pframe+pmodel->get_framesize());
  }

  if (pmodel->get_numframes() == 1 && validTri.length()) {
    // rebuild triangle indicies, why not
    #if 0
    if (hadError) {
      VMeshFrame &Frame = this->Frames[0];
      TArray<VMeshTri> NewTris; // vetex indicies
      Frame.TriCount = 0;
      for (unsigned j = 0; j < pmodel->get_numtris(); ++j) {
        if (validTri[j]) {
          NewTris.append(this->Tris[j]);
          ++Frame.TriCount;
        }
      }
      if (Frame.TriCount == 0) Sys_Error("model %s has no valid triangles", *this->Name);
      // replace index array
      this->Tris.setLength(NewTris.length());
      if (NewTris.length()) memcpy(this->Tris.ptr(), NewTris.ptr(), NewTris.length()*sizeof(VMeshTri));
      pmodel->get_numtris() = Frame.TriCount;
      if (showError) {
        GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u! model rebuilt.", *this->Name, triIgnored, pmodel->get_numtris());
      }
      // rebuild edges
      this->Edges.SetNum(0);
      for (unsigned i = 0; i < pmodel->get_numtris(); ++i) {
        for (unsigned j = 0; j < 3; ++j) {
          //AddEdge(Edges, this->Tris[i].VertIndex[j], ptri[i].get_vertindex(j), this->Tris[i].VertIndex[(j+1)%3], ptri[i].get_vertindex((j+1)%3), i);
          AddEdge(Edges, this->Tris[i].VertIndex[j], this->Tris[i].VertIndex[(j+1)%3], i);
        }
      }
    }
    #endif
  } else {
    if (hadError && showError) {
      GCon->Logf(NAME_Warning, "Alias model '%s' has %d degenerate triangles out of %u!", *this->Name, triIgnored, pmodel->get_numtris());
    }
  }

  // if there were some errors, disable shadows for this model, it is probably broken anyway
  this->HadErrors = hadError;

  // store edges
  CopyEdgesTo(this->Edges, Edges);

  // skins
  const MD2Skin *pskindesc = (const MD2Skin *)((const vuint8 *)pmodel+pmodel->get_ofsskins());
  for (unsigned i = 0; i < pmodel->get_numskins(); ++i) {
    //this->Skins.Append(*getStrZ(pskindesc[i].name, 64).ToLower());
    VStr name = getStrZ(pskindesc[i].name, 64).toLowerCase();
    // prepend model path
    if (!name.isEmpty()) name = this->Name.ExtractFilePath()+name.ExtractFileBaseName();
    //GCon->Logf("model '%s' has skin #%u '%s'", *this->Name, i, *name);
    VMeshModel::SkinInfo &si = this->Skins.alloc();
    si.fileName = *name;
    si.textureId = -1;
    si.shade = -1;
  }

  this->loaded = true;
}
