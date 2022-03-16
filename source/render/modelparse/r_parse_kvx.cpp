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

static VCvarI vox_cache_compression_level("vox_cache_compression_level", "6", "Voxel cache file compression level [0..9].", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarB vox_cache_enabled("vox_cache_enabled", false, "Enable caching of converted voxel models?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarB vox_verbose_conversion("vox_verbose_conversion", false, "Show info messages from voxel converter?", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);
static VCvarI kvx_optimized("vox_optimisation", "4", "Voxel loader optimisation (higher is better, but with space ants) [0..4].", CVAR_PreInit|CVAR_Archive|CVAR_NoShadow);

// useful for debugging, but not really needed
#define VOX_ENABLE_INVARIANT_CHECK


#define VOX_CACHE_SIGNATURE  "k8vavoom voxel model cache file, version 2\n"


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


// ////////////////////////////////////////////////////////////////////////// //
// loaded voxel data to query coords, etc.
// slab is always going up
struct __attribute__((packed)) VoxPix {
  vuint8 b, g, r;
  vuint8 cull;
  vuint32 nextz; // voxel with the next z; 0 means "no more"
  vuint16 z; // z of the current voxel

  inline vuint32 rgb () const noexcept {
    return 0xff000000U|b|((vuint32)g<<8)|((vuint32)r<<16);
  }

  inline vuint32 rgbcull () const noexcept {
    return b|((vuint32)g<<8)|((vuint32)r<<16)|((vuint32)cull<<24);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct __attribute__((packed)) VoxXYZ16 {
  vuint16 x, y, z;

  VVA_FORCEINLINE VoxXYZ16 () {}

  VVA_FORCEINLINE VoxXYZ16 (int ax, int ay, int az)
    : x((vuint16)ax)
    , y((vuint16)ay)
    , z((vuint16)az)
  {}
};

// used for hollow fills
struct VoxBitmap {
  vuint32 xsize, ysize, zsize;
  vuint32 xwdt, xywdt;
  vuint32 *bmp; // bit per voxel

public:
  inline VoxBitmap () noexcept : xsize(0), ysize(0), zsize(0), xwdt(0), xywdt(0), bmp(nullptr) {}
  inline ~VoxBitmap () noexcept { clear(); }

  inline void clear () noexcept {
    if (bmp) Z_Free(bmp);
    bmp = nullptr;
    xsize = ysize = zsize = xwdt = xywdt = 0;
  }

  inline void setSize (vuint32 xs, vuint32 ys, vuint32 zs) {
    clear();
    if (!xs || !ys || !zs) return;
    xsize = xs;
    ysize = ys;
    zsize = zs;
    xwdt = (xs+31)/32;
    vassert(xwdt<<5 >= xs);
    xywdt = xwdt*ysize;
    bmp = (vuint32 *)Z_Malloc(xywdt*zsize*sizeof(vuint32));
    memset(bmp, 0, xywdt*zsize*sizeof(vuint32));
  }

  VVA_FORCEINLINE void setPixel (int x, int y, int z) noexcept {
    if (x < 0 || y < 0 || z < 0) return;
    if ((unsigned)x >= xsize || (unsigned)y >= ysize || (unsigned)z >= zsize) return;
    bmp[(unsigned)z*xywdt+(unsigned)y*xwdt+((unsigned)x>>5)] |= 1U<<((unsigned)x&0x1f);
  }

  VVA_FORCEINLINE vuint32 getPixel (int x, int y, int z) const noexcept {
    if (x < 0 || y < 0 || z < 0) return 1;
    if ((unsigned)x >= xsize || (unsigned)y >= ysize || (unsigned)z >= zsize) return 1;
    return (bmp[(unsigned)z*xywdt+(unsigned)y*xwdt+((unsigned)x>>5)]&(1U<<((unsigned)x&0x1f)));
  }
};


// ////////////////////////////////////////////////////////////////////////// //
struct VoxelData {
public:
  vuint32 xsize, ysize, zsize;
  float cx, cy, cz;

  TArray<VoxPix> data; // [0] is never used
  // xsize*ysize array, offsets in `data`; 0 means "no data here"
  // slabs are sorted from bottom to top, and never intersects
  TArray<vuint32> xyofs;
  vuint32 freelist;
  vuint32 voxpixtotal;

private:
  vuint32 allocVox ();

public:
  inline VoxelData () noexcept
    : xsize(0)
    , ysize(0)
    , zsize(0)
    , cx(0.0f)
    , cy(0.0f)
    , cz(0.0f)
    , data()
    , xyofs()
    , freelist(0)
    , voxpixtotal(0)
  {}

  inline ~VoxelData () { clear(); }

  static VVA_FORCEINLINE vuint8 cullmask (vuint32 cidx) noexcept {
    return (vuint8)(1U<<cidx);
  }

  // opposite mask
  static VVA_FORCEINLINE vuint8 cullopmask (vuint32 cidx) noexcept {
    return (vuint8)(1U<<(cidx^1));
  }

  // high byte is cull info
  // returns 0 if there is no such voxel
  VVA_FORCEINLINE vuint32 voxofs (int x, int y, int z) const noexcept {
    if (x < 0 || y < 0 || z < 0) return 0;
    if ((vuint32)x >= xsize || (vuint32)y >= ysize || (vuint32)z >= zsize) return 0;
    vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x];
    while (dofs) {
      if (data[dofs].z == (vuint16)z) return dofs;
      if (data[dofs].z > (vuint16)z) return 0;
      dofs = data[dofs].nextz;
    }
    return 0;
  }

  VVA_FORCEINLINE vuint32 getDOfs (int x, int y) const noexcept {
    if (x < 0 || y < 0) return 0;
    if ((vuint32)x >= xsize || (vuint32)y >= ysize) return 0;
    return xyofs[(vuint32)y*xsize+(vuint32)x];
  }

  // high byte is cull info
  // returns 0 if there is no such voxel
  VVA_FORCEINLINE vuint32 query (int x, int y, int z) const noexcept {
    const vuint32 dofs = voxofs(x, y, z);
    if (!dofs) return 0;
    return (data[dofs].cull ? data[dofs].rgbcull() : 0);
  }

  VVA_FORCEINLINE VoxPix *queryVP (int x, int y, int z) noexcept {
    const vuint32 dofs = voxofs(x, y, z);
    return (dofs ? &data[dofs] : nullptr);
  }

  VVA_FORCEINLINE vuint8 queryCull (int x, int y, int z) const noexcept {
    const vuint32 dofs = voxofs(x, y, z);
    return (dofs ? data[dofs].cull : 0);
  }

  void clear ();
  void setSize (vuint32 xs, vuint32 ys, vuint32 zs);

  void removeVoxel (int x, int y, int z);
  void addVoxel (int x, int y, int z, vuint32 rgb, vuint8 cull);

  // only for existing voxels; won't remove empty voxels
  void setVoxelCull (int x, int y, int z, vuint8 cull);

  #ifdef VOX_ENABLE_INVARIANT_CHECK
  void checkInvariants ();
  #endif
  void removeEmptyVoxels ();
  // remove inside voxels, leaving only contour
  void removeInside ();
  vuint32 fixFaceVisibility ();
  vuint32 hollowFill ();
  void optimise (bool hollowOpt);

public:
  static const int cullofs[6][3];
};


const int VoxelData::cullofs[6][3] = {
  { 1, 0, 0}, // left
  {-1, 0, 0}, // right
  { 0,-1, 0}, // near
  { 0, 1, 0}, // far
  { 0, 0, 1}, // top
  { 0, 0,-1}, // bottom
};


//==========================================================================
//
//  VoxelData::allocVox
//
//==========================================================================
vuint32 VoxelData::allocVox () {
  vassert(data.length());
  ++voxpixtotal;
  if (!freelist) {
    if (data.length() >= 0x3fffffff) Sys_Error("too many voxels in voxel data");
    const vuint32 lastel = (vuint32)data.length();
    data.setLength(data.length()+1024);
    freelist = (vuint32)data.length()-1;
    while (freelist >= lastel) {
      data[freelist].nextz = freelist+1;
      --freelist;
    }
    freelist = lastel;
    data[data.length()-1].nextz = 0;
  }
  const vuint32 res = freelist;
  freelist = data[res].nextz;
  return res;
}


//==========================================================================
//
//  VoxelData::clear
//
//==========================================================================
void VoxelData::clear () {
  data.clear();
  xyofs.clear();
  xsize = ysize = zsize = 0;
  cx = cy = cz = 0.0f;
  freelist = 0;
  voxpixtotal = 0;
}


//==========================================================================
//
//  VoxelData::setSize
//
//==========================================================================
void VoxelData::setSize (vuint32 xs, vuint32 ys, vuint32 zs) {
  clear();
  if (!xs || !ys || !zs) return;
  xsize = xs;
  ysize = ys;
  zsize = zs;
  xyofs.setLength(xsize*ysize);
  memset(xyofs.ptr(), 0, xsize*ysize*sizeof(vuint32));
  data.setLengthReserve(1); // data[0] is never used
}


//==========================================================================
//
//  VoxelData::removeVoxel
//
//==========================================================================
void VoxelData::removeVoxel (int x, int y, int z) {
  if (x < 0 || y < 0 || z < 0) return;
  if ((vuint32)x >= xsize || (vuint32)y >= ysize || (vuint32)z >= zsize) return;
  vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x];
  vuint32 prevdofs = 0;
  while (dofs) {
    if (data[dofs].z == (vuint16)z) {
      // remove this voxel
      if (prevdofs) {
        data[prevdofs].nextz = data[dofs].nextz;
      } else {
        xyofs[(vuint32)y*xsize+(vuint32)x] = data[dofs].nextz;
      }
      data[dofs].nextz = freelist;
      freelist = dofs;
      --voxpixtotal;
      return;
    }
    if (data[dofs].z > (vuint16)z) return;
    prevdofs = dofs;
    dofs = data[dofs].nextz;
  }
}


//==========================================================================
//
//  VoxelData::addVoxel
//
//==========================================================================
void VoxelData::addVoxel (int x, int y, int z, vuint32 rgb, vuint8 cull) {
  cull &= 0x3f;
  if (!cull) { removeVoxel(x, y, z); return; }
  if (x < 0 || y < 0 || z < 0) return;
  if ((vuint32)x >= xsize || (vuint32)y >= ysize || (vuint32)z >= zsize) return;
  vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x];
  vuint32 prevdofs = 0;
  while (dofs) {
    if (data[dofs].z == (vuint16)z) {
      // replace this voxel
      data[dofs].b = rgb&0xff;
      data[dofs].g = (rgb>>8)&0xff;
      data[dofs].r = (rgb>>16)&0xff;
      data[dofs].cull = cull;
      return;
    }
    if (data[dofs].z > (vuint16)z) break;
    prevdofs = dofs;
    dofs = data[dofs].nextz;
  }
  // insert before dofs
  vuint32 vidx = allocVox();
  data[vidx].b = rgb&0xff;
  data[vidx].g = (rgb>>8)&0xff;
  data[vidx].r = (rgb>>16)&0xff;
  data[vidx].cull = cull;
  data[vidx].z = (vuint16)z;
  data[vidx].nextz = dofs;
  if (prevdofs) {
    vassert(data[prevdofs].nextz == dofs);
    data[prevdofs].nextz = vidx;
  } else {
    xyofs[(vuint32)y*xsize+(vuint32)x] = vidx;
  }
}


//==========================================================================
//
//  VoxelData::setVoxelCull
//
//  only for existing voxels; won't remove empty voxels
//
//==========================================================================
void VoxelData::setVoxelCull (int x, int y, int z, vuint8 cull) {
  cull &= 0x3f;
  if (x < 0 || y < 0 || z < 0) return;
  if ((vuint32)x >= xsize || (vuint32)y >= ysize || (vuint32)z >= zsize) return;
  vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x];
  while (dofs) {
    if (data[dofs].z == (vuint16)z) {
      // replace this voxel
      data[dofs].cull = cull;
      return;
    }
    dofs = data[dofs].nextz;
  }
}


