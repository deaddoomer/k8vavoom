//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2022 Ketmar Dark
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
#include "voxelib.h"

//#define VOXELIB_CHECK_INVARIANTS

bool voxlib_verbose = false;


// ////////////////////////////////////////////////////////////////////////// //
// Vox2DBitmap
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  Vox2DBitmap::doOne
//
//==========================================================================
bool Vox2DBitmap::doOne (int *rx0, int *ry0, int *rx1, int *ry1) noexcept {
  if (dotCount == 0) return false;

  if (cache.length() < wdt+1) cache.setLength(wdt+1);
  if (stack.length() < wdt+1) stack.setLength(wdt+1);

  Pair best_ll;
  Pair best_ur;
  int best_area;

  best_ll.one = best_ll.two = 0;
  best_ur.one = best_ur.two = -1;
  best_area = 0;
  top = 0;
  bool cacheCleared = true;

  for (int m = 0; m <= wdt; ++m) cache[m] = stack[m].one = stack[m].two = 0;

  // main algorithm
  for (int n = 0; n < hgt; ++n) {
    // there is no need to scan empty lines
    // (and we usually have quite a lot of them)
    if (ydotCount/*.ptr()*/[n] == 0) {
      if (!cacheCleared) {
        cacheCleared = true;
        memset(cache.ptr(), 0, wdt*sizeof(int));
      }
      continue;
    }
    int openWidth = 0;
    updateCache(n);
    cacheCleared = false;
    const int *cp = cache.ptr();
    for (int m = 0; m <= wdt; ++m) {
      const int cvl = *cp++;
      if (cvl > openWidth) {
        // open new rectangle
        push(m, openWidth);
        openWidth = cvl;
      } else if (cvl < openWidth) {
        // close rectangle(s)
        int m0, w0;
        do {
          pop(&m0, &w0);
          const int area = openWidth*(m-m0);
          if (area > best_area) {
            best_area = area;
            best_ll.one = m0; best_ll.two = n;
            best_ur.one = m-1; best_ur.two = n-openWidth+1;
          }
          openWidth = w0;
        } while (cvl < openWidth);
        openWidth = cvl;
        if (openWidth != 0) push(m0, w0);
      }
    }
  }

  *rx0 = (best_ll.one < best_ur.one ? best_ll.one : best_ur.one);
  *ry0 = (best_ll.two < best_ur.two ? best_ll.two : best_ur.two);
  *rx1 = (best_ll.one > best_ur.one ? best_ll.one : best_ur.one);
  *ry1 = (best_ll.two > best_ur.two ? best_ll.two : best_ur.two);

  // remove dots
  /*
  for (int y = *ry0; y <= *ry1; ++y) {
    for (int x = *rx0; x <= *rx1; ++x) {
      resetPixel(x, y);
    }
  }
  */

  return true;
};


// ////////////////////////////////////////////////////////////////////////// //
// VoxTexAtlas
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VoxTexAtlas::findBestFit
//
//  node id or BadRect
//
//==========================================================================
vuint32 VoxTexAtlas::findBestFit (int w, int h) noexcept {
  vuint32 fitW = BadRect, fitH = BadRect, biggest = BadRect;

  const Rect *r = rects.ptr();
  const int rlen = rects.length();
  for (int idx = 0; idx < rlen; ++idx, ++r) {
    if (r->w < w || r->h < h) continue; // absolutely can't fit
    if (r->w == w && r->h == h) return (vuint32)idx; // perfect fit
    if (r->w == w) {
      // width fit
      if (fitW == BadRect || rects/*.ptr()*/[fitW].h < r->h) fitW = (vuint32)idx;
    } else if (r->h == h) {
      // height fit
      if (fitH == BadRect || rects/*.ptr()*/[fitH].w < r->w) fitH = (vuint32)idx;
    } else {
      // get biggest rect
      if (biggest == BadRect || rects/*.ptr()*/[biggest].getArea() > r->getArea()) {
        biggest = (vuint32)idx;
      }
    }
  }

  // both?
  if (fitW != BadRect && fitH != BadRect) {
    return (rects/*.ptr()*/[fitW].getArea() > rects/*.ptr()*/[fitH].getArea() ? fitW : fitH);
  }
  if (fitW != BadRect) return fitW;
  if (fitH != BadRect) return fitH;
  return biggest;
}


//==========================================================================
//
//  VoxTexAtlas::insert
//
//  returns invalid rect if there's no room
//
//==========================================================================
VoxTexAtlas::Rect VoxTexAtlas::insert (int cwdt, int chgt) noexcept {
  vassert(cwdt > 0 && chgt > 0);
  if (cwdt > imgWidth || chgt > imgHeight) return Rect::Invalid();
  vuint32 ri = findBestFit(cwdt, chgt);
  if (ri == BadRect) return Rect::Invalid();
  Rect rc = rects[ri];
  auto res = Rect(rc.x, rc.y, cwdt, chgt);
  // split this rect
  if (rc.w == res.w && rc.h == res.h) {
    // best fit, simply remove this rect
    rects.removeAt((int)ri);
  } else {
    if (rc.w == res.w) {
      // split vertically
      rc.y += res.h;
      rc.h -= res.h;
    } else if (rc.h == res.h) {
      // split horizontally
      rc.x += res.w;
      rc.w -= res.w;
    } else {
      Rect nr = rc;
      // split in both directions (by longer edge)
      if (rc.w-res.w > rc.h-res.h) {
        // cut the right part
        nr.x += res.w;
        nr.w -= res.w;
        // cut the bottom part
        rc.y += res.h;
        rc.h -= res.h;
        rc.w = res.w;
      } else {
        // cut the bottom part
        nr.y += res.h;
        nr.h -= res.h;
        // cut the right part
        rc.x += res.w;
        rc.w -= res.w;
        rc.h = res.h;
      }
      rects.append(nr);
    }
    rects/*.ptr()*/[ri] = rc;
  }
  return res;
}


// ////////////////////////////////////////////////////////////////////////// //
// VoxColorPack
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VoxColorPack::growImage
//
//  grow image, and relayout everything
//
//==========================================================================
void VoxColorPack::growImage (vuint32 inswdt, vuint32 inshgt) {
  vuint32 neww = clrwdt, newh = clrhgt;
  while (neww < inswdt) neww <<= 1;
  while (newh < inshgt) newh <<= 1;
  for (;;) {
    if (neww < newh) neww <<= 1; else newh <<= 1;
    // relayout data
    bool again = false;
    atlas.setSize(neww, newh);
    for (int f = 0; f < citems.length(); ++f) {
      ColorItem &ci = citems[f];
      auto rc = atlas.insert((int)ci.wh.getW(), (int)ci.wh.getH());
      if (!rc.isValid()) {
        // alas, no room
        again = true;
        break;
      }
      // record new coords
      ci.newxy = VoxXY16(rc.x, rc.y);
    }
    if (!again) break; // done
  }

  // allocate new image, copy old data
  //GLog.Logf(NAME_Debug, "ATLAS: resized from %ux%u to %ux%u", clrwdt, clrhgt, neww, newh);
  TArray<vuint32> newclr;
  newclr.setLength(neww*newh);
  memset(newclr.ptr(), 0, neww*newh*sizeof(vuint32));
  for (int f = 0; f < citems.length(); ++f) {
    ColorItem &ci = citems[f];
    const vuint32 rcw = ci.wh.getW();
    vuint32 oaddr = ci.xy.getY()*clrwdt+ci.xy.getX();
    vuint32 naddr = ci.newxy.getY()*neww+ci.newxy.getX();
    vuint32 dy = ci.wh.getH();
    /*
    conwriteln(": : : oldpos=(", ci.rc.getX(), ",", ci.rc.getY(), "); newpos=(", newx, ",",
               newy, "); size=(", rcw, "x", ci.rc.getH(), "); oaddr=", oaddr, "; naddr=", naddr);
    */
    while (dy--) {
      memcpy(newclr.ptr()+naddr, colors.ptr()+oaddr, rcw*sizeof(vuint32));
      oaddr += clrwdt;
      naddr += neww;
    }
    ci.xy = ci.newxy;
  }
  /*
  colors.setLength(0);
  colors = newclr;
  newclr.clear();
  */
  colors.transferDataFrom(newclr);
  vassert(newclr.length() == 0);
  clrwdt = neww;
  clrhgt = newh;
  vassert((vuint32)colors.length() == clrwdt*clrhgt);
}


//==========================================================================
//
//  VoxColorPack::findRectEx
//
//  returns true if found, and sets `*cidxp` and `*xyofsp`
//  `*xyofsp` is offset inside `cidxp`
//
//==========================================================================
bool VoxColorPack::findRectEx (const vuint32 *clrs, vuint32 cwdt, vuint32 chgt,
                               vuint32 cxofs, vuint32 cyofs,
                               vuint32 wdt, vuint32 hgt, vuint32 *cidxp, VoxWH16 *whp)
{
  vassert(wdt > 0 && hgt > 0);
  vassert(cwdt >= wdt && chgt >= hgt);

  const vuint32 saddrOrig = cyofs*cwdt+cxofs;
  auto cp = citemhash.get(clrs[saddrOrig]);
  if (!cp) return false;

  for (int cidx = *cp; cidx >= 0; cidx = citems[cidx].next) {
    const ColorItem &ci = citems[cidx];
    if (wdt > ci.wh.getW() || hgt > ci.wh.getH()) continue; // impossibiru
    // compare colors
    bool ok = true;
    vuint32 saddr = saddrOrig;
    vuint32 caddr = ci.xy.getY()*clrwdt+ci.xy.getX();
    for (vuint32 dy = 0; dy < hgt; ++dy) {
      if (memcmp(colors.ptr()+caddr, clrs+saddr, wdt*sizeof(vuint32)) != 0) {
        ok = false;
        break;
      }
      saddr += cwdt;
      caddr += clrwdt;
    }
    if (ok) {
      // i found her!
      // topmost
      if (cidxp) *cidxp = (vuint32)cidx;
      if (whp) *whp = VoxWH16(wdt, hgt);
      return true;
    }
  }

  return false;
}


