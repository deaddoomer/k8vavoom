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
#include "../../gamedefs.h"
#include "../r_local.h"
#include "../../filesys/files.h"
#include "voxelib.h"

static VCvarI vox_cache_compression_level("vox_cache_compression_level", "6", "Voxel cache file compression level [0..9].", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarB vox_cache_enabled("vox_cache_enabled", false, "Enable caching of converted voxel models?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarB vox_verbose_conversion("vox_verbose_conversion", false, "Show info messages from voxel converter?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarI vox_optimisation("vox_optimisation", "3", "Voxel loader optimisation (higher is better, but with space ants) [0..3].", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarB vox_fix_faces("vox_fix_faces", true, "Fix voxel face visibility info?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarB vox_fix_tjunctions("vox_fix_tjunctions", false, "Show info messages from voxel converter?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);

// useful for debugging, but not really needed
#define VOX_ENABLE_INVARIANT_CHECK


#define VOX_CACHE_SIGNATURE  "k8vavoom voxel model cache file, version 4\n"

#define BreakIndex  (65535)


// ////////////////////////////////////////////////////////////////////////// //
class VVoxTexture : public VTexture {
public:
  int VoxTexIndex;
  vint32 ShadeVal;
  bool needReupload;

protected:
  virtual bool IsSpecialReleasePixelsAllowed () override;

public:
  VVoxTexture (int vidx, int ashade);
  virtual ~VVoxTexture () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
  virtual int CheckModified () override;
};


struct VoxelTexture {
  int width, height;
  vuint8 *data;
  VStr fname;
  TArray<VVoxTexture *> tex;
};

static TArray<VoxelTexture> voxTextures;


//==========================================================================
//
//  VVoxTexture::VVoxTexture
//
//==========================================================================
VVoxTexture::VVoxTexture (int vidx, vint32 ashade)
  : VTexture()
  , VoxTexIndex(vidx)
  , ShadeVal(ashade)
  , needReupload(false)
{
  vassert(vidx >= 0 && vidx < voxTextures.length());
  SourceLump = -1;
  Type = TEXTYPE_Pic;
  Name = NAME_None; //VName(*voxTextures[vidx].fname);
  Width = voxTextures[vidx].width;
  Height = voxTextures[vidx].height;
  mFormat = mOrigFormat = TEXFMT_RGBA;
  bForceNoFilter = true;
}


//==========================================================================
//
//  VVoxTexture::~VVoxTexture
//
//==========================================================================
VVoxTexture::~VVoxTexture () {
  if (Pixels) {
    delete[] Pixels;
    Pixels = nullptr;
  }
}


//==========================================================================
//
//  VVoxTexture::GetPixels
//
//==========================================================================
vuint8 *VVoxTexture::GetPixels () {
  // if already got pixels, then just return them
  if (Pixels) return Pixels;

  bForceNoFilter = true;
  transFlags = TransValueSolid; // anyway
  mFormat = mOrigFormat = TEXFMT_RGBA;

  //GCon->Logf(NAME_Init, "getting pixels for voxel texture #%d (shade=%d); %dx%d", VoxTexIndex, ShadeVal, Width, Height);

  Pixels = new vuint8[Width*Height*4];
  memcpy(Pixels, voxTextures[VoxTexIndex].data, Width*Height*4);

  ConvertPixelsToShaded();
  PrepareBrightmap();
  return Pixels;
}


//==========================================================================
//
//  VVoxTexture::GetPalette
//
//==========================================================================
rgba_t *VVoxTexture::GetPalette () {
  return r_palette;
}


//==========================================================================
//
//  VVoxTexture::CheckModified
//
//  returns 0 if not, positive if only data need to be updated, or
//  negative to recreate texture
//
//==========================================================================
int VVoxTexture::CheckModified () {
  if (needReupload) {
    needReupload = false;
    return -1;
  }
  return 0;
}


//==========================================================================
//
//  VVoxTexture::IsSpecialReleasePixelsAllowed
//
//==========================================================================
bool VVoxTexture::IsSpecialReleasePixelsAllowed () {
  return true;
}



//==========================================================================
//
//  voxTextureLoader
//
//==========================================================================
static VTexture *voxTextureLoader (VStr fname, vint32 shade) {
  if (!fname.startsWith("\x01:voxtexure:")) return nullptr;
  VStr ss = fname;
  ss.chopLeft(12);
  if (ss.length() == 0) return nullptr;
  int idx = VStr::atoi(*ss);
  if (idx < 0 || idx >= voxTextures.length()) return nullptr;
  //GCon->Logf(NAME_Debug, "***VOXEL TEXTURE #%d", idx);
  for (int f = 0; f < voxTextures[idx].tex.length(); ++f) {
    if (voxTextures[idx].tex[f]->ShadeVal == shade) return voxTextures[idx].tex[f];
  }
  VVoxTexture *vv = new VVoxTexture(idx, shade);
  voxTextures[idx].tex.append(vv);
  return vv;
}


//==========================================================================
//
//  loadKVX
//
//==========================================================================
static bool loadKVX (VStream &st, VoxelData &vox) {
  st.Seek(0);
  vint32 fsize;
  st << fsize;
  if (fsize < 8 || fsize > 0x00ffffff) {
    GCon->Logf(NAME_Error, "voxel data too big (kvx)");
    return false;
  }

  int nextpos = st.Tell()+fsize;
  TArray<vuint32> xofs;
  //ushort[][] xyofs;
  TArray<vuint16> xyofs;
  TArray<vuint8> data;
  vint32 xsiz, ysiz, zsiz;
  vint32 xpivot, ypivot, zpivot;

  st << xsiz << ysiz << zsiz;
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 || xsiz > 1024 || ysiz > 1024 || zsiz > 1024) {
    GCon->Logf(NAME_Error, "invalid voxel size (kvx)");
    return false;
  }
  const int ww = ysiz+1;

  st << xpivot << ypivot << zpivot;
  vuint32 xstart = (xsiz+1)*4+xsiz*(ysiz+1)*2;
  //TArray<vuint32> xofs;
  xofs.setLength(xsiz+1);
  for (int f = 0; f <= xsiz; ++f) {
    st << xofs[f];
    xofs[f] -= xstart;
  }
  //xyofs = new ushort[][](xsiz, ysiz+1);
  xyofs.setLength(xsiz*ww);
  for (int x = 0; x < xsiz; ++x) {
    for (int y = 0; y <= ysiz; ++y) {
      st << xyofs[x*ww+y];
    }
  }
  //assert(fx.size-fx.tell == fsize-24-(xsiz+1)*4-xsiz*(ysiz+1)*2);
  //data = new vuint8[](cast(vuint32)(fx.size-fx.tell));
  data.setLength(fsize-24-(xsiz+1)*4-xsiz*(ysiz+1)*2);
  st.Serialise(data.ptr(), data.length());

  st.Seek(nextpos);
  // skip mipmaps
  while (st.TotalSize()-st.Tell() > 768) {
    st << fsize; // size of this voxel (useful for mipmaps)
    st.Seek(st.Tell()+fsize);
  }

  vuint8 pal[768];
  if (st.TotalSize()-st.Tell() == 768) {
    st.Serialise(pal, 768);
    for (int cidx = 0; cidx < 768; ++cidx) pal[cidx] = clampToByte(255*pal[cidx]/64);
  } else {
    for (int cidx = 0; cidx < 256; ++cidx) {
      pal[cidx*3+0] = r_palette[cidx].r;
      pal[cidx*3+1] = r_palette[cidx].g;
      pal[cidx*3+2] = r_palette[cidx].b;
    }
  }

  const float px = (float)xpivot/256.0f;
  const float py = (float)ypivot/256.0f;
  const float pz = (float)zpivot/256.0f;

  // now build cubes
  vox.setSize(xsiz, ysiz, zsiz);
  for (int y = 0; y < ysiz; ++y) {
    for (int x = 0; x < xsiz; ++x) {
      vuint32 sofs = xofs[x]+xyofs[x*ww+y];
      vuint32 eofs = xofs[x]+xyofs[x*ww+y+1];
      //if (sofs == eofs) continue;
      //assert(sofs < data.length && eofs <= data.length);
      while (sofs < eofs) {
        int ztop = data[sofs++];
        vuint32 zlen = data[sofs++];
        vuint8 cull = data[sofs++];
        // colors
        for (vuint32 cidx = 0; cidx < zlen; ++cidx) {
          vuint8 palcol = data[sofs++];
          /*
          vuint8 cl = cull;
          if (cidx != 0) cl &= ~0x10;
          if (cidx != zlen-1) cl &= ~0x20;
          */
          const vuint32 rgb =
            pal[palcol*3+2]|
            ((vuint32)pal[palcol*3+1]<<8)|
            ((vuint32)pal[palcol*3+0]<<16);
          ++ztop;
          vox.addVoxel(xsiz-x-1, y, zsiz-ztop, rgb, cull);
        }
      }
      //assert(sofs == eofs);
    }
  }

  vox.cx = px;
  vox.cy = py;
  vox.cz = pz;

  return true;
}


//==========================================================================
//
//  loadKV6
//
//==========================================================================
static bool loadKV6 (VStream &st, VoxelData &vox) {
  struct KVox {
    vuint32 rgb;
    vuint8 zlo, zhi;
    vuint8 cull;
    vuint8 normidx;
  };

  st.Seek(0);

  vint32 xsiz, ysiz, zsiz;
  st << xsiz << ysiz << zsiz;
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 || xsiz > 1024 || ysiz > 1024 || zsiz > 1024) {
    GCon->Logf(NAME_Error, "invalid voxel size (kv6)");
    return false;
  }

  float xpivot, ypivot, zpivot;
  st << xpivot << ypivot << zpivot;

  vint32 voxcount;
  st << voxcount;
  if (voxcount <= 0 || voxcount > 0x00ffffff) {
    GCon->Logf(NAME_Error, "invalid number of voxels (kv6)");
    return false;
  }

  TArray<KVox> kvox;
  kvox.setLength(voxcount);
  for (vint32 vidx = 0; vidx < voxcount; ++vidx) {
    KVox &kv = kvox[vidx];
    vuint8 r8, g8, b8;
    st << r8 << g8 << b8;
    kv.rgb = b8|(g8<<8)|(r8<<16);
    vuint8 dummy;
    st << dummy; // always 128; ignore
    vuint8 zlo, zhi, cull, normidx;
    st << zlo << zhi << cull << normidx;
    kv.zlo = zlo;
    kv.zhi = zhi;
    kv.cull = cull;
    kv.normidx = normidx;
    if (kv.zhi != 0) {
      GCon->Logf(NAME_Error, "wtf?! zhi is not 0! (kv6)");
      return false;
    }
  }

  TArray<vuint32> xofs;
  xofs.setLength(xsiz+1);
  vuint32 curvidx = 0;
  for (int vidx = 0; vidx < xsiz; ++vidx) {
    xofs[vidx] = curvidx;
    vuint32 count;
    st << count;
    curvidx += count;
  }
  xofs[xofs.length()-1] = curvidx;

  TArray<vuint32> xyofs;
  const int ww = ysiz+1;
  xyofs.setLength(xsiz*ww);
  //auto xyofs = new vuint32[][](xsiz, ysiz+1);
  for (int xxidx = 0; xxidx < xsiz; ++xxidx) {
    curvidx = 0;
    for (int yyidx = 0; yyidx < ysiz; ++yyidx) {
      xyofs[xxidx*ww+yyidx] = curvidx;
      vuint32 count;
      st << count;
      curvidx += count;
    }
    xyofs[xxidx*ww+ysiz] = curvidx;
  }

  // now build cubes
  vox.setSize(xsiz, ysiz, zsiz);
  for (int y = 0; y < ysiz; ++y) {
    for (int x = 0; x < xsiz; ++x) {
      vuint32 sofs = xofs[x]+xyofs[x*ww+y];
      vuint32 eofs = xofs[x]+xyofs[x*ww+y+1];
      //if (sofs == eofs) continue;
      //assert(sofs < data.length && eofs <= data.length);
      while (sofs < eofs) {
        const KVox &kv = kvox[sofs++];
        int z = kv.zlo;
        for (int cidx = 0; cidx <= kv.zhi; ++cidx) {
          ++z;
          vox.addVoxel(xsiz-x-1, y, zsiz-z, kv.rgb, kv.cull);
        }
      }
    }
  }

  vox.cx = xpivot;
  vox.cy = ypivot;
  vox.cz = zpivot;

  return true;
}


//==========================================================================
//
//  VMeshModel::VoxNeedReload
//
//==========================================================================
bool VMeshModel::VoxNeedReload () {
  return
    voxOptLevel != clampval(vox_optimisation.asInt(), -1, 3)+1 ||
    voxFixTJunk != vox_fix_tjunctions.asBool() ||
    voxHollowFill != vox_fix_faces.asBool();
}


//==========================================================================
//
//  VMeshModel::Load_KVX
//
//  FIXME: use `DataSize` for checks!
//
//==========================================================================
void VMeshModel::Load_KVX (const vuint8 *Data, int DataSize) {
  (void)DataSize;

  if (DataSize < 8) Sys_Error("invalid voxel model '%s'", *this->Name);

  voxlib_verbose = vox_verbose_conversion;

  this->Uploaded = false;
  this->VertsBuffer = 0;
  this->IndexBuffer = 0;
  this->GlMode = GlNone;

  this->voxOptLevel = clampval(vox_optimisation.asInt(), -1, 3)+1;
  this->voxFixTJunk = vox_fix_tjunctions.asBool();
  this->voxHollowFill = vox_fix_faces.asBool();

  VStr ccname = GenKVXCacheName(Data, DataSize);
  VStr cacheFileName = VStr("voxmdl_")+ccname+".cache";
  cacheFileName = FL_GetVoxelCacheDir()+"/"+cacheFileName;

  VStream *strm = FL_OpenSysFileRead(cacheFileName);
  if (strm) {
    if (vox_cache_enabled.asBool()) {
      char tbuf[128];
      const int slen = strlen(VOX_CACHE_SIGNATURE);
      vassert(slen < (int)sizeof(tbuf));
      strm->Serialise(tbuf, slen);
      bool ok = (!strm->IsError() && memcmp(tbuf, VOX_CACHE_SIGNATURE, (size_t)slen) == 0);
      if (ok) {
        VStr tmpstr;
        *strm << tmpstr;
        ok = !strm->IsError();
      }
      vint32 optlevel = -1;
      if (ok) {
        *strm << optlevel;
        if (ok && optlevel != this->voxOptLevel) ok = false;
      }
      vuint8 tjunk = 255;
      if (ok) {
        *strm << tjunk;
        if (ok && (tjunk > 1 || this->voxFixTJunk != tjunk)) ok = false;
      }
      vuint8 hollow = 255;
      if (ok) {
        *strm << hollow;
        if (ok && (hollow > 1 || this->voxHollowFill != hollow)) ok = false;
      }
      if (ok) {
        VZLibStreamReader *zstrm = new VZLibStreamReader(true, strm);
        ok = Load_KVXCache(zstrm);
        if (ok) ok = !zstrm->IsError();
        zstrm->Close();
        delete zstrm;
      }
      if (ok) ok = !strm->IsError();
      VStream::Destroy(strm);
      if (ok) {
        this->loaded = true;
        //GCon->Logf(NAME_Init, "voxel model '%s' loaded from cache file '%s'", *this->Name, *ccname);
        return;
      }
      Sys_FileDelete(cacheFileName);
      Skins.clear();
      Frames.clear();
      AllVerts.clear();
      AllNormals.clear();
      STVerts.clear();
      Tris.clear();
      TriVerts.clear();
    } else {
      VStream::Destroy(strm);
      Sys_FileDelete(cacheFileName);
    }
  }

  if (vox_verbose_conversion.asBool()) {
    GCon->Logf(NAME_Init, "converting voxel model '%s'...", *this->Name);
  }

  GLVoxelMesh glvmesh;
  {
    VMemoryStreamRO memst(this->Name, Data, DataSize);
    vuint32 sign;
    memst << sign;
    memst.Seek(0);

    VoxelData vox;
    bool ok = false;
    if (sign == 0x6c78764bU) {
      if (vox_verbose_conversion.asBool()) GCon->Logf(NAME_Init, "  loading KV6...");
      ok = loadKV6(memst, vox);
    } else {
      if (vox_verbose_conversion.asBool()) GCon->Logf(NAME_Init, "  loading KVX...");
      ok = loadKVX(memst, vox);
    }
    if (!ok || memst.IsError()) Sys_Error("cannot load voxel model '%s'", *this->Name);
    vox.optimise(this->voxHollowFill);

    if (!useVoxelPivotZ) vox.cz = 0.0f;
    VoxelMesh vmesh;
    vmesh.createFrom(vox, this->voxOptLevel);
    vox.clear();

    glvmesh.create(vmesh, this->voxFixTJunk, BreakIndex/*, this->Name*/);
    vmesh.clear();
    glvmesh.recreateTriangles(Tris, BreakIndex);

    if (vox_verbose_conversion.asBool()) {
      GCon->Logf(NAME_Init, "voxel model '%s': %d unique vertices, %d triangles, %dx%d color texture; optlevel=%d",
                 *this->Name, glvmesh.vertices.length(), Tris.length(),
                 (int)glvmesh.imgWidth, (int)glvmesh.imgHeight, this->voxOptLevel);

    }
  }

  if (glvmesh.indicies.length() == 0) Sys_Error("cannot load empty voxel model '%s'", *this->Name);

  // skins
  {
    if (voxTextures.length() == 0) GTextureManager.RegisterTextureLoader(&voxTextureLoader);
    VoxelTexture *vxt;
    if (voxSkinTextureId < 0) {
      voxSkinTextureId = voxTextures.length();
      VStr fname = va("\x01:voxtexure:%d", voxSkinTextureId);
      vxt = &voxTextures.alloc();
      vxt->fname = fname;
      vxt->data = (vuint8 *)Z_Malloc(glvmesh.imgWidth*glvmesh.imgHeight*4);
    } else {
      vxt = &voxTextures[voxSkinTextureId];
      for (int f = 0; f < vxt->tex.length(); ++f) {
        vxt->tex[f]->ReleasePixels();
        vxt->tex[f]->needReupload = true;
        vxt->tex[f]->Width = glvmesh.imgWidth;
        vxt->tex[f]->Height = glvmesh.imgHeight;
      }
      vxt->data = (vuint8 *)Z_Realloc(vxt->data, glvmesh.imgWidth*glvmesh.imgHeight*4);
    }
    vxt->width = glvmesh.imgWidth;
    vxt->height = glvmesh.imgHeight;
    memcpy(vxt->data, glvmesh.img.ptr(), glvmesh.imgWidth*glvmesh.imgHeight*4);
    Skins.setLength(1);
    VMeshModel::SkinInfo &si = Skins[0];
    si.fileName = VName(*vxt->fname);
    si.textureId = -1;
    si.shade = -1;
  }

  // copy vertices
  AllVerts.setLength(glvmesh.vertices.length());
  AllNormals.setLength(AllVerts.length());
  STVerts.setLength(AllVerts.length());

  for (int f = 0; f < glvmesh.vertices.length(); ++f) {
    AllVerts[f] = glvmesh.vertices[f].asTVec();
    AllNormals[f] = glvmesh.vertices[f].normalAsTVec();
    STVerts[f].S = glvmesh.vertices[f].s;
    STVerts[f].T = glvmesh.vertices[f].t;
  }

  const int ilen = glvmesh.indicies.length();
  GlMode = GlTriangleFan;
  TriVerts.setLength(glvmesh.indicies.length());
  for (int f = 0; f < ilen; ++f) TriVerts[f] = glvmesh.indicies[f];

  glvmesh.clear();

  AllNormals.condense();
  AllVerts.condense();
  STVerts.condense();

  Frames.setLength(1);
  {
    VMeshFrame &Frame = Frames[0];
    Frame.Name = "frame_0";
    Frame.Scale = TVec(1.0f, 1.0f, 1.0f);
    Frame.Origin = TVec(0.0f, 0.0f, 0.0f); //TVec(vox.cx, vox.cy, vox.cz);
    Frame.Verts = &AllVerts[0];
    Frame.Normals = &AllNormals[0];
    Frame.Planes = nullptr;
    Frame.TriCount = Tris.length();
    Frame.VertsOffset = 0;
    Frame.NormalsOffset = 0;
  }

  if (vox_cache_enabled.asBool()) {
    strm = FL_OpenSysFileWrite(cacheFileName);
    if (strm) {
          if (vox_verbose_conversion.asBool()) GCon->Logf(NAME_Init, "...writing cache to '%s'...", *ccname);
      strm->Serialise(VOX_CACHE_SIGNATURE, (int)strlen(VOX_CACHE_SIGNATURE));
      *strm << this->Name;
      vint32 optlevel = this->voxOptLevel;
      *strm << optlevel;
      vuint8 tjunk = (this->voxFixTJunk ? 1 : 0);
      *strm << tjunk;
      vuint8 hollow = (this->voxHollowFill ? 1 : 0);
      *strm << hollow;
      VZLibStreamWriter *zstrm = new VZLibStreamWriter(strm, vox_cache_compression_level.asInt());
      Save_KVXCache(zstrm);
      bool ok = !zstrm->IsError();
      if (!zstrm->Close()) ok = false;
      delete zstrm;
      strm->Flush();
      if (ok) ok = !strm->IsError();
      if (!strm->Close()) ok = false;
      delete strm;
      if (!ok) Sys_FileDelete(cacheFileName);
    }
  }

  this->loaded = true;
}


//==========================================================================
//
//  VMeshModel::Load_KVXCache
//
//==========================================================================
bool VMeshModel::Load_KVXCache (VStream *st) {
  char sign[8];
  memset(sign, 0, sizeof(sign));
  st->Serialise(sign, 8);
  if (memcmp(sign, "K8VOXMDL", 8) != 0) return false;

  // vertices
  {
    vuint32 count;
    *st << count;
    AllVerts.setLength((int)count);
    for (int f = 0; f < AllVerts.length(); ++f) {
      *st << AllVerts[f].x << AllVerts[f].y << AllVerts[f].z;
    }
  }

  // stverts
  {
    vuint32 count;
    *st << count;
    STVerts.setLength((int)count);
    for (int f = 0; f < STVerts.length(); ++f) {
      *st << STVerts[f].S << STVerts[f].T;
    }
  }

  // normals
  {
    vuint32 count;
    *st << count;
    AllNormals.setLength((int)count);
    for (int f = 0; f < AllNormals.length(); ++f) {
      *st << AllNormals[f].x << AllNormals[f].y << AllNormals[f].z;
    }
  }

  // triangles
  {
    vuint32 count;
    *st << count;
    Tris.setLength((int)count);
    for (int f = 0; f < Tris.length(); ++f) {
      *st << Tris[f].VertIndex[0] << Tris[f].VertIndex[1] << Tris[f].VertIndex[2];
    }
  }

  // triangle fans
  {
    vuint32 glmode;
    *st << glmode;
    GlMode = glmode;
    if (GlMode != GlNone) {
      vuint32 count;
      *st << count;
      TriVerts.setLength((int)count);
      for (int f = 0; f < TriVerts.length(); ++f) {
        *st << TriVerts[f];
      }
    } else {
      TriVerts.clear();
    }
  }

  // frames
  {
    vuint32 count;
    *st << count;
    Frames.setLength((int)count);
    for (int f = 0; f < Frames.length(); ++f) {
      *st << Frames[f].Name;
      *st << Frames[f].Scale.x << Frames[f].Scale.y << Frames[f].Scale.z;
      *st << Frames[f].Origin.x << Frames[f].Origin.y << Frames[f].Origin.z;
      *st << Frames[f].TriCount;
      vuint32 vidx;
      *st << vidx;
      Frames[f].Verts = &AllVerts[(int)vidx];
      *st << vidx;
      Frames[f].Normals = &AllNormals[(int)vidx];
    }
  }

  // skins
  {
    vuint16 skins;
    *st << skins;
    for (int f = 0; f < (int)skins; ++f) {
      vuint16 w, h;
      *st << w << h;
      vuint8 *data = (vuint8 *)Z_Calloc((vuint32)w*(vuint32)h*4);
      st->Serialise(data, (vuint32)w*(vuint32)h*4);
      if (voxTextures.length() == 0) GTextureManager.RegisterTextureLoader(&voxTextureLoader);
      VStr fname = va("\x01:voxtexure:%d", voxTextures.length());
      VoxelTexture &vxt = voxTextures.alloc();
      vxt.fname = fname;
      vxt.width = w;
      vxt.height = h;
      vxt.data = data;
      VMeshModel::SkinInfo &si = Skins.alloc();
      si.fileName = VName(*fname);
      si.textureId = -1;
      si.shade = -1;
    }
  }

  return true;
}


//==========================================================================
//
//  VMeshModel::Save_KVXCache
//
//==========================================================================
void VMeshModel::Save_KVXCache (VStream *st) {
  const char *sign = "K8VOXMDL";
  st->Serialise(sign, 8);

  // vertices
  {
    vuint32 count = (vuint32)AllVerts.length();
    *st << count;
    for (int f = 0; f < AllVerts.length(); ++f) {
      *st << AllVerts[f].x << AllVerts[f].y << AllVerts[f].z;
    }
  }

  // stverts
  {
    vuint32 count = (vuint32)STVerts.length();
    *st << count;
    for (int f = 0; f < STVerts.length(); ++f) {
      *st << STVerts[f].S << STVerts[f].T;
    }
  }

  // normals
  {
    vuint32 count = (vuint32)AllNormals.length();
    *st << count;
    for (int f = 0; f < AllNormals.length(); ++f) {
      *st << AllNormals[f].x << AllNormals[f].y << AllNormals[f].z;
    }
  }

  // triangles
  {
    vuint32 count = (vuint32)Tris.length();
    *st << count;
    for (int f = 0; f < Tris.length(); ++f) {
      *st << Tris[f].VertIndex[0] << Tris[f].VertIndex[1] << Tris[f].VertIndex[2];
    }
  }

  // triangle fans
  {
    vuint32 glmode = GlMode;
    *st << glmode;
    if (GlMode != GlNone) {
      vuint32 count = (vuint32)TriVerts.length();
      *st << count;
      for (int f = 0; f < TriVerts.length(); ++f) {
        *st << TriVerts[f];
      }
    }
  }

  // frames
  {
    vuint32 count = (vuint32)Frames.length();
    *st << count;
    for (int f = 0; f < Frames.length(); ++f) {
      *st << Frames[f].Name;
      *st << Frames[f].Scale.x << Frames[f].Scale.y << Frames[f].Scale.z;
      *st << Frames[f].Origin.x << Frames[f].Origin.y << Frames[f].Origin.z;
      *st << Frames[f].TriCount;
      vuint32 vidx = (vuint32)(ptrdiff_t)(Frames[f].Verts-&AllVerts[0]);
      *st << vidx;
      vidx = (vuint32)(ptrdiff_t)(Frames[f].Normals-&AllNormals[0]);
      *st << vidx;
    }
  }

  // skins
  {
    vuint16 skins = Skins.length();
    *st << skins;
    for (int f = 0; f < Skins.length(); ++f) {
      VStr fname = VStr(*Skins[f].fileName);
      vassert(fname.startsWith("\x01:voxtexure:"));
      VStr ss = fname;
      ss.chopLeft(12);
      vassert(ss.length());
      int idx = VStr::atoi(*ss);
      vassert(idx >= 0 && idx < voxTextures.length());
      vuint16 w = voxTextures[idx].width;
      vuint16 h = voxTextures[idx].height;
      *st << w << h;
      st->Serialise(voxTextures[idx].data, voxTextures[idx].width*voxTextures[idx].height*4);
    }
  }
}


//==========================================================================
//
//  VMeshModel::GenKVXCacheName
//
//==========================================================================
VStr VMeshModel::GenKVXCacheName (const vuint8 *Data, int DataSize) {
  RIPEMD160_Ctx ctx;
  ripemd160_init(&ctx);
  vuint8 b = (useVoxelPivotZ ? 1 : 0);
  ripemd160_put(&ctx, &b, 1);
  ripemd160_put(&ctx, Data, DataSize);
  vuint8 hash[RIPEMD160_BYTES];
  ripemd160_finish(&ctx, hash);

  VStr res;
  for (int f = 0; f < RIPEMD160_BYTES; ++f) {
    res += va("%02x", hash[f]);
  }

  return res;
}