#ifdef VOX_ENABLE_INVARIANT_CHECK
//==========================================================================
//
//  VoxelData::checkInvariants
//
//==========================================================================
void VoxelData::checkInvariants () {
  vuint32 total = 0;
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x];
      if (!dofs) continue;
      ++total;
      vuint16 prevz = data[dofs].z;
      dofs = data[dofs].nextz;
      while (dofs) {
        vassert(prevz < data[dofs].z);
        prevz = data[dofs].z;
        dofs = data[dofs].nextz;
        ++total;
      }
    }
  }
  vassert(total == voxpixtotal);
}
#endif


//==========================================================================
//
//  VoxelData::removeEmptyVoxels
//
//==========================================================================
void VoxelData::removeEmptyVoxels () {
  vuint32 count = 0;
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x];
      if (!dofs) continue;
      vuint32 prevdofs = 0;
      while (dofs) {
        if (!data[dofs].cull) {
          // remove it
          const vuint32 ndofs = data[dofs].nextz;
          if (prevdofs) {
            data[prevdofs].nextz = ndofs;
          } else {
            xyofs[(vuint32)y*xsize+(vuint32)x] = ndofs;
          }
          data[dofs].nextz = freelist;
          freelist = dofs;
          --voxpixtotal;
          dofs = ndofs;
          ++count;
        } else {
          prevdofs = dofs;
          dofs = data[dofs].nextz;
        }
      }
    }
  }
}