//==========================================================================
//
//  VoxColorPack::addNewRect
//
//  returns index in `citems`
//
//==========================================================================
vuint32 VoxColorPack::addNewRect (const vuint32 *clrs, vuint32 wdt, vuint32 hgt) {
  vassert(wdt > 0 && hgt > 0);
  VoxXY16 coord;

  if (clrwdt == 0) {
    // no rects yet
    vassert(clrhgt == 0);
    clrwdt = 1;
    while (clrwdt < wdt) clrwdt <<= 1;
    clrhgt = 1;
    while (clrhgt < hgt) clrhgt <<= 1;
    if (clrhgt < clrwdt) clrhgt = clrwdt; //!!
    atlas.setSize(clrwdt, clrhgt);
    coord = VoxXY16(0, 0);
    colors.setLength(clrwdt*clrhgt);
    memset(colors.ptr(), 0, clrwdt*clrhgt*sizeof(vuint32));
  } else {
    // insert into atlas; grow texture if cannot insert
    for (;;) {
      auto rc = atlas.insert((int)wdt, (int)hgt);
      if (rc.isValid()) {
        coord = VoxXY16(rc.x, rc.y);
        /*
        GLog.Logf(NAME_Debug, "inserted: (%d,%d) (%u,%u) : atlas:(%ux%u) (%dx%d)",
                  rc.x, rc.y, coord.getX(), coord.getY(),
                  getWidth(), getHeight(),
                  atlas.getWidth(), atlas.getHeight());
        */
        break;
      }
      // no room, grow the texture, and relayout everything
      growImage(wdt, hgt);
    }
  }

  // copy source colors into the atlas image
  vuint32 saddr = 0;
  vuint32 daddr = coord.getY()*clrwdt+coord.getX();
  for (vuint32 dy = 0; dy < hgt; ++dy) {
    memcpy(colors.ptr()+daddr, clrs+saddr, wdt*sizeof(vuint32));
    saddr += wdt;
    daddr += clrwdt;
  }

  // hash main rect
  ColorItem ci;
  ci.xy = coord;
  ci.wh = VoxWH16(wdt, hgt);
  const int parentIdx = citems.length();
  vuint32 cc = clrs[0];
  auto cpp = citemhash.get(cc);
  if (cpp) {
    ci.next = *cpp;
    *cpp = parentIdx;
  } else {
    ci.next = -1;
    citemhash.put(cc, parentIdx);
  }
  citems.append(ci);

  return (vuint32)parentIdx;
}


// ////////////////////////////////////////////////////////////////////////// //
struct __attribute__((packed)) VoxXYZ16 {
  vuint16 x, y, z;

  VVA_FORCEINLINE VoxXYZ16 () noexcept {}
  VVA_FORCEINLINE VoxXYZ16 (vuint16 ax, vuint16 ay, vuint16 az) noexcept : x(ax), y(ay), z(az) {}
};


// ////////////////////////////////////////////////////////////////////////// //
// VoxelData
// ////////////////////////////////////////////////////////////////////////// //

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
    if (data.length() >= 0x3fffffff) Sys_Error("too many voxels");
    const vuint32 lastel = (vuint32)data.length();
    data.setLength((int)lastel+1024);
    freelist = (vuint32)data.length()-1;
    while (freelist >= lastel) {
      data/*.ptr()*/[freelist].nextz = freelist+1;
      --freelist;
    }
    freelist = lastel;
    data/*.ptr()*/[data.length()-1].nextz = 0;
  }
  const vuint32 res = freelist;
  freelist = data/*.ptr()*/[res].nextz;
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
  data.setLength(1); // data[0] is never used
}


//==========================================================================
//
//  VoxelData::removeVoxel
//
//==========================================================================
void VoxelData::removeVoxel (int x, int y, int z) {
  vuint32 dofs = getDOfs(x, y);
  vuint32 prevdofs = 0;
  while (dofs) {
    if (data/*.ptr()*/[dofs].z == (vuint16)z) {
      // remove this voxel
      if (prevdofs) {
        data/*.ptr()*/[prevdofs].nextz = data/*.ptr()*/[dofs].nextz;
      } else {
        xyofs[(vuint32)y*xsize+(vuint32)x] = data/*.ptr()*/[dofs].nextz;
      }
      data/*.ptr()*/[dofs].nextz = freelist;
      freelist = dofs;
      --voxpixtotal;
      return;
    }
    if (data/*.ptr()*/[dofs].z > (vuint16)z) return;
    prevdofs = dofs;
    dofs = data/*.ptr()*/[dofs].nextz;
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
  vuint32 dofs = getDOfs(x, y);
  vuint32 prevdofs = 0;
  while (dofs) {
    if (data/*.ptr()*/[dofs].z == (vuint16)z) {
      // replace this voxel
      data/*.ptr()*/[dofs].b = rgb&0xff;
      data/*.ptr()*/[dofs].g = (rgb>>8)&0xff;
      data/*.ptr()*/[dofs].r = (rgb>>16)&0xff;
      data/*.ptr()*/[dofs].cull = cull;
      return;
    }
    if (data/*.ptr()*/[dofs].z > (vuint16)z) break;
    prevdofs = dofs;
    dofs = data/*.ptr()*/[dofs].nextz;
  }
  // insert before dofs
  const vuint32 vidx = allocVox();
  data/*.ptr()*/[vidx].b = rgb&0xff;
  data/*.ptr()*/[vidx].g = (rgb>>8)&0xff;
  data/*.ptr()*/[vidx].r = (rgb>>16)&0xff;
  data/*.ptr()*/[vidx].cull = cull;
  data/*.ptr()*/[vidx].z = (vuint16)z;
  data/*.ptr()*/[vidx].nextz = dofs;
  if (prevdofs) {
    vassert(data/*.ptr()*/[prevdofs].nextz == dofs);
    data/*.ptr()*/[prevdofs].nextz = vidx;
  } else {
    xyofs[(vuint32)y*xsize+(vuint32)x] = vidx;
  }
}


//==========================================================================
//
//  VoxelData::checkInvariants
//
//==========================================================================
void VoxelData::checkInvariants () {
  vuint32 voxcount = 0;
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      vuint32 dofs = getDOfs(x, y);
      if (!dofs) continue;
      ++voxcount;
      vuint16 prevz = data/*.ptr()*/[dofs].z;
      dofs = data/*.ptr()*/[dofs].nextz;
      while (dofs) {
        ++voxcount;
        vassert(prevz < data/*.ptr()*/[dofs].z); //, "broken voxel data Z invariant");
        prevz = data/*.ptr()*/[dofs].z;
        dofs = data/*.ptr()*/[dofs].nextz;
      }
    }
  }
  vassert(voxcount == voxpixtotal);//, "invalid number of voxels");
}


//==========================================================================
//
//  VoxelData::removeEmptyVoxels
//
//==========================================================================
void VoxelData::removeEmptyVoxels () {
  vuint32 count = 0;
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      vuint32 dofs = getDOfs(x, y);
      if (!dofs) continue;
      vuint32 prevdofs = 0;
      while (dofs) {
        if (!data/*.ptr()*/[dofs].cull) {
          // remove it
          const vuint32 ndofs = data/*.ptr()*/[dofs].nextz;
          if (prevdofs) {
            data/*.ptr()*/[prevdofs].nextz = ndofs;
          } else {
            xyofs[(vuint32)y*xsize+(vuint32)x] = ndofs;
          }
          data/*.ptr()*/[dofs].nextz = freelist;
          freelist = dofs;
          --voxpixtotal;
          dofs = ndofs;
          ++count;
        } else {
          prevdofs = dofs;
          dofs = data/*.ptr()*/[dofs].nextz;
        }
      }
    }
  }
  if (count && voxlib_verbose) GLog.Logf(NAME_Init, "  removed %u empty voxel%s", count, (count != 1 ? "s" : ""));
}


//==========================================================================
//
//  VoxelData::removeInsideFaces
//
//  remove inside voxels, leaving only contour
//
//==========================================================================
void VoxelData::removeInsideFaces () {
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      for (vuint32 dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        if (!data/*.ptr()*/[dofs].cull) continue;
        // check
        const int z = (int)data/*.ptr()*/[dofs].z;
        for (vuint32 cidx = 0; cidx < 6; ++cidx) {
          // go in this dir, removing the corresponding voxel side
          const vuint8 cmask = cullmask(cidx);
          const vuint8 opmask = cullopmask(cidx);
          const vuint8 checkmask = cmask|opmask;
          const int dx = cullofs[cidx][0];
          const int dy = cullofs[cidx][1];
          const int dz = cullofs[cidx][2];
          int vx = x, vy = y, vz = z;
          vuint32 myofs = dofs;
          while (myofs && (data/*.ptr()*/[myofs].cull&cmask)) {
            const int sx = vx+dx;
            const int sy = vy+dy;
            const int sz = vz+dz;
            const vuint32 sofs = voxofs(sx, sy, sz);
            if (!sofs) break;
            if (!(data/*.ptr()*/[sofs].cull&checkmask)) break;
            // fix culls
            data/*.ptr()*/[myofs].cull ^= cmask;
            data/*.ptr()*/[sofs].cull &= (vuint8)(~(vuint32)opmask);
            vx = sx;
            vy = sy;
            vz = sz;
            myofs = sofs;
          }
        }
      }
    }
  }
}


//==========================================================================
//
//  VoxelData::fixFaceVisibility
//
//  if we have ANY voxel at the corresponding side, don't render that face
//  returns number of fixed voxels
//
//==========================================================================
vuint32 VoxelData::fixFaceVisibility () {
  vuint32 count = 0;
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      for (vuint32 dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        const vuint8 ocull = data/*.ptr()*/[dofs].cull;
        if (!ocull) continue;
        const int z = (int)data/*.ptr()*/[dofs].z;
        // if we have ANY voxel at the corresponding side, don't render that face
        for (vuint32 cidx = 0; cidx < 6; ++cidx) {
          const vuint8 cmask = cullmask(cidx);
          if (data/*.ptr()*/[dofs].cull&cmask) {
            if (queryCull(x+cullofs[cidx][0], y+cullofs[cidx][1], z+cullofs[cidx][2])) {
              data/*.ptr()*/[dofs].cull ^= cmask; // reset bit
            }
          }
        }
        count += (data/*.ptr()*/[dofs].cull != ocull);
      }
    }
  }
  return count;
}


