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

static VCvarI vox_cache_compression_level("vox_cache_compression_level", "6", "Voxel cache file compression level [0..9]", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);


#define VOX_CACHE_SIGNATURE  "k8vavoom voxel model cache file, version 1\n"


struct ColorUV {
  float u, v;
};

struct Voxel {
  TArray<VVoxVertex> vertices;
  TArray<int> indicies;

  vuint8 *coltex; // texture with colors, RGBA
  int ctxWidth, ctxHeight;

  TArray<vuint32> colors;
  TArray<ColorUV> colorsUV;
  TMap<vuint32, vuint32> clrhash; // key: RGB; value: index in `colors`
  TMapNC<VVoxVertex, vuint32> vtxhash; // key: vertex; value: index in `vertices`
  int vtxTotal;

  // center point
  float cx, cy, cz;
  bool useVoxelPivotZ;

  Voxel ();
  ~Voxel ();

  inline void appendColor (vuint32 rgb) {
    rgb &= 0xffffffU;
    auto cidxp = clrhash.get(rgb);
    if (!cidxp) {
      const vuint32 cidx = (vuint32)colors.length();
      colors.append(rgb);
      clrhash.put(rgb, cidx);
    }
  }

  inline vuint32 appendVertex (const VVoxVertex &vx) {
    ++vtxTotal;
    auto vxp = vtxhash.get(vx);
    if (vxp) return *vxp;
    appendColor(vx.rgb);
    const vuint32 vidx = (vuint32)vertices.length();
    vertices.append(vx);
    vtxhash.put(vx, vidx);
    return vidx;
  }

  void addCube (vuint8 cull, float x, float y, float z, vuint32 rgb);

  // also sets `uu` and `vv` for vertices
  void createColorTexture (VStr fname);

  bool loadKV6 (VStream &st);
  bool loadKVX (VStream &st);
};


struct VxVox {
  enum VxType {
    X0Y0Z0,
    X0Y0Z1,
    X0Y1Z0,
    X0Y1Z1,
    X1Y0Z0,
    X1Y0Z1,
    X1Y1Z0,
    X1Y1Z1,
    //
    VXMAX,
  };

  int used[VXMAX];

  VxVox () {
    for (int f = 0; f < VXMAX; ++f) used[f] = -1;
  }

  inline int append (Voxel *vox, VxType type, vuint8 num, float x, float y, float z, vuint32 rgb) {
    if (used[type] < 0) {
      VVoxVertex vx;
      vx.x = 1.0f*(x+(type >= 4 ? 1 : 0));
      vx.y = 1.0f*(y+(type%4 >= 2 ? 1 : 0));
      vx.z = 1.0f*(z+(type%2 == 1 ? 1 : 0));
      vx.uu = 0.0f;
      vx.vv = 0.0f;
      vx.rgb = rgb&0xffffffU;
      const int vidx = (int)vox->appendVertex(vx);
      used[type] = vidx;
    }
    return used[type];
  }
};


//==========================================================================
//
//  Voxel::Voxel
//
//==========================================================================
Voxel::Voxel ()
  : vertices()
  , indicies()
  , coltex(nullptr)
  , ctxWidth(0)
  , ctxHeight(0)
  , colors()
  , colorsUV()
  , clrhash()
  , vtxhash()
  , vtxTotal(0)
  , cx(0.0f)
  , cy(0.0f)
  , cz(0.0f),
  useVoxelPivotZ(false)
{
}


//==========================================================================
//
//  Voxel::~Voxel
//
//==========================================================================
Voxel::~Voxel () {
  if (coltex) Z_Free(coltex);
}