//==========================================================================
//
//  VoxelData::removeInside
//
//  remove inside voxels, leaving only contour
//
//==========================================================================
void VoxelData::removeInside () {
  for (vint32 y = 0; y < (vint32)ysize; ++y) {
    for (vint32 x = 0; x < (vint32)xsize; ++x) {
      vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x];
      while (dofs) {
        if (data[dofs].cull) {
          // check
          for (vuint32 cidx = 0; cidx < 6; ++cidx) {
            // go in this dir, removing the corresponding voxel side
            const vuint8 cmask = cullmask(cidx);
            const vuint8 opmask = cullopmask(cidx);
            const vuint8 checkmask = cmask|opmask;
            int vx = x, vy = y, vz = (int)data[dofs].z;
            vuint32 myofs = dofs;
            while (myofs && (data[myofs].cull&cmask)) {
              const int sx = vx+cullofs[cidx][0];
              const int sy = vy+cullofs[cidx][1];
              const int sz = vz+cullofs[cidx][2];
              const vuint32 sofs = voxofs(sx, sy, sz);
              if (!sofs) break;
              if (!(data[sofs].cull&checkmask)) break;
              // fix culls
              data[myofs].cull ^= cmask;
              data[sofs].cull &= (vuint8)(~(vuint32)opmask);
              vx = sx;
              vy = sy;
              vz = sz;
              myofs = sofs;
            }
          }
        }
        dofs = data[dofs].nextz;
      }
    }
  }
}


//==========================================================================
//
//  VoxelData::fixFaceVisibility
//
//  if we have ANY voxel at the corresponding side, don't render that face
//
//  returns number of fixed voxels
//
//==========================================================================
vuint32 VoxelData::fixFaceVisibility () {
  vuint32 count = 0;
  for (vint32 y = 0; y < (vint32)ysize; ++y) {
    for (vint32 x = 0; x < (vint32)xsize; ++x) {
      for (vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x]; dofs; dofs = data[dofs].nextz) {
        const vuint8 ocull = data[dofs].cull;
        if (!ocull) continue;
        const int z = (int)data[dofs].z;
        for (vuint32 cidx = 0; cidx < 6; ++cidx) {
          const vuint8 cmask = cullmask(cidx);
          if (data[dofs].cull&cmask) {
            if (query(x+cullofs[cidx][0], y+cullofs[cidx][1], z+cullofs[cidx][2])) {
              data[dofs].cull ^= cmask; // reset bit
            }
          }
        }
        count += (data[dofs].cull != ocull);
      }
    }
  }
  return count;
}


//==========================================================================
//
//  VoxelData::hollowFill
//
//  this fills everything outside of the voxel, and
//  then resets culling bits for all invisible faces
//  i don't care about memory yet
//
//==========================================================================
vuint32 VoxelData::hollowFill () {
  VoxBitmap bmp;
  VoxXYZ16 xyz;
  VoxXYZ16 *stack;
  vuint32 stacksize = 1024;
  vuint32 stackpos = 1;

  bmp.setSize(xsize+2, ysize+2, zsize+2);
  // this is definitely empty
  xyz.x = xyz.y = xyz.z = 0;
  stack = (VoxXYZ16 *)Z_Malloc(stacksize*sizeof(VoxXYZ16));
  stack[0] = xyz;

  const int deltas[6][3] = {
    {-1, 0, 0},
    { 1, 0, 0},
    {0,-1, 0},
    {0, 1, 0},
    {0, 0,-1},
    {0, 0, 1},
  };

  while (stackpos) {
    xyz = stack[--stackpos];
    if (bmp.getPixel(xyz.x, xyz.y, xyz.z)) continue;
    bmp.setPixel(xyz.x, xyz.y, xyz.z);
    for (unsigned dd = 0; dd < 6; ++dd) {
      const int nx = (int)xyz.x+deltas[dd][0];
      const int ny = (int)xyz.y+deltas[dd][1];
      const int nz = (int)xyz.z+deltas[dd][2];
      if (bmp.getPixel(nx, ny, nz)) continue;
      if (queryCull(nx-1, ny-1, nz-1)) continue;
      if (stackpos == stacksize) {
        stacksize += 16384;
        stack = (VoxXYZ16 *)Z_Realloc(stack, stacksize*sizeof(VoxXYZ16));
      }
      stack[stackpos++] = VoxXYZ16(nx, ny, nz);
    }
  }
  Z_Free(stack);

  // now check it
  vuint32 changed = 0;
  for (vint32 y = 0; y < (vint32)ysize; ++y) {
    for (vint32 x = 0; x < (vint32)xsize; ++x) {
      for (vuint32 dofs = xyofs[(vuint32)y*xsize+(vuint32)x]; dofs; dofs = data[dofs].nextz) {
        const vuint8 omask = data[dofs].cull;
        if (!omask) continue;
        // check
        const int z = (int)data[dofs].z;
        for (vuint32 cidx = 0; cidx < 6; ++cidx) {
          // go in this dir, removing the corresponding voxel side
          const vuint8 cmask = cullmask(cidx);
          if (data[dofs].cull&cmask) {
            if (!bmp.getPixel(x+cullofs[cidx][0]+1, y+cullofs[cidx][1]+1, z+cullofs[cidx][2]+1)) {
              // reset this cull bit
              data[dofs].cull ^= cmask;
            }
          }
        }
        if (omask != data[dofs].cull) ++changed;
      }
    }
  }
  return changed;
}


