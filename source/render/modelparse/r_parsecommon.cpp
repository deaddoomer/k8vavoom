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
#include "../../gamedefs.h"
#include "../r_local.h"


VCvarB r_models_verbose_loading("r_models_verbose_loading", false, "Log loaded 3D models?", CVAR_PreInit|CVAR_NoShadow);
VCvarB mdl_report_errors("mdl_report_errors", false, "Show errors in alias models?", CVAR_NoShadow/*|CVAR_Archive*/);



//==========================================================================
//
//  VMeshModel::AddEdge
//
//==========================================================================
void VMeshModel::AddEdge (TArray<VTempEdge> &Edges, int Vert1, int Vert2, int Tri) {
  // check for a match
  // compare original vertex indices since texture coordinates are not important here
  for (auto &&E : Edges) {
    if (E.Tri2 == -1 && (int)E.Vert1 == Vert2 && (int)E.Vert2 == Vert1) {
      E.Tri2 = Tri;
      return;
    }
  }
  // add new edge
  VTempEdge &e = Edges.Alloc();
  e.Vert1 = Vert1;
  e.Vert2 = Vert2;
  e.Tri1 = Tri;
  e.Tri2 = -1;
}


//==========================================================================
//
//  VMeshModel::CopyEdgesTo
//
//  store edges
//
//==========================================================================
void VMeshModel::CopyEdgesTo (TArray<VMeshEdge> &dest, TArray<VTempEdge> &src) {
  const int len = src.length();
  dest.setLength(len);
  for (int i = 0; i < len; ++i) {
    dest[i].Vert1 = src[i].Vert1;
    dest[i].Vert2 = src[i].Vert2;
    dest[i].Tri1 = src[i].Tri1;
    dest[i].Tri2 = src[i].Tri2;
  }
}


//==========================================================================
//
//  VMeshModel::getStrZ
//
//==========================================================================
VStr VMeshModel::getStrZ (const char *s, unsigned maxlen) {
  if (!s || maxlen == 0 || !s[0]) return VStr::EmptyString;
  const char *se = s;
  while (maxlen-- && *se) ++se;
  return VStr(s, (int)(ptrdiff_t)(se-s));
}


//==========================================================================
//
//  VMeshModel::LoadFromData
//
//==========================================================================
void VMeshModel::LoadFromData (const vuint8 *Data, int DataSize) {
  if (loaded) return;
  if (!Data || DataSize < 4) Sys_Error("Too small data for model '%s'", *Name);

  if (LittleLong(*(const vuint32 *)Data) == IDPOLY2HEADER) {
    // MD2
    if (r_models_verbose_loading) GCon->Logf(NAME_Debug, "loading MD2 model '%s'", *Name);
    Load_MD2(Data, DataSize);
  } else if (LittleLong(*(const vuint32 *)Data) == IDPOLY3HEADER) {
    // MD3
    if (r_models_verbose_loading) GCon->Logf(NAME_Debug, "loading MD3 model '%s'", *Name);
    Load_MD3(Data, DataSize);
  } else if (isVoxel) {
    Load_KVX(Data, DataSize);
  } else {
    Sys_Error("model '%s' is in unknown format", *Name);
  }

  /*k8: nope, don't do this, because `Frames` keeps pointers to arrays
  Skins.condense();
  Frames.condense();
  AllVerts.condense();
  AllNormals.condense();
  STVerts.condense();
  Tris.condense();
  AllPlanes.condense();
  Edges.condense();
  */
}


//==========================================================================
//
//  VMeshModel::IsKnownModelFormat
//
//==========================================================================
bool VMeshModel::IsKnownModelFormat (VStream *strm) {
  if (!strm || strm->IsError()) return false;
  strm->Seek(0);
  if (strm->IsError() || strm->TotalSize() < 4) return false;
  vuint32 sign = 0;
  strm->Serialise(&sign, sizeof(sign));
  if (strm->IsError()) return false;
  if (LittleLong(sign) == IDPOLY2HEADER ||
      LittleLong(sign) == IDPOLY3HEADER)
  {
    return true;
  }
  return false;
}