//==========================================================================
//
//  Voxel::createColorTexture
//
//  also sets `uu` and `vv` for vertices
//
//==========================================================================
void Voxel::createColorTexture (VStr fname) {
  ctxWidth = ctxHeight = 1;
  while (ctxWidth*ctxHeight < colors.length()) {
    if (ctxWidth > ctxHeight) {
      ctxHeight *= 2;
      if (ctxHeight > 1024) Sys_Error("too many voxel colors in '%s'", *fname);
    } else {
      ctxWidth *= 2;
      if (ctxWidth > 1024) Sys_Error("too many voxel colors in '%s'", *fname);
    }
  }

  if (coltex) Z_Free(coltex);
  coltex = (vuint8 *)Z_Calloc(ctxWidth*ctxHeight*4);

  colorsUV.setLength(colors.length());
  for (int f = 0; f < colors.length(); ++f) {
    const vuint32 rgb = colors[f];
    const int x = f%ctxWidth;
    const int y = f/ctxWidth;
    int cofs = (y*ctxWidth+x)*4;
    coltex[cofs++] = (rgb>>16)&0xff;
    coltex[cofs++] = (rgb>>8)&0xff;
    coltex[cofs++] = rgb&0xff;
    coltex[cofs++] = 0xff;
    ColorUV uv;
    uv.u = (0.5f+x)/(float)ctxWidth;
    uv.v = (0.5f+y)/(float)ctxHeight;
    colorsUV[f] = uv;
  }

  // fix vertex (u,v)
  for (int f = 0; f < vertices.length(); ++f) {
    auto cptr = clrhash.get(vertices[f].rgb);
    vassert(cptr);
    vertices[f].uu = colorsUV[*cptr].u;
    vertices[f].vv = colorsUV[*cptr].v;
  }

  GCon->Logf(NAME_Init, "...created voxel color texture (%dx%d) for %d colors", ctxWidth, ctxHeight, colors.length());
}



//==========================================================================
//
//  Voxel::addCube
//
//==========================================================================
void Voxel::addCube (vuint8 cull, float x, float y, float z, vuint32 rgb) {
  #define getVx(tp_,nm_)  indicies.append(vx.append(this, VxVox::VxType::tp_, nm_, x, y, z, rgb))

  if ((cull&0x3f) == 0) return;

  VxVox vx;

  // front
  if (cull&0x20) {
    getVx(X0Y0Z0, 0);
    getVx(X1Y0Z0, 1);
    getVx(X1Y1Z0, 2);
    getVx(X0Y1Z0, 3);
  }
  // back
  if (cull&0x10) {
    getVx(X0Y1Z1, 0);
    getVx(X1Y1Z1, 1);
    getVx(X1Y0Z1, 2);
    getVx(X0Y0Z1, 3);
  }
  // left
  if (cull&0x02) {
    getVx(X0Y0Z0, 0);
    getVx(X0Y1Z0, 1);
    getVx(X0Y1Z1, 2);
    getVx(X0Y0Z1, 3);
  }
  // right
  if (cull&0x01) {
    getVx(X1Y0Z1, 0);
    getVx(X1Y1Z1, 1);
    getVx(X1Y1Z0, 2);
    getVx(X1Y0Z0, 3);
  }
  // top
  if (cull&0x04) {
    getVx(X0Y0Z0, 0);
    getVx(X0Y0Z1, 1);
    getVx(X1Y0Z1, 2);
    getVx(X1Y0Z0, 3);
  }
  // bottom
  if (cull&0x08) {
    getVx(X1Y1Z0, 0);
    getVx(X1Y1Z1, 1);
    getVx(X0Y1Z1, 2);
    getVx(X0Y1Z0, 3);
  }

  #undef getVx
}

struct KVox {
  vuint32 rgb;
  vuint8 zlo, zhi;
  vuint8 cull;
  vuint8 normidx;
};


//==========================================================================
//
//  Voxel::loadKV6
//
//==========================================================================
bool Voxel::loadKV6 (VStream &st) {
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

  TArray<KVox> kvox;// = new KVox[](voxcount);
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
  for (int y = 0; y < ysiz; ++y) {
    for (int x = 0; x < xsiz; ++x) {
      vuint32 sofs = xofs[x]+xyofs[x*ww+y];
      vuint32 eofs = xofs[x]+xyofs[x*ww+y+1];
      //if (sofs == eofs) continue;
      //assert(sofs < data.length && eofs <= data.length);
      while (sofs < eofs) {
        const KVox &kv = kvox[sofs++];
        //debug(kvx_dump) conwritefln("  x=%s; y=%s; zlo=%s; zhi=%s; cull=0x%02x", x, y, kv.zlo, kv.zhi, kv.cull);
        float z = kv.zlo;
        for (int cidx = 0; cidx <= kv.zhi; ++cidx) {
          //addCube(kv.cull, (xsiz-x-1)-xpivot, y-ypivot, (zsiz-z-1)-zpivot, kv.rgb);
          addCube(kv.cull, (xsiz-x-1)-xpivot, y-ypivot, (zsiz-z-1)-(useVoxelPivotZ ? zpivot : 0.0f), kv.rgb);
          z += 1.0f;
        }
      }
    }
  }

  cx = xpivot;
  cy = ypivot;
  cz = zpivot;

  return true;
}