//==========================================================================
//
//  VoxelData::create3DBitmap
//
//==========================================================================
void VoxelData::create3DBitmap (Vox3DBitmap &bmp) {
  bmp.setSize(xsize, ysize, zsize);
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      for (vuint32 dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        if (data/*.ptr()*/[dofs].cull) bmp.setPixel(x, y, (int)data/*.ptr()*/[dofs].z);
      }
    }
  }
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
  Vox3DBitmap bmp;
  bmp.setSize(xsize+2, ysize+2, zsize+2);

  TArray<VoxXYZ16> stack;
  vuint32 stackpos;

  stack.setLength(32768);
  stackpos = 0;
  vassert(xsize <= (vuint32)stack.length());

  // this is definitely empty
  VoxXYZ16 xyz;
  xyz.x = xyz.y = xyz.z = 0;
  bmp.setPixel((int)xyz.x, (int)xyz.y, (int)xyz.z);
  stack/*.ptr()*/[stackpos++] = xyz;

  while (stackpos) {
    xyz = stack/*.ptr()*/[--stackpos];
    for (vuint32 dd = 0; dd < 6; ++dd) {
      const int nx = (int)xyz.x+cullofs[dd][0];
      const int ny = (int)xyz.y+cullofs[dd][1];
      const int nz = (int)xyz.z+cullofs[dd][2];
      if (bmp.setPixel(nx, ny, nz)) continue;
      if (queryCull(nx-1, ny-1, nz-1)) continue;
      if (stackpos == (vuint32)stack.length()) {
        stack.setLength(stack.length()+32768);
      }
      stack/*.ptr()*/[stackpos++] = VoxXYZ16((vuint16)nx, (vuint16)ny, (vuint16)nz);
    }
  }
  //GCon->Logf(NAME_Debug, "*** hollow fill used %d stack items", stack.length());

  // unmark contour voxels
  // this is required for proper face removing
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      for (vuint32 dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        if (!data/*.ptr()*/[dofs].cull) continue;
        const int z = (int)data/*.ptr()*/[dofs].z;
        bmp.resetPixel(x+1, y+1, z+1);
      }
    }
  }

  // now check it
  vuint32 changed = 0;
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      for (vuint32 dofs = getDOfs(x, y); dofs; dofs = data/*.ptr()*/[dofs].nextz) {
        const vuint8 omask = data/*.ptr()*/[dofs].cull;
        if (!omask) continue;
        data/*.ptr()*/[dofs].cull = 0x3f;
        // check
        const int z = (int)data/*.ptr()*/[dofs].z;
        for (vuint32 cidx = 0; cidx < 6; ++cidx) {
          const vuint8 cmask = cullmask(cidx);
          if (!(data/*.ptr()*/[dofs].cull&cmask)) continue;
          const int nx = x+cullofs[cidx][0];
          const int ny = y+cullofs[cidx][1];
          const int nz = z+cullofs[cidx][2];
          if (bmp.getPixel(nx+1, ny+1, nz+1)) continue;
          // reset this cull bit
          data/*.ptr()*/[dofs].cull ^= cmask;
        }
        changed += (omask != data/*.ptr()*/[dofs].cull);
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
void VoxelData::optimise (bool doHollowFill) {
  #ifdef VOXELIB_CHECK_INVARIANTS
  checkInvariants();
  #endif
  //version(voxdata_debug) conwriteln("optimising mesh with ", voxpixtotal, " individual voxels...");
  if (doHollowFill) {
    //version(voxdata_debug) conwriteln("optimising voxel culling...");
    vuint32 count = hollowFill();
    if (count && voxlib_verbose) GLog.Logf(NAME_Init, "  hollow fill fixed %u voxel%s", count, (count != 1 ? "s" : ""));
    count = fixFaceVisibility();
    if (count && voxlib_verbose) GLog.Logf(NAME_Init, "  final fix fixed %u voxel%s", count, (count != 1 ? "s" : ""));
  } else {
    removeInsideFaces();
    //version(voxdata_debug) conwriteln("optimising voxel culling...");
    vuint32 count = fixFaceVisibility();
    if (count && voxlib_verbose) GLog.Logf(NAME_Init, "  fixed %u voxel%s", count, (count != 1 ? "s" : ""));
  }
  removeEmptyVoxels();
  #ifdef VOXELIB_CHECK_INVARIANTS
  checkInvariants();
  #endif
  if (voxlib_verbose) GLog.Logf(NAME_Init, "  final optimised mesh contains %u individual voxels", voxpixtotal);
}


// ////////////////////////////////////////////////////////////////////////// //
/*
  voxel data optimised for queries
  (about two times faster than `VoxelData`, but immutable)

  each slab is kept in this format:
    ushort zlo, zhi
    ushort runcount
  then run index follows:
    ushort z0
    ushort z1+1
    ushort ofs (from this z)
    ushort reserved
  index always ends with zhi+1, zhi+1
  then (b,g,r,cull) array
 */
struct VoxelDataSmall {
public:
  vuint32 xsize = 0, ysize = 0, zsize = 0;
  float cx = 0.0f, cy = 0.0f, cz = 0.0f;

  TArray<vuint8> data;
  // xsize*ysize array, offsets in `data`; 0 means "no data here"
  // slabs are sorted from bottom to top, and never intersects
  TArray<vuint32> xyofs;

public:
  VoxelDataSmall () noexcept
    : xsize(0)
    , ysize(0)
    , zsize(0)
    , cx(0.0f)
    , cy(0.0f)
    , cz(0.0f)
  {}

private:
  VVA_FORCEINLINE void appendByte (vuint8 v) noexcept {
    data.append(v);
  }

  VVA_FORCEINLINE void appendShort (vuint16 v) noexcept {
    data.append((vuint8)v);
    data.append((vuint8)(v>>8));
  }

private:
  vuint32 createSlab (VoxelData &vox, vuint32 dofs0);

  void checkValidity (VoxelData &vox);

public:
  void clear () {
    data.clear();
    xyofs.clear();
    xsize = ysize = zsize = 0;
    cx = cy = cz = 0.0f;
  }

  void createFrom (VoxelData &vox);

  vuint32 queryVox (int x, int y, int z) noexcept;
};


//==========================================================================
//
//  VoxelDataSmall::checkValidity
//
//==========================================================================
void VoxelDataSmall::checkValidity (VoxelData &vox) {
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      for (vuint32 z = 0; z < zsize; ++z) {
        const vuint32 vd = vox.query(x, y, z);
        if (vd != queryVox(x, y, z)) Sys_Error("internal error in compressed voxel data");
      }
    }
  }
}


//==========================================================================
//
//  VoxelDataSmall::createSlab
//
//==========================================================================
vuint32 VoxelDataSmall::createSlab (VoxelData &vox, vuint32 dofs0) {
  while (dofs0 && !vox.data/*.ptr()*/[dofs0].cull) dofs0 = vox.data/*.ptr()*/[dofs0].nextz;
  if (!dofs0) return 0;
  // calculate zlo and zhi, and count runs
  vuint16 runcount = 0;
  vuint16 z0 = vox.data/*.ptr()*/[dofs0].z;
  vuint16 z1 = z0;
  vuint16 nxz = (vuint16)(z0-1);
  for (vuint32 dofs = dofs0; dofs; dofs = vox.data/*.ptr()*/[dofs].nextz) {
    if (!vox.data/*.ptr()*/[dofs].cull) continue;
    z1 = vox.data/*.ptr()*/[dofs].z;
    if (z1 != nxz) ++runcount;
    nxz = (vuint16)(z1+1);
  }
  vassert(runcount);
  if (data.length() == 0) appendByte(0); // unused
  const vuint32 startofs = (vuint32)data.length();
  // zlo
  appendShort(z0);
  // zhi
  appendShort(z1);
  // runcount
  appendShort(runcount);
  // run index (will be filled later)
  vuint32 idxofs = (vuint32)data.length();
  for (vuint32 f = 0; f < runcount; ++f) {
    appendShort(0); // z0
    appendShort(0); // z1
    appendShort(0); // offset
    appendShort(0); // reserved
  }
  // last index item
  appendShort((vuint16)(z1+1));
  appendShort((vuint16)(z1+1));
  appendShort(0); // offset
  appendShort(0); // reserved
  nxz = (vuint16)(z0-1);
  vuint16 lastz = 0xffffU;
  // put runs
  for (vuint32 dofs = dofs0; dofs; dofs = vox.data/*.ptr()*/[dofs].nextz) {
    if (!vox.data/*.ptr()*/[dofs].cull) continue;
    z1 = vox.data/*.ptr()*/[dofs].z;
    if (z1 != nxz) {
      // new run
      // fix prev run?
      if (lastz != 0xffffU) {
        data/*.ptr()*/[idxofs-6] = (vuint8)lastz;
        data/*.ptr()*/[idxofs-5] = (vuint8)(lastz>>8);
      }
      // set run info
      // calculate offset
      const vuint32 rofs = (vuint32)data.length()-idxofs;
      vassert(rofs <= 0xffffU);
      // z0
      data/*.ptr()*/[idxofs++] = (vuint8)z1;
      data/*.ptr()*/[idxofs++] = (vuint8)(z1>>8);
      // skip z1
      idxofs += 2;
      // offset
      data/*.ptr()*/[idxofs++] = (vuint8)rofs;
      data/*.ptr()*/[idxofs++] = (vuint8)(rofs>>8);
      // skip reserved
      idxofs += 2;
    }
    lastz = nxz = (vuint16)(z1+1);
    // b, g, r, cull
    appendByte(vox.data/*.ptr()*/[dofs].b);
    appendByte(vox.data/*.ptr()*/[dofs].g);
    appendByte(vox.data/*.ptr()*/[dofs].r);
    appendByte(vox.data/*.ptr()*/[dofs].cull);
  }
  // fix prev run?
  vassert(lastz != 0xffffU);
  data/*.ptr()*/[idxofs-6] = (vuint8)lastz;
  data/*.ptr()*/[idxofs-5] = (vuint8)(lastz>>8);
  return startofs;
}


//==========================================================================
//
//  VoxelDataSmall::createFrom
//
//==========================================================================
void VoxelDataSmall::createFrom (VoxelData &vox) {
  clear();
  //vox.removeEmptyVoxels(); // just in case
  xsize = vox.xsize;
  ysize = vox.ysize;
  zsize = vox.zsize;
  xyofs.setLength(xsize*ysize);
  cx = vox.cx;
  cy = vox.cy;
  cz = vox.cz;
  for (vuint32 y = 0; y < ysize; ++y) {
    for (vuint32 x = 0; x < xsize; ++x) {
      const vuint32 dofs = createSlab(vox, vox.getDOfs(x, y));
      xyofs[(vuint32)y*xsize+(vuint32)x] = dofs;
    }
  }
  /*
  conwriteln("*** created compressed voxel data; ", data.length, " bytes for ",
             xsize, "x", ysize, "x", zsize);
  */
  checkValidity(vox);
}