//==========================================================================
//
//  VMeshModel::ClearData
//
//==========================================================================
void VMeshModel::ClearData () {
  if (nextMip != nullptr) {
    nextMip->ClearData();
    delete nextMip;
    nextMip = nullptr;
  }

  if (loaded) {
    if (Drawer) Drawer->UnloadAliasModel(this);
  }

  loaded = false;
  Skins.clear();
  Frames.clear();
  AllVerts.clear();
  AllNormals.clear();
  STVerts.clear();
  Tris.clear();
  TriVerts.clear();
  AllPlanes.clear();
  Edges.clear();
  GlMode = GlNone;
  vassert(!Uploaded);
  HadErrors = false;
  nextMip = nullptr;
  mipXScale = mipYScale = mipZScale = 1.0f;

  VertsBuffer = IndexBuffer = 0;
}


//==========================================================================
//
//  VMeshModel::LoadFromWad
//
//==========================================================================
void VMeshModel::LoadFromWad () {
  if (loaded) {
    if (!isVoxel) return;
    if (!VoxNeedReload()) return;
    //if (Drawer) Drawer->UnloadAliasModel(this);
  }

  ClearData();

  // load the file
  VStream *Strm = FL_OpenFileRead(Name);
  if (!Strm) Sys_Error("Couldn't open model data '%s'", *Name);

  int DataSize = Strm->TotalSize();
  if (DataSize < 1) Sys_Error("Cannot read model data '%s'", *Name);

  vuint8 *Data = (vuint8 *)Z_Malloc(DataSize);
  Strm->Serialise(Data, DataSize);
  bool wasError = Strm->IsError();
  VStream::Destroy(Strm);
  if (wasError) Sys_Error("Error loading model data '%s'", *Name);

  LoadFromData(Data, DataSize);

  Z_Free(Data);
}


//==========================================================================
//
//  calcTriNormal
//
//==========================================================================
static inline void calcTriNormal (TPlane &plane, TVec v1, TVec v2, TVec v3) {
  const TVec d1 = v2-v3;
  const TVec d2 = v1-v3;
  TVec PlaneNormal = d1.cross(d2);
  if (PlaneNormal.lengthSquared() == 0.0f) {
    PlaneNormal = TVec(0.0f, 0.0f, 1.0f);
  } else {
    PlaneNormal = PlaneNormal.normalise();
  }
  const float PlaneDist = PlaneNormal.dot(v3);
  plane.Set(PlaneNormal, PlaneDist);
}


//==========================================================================
//
//  VMeshModel::EnsureEdgesPlanes
//
//==========================================================================
void VMeshModel::EnsureEdgesPlanes () {
  if (AllPlanes.length() || Edges.length()) return;
  if (Frames.length() == 0 || Tris.length() == 0) return;

  // build planes
  AllPlanes.setLength(Tris.length()*Frames.length());
  int pidx = 0;
  for (int fidx = 0; fidx < Frames.length(); ++fidx) {
    VMeshFrame &frm = Frames[fidx];
    const int tcount = frm.TriCount;
    vassert(tcount <= Tris.length());
    for (int tidx = 0; tidx < tcount; ++tidx) {
      const VMeshTri &tri = Tris[tidx];
      calcTriNormal(AllPlanes[pidx++],
                    frm.Verts[tri.VertIndex[0]],
                    frm.Verts[tri.VertIndex[1]],
                    frm.Verts[tri.VertIndex[2]]);
    }
    for (int tidx = tcount; tidx < Tris.length(); ++tidx) {
      const VMeshTri &tri = Tris[tidx];
      calcTriNormal(AllPlanes[pidx++],
                    AllVerts[tri.VertIndex[0]],
                    AllVerts[tri.VertIndex[1]],
                    AllVerts[tri.VertIndex[2]]);
    }
  }
  vassert(pidx == AllPlanes.length());

  // build edges
  TArray<VTempEdge> bldEdges;
  for (int tidx = 0; tidx < Tris.length(); ++tidx) {
    // plane (for each triangle)
    const VMeshTri &tri = Tris[tidx];
    for (unsigned j = 0; j < 3; ++j) {
      AddEdge(bldEdges, tri.VertIndex[j], tri.VertIndex[(j+1)%3], tidx);
    }
  }

  // store edges
  CopyEdgesTo(Edges, bldEdges);

  // assign planes to frames
  pidx = 0;
  for (int fidx = 0; fidx < Frames.length(); ++fidx) {
    VMeshFrame &frm = Frames[fidx];
    frm.Planes = &AllPlanes[pidx];
    pidx += Tris.length();
  }
}