//==========================================================================
//
//  VoxelData::optimise
//
//==========================================================================
void VoxelData::optimise (bool hollowOpt) {
  #ifdef VOX_ENABLE_INVARIANT_CHECK
  checkInvariants();
  #endif
  const vuint32 oldtt = voxpixtotal;

  removeInside();
  const vuint32 newtt = voxpixtotal;
  if (oldtt != newtt) {
    vassert(oldtt > newtt);
    const vuint32 vdifftt = oldtt-newtt;
    if (vox_verbose_conversion.asBool()) {
      GCon->Logf(NAME_Init, "  removed %u interior voxel%s", vdifftt, (vdifftt != 1 ? "s" : ""));
    }
  }

  vuint32 count = fixFaceVisibility();
  if (count) {
    if (vox_verbose_conversion.asBool()) {
      GCon->Logf(NAME_Init, "  culling: fixed %u voxel%s", count, (count != 1 ? "s" : ""));
    }
  }

  if (hollowOpt) {
    count = hollowFill();
    if (count && vox_verbose_conversion.asBool()) {
      GCon->Logf(NAME_Init, "  hollow check fixed %u voxel%s", count, (count != 1 ? "s" : ""));
    }
  }

  removeEmptyVoxels();
  if (vox_verbose_conversion.asBool()) {
    if (voxpixtotal != oldtt) {
      vassert(oldtt > voxpixtotal);
      GCon->Logf(NAME_Init, "  mesh optimised from %u to %u voxel%s", oldtt, voxpixtotal, (voxpixtotal != 1 ? "s" : ""));
    } else {
      GCon->Logf(NAME_Init, "  mesh contains %u voxel%s", voxpixtotal, (voxpixtotal != 1 ? "s" : ""));
    }
  }

  #ifdef VOX_ENABLE_INVARIANT_CHECK
  checkInvariants();
  #endif
}


// ////////////////////////////////////////////////////////////////////////// //
struct VoxMeshColorItem {
  vuint32 cidx;
  int next; // -1: no more
};

struct VoxelMesh {
public:
  // quad type
  enum {
    Point,
    XLong,
    YLong,
    ZLong,
    Invalid,
  };

  enum {
    X0_Y0_Z0,
    X0_Y0_Z1,
    X0_Y1_Z0,
    X0_Y1_Z1,
    X1_Y0_Z0,
    X1_Y0_Z1,
    X1_Y1_Z0,
    X1_Y1_Z1,
  };

  enum {
    DMV_X = 0b100,
    DMV_Y = 0b010,
    DMV_Z = 0b001,
  };

  struct Vertex {
    float x, y, z;
    float dx, dy, dz;
  };

  // quad is always one texel strip
  struct VoxQuad {
    Vertex vx[4];
    vuint32 cidx; // index in `colors`
    vuint32 clen; // color run length (can be 1 for same colors)
    vuint32 imgidx; // used by GL builder
    Vertex normal;
    int type;

    inline VoxQuad () noexcept : type(Invalid) {}

    void calcNormal () {
      const TVec v1 = TVec(vx[0].x, vx[0].y, vx[0].z);
      const TVec v2 = TVec(vx[1].x, vx[1].y, vx[1].z);
      const TVec v3 = TVec(vx[2].x, vx[2].y, vx[2].z);
      const TVec d1 = v2-v3;
      const TVec d2 = v1-v3;
      TVec PlaneNormal = d1.cross(d2);
      if (PlaneNormal.lengthSquared() == 0.0f) {
        PlaneNormal = TVec(0.0f, 0.0f, 1.0f);
      } else {
        PlaneNormal = PlaneNormal.normalise();
      }
      normal.x = normal.dx = PlaneNormal.x;
      normal.y = normal.dy = PlaneNormal.y;
      normal.z = normal.dz = PlaneNormal.z;
    }
  };

  TArray<VoxQuad> quads;
  TArray<vuint32> colors; // all colors, in runs
  vuint32 maxColorRun;

  TArray<VoxMeshColorItem> citems;
  TMapNC<vuint32, int> citemhash; // key: color index; value: index in `citems`

  // voxel center point
  float cx, cy, cz;

private:
  vuint32 addColors (const vuint32 *clrs, vuint32 clen);

  static inline Vertex genVertex (unsigned type, const float x, const float y, const float z,
                                  const float xlen, const float ylen, const float zlen)
  {
    Vertex vx;
    vx.dx = vx.dy = vx.dz = 0.0f;
    vx.x = x;
    vx.y = y;
    vx.z = z;
    if (type&DMV_X) { vx.x += xlen; vx.dx = 1.0f; }
    if (type&DMV_Y) { vx.y += ylen; vx.dy = 1.0f; }
    if (type&DMV_Z) { vx.z += zlen; vx.dz = 1.0f; }
    return vx;
  }

  void addSlabFace (vuint8 cull, vuint8 dmv,
                    float x, float y, float z,
                    int len, const vuint32 *colors);

  void addCube (vuint8 cull, float x, float y, float z, vuint32 rgb);

  void buildOpt0 (VoxelData &vox, bool pivotz);
  void buildOpt1 (VoxelData &vox, bool pivotz);
  void buildOpt2 (VoxelData &vox, bool pivotz);
  void buildOpt3 (VoxelData &vox, bool pivotz);

public:
  inline VoxelMesh () noexcept
    : quads()
    , colors()
    , maxColorRun(0)
    , citems()
    , citemhash()
    , cx(0.0f)
    , cy(0.0f)
    , cz(0.0f)
  {}

  void clear ();
  void buildFrom (VoxelData &vox, bool pivotz);

public:
  static const uint8_t quadFaces[6][4];
};


const uint8_t VoxelMesh::quadFaces[6][4] = {
  // right (&0x01) (right)
  {
    X1_Y1_Z0,
    X1_Y0_Z0,
    X1_Y0_Z1,
    X1_Y1_Z1,
  },
  // left (&0x02) (left)
  {
    X0_Y0_Z0,
    X0_Y1_Z0,
    X0_Y1_Z1,
    X0_Y0_Z1,
  },
  // top (&0x04) (near)
  {
    X0_Y0_Z0,
    X0_Y0_Z1,
    X1_Y0_Z1,
    X1_Y0_Z0,
  },
  // bottom (&0x08) (far)
  {
    X1_Y1_Z0,
    X1_Y1_Z1,
    X0_Y1_Z1,
    X0_Y1_Z0,
  },
  // back (&0x10)  (top)
  {
    X0_Y1_Z1,
    X1_Y1_Z1,
    X1_Y0_Z1,
    X0_Y0_Z1,
  },
  // front (&0x20)  (bottom)
  {
    X0_Y0_Z0,
    X1_Y0_Z0,
    X1_Y1_Z0,
    X0_Y1_Z0,
  }
};