//==========================================================================
//
//  Voxel::loadKVX
//
//==========================================================================
bool Voxel::loadKVX (VStream &st) {
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
        //conwritefln("  x=%s; y=%s; z=%s; len=%s; cull=0x%02x", x, y, ztop, zlen, cull);
        // colors
        for (vuint32 cidx = 0; cidx < zlen; ++cidx) {
          vuint8 palcol = data[sofs++];
          vuint8 cl = cull;
          if (cidx != 0) cl &= ~0x10;
          if (cidx != zlen-1) cl &= ~0x20;
          const vuint8 r = pal[palcol*3+0];
          const vuint8 g = pal[palcol*3+1];
          const vuint8 b = pal[palcol*3+2];
          //addCube(cl, (xsiz-x-1)-px, y-py, (zsiz-ztop-1)-pz, b|(g<<8)|(r<<16));
          addCube(cl, (xsiz-x-1)-px, y-py, (zsiz-ztop-1)-(useVoxelPivotZ ? pz : 0.0f), b|(g<<8)|(r<<16));
          ++ztop;
        }
      }
      //assert(sofs == eofs);
    }
  }

  cx = px;
  cy = py;
  cz = pz;

  return true;
}


// ////////////////////////////////////////////////////////////////////////// //
class VVoxTexture : public VTexture {
public:
  int VoxTexIndex;
  vint32 ShadeVal;

public:
  VVoxTexture (int vidx, int ashade);
  virtual ~VVoxTexture () override;
  virtual void ReleasePixels () override;
  virtual vuint8 *GetPixels () override;
  virtual rgba_t *GetPalette () override;
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
{
  vassert(vidx >= 0 && vidx < voxTextures.length());
  SourceLump = -1;
  Type = TEXTYPE_Pic;
  Name = NAME_None; //VName(*voxTextures[vidx].fname);
  Width = voxTextures[vidx].width;
  Height = voxTextures[vidx].height;
  mFormat = mOrigFormat = TEXFMT_RGBA;
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
//  VVoxTexture::ReleasePixels
//
//==========================================================================
void VVoxTexture::ReleasePixels () {
  VTexture::ReleasePixels();
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


struct XVtxInfo {
  VMeshModel *mdl;
  Voxel *vox;
  TMapNC<int, int> firstVIdx; // key: vox vidx; value: first index in mdl
  TArray<int> mdlVList; // index: mdl vidx; value: next index in mdl, or -1
  int total;

  inline XVtxInfo () noexcept : mdl(nullptr), vox(nullptr), firstVIdx(), mdlVList(), total(0) {}
};


//==========================================================================
//
//  insVertex
//
//  FIXME: merge vertices
//
//==========================================================================
static int insVertex (int vidx, const TVec &normal, XVtxInfo &nfo) {
  ++nfo.total;

  // check for duplicate
  auto xpp = nfo.firstVIdx.get(vidx);
  if (xpp) {
    int idx = *xpp;
    while (idx >= 0) {
      if (nfo.mdl->AllNormals[idx] == normal) {
        return idx;
      }
      idx = nfo.mdlVList[idx];
    }
  }

  // new one
  const int res = nfo.mdl->AllVerts.length();
  nfo.mdl->AllVerts.append(nfo.vox->vertices[vidx].asTVec());

  vassert(nfo.mdl->STVerts.length() == res);
  VMeshSTVert st;
  st.S = nfo.vox->vertices[vidx].uu;
  st.T = nfo.vox->vertices[vidx].vv;
  nfo.mdl->STVerts.append(st);

  vassert(nfo.mdl->AllNormals.length() == res);
  nfo.mdl->AllNormals.append(normal);

  nfo.mdlVList.append(xpp ? *xpp : -1);
  nfo.firstVIdx.put(vidx, res);

  return res;
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

  this->Uploaded = false;
  this->VertsBuffer = 0;
  this->IndexBuffer = 0;
  this->GlMode = GlNone;

  VStr ccname = GenKVXCacheName(Data, DataSize);
  VStr cacheFileName = VStr("voxmdl_")+ccname+".cache";
  cacheFileName = FL_GetVoxelCacheDir()+"/"+cacheFileName;

  VStream *strm = FL_OpenSysFileRead(cacheFileName);
  if (strm) {
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
  }

  Voxel vox;
  vox.useVoxelPivotZ = useVoxelPivotZ;
  {
    VMemoryStreamRO memst(this->Name, Data, DataSize);
    vuint32 sign;
    memst << sign;
    memst.Seek(0);
    bool ok = false;
    if (sign == 0x6c78764bU) ok = vox.loadKV6(memst); else ok = vox.loadKVX(memst);
    if (!ok || memst.IsError()) Sys_Error("cannot load voxel model '%s'", *this->Name);
  }
  if (vox.indicies.length() == 0) Sys_Error("cannot load empty voxel model '%s'", *this->Name);
  if (vox.indicies.length()%4 != 0) Sys_Error("internal error converting voxel model '%s'", *this->Name);

  for (int f = 0; f < vox.indicies.length(); ++f) {
    const int vidx = vox.indicies[f];
    if (vidx < 0 || vidx >= vox.vertices.length()) {
      Sys_Error("internal error loading voxel model '%s' (vertex index %d is out of range)", *this->Name, f);
    }
  }

  GCon->Logf(NAME_Init, "voxel model '%s': %d total vertices, %d unique vertices, %d triangles",
             *this->Name, vox.vtxTotal, vox.vertices.length(), vox.indicies.length()/4*2);
  vox.createColorTexture(this->Name);

  // skins
  {
    if (voxTextures.length() == 0) GTextureManager.RegisterTextureLoader(&voxTextureLoader);
    VStr fname = va("\x01:voxtexure:%d", voxTextures.length());
    VoxelTexture &vxt = voxTextures.alloc();
    vxt.fname = fname;
    vxt.width = vox.ctxWidth;
    vxt.height = vox.ctxHeight;
    vxt.data = vox.coltex;
    //vxt.tex = nullptr;
    vox.coltex = nullptr;
    Skins.setLength(1);
    VMeshModel::SkinInfo &si = Skins[0];
    si.fileName = VName(*fname);
    si.textureId = -1;
    si.shade = -1;
  }

  // quads -> triangles
  // also create triangle strips
  XVtxInfo nfo;
  nfo.mdl = this;
  nfo.vox = &vox;
  Tris.setLength(vox.indicies.length()/4*2);
  GlMode = GlTriangleFan;
  TriVerts.setLength(vox.indicies.length()/4*5);
  int triidx = 0;
  int tvxidx = 0;
  for (int qidx = 0; qidx < vox.indicies.length(); qidx += 4, triidx += 2) {
    const TVec v1 = vox.vertices[vox.indicies[qidx+0]].asTVec();
    const TVec v2 = vox.vertices[vox.indicies[qidx+1]].asTVec();
    const TVec v3 = vox.vertices[vox.indicies[qidx+2]].asTVec();
    const TVec d1 = v2-v3;
    const TVec d2 = v1-v3;
    TVec PlaneNormal = d1.cross(d2);
    if (PlaneNormal.lengthSquared() == 0.0f) {
      PlaneNormal = TVec(0.0f, 0.0f, 1.0f);
    } else {
      PlaneNormal = PlaneNormal.normalise();
    }

    const int vx0 = insVertex(vox.indicies[qidx+0], PlaneNormal, nfo);
    const int vx1 = insVertex(vox.indicies[qidx+1], PlaneNormal, nfo);
    const int vx2 = insVertex(vox.indicies[qidx+2], PlaneNormal, nfo);
    const int vx3 = insVertex(vox.indicies[qidx+3], PlaneNormal, nfo);

    VMeshTri &tri0 = Tris[triidx+0];
    tri0.VertIndex[0] = vx0;
    tri0.VertIndex[1] = vx1;
    tri0.VertIndex[2] = vx2;

    VMeshTri &tri1 = Tris[triidx+1];
    tri1.VertIndex[0] = vx2;
    tri1.VertIndex[1] = vx3;
    tri1.VertIndex[2] = vx0;

    TriVerts[tvxidx++] = vx0;
    TriVerts[tvxidx++] = vx1;
    TriVerts[tvxidx++] = vx2;
    TriVerts[tvxidx++] = vx3;
    TriVerts[tvxidx++] = 65535; // primitive break
  }

  if (AllVerts.length() > 65534) {
    GCon->Logf(NAME_Init, "3d model for voxel '%s' has too many vertices", *this->Name);
  }

  AllNormals.condense();
  AllVerts.condense();
  STVerts.condense();
  vassert(AllNormals.length() == AllVerts.length());

  GCon->Logf(NAME_Init, "created 3d model for voxel '%s'; %d total vertices, %d unique vertices",
             *this->Name, nfo.total, AllVerts.length());

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

  strm = FL_OpenSysFileWrite(cacheFileName);
  if (strm) {
    GCon->Logf(NAME_Init, "...writing cache to '%s'...", *ccname);
    strm->Serialise(VOX_CACHE_SIGNATURE, (int)strlen(VOX_CACHE_SIGNATURE));
    *strm << this->Name;
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