//==========================================================================
//
//  VoxelDataSmall::queryVox
//
//==========================================================================
vuint32 VoxelDataSmall::queryVox (int x, int y, int z) noexcept {
  //pragma(inline, true);
  if (x < 0 || y < 0 || z < 0) return 0;
  if ((vuint32)x >= xsize || (vuint32)y >= ysize) return 0;
  vuint32 dofs = xyofs/*.ptr()*/[(vuint32)y*xsize+(vuint32)x];
  if (!dofs) return 0;
  const vuint16 *dptr = (const vuint16 *)(data.ptr()+dofs);
    //conwriteln("z=", z, "; zlo=", dptr[0], "; zhi=", dptr[1], "; runcount=", dptr[2]);
  if ((vuint16)z < *dptr++) return 0;
  if ((vuint16)z > *dptr++) return 0;
  vuint32 runcount = *dptr++;
  if (runcount <= 4) {
    // there is no reason to perform binary search here
    while ((vuint16)z > *dptr) dptr += 4;
      //conwriteln("  rz0=", dptr[0], "; rz1=", dptr[1], "; rofs=", dptr[2]);
    if (z == *dptr) {
      const vuint32 *dv = (const vuint32 *)(((const vuint8 *)dptr)+dptr[2]);
      return *dv;
    } else {
      dptr -= 4;
      const vuint16 cz = *dptr;
        //conwriteln("  cz=", cz, "; cz1=", dptr[1], "; rofs=", dptr[2]);
      vassert(cz < z);
      if ((vuint16)z >= dptr[1]) return 0; // no such voxel
      const vuint32 *dv = (const vuint32 *)(((const vuint8 *)dptr)+dptr[2]);
      return *(dv+z-cz);
    }
  } else {
    //{ import core.stdc.stdio : printf; printf("runcount=%u\n", cast(uint)runcount); }
    //assert(0);
    // perform binary search
    vuint32 lo = 0, hi = runcount-1;
    for (;;) {
      vuint32 mid = (lo+hi)>>1;
      const vuint16 *dp = dptr+(mid<<2);
      if ((vuint16)z >= dp[0] && (vuint16)z < dp[1]) {
        const vuint32 *dv = (const vuint32 *)(((const vuint8 *)dp)+dp[2]);
        return *(dv+z-*dp);
      }
      if ((vuint16)z < dp[0]) {
        if (mid == lo) break;
        hi = mid-1;
      } else {
        if (mid == hi) { lo = hi; break; }
        lo = mid+1;
      }
    }
    const vuint16 *dp = dptr+(lo<<2);
    //{ import core.stdc.stdio : printf; printf("000: z=%u; lo=%u; cz0=%u; cz1=%u\n", cast(uint)z, cast(uint)lo, cast(uint)dp[0], cast(uint)dp[1]); }
    while ((vuint16)z >= dp[1]) dp += 4;
    //{ import core.stdc.stdio : printf; printf("001:   lo=%u; cz0=%u; cz1=%u\n", cast(uint)z, cast(uint)lo, cast(uint)dp[0], cast(uint)dp[1]); }
    if ((vuint16)z < dp[0]) return 0;
    const vuint32 *dv = (const vuint32 *)(((const vuint8 *)dp)+dp[2]);
    return *(dv+z-*dp);
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// VoxelMesh
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  VoxQuad::calcNormal
//
//==========================================================================
void VoxQuad::calcNormal () noexcept {
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


// ////////////////////////////////////////////////////////////////////////// //
const vuint8 VoxelMesh::quadFaces[6][4] = {
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
void VoxelMesh::clear () noexcept {
  quads.clear();
  catlas.clear();
  cx = cy = cz = 0.0f;
}



//==========================================================================
//
//  VoxelMesh::setColors
//
//==========================================================================
void VoxelMesh::setColors (VoxQuad &vq, const vuint32 *clrs, vuint32 wdt, vuint32 hgt) {
  if (catlas.findRect(clrs, wdt, hgt, &vq.cidx, &vq.wh)) {
    /*
    conwriteln("  ...reused rect: (", catlas.getTexX(vq.cidx, vq.rc), ",",
               catlas.getTexY(vq.cidx, vq.rc), ")-(", wdt, "x", hgt, ")");
    */
    return;
  }
  vq.cidx = catlas.addNewRect(clrs, wdt, hgt);
  vq.wh = VoxWH16(wdt, hgt);
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

  bool allsame = (len == 1);
  if (!allsame) {
    for (int cidx = 1; cidx < len; ++cidx) {
      if (colors[cidx] != colors[0]) {
        allsame = false;
        break;
      }
    }
  }
  //if (allsame) colors = colors[0..1];

  const int qtype =
    allsame ? Point :
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
    default: __builtin_trap();
  }
  VoxQuad vq;
  for (vuint32 vidx = 0; vidx < 4; ++vidx) {
    vq.vx[vidx] = genVertex(quadFaces[qidx][vidx], x, y, z, dx, dy, dz);
  }
  setColors(vq, colors, (vuint32)(allsame ? 1 : len), 1);
  vq.type = qtype;
  vq.cull = cull;
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
//  VoxelMesh::addQuad
//
//==========================================================================
void VoxelMesh::addQuad (vuint8 cull,
                         float x, float y, float z,
                         int wdt, int hgt, // quad size
                         const vuint32 *colors)
{
  vassert(wdt > 0 && hgt > 0);
  vassert(cull == 0x01 || cull == 0x02 || cull == 0x04 || cull == 0x08 || cull == 0x10 || cull == 0x20);

  bool allsame = (wdt == 1 && hgt == 1);
  if (!allsame) {
    const int csz = wdt*hgt;
    for (auto cidx = 1; cidx < csz; ++cidx) {
      if (colors[cidx] != colors[0]) {
        allsame = false;
        break;
      }
    }
  }
  //if (allsame) colors = colors[0..1];

  const int qtype = Quad;
  vuint32 qidx;
  switch (cull) {
    case 0x01: qidx = 0; break;
    case 0x02: qidx = 1; break;
    case 0x04: qidx = 2; break;
    case 0x08: qidx = 3; break;
    case 0x10: qidx = 4; break;
    case 0x20: qidx = 5; break;
    default: __builtin_trap();
  }

  VoxQuad vq;
  for (vuint32 vidx = 0; vidx < 4; ++vidx) {
    const vuint8 vtype = quadFaces[qidx][vidx];
    VoxQuadVertex vx;
    vx.qtype = vtype;
    vx.dx = vx.dy = vx.dz = 0.0f;
    vx.x = x;
    vx.y = y;
    vx.z = z;
    if (cull&Cull_ZAxisMask) {
      if (vtype&DMV_X) vx.dx = (float)wdt;
      if (vtype&DMV_Y) vx.dy = (float)hgt;
      if (vtype&DMV_Z) vx.dz = 1.0f;
    } else if (cull&Cull_XAxisMask) {
      if (vtype&DMV_X) vx.dx = 1.0f;
      if (vtype&DMV_Y) vx.dy = (float)wdt;
      if (vtype&DMV_Z) vx.dz = (float)hgt;
    } else if (cull&Cull_YAxisMask) {
      if (vtype&DMV_X) vx.dx = (float)wdt;
      if (vtype&DMV_Y) vx.dy = 1.0f;
      if (vtype&DMV_Z) vx.dz = (float)hgt;
    } else {
      vassert(0);
    }
    vx.x += vx.dx;
    vx.y += vx.dy;
    vx.z += vx.dz;
    vq.vx[vidx] = vx;
  }

  if (allsame) {
    setColors(vq, colors, 1, 1);
  } else {
    setColors(vq, colors, wdt, hgt);
  }
  vq.type = qtype;
  vq.cull = cull;

  quads.append(vq);
}


//==========================================================================
//
//  VoxelMesh::buildOpt0
//
//==========================================================================
void VoxelMesh::buildOpt0 (VoxelData &vox) {
  if (voxlib_verbose) GLog.Logf(NAME_Init, "  method: quad per face...");
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;
  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      vuint32 dofs = vox.getDOfs(x, y);
      while (dofs) {
        addCube(vox.data/*.ptr()*/[dofs].cull, x-px, y-py, vox.data/*.ptr()*/[dofs].z-pz, vox.data/*.ptr()*/[dofs].rgb());
        dofs = vox.data/*.ptr()*/[dofs].nextz;
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::buildOpt1
//
//==========================================================================
void VoxelMesh::buildOpt1 (VoxelData &vox) {
  if (voxlib_verbose) GLog.Logf(NAME_Init, "  method: quad per vertical slab...");
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;

  vuint32 slab[1024];

  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      // try slabs in all 6 directions?
      vuint32 dofs = vox.getDOfs(x, y);
      if (!dofs) continue;

      // long top and bottom quads
      while (dofs) {
        for (vuint32 cidx = 4; cidx < 6; ++cidx) {
          const vuint8 cmask = VoxelData::cullmask(cidx);
          if ((vox.data/*.ptr()*/[dofs].cull&cmask) == 0) continue;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          slab[0] = vox.data/*.ptr()*/[dofs].rgb();
          addSlabFace(cmask, DMV_X, x-px, y-py, z-pz, 1, slab);
        }
        dofs = vox.data/*.ptr()*/[dofs].nextz;
      }

      // build long quads for each side
      for (vuint32 cidx = 0; cidx < 4; ++cidx) {
        const vuint8 cmask = VoxelData::cullmask(cidx);
        dofs = vox.getDOfs(x, y);
        while (dofs) {
          while (dofs && (vox.data/*.ptr()*/[dofs].cull&cmask) == 0) dofs = vox.data/*.ptr()*/[dofs].nextz;
          if (!dofs) break;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          int count = 0;
          vuint32 eofs = dofs;
          while (eofs && (vox.data/*.ptr()*/[eofs].cull&cmask)) {
            if ((int)vox.data/*.ptr()*/[eofs].z != z+count) break;
            vox.data/*.ptr()*/[eofs].cull ^= cmask;
            slab[count] = vox.data/*.ptr()*/[eofs].rgb();
            eofs = vox.data/*.ptr()*/[eofs].nextz;
            ++count;
            if (count == (int)(sizeof(slab)/sizeof(slab[0]))) break;
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
void VoxelMesh::buildOpt2 (VoxelData &vox) {
  if (voxlib_verbose) GLog.Logf(NAME_Init, "  method: quad per vertical slab, top and bottom slabs...");
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;

  vuint32 slab[1024];

  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      // try slabs in all 6 directions?
      vuint32 dofs = vox.getDOfs(x, y);
      if (!dofs) continue;

      // long top and bottom quads
      while (dofs) {
        for (vuint32 cidx = 4; cidx < 6; ++cidx) {
          const vuint8 cmask = VoxelData::cullmask(cidx);
          if ((vox.data/*.ptr()*/[dofs].cull&cmask) == 0) continue;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          vassert(vox.queryCull(x, y, z) == vox.data/*.ptr()*/[dofs].cull);
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
        dofs = vox.data/*.ptr()*/[dofs].nextz;
      }

      // build long quads for each side
      for (vuint32 cidx = 0; cidx < 4; ++cidx) {
        const vuint8 cmask = VoxelData::cullmask(cidx);
        dofs = vox.getDOfs(x, y);
        while (dofs) {
          while (dofs && (vox.data/*.ptr()*/[dofs].cull&cmask) == 0) dofs = vox.data/*.ptr()*/[dofs].nextz;
          if (!dofs) break;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          int count = 0;
          vuint32 eofs = dofs;
          while (eofs && (vox.data/*.ptr()*/[eofs].cull&cmask)) {
            if ((int)vox.data/*.ptr()*/[eofs].z != z+count) break;
            vox.data/*.ptr()*/[eofs].cull ^= cmask;
            slab[count] = vox.data/*.ptr()*/[eofs].rgb();
            eofs = vox.data/*.ptr()*/[eofs].nextz;
            ++count;
            if (count == (int)(sizeof(slab)/sizeof(slab[0]))) break;
          }
          vassert(count);
          dofs = eofs;
          addSlabFace(cmask, DMV_Z, x-px, y-py, z-pz, count, slab);
        }
      }
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static VVA_FORCEINLINE int getDX (vuint8 dmv) noexcept { return !!(dmv&VoxelMesh::DMV_X); }
static VVA_FORCEINLINE int getDY (vuint8 dmv) noexcept { return !!(dmv&VoxelMesh::DMV_Y); }
static VVA_FORCEINLINE int getDZ (vuint8 dmv) noexcept { return !!(dmv&VoxelMesh::DMV_Z); }

static VVA_FORCEINLINE void incXYZ (vuint8 dmv, int &sx, int &sy, int &sz) noexcept {
  sx += getDX(dmv);
  sy += getDY(dmv);
  sz += getDZ(dmv);
}


//==========================================================================
//
//  VoxelMesh::buildOpt3
//
//==========================================================================
void VoxelMesh::buildOpt3 (VoxelData &vox) {
  if (voxlib_verbose) GLog.Logf(NAME_Init, "  method: quad per slab in any direction...");
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;

  // try slabs in all 6 directions?
  vuint32 slab[1024];

  const vuint8 dmove[3][2] = {
    {DMV_Y, DMV_Z}, // left, right
    {DMV_X, DMV_Z}, // near, far
    {DMV_X, DMV_Y}, // top, bottom
  };

  for (int y = 0; y < (int)vox.ysize; ++y) {
    for (int x = 0; x < (int)vox.xsize; ++x) {
      for (vuint32 dofs = vox.getDOfs(x, y); dofs; dofs = vox.data/*.ptr()*/[dofs].nextz) {
        while (vox.data/*.ptr()*/[dofs].cull) {
          vuint32 count = 0;
          vuint8 clrdmv = 0;
          vuint8 clrmask = 0;
          const int z = (int)vox.data/*.ptr()*/[dofs].z;
          // check all faces
          for (vuint32 cidx = 0; cidx < 6; ++cidx) {
            const vuint8 cmask = VoxelData::cullmask(cidx);
            if ((vox.data/*.ptr()*/[dofs].cull&cmask) == 0) continue;
            // try two dirs
            for (vuint32 ndir = 0; ndir < 2; ++ndir) {
              const vuint8 dmv = dmove[cidx>>1][ndir];
              int cnt = 1;
              int sx = x, sy = y, sz = z;
              incXYZ(dmv, sx, sy, sz);
              for (;;) {
                const vuint8 vxc = vox.queryCull(sx, sy, sz);
                if ((vxc&cmask) == 0) break;
                ++cnt;
                incXYZ(dmv, sx, sy, sz);
              }
              if (cnt > (int)count) {
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
//  VoxelMesh::buildOpt4
//
//  this tries to create big quads
//
//==========================================================================
void VoxelMesh::buildOpt4 (VoxelData &vox) {
  if (voxlib_verbose) GLog.Logf(NAME_Init, "  method: optimal quad fill...");
  const float px = vox.cx;
  const float py = vox.cy;
  const float pz = vox.cz;

  TArray<vuint32> slab;

  // for faster scans
  Vox3DBitmap bmp3d;
  vox.create3DBitmap(bmp3d);

  VoxelDataSmall vxopt;
  vxopt.createFrom(vox);

  Vox2DBitmap bmp2d;
  for (vuint32 cidx = 0; cidx < 6; ++cidx) {
    const vuint8 cmask = VoxelData::cullmask(cidx);

    vuint32 vwdt, vhgt, vlen;
    if (cmask&Cull_ZAxisMask) {
      vwdt = vox.xsize;
      vhgt = vox.ysize;
      vlen = vox.zsize;
    } else if (cmask&Cull_XAxisMask) {
      vwdt = vox.ysize;
      vhgt = vox.zsize;
      vlen = vox.xsize;
    } else {
      vwdt = vox.xsize;
      vhgt = vox.zsize;
      vlen = vox.ysize;
    }
    bmp2d.setSize(vwdt, vhgt);

    for (vuint32 vcrd = 0; vcrd < vlen; ++vcrd) {
      //bmp2d.clearBmp(); // no need to, it is guaranteed
      vassert(bmp2d.dotCount == 0);
      for (vuint32 vdy = 0; vdy < vhgt; ++vdy) {
        for (vuint32 vdx = 0; vdx < vwdt; ++vdx) {
          vuint32 vx, vy, vz;
               if (cmask&Cull_ZAxisMask) { vx = vdx; vy = vdy; vz = vcrd; }
          else if (cmask&Cull_XAxisMask) { vx = vcrd; vy = vdx; vz = vdy; }
          else { vx = vdx; vy = vcrd; vz = vdy; }
          //conwriteln("*vcrd=", vcrd, "; vdx=", vdx, "; vdy=", vdy);
          if (!bmp3d.getPixel(vx, vy, vz)) continue;
          const vuint32 vd = vxopt.queryVox(vx, vy, vz);
          if (((vd>>24)&cmask) == 0) continue;
          bmp2d.setPixel(vdx, vdy, vd|0xff000000U);
        }
      }
      //conwriteln(":: cidx=", cidx, "; vcrd=", vcrd, "; dotCount=", bmp2d.dotCount);
      if (bmp2d.dotCount == 0) continue;
      // ok, we have some dots, go create quads
      int x0, y0, x1, y1;
      while (bmp2d.doOne(&x0, &y0, &x1, &y1)) {
        const vuint32 cwdt = (x1-x0)+1;
        const vuint32 chgt = (y1-y0)+1;
        if (slab.length() < (int)(cwdt*chgt)) slab.setLength((int)((cwdt*chgt)|0xff)+1);
        // get colors
        vuint32 *dp = slab.ptr();
        for (int dy = y0; dy <= y1; ++dy) {
          for (int dx = x0; dx <= x1; ++dx) {
            *dp++ = bmp2d.resetPixel(dx, dy);
          }
        }
        float fx, fy, fz;
             if (cmask&Cull_ZAxisMask) { fx = x0; fy = y0; fz = vcrd; }
        else if (cmask&Cull_XAxisMask) { fx = vcrd; fy = x0; fz = y0; }
        else { fx = x0; fy = vcrd; fz = y0; }
        addQuad(cmask, fx-px, fy-py, fz-pz, cwdt, chgt, slab.ptr());
      }
    }
  }
}


//==========================================================================
//
//  VoxelMesh::createFrom
//
//==========================================================================
void VoxelMesh::createFrom (VoxelData &vox, int optlevel) {
  vassert(vox.xsize && vox.ysize && vox.zsize);
  // now build cubes
  //conwriteln("building slabs...");
  //auto tm = iv.timer.Timer(true);
  if (optlevel < 0) optlevel = 0;
  switch (optlevel) {
    case 0: buildOpt0(vox); break;
    case 1: buildOpt1(vox); break;
    case 2: buildOpt2(vox); break;
    case 3: buildOpt3(vox); break;
    case 4: default: buildOpt4(vox); break;
  }
  //tm.stop();
  //conwriteln("basic conversion: ", quads.length, " quads (", quads.length*2, " tris).");
  //conwriteln("converted in ", tm.toString(), "; optlevel is ", vox_optimisation, "/", kvx_max_optim_level);
  if (voxlib_verbose) GLog.Logf(NAME_Init, "  basic conversion: %d quads (%d tris).", quads.length(), quads.length()*2);
  cx = vox.cx;
  cy = vox.cy;
  cz = vox.cz;
}


// ////////////////////////////////////////////////////////////////////////// //
// moved to vecmat, because idiotic shit-plus-plus wants
// `GetTypeHash()` for it to be defined before including templated hash table
/*
struct VVoxVertexEx {
  float x, y, z;
  float s, t; // will be calculated after texture creation
  float nx, ny, nz; // normal

  inline VVoxVertexEx () noexcept
    : x(0.0f)
    , y(0.0f)
    , z(0.0f)
    , s(0.0f)
    , t(0.0f)
    , nx(0.0f)
    , ny(0.0f)
    , nz(0.0f)
  {}

  inline bool operator == (const VVoxVertexEx &b) const noexcept {
    return
      x == b.x && y == b.y && z == b.z &&
      s == b.s && t == b.t &&
      nx == b.nx && ny == b.ny && nz == b.nz;
  }

  VVA_FORCEINLINE TVec asTVec () const noexcept { return TVec(x, y, z); }
  VVA_FORCEINLINE TVec normalAsTVec () const noexcept { return TVec(nx, ny, nz); }

  VVA_FORCEINLINE float get (unsigned idx) const noexcept{
    return (idx == 0 ? x : idx == 1 ? y : z);
  }

  VVA_FORCEINLINE void set (unsigned idx, const float v) noexcept{
    if (idx == 0) x = v; else if (idx == 1) y = v; else z = v;
  }
};
*/


// ////////////////////////////////////////////////////////////////////////// //
// GLVoxelMesh
// ////////////////////////////////////////////////////////////////////////// //

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
  // fix min coords
  if (vmin[0] > gv.x) vmin[0] = gv.x;
  if (vmin[1] > gv.y) vmin[1] = gv.y;
  if (vmin[2] > gv.z) vmin[2] = gv.z;
  // fix max coords
  if (vmax[0] < gv.x) vmax[0] = gv.x;
  if (vmax[1] < gv.y) vmax[1] = gv.y;
  if (vmax[2] < gv.z) vmax[2] = gv.z;
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
  // our voxels are 1024x1024x1024 at max
  vmin[0] = vmin[1] = vmin[2] = +8192.0f;
  vmax[0] = vmax[1] = vmax[2] = -8192.0f;

  img.clear();
  imgWidth = imgHeight = 0;
}


/*
  here starts the t-junction fixing part
  probably the most complex piece of code here
  (because almost everything else is done elsewhere)

  the algorithm is very simple and fast, tho, because i can
  abuse the fact that vertices are always snapped onto the grid.
  so i simply created a bitmap that tells if there is any vertex
  at the given grid coords, and then walk over the edge, checking
  the bitmap, and adding the vertices. this is easy too, because
  all vertices are parallel to one of the coordinate axes. so no
  complex math required at all.

  another somewhat complex piece of code is triangle fan creator.
  there are four basic cases here.

  first: normal quad without any added vertices.
  we can simply copy its vertices, because such quad makes a valid fan.

  second: quad that have at least two edges without added vertices.
  we can use the shared vertex of those two edges as a starting point of
  a fan.

  third: two opposite edges have no added vertices.
  this can happen in "run conversion", and we can create two fans here.

  fourth: no above conditions are satisfied.
  this is the most complex case: to create a fan without degenerate triangles,
  we have to add a vertex in the center of the quad, and used it as a start of
  a triangle fan.

  note that we can always convert our triangle fans into triangle soup, so i
  didn't bothered to create a separate triangle soup code.
 */

//==========================================================================
//
//  GLVoxelMesh::freeSortStructs
//
//==========================================================================
void GLVoxelMesh::freeSortStructs () {
  gridbmp.clear();
}


//==========================================================================
//
//  GLVoxelMesh::createGrid
//
//==========================================================================
void GLVoxelMesh::createGrid () {
  // create the grid
  for (vuint32 f = 0; f < 3; ++f) {
    gridmin[f] = (int)vmin[f];
    gridmax[f] = (int)vmax[f];
  }
  vuint32 gxs = (vuint32)(gridmax[0]-gridmin[0]+1);
  vuint32 gys = (vuint32)(gridmax[1]-gridmin[1]+1);
  vuint32 gzs = (vuint32)(gridmax[2]-gridmin[2]+1);
  /*
  conwriteln("vox dims: (", gridmin[0], ",", gridmin[1], ",", gridmin[2], ")-(",
             gridmax[0], ",", gridmax[1], ",", gridmax[2], ")");
  conwriteln("grid size: (", gxs, ",", gys, ",", gzs, ")");
  */
  gridbmp.setSize(gxs, gys, gzs);
}


//==========================================================================
//
//  GLVoxelMesh::sortEdges
//
//  create 3d grid, put edges into it
//
//==========================================================================
void GLVoxelMesh::sortEdges () {
  createGrid();
  for (vuint32 f = 0; f < (vuint32)edges.length(); ++f) putEdgeToGrid(f);
}


//==========================================================================
//
//  GLVoxelMesh::createEdges
//
//  create list of edges
//
//==========================================================================
void GLVoxelMesh::createEdges () {
  totaltadded = 0;
  edges.setLength(indicies.length()/5*4); // one quad is 4 edges
  vuint32 eidx = 0;
  vuint32 uqcount = 0;
  for (vuint32 f = 0; f < (vuint32)indicies.length(); f += 5) {
    bool unitquad = true;
    for (vuint32 vx0 = 0; vx0 < 4; ++vx0) {
      const vuint32 vx1 = (vx0+1)&3;
      VoxEdge &e = edges[eidx++];
      e.v0 = indicies[f+vx0];
      e.v1 = indicies[f+vx1];
      if (vertices[e.v0].x != vertices[e.v1].x) {
        vassert(vertices[e.v0].y == vertices[e.v1].y);
        vassert(vertices[e.v0].z == vertices[e.v1].z);
        e.axis = AXIS_X;
      } else if (vertices[e.v0].y != vertices[e.v1].y) {
        vassert(vertices[e.v0].x == vertices[e.v1].x);
        vassert(vertices[e.v0].z == vertices[e.v1].z);
        e.axis = AXIS_Y;
      } else {
        vassert(vertices[e.v0].x == vertices[e.v1].x);
        vassert(vertices[e.v0].y == vertices[e.v1].y);
        vassert(vertices[e.v0].z != vertices[e.v1].z);
        e.axis = AXIS_Z;
      }
      e.clo = vertices[e.v0].get(e.axis);
      e.chi = vertices[e.v1].get(e.axis);
      e.dir = e.chi-e.clo;
      if (unitquad) unitquad = (e.dir == +1.0f || e.dir == -1.0f);
    }
    // "unit quads" can be ignored, they aren't interesting
    // also, each quad always have at least one "unit edge"
    // (this will be used to build triangle strips)
    uqcount += unitquad;
  }
  vassert(eidx == (vuint32)edges.length());
  //conwriteln(uqcount, " unit quad", (uqcount != 1 ? "s" : ""), " found.");
}


//==========================================================================
//
//  GLVoxelMesh::fixEdgeWithVert
//
//==========================================================================
void GLVoxelMesh::fixEdgeWithVert (VoxEdge &edge, float crd) {
  // calculate time
  const float tm = (crd-edge.clo)/edge.dir;

  const VVoxVertexEx &evx0 = vertices[edge.v0];
  const VVoxVertexEx &evx1 = vertices[edge.v1];

  VVoxVertexEx nvx = evx0;
  // set coord
  nvx.set(edge.axis, crd);
  // calc new (s,t)
  nvx.s += (evx1.s-evx0.s)*tm;
  nvx.t += (evx1.t-evx0.t)*tm;

  edge.moreverts.append(appendVertex(nvx));
  ++totaltadded;
}


//==========================================================================
//
//  GLVoxelMesh::fixEdgeNew
//
//  fix one edge
//
//==========================================================================
void GLVoxelMesh::fixEdgeNew (vuint32 eidx) {
  VoxEdge &edge = edges[eidx];
  if (edge.dir == +1.0f || edge.dir == -1.0f) return; // and here
  // check grid by the edge axis
  float gxyz[3];
  for (vuint32 f = 0; f < 3; ++f) gxyz[f] = vertices[edge.v0].get(f);
  const float step = (edge.dir < 0.0f ? -1.0f : +1.0f);
  gxyz[edge.axis] += step;
  while (gxyz[edge.axis] != edge.chi) {
    if (hasVertexAt(gxyz[0], gxyz[1], gxyz[2])) {
      fixEdgeWithVert(edge, gxyz[edge.axis]);
    }
    gxyz[edge.axis] += step;
  }
}


//==========================================================================
//
//  GLVoxelMesh::rebuildEdges
//
//==========================================================================
void GLVoxelMesh::rebuildEdges (vuint32 BreakIndex) {
  // now we have to rebuild quads
  // each quad will have at least two unmodified edges of unit length
  vuint32 newindcount = (vuint32)edges.length()*5;
  for (vuint32 f = 0; f < (vuint32)edges.length(); ++f) {
    newindcount += (vuint32)edges[f].moreverts.length()+8;
  }
  indicies.setLength(newindcount);

  newindcount = 0;
  for (vuint32 f = 0; f < (vuint32)edges.length(); f += 4) {
    // check if this quad is modified at all
    if (edges[f+0].moreverts.length() ||
        edges[f+1].moreverts.length() ||
        edges[f+2].moreverts.length() ||
        edges[f+3].moreverts.length())
    {
      // this can be a quad that needs to be converted into a "centroid fan"
      // if we have at least two adjacent edges without extra points, we can use them
      // otherwise, we need to append a centroid
      int firstGoodFace = -1;
      for (vuint32 c = 0; c < 4; ++c) {
        if (edges[f+c].moreverts.length() == 0 &&
            edges[f+((c+1)&3)].moreverts.length() == 0)
        {
          // i found her!
          firstGoodFace = (int)c;
          break;
        }
      }

      // have two good faces?
      if (firstGoodFace >= 0) {
        // yay, we can use v1 of the first face as the start of the fan
        vassert(edges[f+firstGoodFace].moreverts.length() == 0);
        indicies[newindcount++] = edges[f+firstGoodFace].v1;
        // then v1 of the second good face
        firstGoodFace = (firstGoodFace+1)&3;
        vassert(edges[f+firstGoodFace].moreverts.length() == 0);
        indicies[newindcount++] = edges[f+firstGoodFace].v1;
        // then add points of the other two faces (ignoring v0)
        for (vuint32 c = 0; c < 2; ++c) {
          firstGoodFace = (firstGoodFace+1)&3;
          for (vuint32 midx = 0; midx < (vuint32)edges[f+firstGoodFace].moreverts.length(); ++midx) {
            indicies[newindcount++] = edges[f+firstGoodFace].moreverts[midx];
          }
          indicies[newindcount++] = edges[f+firstGoodFace].v1;
        }
        // we're done with this quad
        indicies[newindcount++] = BreakIndex;
        continue;
      }

      // check if we have two opposite quads without extra points
      if ((edges[f+0].moreverts.length() == 0 && edges[f+2].moreverts.length() == 0) ||
          (edges[f+1].moreverts.length() == 0 && edges[f+3].moreverts.length() == 0) ||
          (edges[f+2].moreverts.length() == 0 && edges[f+0].moreverts.length() == 0) ||
          (edges[f+3].moreverts.length() == 0 && edges[f+1].moreverts.length() == 0))
      {
        // yes, we can use the algo for the strips here
        for (vuint32 eic = 0; eic < 4; ++eic) {
          if (!edges[f+eic].moreverts.length()) continue;
          const vuint32 oic = (eic+3)&3; // previous edge
          // sanity checks
          vassert(edges[f+oic].moreverts.length() == 0);
          vassert(edges[f+oic].v1 == edges[f+eic].v0);
          // create triangle fan
          indicies[newindcount++] = edges[f+oic].v0;
          indicies[newindcount++] = edges[f+eic].v0;
          // append additional vertices (they are already properly sorted)
          for (vuint32 tmpf = 0; tmpf < (vuint32)edges[f+eic].moreverts.length(); ++tmpf) {
            indicies[newindcount++] = edges[f+eic].moreverts[tmpf];
          }
          // and the last vertex
          indicies[newindcount++] = edges[f+eic].v1;
          // if the opposite side is not modified, we can finish the fan right now
          const vuint32 loic = (eic+2)&3;
          if (edges[f+loic].moreverts.length() == 0) {
            const vuint32 noic = (eic+1)&3;
            // oic: prev
            // eic: current
            // noic: next
            // loic: last
            indicies[newindcount++] = edges[f+noic].v1;
            indicies[newindcount++] = BreakIndex;
            // we're done here
            break;
          }
          indicies[newindcount++] = BreakIndex;
        }
        continue;
      }

      // alas, this quad should be converted to "centroid quad"
      // i.e. we will use quad center point to start a triangle fan

      // calculate quad center point
      float cx = 0.0f, cy = 0.0f, cz = 0.0f;
      float cs = 0.0f, ct = 0.0f;
      float prevx = 0.0f, prevy = 0.0f, prevz = 0.0f;
      bool xequal = true, yequal = true, zequal = true;
      for (vuint32 eic = 0; eic < 4; ++eic) {
        cs += (vertices[edges[f+eic].v0].s+vertices[edges[f+eic].v1].s)*0.5f;
        ct += (vertices[edges[f+eic].v0].t+vertices[edges[f+eic].v1].t)*0.5f;
        const float vx = vertices[edges[f+eic].v0].x;
        const float vy = vertices[edges[f+eic].v0].y;
        const float vz = vertices[edges[f+eic].v0].z;
        cx += vx;
        cy += vy;
        cz += vz;
        if (eic) {
          xequal = xequal && (prevx == vx);
          yequal = yequal && (prevy == vy);
          zequal = zequal && (prevz == vz);
        }
        prevx = vx;
        prevy = vy;
        prevz = vz;
      }
      cx /= 4.0f;
      cy /= 4.0f;
      cz /= 4.0f;
      cs /= 4.0f;
      ct /= 4.0f;

      // calculate s and t
      float s = cs;
      float t = ct;

      // append center vertex
      VVoxVertexEx nvx = vertices[edges[f+0].v0];
      // set coord
      nvx.x = cx;
      nvx.y = cy;
      nvx.z = cz;
      // calc new (s,t)
      nvx.s = s;
      nvx.t = t;
      indicies[newindcount++] = appendVertex(nvx);
      ++totaltadded;

      // append v0 of the first edge
      indicies[newindcount++] = edges[f+0].v0;
      // append all vertices except v0 for all edges
      for (vuint32 eic = 0; eic < 4; ++eic) {
        for (vuint32 midx = 0; midx < (vuint32)edges[f+eic].moreverts.length(); ++midx) {
          indicies[newindcount++] = edges[f+eic].moreverts[midx];
        }
        indicies[newindcount++] = edges[f+eic].v1;
      }
      indicies[newindcount++] = BreakIndex;
    } else {
      // easy deal, just copy it
      indicies[newindcount++] = edges[f+0].v0;
      indicies[newindcount++] = edges[f+1].v0;
      indicies[newindcount++] = edges[f+2].v0;
      indicies[newindcount++] = edges[f+3].v0;
      indicies[newindcount++] = BreakIndex;
    }
  }

  indicies.setLength(newindcount);
  indicies.condense();
}


//==========================================================================
//
//  GLVoxelMesh::fixTJunctions
//
//  t-junction fixer entry point
//  this will also convert vertex data to triangle strips
//
//==========================================================================
void GLVoxelMesh::fixTJunctions (vuint32 BreakIndex) {
  //const vuint32 oldvtotal = (vuint32)vertices.length();
  createEdges();
  //conwriteln(edges.length, " edges found (", edges.length/4*2, " tris, ", vertices.length, " verts)...");
  sortEdges();
  for (vuint32 f = 0; f < (vuint32)edges.length(); ++f) fixEdgeNew(f);
  freeSortStructs();
  if (totaltadded) {
    //conwriteln(totaltadded, " t-fix vertices added (", vertices.length-oldvtotal, " unique).");
    rebuildEdges(BreakIndex);
    if (voxlib_verbose) GLog.Logf(NAME_Init, "  rebuilt model: %u tris, %d vertices.", countTris(BreakIndex), vertices.length());
  }
  edges.clear();
}


//==========================================================================
//
//  GLVoxelMesh::countTris
//
//  count the number of triangles in triangle fan data
//  used for informational messages
//
//==========================================================================
vuint32 GLVoxelMesh::countTris (vuint32 BreakIndex) {
  vuint32 res = 0;
  vuint32 ind = 0;
  while (ind < (vuint32)indicies.length()) {
    vassert(indicies[ind] != BreakIndex);
    vuint32 end = ind+1;
    while (end < (vuint32)indicies.length() && indicies[end] != BreakIndex) ++end;
    vassert(end < (vuint32)indicies.length());
    vassert(end-ind >= 3);
    if (end-ind == 3) {
      // simple triangle
      res += 1;
    } else if (end-ind == 4) {
      // quad
      res += 2;
    } else {
      // triangle fan
      res += end-ind-2;
    }
    ind = end+1;
  }
  return res;
}


//==========================================================================
//
//  GLVoxelMesh::recreateTriangles
//
//==========================================================================
void GLVoxelMesh::recreateTriangles (TArray<VMeshTri> &Tris, vuint32 BreakIndex) {
  Tris.clear();
  int ind = 0;
  while (ind < indicies.length()) {
    vassert(indicies[ind] != BreakIndex);
    int end = ind+1;
    while (end < indicies.length() && indicies[end] != BreakIndex) ++end;
    vassert(end < indicies.length());
    vassert(end-ind >= 3);
    if (end-ind == 3) {
      // simple triangle
      VMeshTri &tri = Tris.alloc();
      tri.VertIndex[0] = indicies[ind+0];
      tri.VertIndex[1] = indicies[ind+1];
      tri.VertIndex[2] = indicies[ind+2];
    } else if (end-ind == 4) {
      // quad
      VMeshTri &tri0 = Tris.alloc();
      tri0.VertIndex[0] = indicies[ind+0];
      tri0.VertIndex[1] = indicies[ind+1];
      tri0.VertIndex[2] = indicies[ind+2];

      VMeshTri &tri1 = Tris.alloc();
      tri1.VertIndex[0] = indicies[ind+2];
      tri1.VertIndex[1] = indicies[ind+3];
      tri1.VertIndex[2] = indicies[ind+0];
    } else {
      // triangle fan
      for (int f = ind+1; f < end-1; ++f) {
        VMeshTri &tri = Tris.alloc();
        tri.VertIndex[0] = indicies[ind+0];
        tri.VertIndex[1] = indicies[f+0];
        tri.VertIndex[2] = indicies[f+1];
      }
    }
    ind = end+1;
  }
  Tris.condense();
}


// create lines for wireframe view
/*
void createLines () {
  lindicies.length = 0;
  lindicies.assumeSafeAppend;

  vuint32 ind = 0;
  while (ind < (vuint32)indicies.length) {
    vassert(indicies[ind] != BreakIndex);
    vuint32 end = ind+1;
    while (end < (vuint32)indicies.length && indicies[end] != BreakIndex) ++end;
    vassert(end < indicies.length);
    vassert(end-ind >= 3);
    if (end-ind == 3) {
      // simple triangle
      lindicies ~= indicies[ind+0];
      lindicies ~= indicies[ind+1];
      lindicies ~= indicies[ind+2];
      lindicies ~= BreakIndex;
    } else if (end-ind == 4) {
      // quad
      lindicies ~= indicies[ind+0];
      lindicies ~= indicies[ind+1];
      lindicies ~= indicies[ind+2];
      lindicies ~= BreakIndex;

      lindicies ~= indicies[ind+2];
      lindicies ~= indicies[ind+3];
      lindicies ~= indicies[ind+0];
      lindicies ~= BreakIndex;
    } else {
      // triangle fan
      for (int f = ind+1; f < end-1; ++f) {
        lindicies ~= indicies[ind+0];
        lindicies ~= indicies[f+0];
        lindicies ~= indicies[f+1];
        lindicies ~= BreakIndex;
      }
    }
    ind = end+1;
  }
}
*/


//==========================================================================
//
//  GLVoxelMesh::create
//
//  main entry point
//
//==========================================================================
void GLVoxelMesh::create (VoxelMesh &vox, bool tjfix, vuint32 BreakIndex) {
  clear();

  imgWidth = vox.catlas.getWidth();
  imgHeight = vox.catlas.getHeight();
  img.setLength(imgWidth*imgHeight);
  memcpy(img.ptr(), vox.catlas.colors.ptr(), imgWidth*imgHeight*4);

  // swap final colors in GL mesh?
  #ifdef VOXLIB_SWAP_COLORS
  vuint8 *ccs = (vuint8 *)img.ptr();
  for (int f = imgWidth*imgHeight; f--; ccs += 4) {
    const vuint8 ctmp = ccs[0];
    ccs[0] = ccs[2];
    ccs[2] = ctmp;
  }
  #endif

  // calculate quad normals
  const int quadcount = vox.quads.length();
  for (int f = 0; f < quadcount; ++f) vox.quads/*.ptr()*/[f].calcNormal();
  if (voxlib_verbose) GLog.Logf(NAME_Init, "  color texture size: %dx%d", imgWidth, imgHeight);

  // create arrays
  for (int f = 0; f < quadcount; ++f) {
    VoxQuad &vq = vox.quads[f];
    const vuint32 imgx0 = vox.catlas.getTexX(vq.cidx);
    const vuint32 imgx1 = vox.catlas.getTexX(vq.cidx)+vq.wh.getW()-1;
    const vuint32 imgy0 = vox.catlas.getTexY(vq.cidx);
    const vuint32 imgy1 = vox.catlas.getTexY(vq.cidx)+vq.wh.getH()-1;
    vuint32 vxn[4];

    VVoxVertexEx gv;
    const float u = ((float)imgx0+0.5f)/(float)imgWidth;
    const float v = ((float)imgy0+0.5f)/(float)imgHeight;
    const float u0 = ((float)imgx0+0.04f)/(float)imgWidth;
    const float u1 = ((float)imgx1+0.96f)/(float)imgWidth;
    const float v0 = ((float)imgy0+0.04f)/(float)imgHeight;
    const float v1 = ((float)imgy1+0.96f)/(float)imgHeight;
    for (vuint32 nidx = 0; nidx < 4; ++nidx) {
      const VoxQuadVertex &vx = vq.vx[nidx];
      gv.x = vx.x;
      gv.y = vx.y;
      gv.z = vx.z;
      if (vq.type == VoxelMesh::ZLong) {
        gv.s = (vx.dz ? u1 : u0);
        gv.t = v;
      } else if (vq.type == VoxelMesh::XLong) {
        gv.s = (vx.dx ? u1 : u0);
        gv.t = v;
      } else if (vq.type == VoxelMesh::YLong) {
        gv.s = (vx.dy ? u1 : u0);
        gv.t = v;
      } else if (vq.type == VoxelMesh::Point) {
        gv.s = u;
        gv.t = v;
      } else {
        gv.s = u0;
        gv.t = v0;
        vassert(vq.type == VoxelMesh::Quad);
        if (vq.cull&VoxelMesh::Cull_ZAxisMask) {
          if (vx.qtype&VoxelMesh::DMV_X) gv.s = u1;
          if (vx.qtype&VoxelMesh::DMV_Y) gv.t = v1;
        } else if (vq.cull&VoxelMesh::Cull_XAxisMask) {
          if (vx.qtype&VoxelMesh::DMV_Y) gv.s = u1;
          if (vx.qtype&VoxelMesh::DMV_Z) gv.t = v1;
        } else if (vq.cull&VoxelMesh::Cull_YAxisMask) {
          if (vx.qtype&VoxelMesh::DMV_X) gv.s = u1;
          if (vx.qtype&VoxelMesh::DMV_Z) gv.t = v1;
        } else {
          vassert(0);
        }
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
    indicies.append(BreakIndex);
  }

  if (voxlib_verbose) {
    GLog.Logf(NAME_Init, "  OpenGL: %d quads, %u tris, %u unique vertices (of %d)",
              vox.quads.length(), countTris(BreakIndex), uniqueVerts, vox.quads.length()*4);
  }

  if (tjfix) {
    fixTJunctions(BreakIndex);
    if (voxlib_verbose) {
      GLog.Logf(NAME_Init, "  OpenGL: with fixed t-junctions: %u tris", countTris(BreakIndex));
    }
  }

  //createLines();
  vertcache.clear();
}


#if 0
// ////////////////////////////////////////////////////////////////////////// //
// example loaders
// ////////////////////////////////////////////////////////////////////////// //

//==========================================================================
//
//  loadKVX
//
//==========================================================================
private void loadKVX (ref VoxelData vox, VFile fl, vuint32 fsize) {
  if (fsize > int.max/8) throw new Exception("voxel data too big (kvx)");
  version(kvx_dump) conwriteln("loading KVX...");
  auto nextpos = fl.tell+fsize;
  vuint32[] xofs;
  vuint16[][] xyofs;
  vuint8[] data;
  int xsiz, ysiz, zsiz;
  int xpivot, ypivot, zpivot;
  {
    auto fx = wrapStreamRO(fl, fl.tell, fsize);
    xsiz = fx.readNum!vuint32;
    ysiz = fx.readNum!vuint32;
    zsiz = fx.readNum!vuint32;
    version(kvx_dump) conwriteln("xsiz=", xsiz, "; ysiz=", ysiz, "; zsiz=", zsiz);
    if (xsiz < 1 || ysiz < 1 || zsiz < 1 || xsiz > 1024 || ysiz > 1024 || zsiz > 1024) throw new Exception("invalid voxel size");
    xpivot = fx.readNum!vuint32;
    ypivot = fx.readNum!vuint32;
    zpivot = fx.readNum!vuint32;
    version(kvx_dump) conwriteln("xpivot=", xpivot>>8, "; ypivot=", ypivot>>8, "; zpivot=", zpivot>>8);
    vuint32 xstart = (xsiz+1)*4+xsiz*(ysiz+1)*2;
    xofs = new vuint32[](xsiz+1);
    foreach (ref o; xofs) o = fx.readNum!vuint32-xstart;
    //conwriteln(xofs);
    xyofs = new vuint16[][](xsiz, ysiz+1);
    foreach (const x; 0..xsiz) {
      foreach (const y; 0..ysiz+1) {
        xyofs[x][y] = fx.readNum!vuint16;
      }
    }
    vassert(fx.size-fx.tell == fsize-24-(xsiz+1)*4-xsiz*(ysiz+1)*2);
    data = new vuint8[]((vuint32)(fx.size-fx.tell));
    fx.rawReadExact(data);
  }
  fl.seek(nextpos);
  // skip mipmaps
  while (fl.size-fl.tell > 768) {
    fsize = fl.readNum!vuint32; // size of this voxel (useful for mipmaps)
    fl.seek(fsize, Seek.Cur);
    version(kvx_dump) conwriteln("  skiped ", fsize, " bytes of mipmap");
  }
  vuint8[768] pal;
  if (fl.size-fl.tell == 768) {
    version(kvx_dump) conwriteln("  has palette!");
    fl.rawReadExact(pal[]);
    foreach (ref vuint8 c; pal[]) {
      if (c > 63) throw new Exception("invalid palette value");
      c = clampToByte(255*c/64);
    }
  } else {
    foreach (const cidx; 0..256) {
      pal[cidx*3+0] = cidx;
      pal[cidx*3+1] = cidx;
      pal[cidx*3+2] = cidx;
    }
  }
  const float px = 1.0f*xpivot/256.0f;
  const float py = 1.0f*ypivot/256.0f;
  const float pz = 1.0f*zpivot/256.0f;
  // now build cubes
  version(kvx_dump) conwriteln("building voxel data...");
  vox.setSize(xsiz, ysiz, zsiz);
  foreach (const y; 0..ysiz) {
    foreach (const x; 0..xsiz) {
      vuint32 sofs = xofs[x]+xyofs[x][y];
      vuint32 eofs = xofs[x]+xyofs[x][y+1];
      while (sofs < eofs) {
        int ztop = data/*.ptr()*/[sofs++];
        vuint32 zlen = data/*.ptr()*/[sofs++];
        vuint8 cull = data/*.ptr()*/[sofs++];
        if (!zlen) continue;
        //conwritefln("  x=%s; y=%s; z=%s; len=%s; cull=0x%02x", x, y, ztop, zlen, cull);
        // colors
        foreach (const cidx; 0..zlen) {
          vuint8 palcol = data/*.ptr()*/[sofs++];
          vuint8 cl = cull;
          version(none) {
            // no need to do this, voxel optimiser will take care of it
            if (cidx != 0) cl &= (vuint8)~0x10;
            if (cidx != zlen-1) cl &= (vuint8)~0x20;
          }
          const vuint8 r = pal[palcol*3+0];
          const vuint8 g = pal[palcol*3+1];
          const vuint8 b = pal[palcol*3+2];
          //addCube(cl, (xsiz-x-1)-px, y-py, (zsiz-ztop-1)-pz, b|(g<<8)|(r<<16));
          //version(kvx_dump) conwriteln("  adding voxel at (", x, ",", y, ",", ztop, ")");
          vox.addVoxel(xsiz-x-1, y, zsiz-ztop-1, b|(g<<8)|(r<<16), cl);
          //version(kvx_dump) conwriteln("    added.");
          ++ztop;
        }
      }
      vassert(sofs == eofs);
    }
  }
  vox.cx = px;
  vox.cy = py;
  vox.cz = pz;
}


//==========================================================================
//
//  loadKV6
//
//==========================================================================
private void loadKV6 (ref VoxelData vox, VFile fl) {
  version(kvx_dump) conwriteln("loading KV6...");
  int xsiz, ysiz, zsiz;
  float xpivot, ypivot, zpivot;
  xsiz = fl.readNum!vuint32;
  ysiz = fl.readNum!vuint32;
  zsiz = fl.readNum!vuint32;
  version(kvx_dump) conwriteln("xsiz=", xsiz, "; ysiz=", ysiz, "; zsiz=", zsiz);
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 || xsiz > 1024 || ysiz > 1024 || zsiz > 1024) {
    throw new Exception("invalid voxel size (kv6)");
  }
  xpivot = fl.readNum!float;
  ypivot = fl.readNum!float;
  zpivot = fl.readNum!float;
  version(kvx_dump) conwriteln("xpivot=", xpivot, "; ypivot=", ypivot, "; zpivot=", zpivot);
  vuint32 voxcount = fl.readNum!vuint32;
  if (voxcount == 0 || voxcount > int.max/8) throw new Exception("invalid number of voxels");
  static struct KVox {
    vuint32 rgb;
    vuint8 zlo;
    vuint8 zhi;
    vuint8 cull;
    vuint8 normidx;
  }
  auto kvox = new KVox[](voxcount);
  foreach (const vidx; 0..voxcount) {
    auto kv = &kvox[vidx];
    kv.rgb = 0;
    kv.rgb |= fl.readNum!vuint8;
    kv.rgb |= fl.readNum!vuint8<<8;
    kv.rgb |= fl.readNum!vuint8<<16;
    kv.rgb |= 0xff_00_00_00U;
    fl.readNum!vuint8; // always 128; ignore
    kv.zlo = fl.readNum!vuint8;
    kv.zhi = fl.readNum!vuint8;
    if (kv.zhi) vassert(0, "zhi is not zero!");
    kv.cull = fl.readNum!vuint8;
    kv.normidx = fl.readNum!vuint8;
  }
  auto xofs = new vuint32[](xsiz+1);
  vuint32 curvidx = 0;
  foreach (ref v; xofs[0..$-1]) {
    v = curvidx;
    auto count = fl.readNum!vuint32;
    curvidx += count;
  }
  xofs[$-1] = curvidx;
  auto xyofs = new vuint32[][](xsiz, ysiz+1);
  foreach (const xx; 0..xsiz) {
    curvidx = 0;
    foreach (const yy; 0..ysiz) {
      xyofs[xx][yy] = curvidx;
      auto count = fl.readNum!vuint16;
      curvidx += count;
    }
    xyofs[xx][$-1] = curvidx;
  }
  // now build cubes
  vox.setSize(xsiz, ysiz, zsiz);
  foreach (const y; 0..ysiz) {
    foreach (const x; 0..xsiz) {
      vuint32 sofs = xofs[x]+xyofs[x][y];
      vuint32 eofs = xofs[x]+xyofs[x][y+1];
      //if (sofs == eofs) continue;
      //assert(sofs < data.length && eofs <= data.length);
      while (sofs < eofs) {
        auto kv = &kvox[sofs++];
        //debug(kvx_dump) conwritefln("  x=%s; y=%s; zlo=%s; zhi=%s; cull=0x%02x", x, y, kv.zlo, kv.zhi, kv.cull);
        int z = kv.zlo;
        foreach (const cidx; 0..kv.zhi+1) {
          z += 1;
          vox.addVoxel(xsiz-x-1, y, zsiz-z, kv.rgb, kv.cull);
        }
      }
    }
  }
  //conwriteln(texpos);
  vox.cx = xpivot;
  vox.cy = ypivot;
  vox.cz = zpivot;
}


//==========================================================================
//
//  loadVox
//
//==========================================================================
private void loadVox (ref VoxelData vox, VFile fl, vuint32 xsiz) {
  version(kvx_dump) conwriteln("loading VOX...");
  vuint32 ysiz = fl.readNum!vuint32;
  vuint32 zsiz = fl.readNum!vuint32;
  /*debug(kvx_dump)*/ conwriteln("xsiz=", xsiz, "; ysiz=", ysiz, "; zsiz=", zsiz);
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 || xsiz > 1024 || ysiz > 1024 || zsiz > 1024) {
    throw new Exception("invalid voxel size (vox)");
  }
  auto fpos = fl.tell();
  fl.seek(fpos+xsiz*ysiz*zsiz);
  //auto data = new ubyte[](xsiz*ysiz*zsiz);
  //fl.rawReadExact(data);
  vuint8[768] pal;
  fl.rawReadExact(pal[]);
  foreach (ref vuint8 c; pal[]) {
    if (c > 63) throw new Exception("invalid palette value");
    c = clampToByte(255*c/64);
  }
  const float px = 1.0f*xsiz/2.0f;
  const float py = 1.0f*ysiz/2.0f;
  const float pz = 1.0f*zsiz/2.0f;
  // now build cubes
  fl.seek(fpos);
  vox.setSize(xsiz, ysiz, zsiz);
  foreach (const x; 0..xsiz) {
    foreach (const y; 0..ysiz) {
      foreach (const z; 0..zsiz) {
        vuint8 palcol = fl.readNum!vuint8;
        if (palcol != 255) {
          vuint32 rgb = pal[palcol*3+2]|((vuint32)pal[palcol*3+1]<<8)|((vuint32)pal[palcol*3+0]<<16);
          //addCube(0xff, (xsiz-x-1)-px, y-py, (zsiz-z-1)-pz, b|(g<<8)|(r<<16));
          vox.addVoxel(xsiz-x-1, y, zsiz-z-1, rgb, 0xff);
        }
      }
    }
  }
  vox.cx = px;
  vox.cy = py;
  vox.cz = pz;
}


//==========================================================================
//
//  loadVxl
//
//==========================================================================
private void loadVxl (ref VoxelData vox, VFile fl) {
  vuint32 xsiz = fl.readNum!vuint32;
  vuint32 ysiz = fl.readNum!vuint32;
  vuint32 zsiz = 256;
  /*debug(kvx_dump)*/ conwriteln("xsiz=", xsiz, "; ysiz=", ysiz, "; zsiz=", zsiz);
  if (xsiz < 1 || ysiz < 1 || zsiz < 1 || xsiz > 1024 || ysiz > 1024 || zsiz > 1024) throw new Exception("invalid voxel size");
  //immutable float px = 1.0f*xsiz/2.0f;
  //immutable float py = 1.0f*ysiz/2.0f;
  //immutable float pz = 1.0f*zsiz/2.0f;
  float px, py, pz;
  // camera
  px = fl.readNum!double;
  py = fl.readNum!double;
  pz = fl.readNum!double;
  pz = zsiz-1-pz;
  // unit right
  fl.readNum!double;
  fl.readNum!double;
  fl.readNum!double;
  // unit down
  fl.readNum!double;
  fl.readNum!double;
  fl.readNum!double;
  // unit forward
  fl.readNum!double;
  fl.readNum!double;
  fl.readNum!double;

  vox.setSize(xsiz, ysiz, zsiz);

  void vxlReset (int x, int y, int z) {
    vox.removeVoxel(xsiz-x-1, y, zsiz-z-1);
  }

  void vxlPaint (int x, int y, int z, vuint32 clr) {
    vox.addVoxel(xsiz-x-1, y, zsiz-z-1, clr, 0x3f);
  }

  // now carve crap out of it
  auto data = new vuint8[]((vuint32)(fl.size-fl.tell));
  fl.rawReadExact(data);
  const(vuint8)* v = data.ptr;
  foreach (const y; 0..ysiz) {
    foreach (const x; 0..xsiz) {
      vuint32 z = 0;
      for (;;) {
        foreach (const i; z..v[1]) vxlReset(x, y, i);
        for (z = v[1]; z <= v[2]; ++z) {
          const(vuint32)* cp = (const(vuint32)*)(v+(z-v[1]+1)*4);
          vxlPaint(x, y, z, *cp);
        }
        if (!v[0]) break;
        z = v[2]-v[1]-v[0]+2;
        v += v[0]*4;
        for (z += v[3]; z < v[3]; ++z) {
          const(vuint32)* cp = (const(vuint32)*)(v+(z-v[3])*4);
          vxlPaint(x, y, z, *cp);
        }
      }
      v += (v[2]-v[1]+2)*4;
    }
  }
  vox.cx = px;
  vox.cy = py;
  vox.cz = pz;
}


//==========================================================================
//
//  loadVoxel
//
//==========================================================================
public void loadVoxel (ref VoxelData vox, string fname, bool asvox=false) {
  auto fl = VFile(fname);
  vuint32 fsize = fl.readNum!vuint32; // size of this voxel (useful for mipmaps)
  auto tm = iv.timer.Timer(true);
  if (fsize == 0x6c78764bU) {
    loadKV6(vox, fl);
  } else if (fsize == 0x09072000U) {
    loadVxl(vox, fl);
  } else {
    if (fname.endsWithCI(".vox")) loadVox(vox, fl, fsize);
    else loadKVX(vox, fl, fsize);
  }
  tm.stop();
  conwriteln("loaded in ", tm.toString(), "; grid size: ", vox.xsize, "x", vox.ysize, "x", vox.zsize);
  tm.restart();
  vox.optimise(vox_optimize_hollow);
  tm.stop();
  conwriteln("optimised in ", tm.toString());
}
#endif