//==========================================================================
//
//  VoxelMesh::clear
//
//==========================================================================
void VoxelMesh::clear () {
  quads.clear();
  colors.clear();
  maxColorRun = 0;
  citems.clear();
  citemhash.clear();
  cx = cy = cz = 0.0f;
}


//==========================================================================
//
//  VoxelMesh::addColors
//
//==========================================================================
vuint32 VoxelMesh::addColors (const vuint32 *clrs, vuint32 clen) {
  vassert(clen);
  if (maxColorRun < clen) maxColorRun = clen;
  // look for duplicates
  const vuint32 cidx = (vuint32)colors.length();
  if (cidx >= clen) {
    auto cp = citemhash.get(clrs[0]);
    if (cp) {
      for (int idx = *cp; idx >= 0; idx = citems[idx].next) {
        const VoxMeshColorItem &it = citems[idx];
        if (cidx-it.cidx < clen) continue;
        if (memcmp(colors.ptr()+it.cidx, clrs, clen*sizeof(vuint32)) == 0) return it.cidx;
      }
    }
  }
  // register in hash
  VoxMeshColorItem &n = citems.alloc();
  n.cidx = cidx;
  auto xcp = citemhash.get(clrs[0]);
  if (xcp) n.next = *xcp; else n.next = -1;
  citemhash.put(clrs[0], (int)citems.length()-1);
  // append
  colors.setLengthReserve(colors.length()+clen);
  memcpy(colors.ptr()+cidx, clrs, clen*sizeof(vuint32));
  return cidx;
}


//==========================================================================
//
//  VoxelMesh::addSlabFace
//
//  dmv: bit 2 means XLong, bit 1 means YLong, bit 0 means ZLong
//
//==========================================================================
void VoxelMesh::addSlabFace (vuint8 cull, vuint8 dmv,
                             float x, float y, float z,
                             int len, const vuint32 *colors)
{
  if (len < 1) return;
  vassert(dmv == DMV_X || dmv == DMV_Y || dmv == DMV_Z);
  vassert(cull == 0x01 || cull == 0x02 || cull == 0x04 || cull == 0x08 || cull == 0x10 || cull == 0x20);

  vuint32 clen = (vuint32)len;

  bool allsame = true;
  for (vuint32 cidx = 1; cidx < clen; ++cidx) {
    if (colors[cidx] != colors[0]) {
      allsame = false;
      break;
    }
  }
  if (allsame) clen = 1;

  const int qtype =
    clen == 1 ? Point :
    (dmv&DMV_X) ? XLong :
    (dmv&DMV_Y) ? YLong :
    ZLong;
  const float dx = (dmv&DMV_X ? (float)len : 1.0f);
  const float dy = (dmv&DMV_Y ? (float)len : 1.0f);
  const float dz = (dmv&DMV_Z ? (float)len : 1.0f);
  vuint32 qidx;
  switch (cull) {
    case 0x01: qidx = 0; break;
    case 0x02: qidx = 1; break;
    case 0x04: qidx = 2; break;
    case 0x08: qidx = 3; break;
    case 0x10: qidx = 4; break;
    case 0x20: qidx = 5; break;
    default: Sys_Error("something is VERY wrong in `addSlabFace()`");
  }
  VoxQuad vq;
  for (vuint32 vidx = 0; vidx < 4; ++vidx) {
    vq.vx[vidx] = genVertex(quadFaces[qidx][vidx], x, y, z, dx, dy, dz);
  }
  vq.cidx = addColors(colors, clen);
  vq.clen = clen;
  vq.type = qtype;
  quads.append(vq);
}


//==========================================================================
//
//  VoxelMesh::addCube
//
//==========================================================================
void VoxelMesh::addCube (vuint8 cull, float x, float y, float z, vuint32 rgb) {
  // generate quads
  for (vuint32 qidx = 0; qidx < 6; ++qidx) {
    const vuint8 cmask = VoxelData::cullmask(qidx);
    if (cull&cmask) {
      addSlabFace(cmask, DMV_X/*doesn't matter*/, x, y, z, 1, &rgb);
    }
  }
}


//==========================================================================
//
//  VoxelMesh::buildOpt0
//
//==========================================================================
void VoxelMesh::buildOpt0 (VoxelData &vox, bool pivotz) {
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = (pivotz ? vox.cz : 0.0f);
  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      vuint32 dofs = vox.getDOfs(x, y);
      while (dofs) {
        addCube(vox.data[dofs].cull, x-px, y-py, vox.data[dofs].z-pz, vox.data[dofs].rgb());
        dofs = vox.data[dofs].nextz;
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::buildOpt1
//
//==========================================================================
void VoxelMesh::buildOpt1 (VoxelData &vox, bool pivotz) {
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = (pivotz ? vox.cz : 0.0f);
  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      // try slabs in all 6 directions?
      vuint32 dofs = vox.getDOfs(x, y);
      if (!dofs) continue;

      vuint32 slab[1024];

      // long top and bottom quads
      while (dofs) {
        for (vuint32 cidx = 4; cidx < 6; ++cidx) {
          const vuint8 cmask = VoxelData::cullmask(cidx);
          if ((vox.data[dofs].cull&cmask) == 0) continue;
          const int z = (int)vox.data[dofs].z;
          slab[0] = vox.data[dofs].rgb();
          vox.data[dofs].cull ^= cmask;
          addSlabFace(cmask, DMV_X, x-px, y-py, z-pz, 1, slab);
        }
        dofs = vox.data[dofs].nextz;
      }

      // build long quads for each side
      for (vuint32 cidx = 0; cidx < 4; ++cidx) {
        const vuint8 cmask = VoxelData::cullmask(cidx);
        dofs = vox.getDOfs(x, y);
        while (dofs) {
          while (dofs && (vox.data[dofs].cull&cmask) == 0) dofs = vox.data[dofs].nextz;
          if (!dofs) break;
          const int z = (int)vox.data[dofs].z;
          int count = 0;
          vuint32 eofs = dofs;
          while (eofs && (vox.data[eofs].cull&cmask)) {
            if ((int)vox.data[eofs].z != z+count) break;
            vox.data[eofs].cull ^= cmask;
            slab[count] = vox.data[eofs].rgb();
            eofs = vox.data[eofs].nextz;
            ++count;
            //if (count == (int)slab.length()) break;
          }
          vassert(count);
          dofs = eofs;
          addSlabFace(cmask, DMV_Z, x-px, y-py, z-pz, count, slab);
        }
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::buildOpt2
//
//==========================================================================
void VoxelMesh::buildOpt2 (VoxelData &vox, bool pivotz) {
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = (pivotz ? vox.cz : 0.0f);
  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      // try slabs in all 6 directions?
      vuint32 dofs = vox.getDOfs(x, y);
      if (!dofs) continue;

      vuint32 slab[1024];

      // long top and bottom quads
      while (dofs) {
        for (vuint32 cidx = 4; cidx < 6; ++cidx) {
          const vuint8 cmask = VoxelData::cullmask(cidx);
          if ((vox.data[dofs].cull&cmask) == 0) continue;
          const int z = (int)vox.data[dofs].z;
          vassert(vox.queryCull(x, y, z) == vox.data[dofs].cull);
          // by x
          int xcount = 0;
          while (x+xcount < (int)vox.xsize) {
            const vuint8 vcull = vox.queryCull(x+xcount, y, z);
            if ((vcull&cmask) == 0) break;
            ++xcount;
          }
          // by y
          int ycount = 0;
          while (y+ycount < (int)vox.ysize) {
            const vuint8 vcull = vox.queryCull(x, y+ycount, z);
            if ((vcull&cmask) == 0) break;
            ++ycount;
          }
          vassert(xcount && ycount);
          // now use the longest one
          if (xcount >= ycount) {
            xcount = 0;
            while (x+xcount < (int)vox.xsize) {
              const vuint32 vrgb = vox.query(x+xcount, y, z);
              if (((vrgb>>24)&cmask) == 0) break;
              slab[xcount] = vrgb|0xff000000U;
              vox.setVoxelCull(x+xcount, y, z, (vrgb>>24)^cmask);
              ++xcount;
            }
            vassert(xcount);
            addSlabFace(cmask, DMV_X, x-px, y-py, z-pz, xcount, slab);
          } else {
            ycount = 0;
            while (y+ycount < (int)vox.ysize) {
              const vuint32 vrgb = vox.query(x, y+ycount, z);
              if (((vrgb>>24)&cmask) == 0) break;
              slab[ycount] = vrgb|0xff000000U;
              vox.setVoxelCull(x, y+ycount, z, (vrgb>>24)^cmask);
              ++ycount;
            }
            vassert(ycount);
            addSlabFace(cmask, DMV_Y, x-px, y-py, z-pz, ycount, slab);
          }
        }
        dofs = vox.data[dofs].nextz;
      }

      // build long quads for each side
      for (vuint32 cidx = 0; cidx < 4; ++cidx) {
        const vuint8 cmask = VoxelData::cullmask(cidx);
        dofs = vox.getDOfs(x, y);
        while (dofs) {
          while (dofs && (vox.data[dofs].cull&cmask) == 0) dofs = vox.data[dofs].nextz;
          if (!dofs) break;
          const int z = (int)vox.data[dofs].z;
          int count = 0;
          vuint32 eofs = dofs;
          while (eofs && (vox.data[eofs].cull&cmask)) {
            if ((int)vox.data[eofs].z != z+count) break;
            vox.data[eofs].cull ^= cmask;
            slab[count] = vox.data[eofs].rgb();
            eofs = vox.data[eofs].nextz;
            ++count;
            //if (count == (int)slab.length()) break;
          }
          vassert(count);
          dofs = eofs;
          addSlabFace(cmask, DMV_Z, x-px, y-py, z-pz, count, slab);
        }
      }
    }
  }
}


static inline int getDX (vuint8 dmv) noexcept { return !!(dmv&VoxelMesh::DMV_X); }
static inline int getDY (vuint8 dmv) noexcept { return !!(dmv&VoxelMesh::DMV_Y); }
static inline int getDZ (vuint8 dmv) noexcept { return !!(dmv&VoxelMesh::DMV_Z); }

static inline void incXYZ (vuint8 dmv, int &sx, int &sy, int &sz) noexcept {
  sx += getDX(dmv);
  sy += getDY(dmv);
  sz += getDZ(dmv);
}


//==========================================================================
//
//  VoxelMesh::buildOpt3
//
//==========================================================================
void VoxelMesh::buildOpt3 (VoxelData &vox, bool pivotz) {
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = (pivotz ? vox.cz : 0.0f);

  const vuint8 dmove[3][2] = {
    {DMV_Y, DMV_Z}, // left, right
    {DMV_X, DMV_Z}, // near, far
    {DMV_X, DMV_Y}, // top, bottom
  };

  // try slabs in all 6 directions?
  vuint32 slab[1024];

  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      for (vuint32 dofs = vox.getDOfs(x, y); dofs; dofs = vox.data[dofs].nextz) {
        while (vox.data[dofs].cull) {
          vuint32 count = 0;
          vuint8 clrdmv = 0;
          vuint8 clrmask = 0;
          const int z = (int)vox.data[dofs].z;
          // check all faces
          for (vuint32 cidx = 0; cidx < 6; ++cidx) {
            const vuint8 cmask = VoxelData::cullmask(cidx);
            if ((vox.data[dofs].cull&cmask) == 0) continue;
            // try two dirs
            for (vuint32 ndir = 0; ndir < 2; ++ndir) {
              const vuint8 dmv = dmove[cidx>>1][ndir];
              vuint32 cnt = 1;
              int sx = x, sy = y, sz = z;
              incXYZ(dmv, sx, sy, sz);
              for (;;) {
                const vuint8 vxc = vox.queryCull(sx, sy, sz);
                if ((vxc&cmask) == 0) break;
                ++cnt;
                incXYZ(dmv, sx, sy, sz);
              }
              if (cnt > count) {
                count = cnt;
                clrdmv = dmv;
                clrmask = cmask;
              }
            }
          }
          if (clrmask) {
            vassert(count);
            vassert(clrdmv == DMV_X || clrdmv == DMV_Y || clrdmv == DMV_Z);
            int sx = x, sy = y, sz = z;
            for (vuint32 f = 0; f < count; ++f) {
              VoxPix *vp = vox.queryVP(sx, sy, sz);
              slab[f] = vp->rgb();
              vassert(vp->cull&clrmask);
              vp->cull ^= clrmask;
              incXYZ(clrdmv, sx, sy, sz);
            }
            addSlabFace(clrmask, clrdmv, x-px, y-py, z-pz, count, slab);
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::buildFrom
//
//==========================================================================
void VoxelMesh::buildFrom (VoxelData &vox, bool pivotz) {
  vassert(vox.xsize && vox.ysize && vox.zsize);
  // now build cubes
       if (kvx_optimized.asInt() <= 0) buildOpt0(vox, pivotz);
  else if (kvx_optimized.asInt() <= 1) buildOpt1(vox, pivotz);
  else if (kvx_optimized.asInt() <= 2) buildOpt2(vox, pivotz);
  else /*if (kvx_optimized.asInt() <= 3)*/ buildOpt3(vox, pivotz);
  vassert(maxColorRun <= 1024);
  cx = vox.cx;
  cy = vox.cy;
  cz = vox.cz;
}


// ////////////////////////////////////////////////////////////////////////// //
struct GLVoxMeshColorItem {
  vuint32 imgofs;
  vuint32 imgx; // to avoid calculation, why not
  vuint32 imgy; // to avoid calculation, why not
  int next; // -1: no more
};


struct GLVoxelMesh {
  TArray<VVoxVertexEx> vertices;
  TArray<vuint32> indicies;
  vuint32 uniqueVerts;

  TMapNC<VVoxVertexEx, vuint32> vertcache;

  // color texture size
  vuint32 imgWidth, imgHeight;
  // texture
  TArray<vuint32> img;
  // used pixels in each row
  TArray<vuint32> rowused;

  TArray<GLVoxMeshColorItem> citems;
  TMapNC<int, int> citemhash; // key: color index; value: index in `citems`
  vuint32 uniqueRuns;

private:
  void hashColorRun (vuint32 imgofs, vuint32 clen);
  int findColorRun (const vuint32 *run, vuint32 clen);
  vuint32 appendRun (const vuint32 *run, vuint32 clen);
  vuint32 appendVertex (const VVoxVertexEx &gv);

public:
  inline GLVoxelMesh () noexcept
    : vertices()
    , indicies()
    , uniqueVerts(0)
    , vertcache()
    , imgWidth(0)
    , imgHeight(0)
    , img()
    , rowused()
    , citems()
    , citemhash()
    , uniqueRuns(0)
  {}

  inline ~GLVoxelMesh () { clear(); }

  void clear ();
  void create (VoxelMesh &vox);
};


//==========================================================================
//
//  GLVoxelMesh::hashColorRun
//
//==========================================================================
void GLVoxelMesh::hashColorRun (vuint32 imgofs, vuint32 clen) {
  vuint32 cidx = (vuint32)citems.length();
  citems.setLengthReserve((int)cidx+(int)clen);
  while (clen--) {
    citems[cidx].imgofs = imgofs;
    citems[cidx].imgx = imgofs%imgWidth;
    citems[cidx].imgy = imgofs/imgWidth;
    const vuint32 clr = img[imgofs++];
    auto prp = citemhash.get(clr);
    citems[cidx].next = (prp ? *prp : -1);
    citemhash.put(clr, cidx);
    ++cidx;
  }
}


//==========================================================================
//
//  GLVoxelMesh::findColorRun
//
//==========================================================================
int GLVoxelMesh::findColorRun (const vuint32 *run, vuint32 clen) {
  if (clen == 0) return -1;
  auto prp = citemhash.get(run[0]);
  if (!prp) return -1;
  for (int pidx = *prp; pidx >= 0; pidx = citems[pidx].next) {
    const GLVoxMeshColorItem &it = citems[pidx];
    const vuint32 maxlen = rowused[it.imgy]-it.imgx;
    if (maxlen < clen) continue;
    if (memcmp(img.ptr()+it.imgofs, run, clen*sizeof(vuint32)) == 0) return it.imgofs;
  }
  return -1;
}


//==========================================================================
//
//  GLVoxelMesh::appendRun
//
//  returns index in `runs`
//
//==========================================================================
vuint32 GLVoxelMesh::appendRun (const vuint32 *run, vuint32 clen) {
  vassert(clen);

  const int ridx = findColorRun(run, clen);
  if (ridx != -1) return (vuint32)ridx; // i found her!

  // need to append a new run
  int ry = -1;

  // find free space in some row
  for (int cy = 0; cy < rowused.length(); ++cy) {
    if (imgWidth-rowused[cy] >= clen) {
      ry = cy;
      break;
    }
  }
  // no free space?
  if (ry < 0) {
    // append new row
    vassert(imgHeight < 2048);
    imgHeight <<= 1;
    const int oisz = img.length();
    img.setLength(imgWidth*imgHeight);
    memset(img.ptr()+oisz, 0, (imgWidth*imgHeight-oisz)*sizeof(vuint32));
    const int orsz = rowused.length();
    rowused.setLength(imgHeight);
    memset(rowused.ptr()+orsz, 0, (imgHeight-orsz)*sizeof(vuint32));
    ry = orsz;
  }

  ++uniqueRuns;
  const vuint32 imgofs = (vuint32)ry*imgWidth+rowused[ry];
  memcpy(img.ptr()+imgofs, run, clen*sizeof(vuint32));
  rowused[ry] += clen;
  hashColorRun(imgofs, clen);
  return imgofs;
}


//==========================================================================
//
//  GLVoxelMesh::appendVertex
//
//==========================================================================
vuint32 GLVoxelMesh::appendVertex (const VVoxVertexEx &gv) {
  auto vp = vertcache.get(gv);
  if (vp) return *vp;
  ++uniqueVerts;
  const vuint32 res = (vuint32)vertices.length();
  vertices.append(gv);
  vertcache.put(gv, res);
  return res;
}


//==========================================================================
//
//  GLVoxelMesh::clear
//
//==========================================================================
void GLVoxelMesh::clear () {
  vertices.clear();
  indicies.clear();
  vertcache.clear();
  uniqueVerts = 0;
  imgWidth = imgHeight = 0;
  img.clear();
  rowused.clear();
  citems.clear();
  citemhash.clear();
  uniqueRuns = 0;
}


//==========================================================================
//
//  GLVoxelMesh::create
//
//==========================================================================
void GLVoxelMesh::create (VoxelMesh &vox) {
  vassert(vox.maxColorRun > 0);
  vassert(vox.maxColorRun <= 1024);
  clear();
  imgWidth = imgHeight = 1;
  if (vox.maxColorRun <= 256) {
    imgWidth = 256;
  } else {
    while (imgWidth < vox.maxColorRun) imgWidth <<= 1;
  }
  img.setLength(imgWidth);
  memset(img.ptr(), 0, imgWidth*sizeof(vuint32));
  rowused.setLength(1);
  rowused[0] = 0;

  // register runs, fill color texture, calculate quad normals
  for (int f = 0; f < vox.quads.length(); ++f) {
    VoxelMesh::VoxQuad &vq = vox.quads[f];
    vq.calcNormal();
    vq.imgidx = appendRun(vox.colors.ptr()+vq.cidx, vq.clen);
  }

  // create arrays
  for (int f = 0; f < vox.quads.length(); ++f) {
    VoxelMesh::VoxQuad &vq = vox.quads[f];
    const vuint32 imgx0 = vq.imgidx%imgWidth;
    const vuint32 imgx1 = imgx0+vq.clen-1;
    const vuint32 imgy = vq.imgidx/imgWidth;
    vuint32 vxn[4];

    VVoxVertexEx gv;
    const float u = ((float)imgx0+0.5f)/(float)imgWidth;
    const float v = ((float)imgy+0.5f)/(float)imgHeight;
    const float u0 = ((float)imgx0+0.0f)/(float)imgWidth;
    const float u1 = ((float)imgx1+1.0f)/(float)imgWidth;
    for (vuint32 nidx = 0; nidx < 4; ++nidx) {
      gv.x = vq.vx[nidx].x;
      gv.y = vq.vx[nidx].y;
      gv.z = vq.vx[nidx].z;
      if (vq.type == VoxelMesh::ZLong) {
        gv.s = (vq.vx[nidx].dz ? u1 : u0);
        gv.t = v;
      } else if (vq.type == VoxelMesh::XLong) {
        gv.s = (vq.vx[nidx].dx ? u1 : u0);
        gv.t = v;
      } else if (vq.type == VoxelMesh::YLong) {
        gv.s = (vq.vx[nidx].dy ? u1 : u0);
        gv.t = v;
      } else {
        vassert(vq.type == VoxelMesh::Point);
        gv.s = u;
        gv.t = v;
      }
      gv.nx = vq.normal.x;
      gv.ny = vq.normal.y;
      gv.nz = vq.normal.z;
      vxn[nidx] = appendVertex(gv);
    }

    indicies.append(vxn[0]);
    indicies.append(vxn[1]);
    indicies.append(vxn[2]);
    indicies.append(vxn[3]);
    //indicies.append(65535);
  }

  // fix image colors (sorry)
  vuint8 *ip = (vuint8 *)img.ptr();
  for (unsigned f = imgWidth*imgHeight; f--; ip += 4) {
    vuint8 b0 = ip[0];
    //vuint8 b1 = ip[1];
    vuint8 b2 = ip[2];
    ip[0] = b2;
    ip[2] = b0;
  }
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
        if (ok && optlevel != kvx_optimized.asInt()) ok = false;
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
    vox.optimise(kvx_optimized.asInt() > 3);

    VoxelMesh vmesh;
    vmesh.buildFrom(vox, useVoxelPivotZ);
    vox.clear();

    glvmesh.create(vmesh);
    vmesh.clear();

    if (vox_verbose_conversion.asBool()) {
      GCon->Logf(NAME_Init, "voxel model '%s': %d unique vertices, %d triangles, %dx%d color texture; optlevel=%d",
                 *this->Name, glvmesh.vertices.length(), glvmesh.indicies.length()/4*2,
                 (int)glvmesh.imgWidth, (int)glvmesh.imgHeight, kvx_optimized.asInt());

    }
  }
  if (glvmesh.indicies.length() == 0) Sys_Error("cannot load empty voxel model '%s'", *this->Name);
  if (glvmesh.indicies.length()%4 != 0) Sys_Error("internal error converting voxel model '%s'", *this->Name);

  if (glvmesh.vertices.length() > 65534) {
    GCon->Logf(NAME_Init, "3d model for voxel '%s' has too many vertices", *this->Name);
  }

  for (int f = 0; f < glvmesh.indicies.length(); ++f) {
    const int vidx = glvmesh.indicies[f];
    if (vidx < 0 || vidx >= glvmesh.vertices.length()) {
      Sys_Error("internal error loading voxel model '%s' (vertex index %d is out of range)", *this->Name, f);
    }
  }

  //vox.createColorTexture(this->Name);

  // skins
  {
    if (voxTextures.length() == 0) GTextureManager.RegisterTextureLoader(&voxTextureLoader);
    VStr fname = va("\x01:voxtexure:%d", voxTextures.length());
    VoxelTexture &vxt = voxTextures.alloc();
    vxt.fname = fname;
    vxt.width = glvmesh.imgWidth;
    vxt.height = glvmesh.imgHeight;
    vxt.data = (vuint8 *)Z_Malloc(glvmesh.imgWidth*glvmesh.imgHeight*4);
    memcpy(vxt.data, glvmesh.img.ptr(), glvmesh.imgWidth*glvmesh.imgHeight*4);
    Skins.setLength(1);
    VMeshModel::SkinInfo &si = Skins[0];
    si.fileName = VName(*fname);
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

  // quads -> triangles
  // also create triangle strips
  Tris.setLength(glvmesh.indicies.length()/4*2);
  GlMode = GlTriangleFan;
  TriVerts.setLength(glvmesh.indicies.length()/4*5);
  int triidx = 0;
  int tvxidx = 0;
  for (int qidx = 0; qidx < glvmesh.indicies.length(); qidx += 4) {
    const int vx0 = glvmesh.indicies[qidx+0];
    const int vx1 = glvmesh.indicies[qidx+1];
    const int vx2 = glvmesh.indicies[qidx+2];
    const int vx3 = glvmesh.indicies[qidx+3];

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
      vint32 optlevel = kvx_optimized.asInt();
      *strm << optlevel;
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
