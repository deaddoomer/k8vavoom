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
#ifndef VV_VOXELIB_HEADER
#define VV_VOXELIB_HEADER

#include "../../../libs/core/core.h"

// swap final colors in GL mesh?
#define VOXLIB_SWAP_COLORS


enum VoxLibMsg {
  VoxLibMsg_None,
  // also used for message types
  VoxLibMsg_Error, // this message is ALWAYS generated
  VoxLibMsg_Normal,
  VoxLibMsg_Warning,
  VoxLibMsg_Debug,
  // used only for `voxlib_verbose`
  VoxLibMsg_MaxVerbosity,
};

// verbose conversion? set this to `false` for somewhat faster processing
// (it is faster, because the library doesn't need to calculate some otherwise
// useless things)
extern VoxLibMsg voxlib_verbose;

// `msg` is never `nullptr` or empty string
extern void (*voxlib_message) (VoxLibMsg type, const char *msg);


// ////////////////////////////////////////////////////////////////////////// //
// very simple dynamic array implementation
// works only for POD types, but i don't need anything else anyway


// ////////////////////////////////////////////////////////////////////////// //
/*
  just a 2d bitmap that keeps rgb colors.
  there is also the code to find the biggest non-empty rectangle on the bitmap.
  empty pixels are represented with zeroes.

  the algorithm was taken from this SO topic: https://stackoverflow.com/questions/7245/
 */
struct Vox2DBitmap {
public:
  struct Pair {
    int one;
    int two;
  };

  TArray<int> cache; // cache
  TArray<Pair> stack; // stack
  int top; // top of stack

  int wdt, hgt; // dimension of input
  TArray<uint32_t> grid;
  TArray<uint32_t> ydotCount; // for each y coord
  int dotCount;

public:
  inline Vox2DBitmap () noexcept : top(0), wdt(0), hgt(0), dotCount(0) {}

  inline ~Vox2DBitmap () noexcept { clear(); }

  inline void push (int a, int b) noexcept {
    stack[top].one = a;
    stack[top].two = b;
    ++top;
  }

  inline void pop (int *a, int *b) noexcept {
    --top;
    *a = stack[top].one;
    *b = stack[top].two;
  }

  inline void clear () noexcept {
    cache.clear();
    stack.clear();
    grid.clear();
    ydotCount.clear();
    wdt = hgt = dotCount = 0;
  }

  void setSize (int awdt, int ahgt) noexcept {
    if (awdt < 1) awdt = 0;
    if (ahgt < 1) ahgt = 0;
    wdt = awdt;
    hgt = ahgt;
    if (grid.length() < awdt*ahgt) grid.setLength(awdt*ahgt);
    if (ydotCount.length() < ahgt) ydotCount.setLength(ahgt);
    clearBmp();
  }

  inline void clearBmp () noexcept {
    memset(grid.ptr(), 0, wdt*hgt*sizeof(uint32_t));
    memset(ydotCount.ptr(), 0, hgt*sizeof(uint32_t));
    dotCount = 0;
  }

  VVA_FORCEINLINE void setPixel (int x, int y, uint32_t rgb) noexcept {
    if (x < 0 || y < 0 || x >= wdt || y >= hgt) return;
    uint32_t *cp = grid.ptr()+(uint32_t)y*(uint32_t)wdt+(uint32_t)x;
    if (!*cp) {
      ++dotCount;
      ++ydotCount.ptr()[y];
    }
    *cp = rgb;
  }

  VVA_FORCEINLINE uint32_t resetPixel (int x, int y) noexcept {
    if (x < 0 || y < 0 || x >= wdt || y >= hgt) return 0;
    uint32_t *cp = grid.ptr()+(uint32_t)y*(uint32_t)wdt+(uint32_t)x;
    const uint32_t res = *cp;
    if (res) {
      *cp = 0;
      --dotCount;
      --ydotCount.ptr()[y];
    }
    return res;
  }

  inline void updateCache (int currY) noexcept {
    /*
    for (int m = 0; m < wdt; ++m, ++cp) {
      if (!grid[currY*wdt+m]) cache[m] = 0; else ++cache[m];
    }
    */
    const uint32_t *lp = grid.ptr()+(uint32_t)currY*(uint32_t)wdt;
    int *cp = cache.ptr();
    for (int m = wdt; m--; ++cp) {
      if (*lp++) ++(*cp); else *cp = 0;
    }
  }

  // this is the slowest part of the conversion code.
  bool doOne (int *rx0, int *ry0, int *rx1, int *ry1) noexcept;
};


// ////////////////////////////////////////////////////////////////////////// //
// just a compact representation of a rectange
// splitted to two copy-pasted structs for better type checking
struct __attribute__((packed)) VoxXY16 {
  uint32_t xy; // low word: x; high word: y

  VVA_FORCEINLINE VoxXY16 () noexcept {}
  VVA_FORCEINLINE VoxXY16 (uint32_t x, uint32_t y) noexcept { xy = (y<<16)|(x&0xffffU); }
  VVA_FORCEINLINE uint32_t getX () const noexcept { return (xy&0xffffU); }
  VVA_FORCEINLINE uint32_t getY () const noexcept { return (xy>>16); }
  VVA_FORCEINLINE void setX (uint32_t x) noexcept { xy = (xy&0xffff0000U)|(x&0xffffU); }
  VVA_FORCEINLINE void setY (uint32_t y) noexcept { xy = (xy&0x0000ffffU)|(y<<16); }
};


struct __attribute__((packed)) VoxWH16 {
  uint32_t wh; // low word: x; high word: y

  VVA_FORCEINLINE VoxWH16 () noexcept {}
  VVA_FORCEINLINE VoxWH16 (uint32_t w, uint32_t h) noexcept { wh = (h<<16)|(w&0xffffU); }
  VVA_FORCEINLINE uint32_t getW () const noexcept { return (wh&0xffffU); }
  VVA_FORCEINLINE uint32_t getH () const noexcept { return (wh>>16); }
  VVA_FORCEINLINE void setW (uint32_t w) noexcept { wh = (wh&0xffff0000U)|(w&0xffffU); }
  VVA_FORCEINLINE void setH (uint32_t h) noexcept { wh = (wh&0x0000ffffU)|(h<<16); }
};


// ////////////////////////////////////////////////////////////////////////// //
// packing rectangles into atlas
struct VoxTexAtlas {
public:
  struct Rect {
    int x, y, w, h;
    static Rect Invalid () noexcept { return Rect(0, 0, 0, 0); }
    VVA_FORCEINLINE Rect () noexcept : x(0), y(0), w(0), h(0) {}
    VVA_FORCEINLINE Rect (int ax, int ay, int aw, int ah) noexcept : x(ax), y(ay), w(aw), h(ah) {}
    VVA_FORCEINLINE bool isValid () const noexcept { return (x >= 0 && y >= 0 && w > 0 && h > 0); }
    VVA_FORCEINLINE int getArea () const noexcept { return w*h; }
    VVA_FORCEINLINE int getX1 () const noexcept { return x+w; }
    VVA_FORCEINLINE int getY1 () const noexcept { return y+h; }
  };

private:
  int imgWidth, imgHeight;
  TArray<Rect> rects;

  enum { BadRect = MAX_VUINT32 };

  // node id or BadRect
  uint32_t findBestFit (int w, int h) noexcept;

public:
  void clear () {
    rects.clear();
    imgWidth = imgHeight = 0;
  }

  void setSize (int awdt, int ahgt) noexcept{
    vassert(awdt > 0 && ahgt > 0);
    imgWidth = awdt;
    imgHeight = ahgt;
    rects.reset();
    rects.append(Rect(0, 0, awdt, ahgt)); // one big rect
  }

  inline int getWidth () const noexcept{ return imgWidth; }
  inline int getHeight () const noexcept{ return imgHeight; }

  // returns invalid rect if there's no room
  Rect insert (int cwdt, int chgt) noexcept;
};


// ////////////////////////////////////////////////////////////////////////// //
// color atlas, ready to be uploaded to the GPU
struct VoxColorPack {
public:
  struct ColorItem {
    VoxXY16 xy; // start position
    VoxWH16 wh; // size
    VoxXY16 newxy; // used in relayouter
    int next; // -1: no more
  };

public:
  uint32_t clrwdt, clrhgt;
  TArray<uint32_t> colors; // clrwdt by clrhgt

  TArray<ColorItem> citems;
  TMapNC<uint32_t, int> citemhash; // key: color index; value: index in `citems`

  VoxTexAtlas atlas;

private:
  bool findRectEx (const uint32_t *clrs, uint32_t cwdt, uint32_t chgt,
                   uint32_t cxofs, uint32_t cyofs,
                   uint32_t wdt, uint32_t hgt, uint32_t *cidxp, VoxWH16 *whp);

public:
  uint32_t getWidth () const noexcept { return clrwdt; }
  uint32_t getHeight () const noexcept { return clrhgt; }

  VVA_FORCEINLINE uint32_t getTexX (uint32_t cidx) const noexcept {
    return citems[cidx].xy.getX();
  }

  VVA_FORCEINLINE uint32_t getTexY (uint32_t cidx) const noexcept {
    return citems[cidx].xy.getY();
  }

  void clear () {
    colors.clear();
    citems.clear();
    citemhash.clear();
    atlas.clear();
  }

  // prepare for new run
  inline void reset () {
    clear();
  }

  // grow image, and relayout everything
  void growImage (uint32_t inswdt, uint32_t inshgt);


  inline bool findRect (const uint32_t *clrs, uint32_t wdt, uint32_t hgt,
                        uint32_t *cidxp, VoxWH16 *whp)
  {
    return findRectEx(clrs, wdt, hgt, 0, 0, wdt, hgt, cidxp, whp);
  }

  // returns index in `citems`
  uint32_t addNewRect (const uint32_t *clrs, uint32_t wdt, uint32_t hgt);
};


// ////////////////////////////////////////////////////////////////////////// //
/*
  this is a struct that keeps info about an individual voxel.
  it keeps voxel color, and face visibility info.
 */
struct __attribute__((packed)) VoxPix {
  uint8_t b, g, r;
  uint8_t cull;
  uint32_t nextz; // voxel with the next z; 0 means "no more"
  uint16_t z; // z of the current voxel

  VVA_FORCEINLINE uint32_t rgb () const noexcept {
    return 0xff000000U|b|((uint32_t)g<<8)|((uint32_t)r<<16);
  }

  VVA_FORCEINLINE uint32_t rgbcull () const noexcept {
    return b|((uint32_t)g<<8)|((uint32_t)r<<16)|((uint32_t)cull<<24);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
/*
  this is just a voxel cube, where each voxel represents by one bit.
  it is useful for fast "yes/no" queries and modifications, and is used
  in "hollow fill" and t-junction fixer algorithms.
 */
struct Vox3DBitmap {
  uint32_t xsize, ysize, zsize;
  uint32_t xwdt, xywdt;
  TArray<uint32_t> bmp; // bit per voxel

public:
  inline Vox3DBitmap () noexcept
    : xsize(0)
    , ysize(0)
    , zsize(0)
    , xwdt(0)
    , xywdt(0)
  {}

  void clear () noexcept {
    bmp.clear();
    xsize = ysize = zsize = xwdt = xywdt = 0;
  }

  void setSize (uint32_t xs, uint32_t ys, uint32_t zs) noexcept {
    clear();
    if (!xs || !ys || !zs) return;
    xsize = xs;
    ysize = ys;
    zsize = zs;
    xwdt = (xs+31)/32;
    vassert(xwdt<<5 >= xs);
    xywdt = xwdt*ysize;
    bmp.setLength(xywdt*zsize);
    memset(bmp.ptr(), 0, bmp.length()*sizeof(uint32_t));
  }

  // returns old value
  VVA_FORCEINLINE uint32_t setPixel (int x, int y, int z) noexcept {
    if (x < 0 || y < 0 || z < 0) return 1;
    if ((uint32_t)x >= xsize || (uint32_t)y >= ysize || (uint32_t)z >= zsize) return 1;
    //vuint32 *bp = bmp.ptr()+(vuint32)z*xywdt+(vuint32)y*xwdt+((vuint32)x>>5);
    uint32_t *bp = &bmp[(uint32_t)z*xywdt+(uint32_t)y*xwdt+((uint32_t)x>>5)];
    const uint32_t bmask = 1U<<((uint32_t)x&0x1f);
    const uint32_t res = (*bp)&bmask;
    *bp |= bmask;
    return res;
  }

  VVA_FORCEINLINE void resetPixel (int x, int y, int z) noexcept {
    if (x < 0 || y < 0 || z < 0) return;
    if ((uint32_t)x >= xsize || (uint32_t)y >= ysize || (uint32_t)z >= zsize) return;
    bmp/*.ptr()*/[(uint32_t)z*xywdt+(uint32_t)y*xwdt+((uint32_t)x>>5)] &= ~(1U<<((uint32_t)x&0x1f));
  }

  VVA_FORCEINLINE uint32_t getPixel (int x, int y, int z) const noexcept {
    if (x < 0 || y < 0 || z < 0) return 1;
    if ((uint32_t)x >= xsize || (uint32_t)y >= ysize || (uint32_t)z >= zsize) return 1;
    return (bmp/*.ptr()*/[(uint32_t)z*xywdt+(uint32_t)y*xwdt+((uint32_t)x>>5)]&(1U<<((uint32_t)x&0x1f)));
  }
};


// ////////////////////////////////////////////////////////////////////////// //
/*
  this keeps voxel "voxmap" (it's like a pixmap, but with voxels instead
  of pixels). the representation is slightly optimised, tho: it keeps only
  actually used voxels, in vertical "slabs". as most voxel models have only
  contour voxels stored, this greatly reduces memory consumption. quering
  the individial voxel data is slightly slower, tho, but for our case it is
  not really important.

  there are some methods to "optimise" the voxmap. for example, to fix voxel
  face visibility info, and to remove "internal" voxels (it is called "hollow fill").

  also note that some optimisation methods may leave empty voxels in the voxmap.
  they have `cull` field equal to zero.

  to build a voxmap you simply call `addVoxel()` method, optionally providing
  the face visibility info. but you can use `0x3f` as a visibility, and then ask
  `VoxelData` object to calculate the proprer "cull" values for you. you can also
  add "internal" voxels, and then ask the object to fix the vixmap for you, removing
  all the unnecessary data.
 */
struct VoxelData {
public:
  uint32_t xsize, ysize, zsize;
  float cx, cy, cz;

  TArray<VoxPix> data; // [0] is never used
  // xsize*ysize array, offsets in `data`; 0 means "no data here"
  // slabs are sorted from bottom to top, and never intersects
  TArray<uint32_t> xyofs;
  uint32_t freelist;
  uint32_t voxpixtotal;

private:
  uint32_t allocVox ();

public:
  inline VoxelData ()
    : xsize(0)
    , ysize(0)
    , zsize(0)
    , cx(0.0f)
    , cy(0.0f)
    , cz(0.0f)
    , freelist(0)
    , voxpixtotal(0)
  {}

  inline ~VoxelData () { clear(); }

  void clear ();
  void setSize (uint32_t xs, uint32_t ys, uint32_t zs);

  static VVA_FORCEINLINE uint8_t cullmask (uint32_t cidx) noexcept {
    return (uint8_t)(1U<<cidx);
  }

  // opposite mask
  static VVA_FORCEINLINE uint8_t cullopmask (uint32_t cidx) noexcept {
    return (uint8_t)(1U<<(cidx^1));
  }

  VVA_FORCEINLINE uint32_t getDOfs (int x, int y) const noexcept {
    if (x < 0 || y < 0) return 0;
    if ((uint32_t)x >= xsize || (uint32_t)y >= ysize) return 0;
    return xyofs/*.ptr()*/[(uint32_t)y*xsize+(uint32_t)x];
  }

  // high byte is cull info
  // returns 0 if there is no such voxel
  VVA_FORCEINLINE uint32_t voxofs (int x, int y, int z) const noexcept {
    uint32_t dofs = getDOfs(x, y);
    while (dofs) {
      if (data/*.ptr()*/[dofs].z == (uint16_t)z) return dofs;
      if (data/*.ptr()*/[dofs].z > (uint16_t)z) return 0;
      dofs = data/*.ptr()*/[dofs].nextz;
    }
    return 0;
  }

  // high byte is cull info
  // returns 0 if there is no such voxel
  VVA_FORCEINLINE uint32_t query (int x, int y, int z) const noexcept {
    const uint32_t dofs = voxofs(x, y, z);
    return (dofs && data/*.ptr()*/[dofs].cull ? data/*.ptr()*/[dofs].rgbcull() : 0);
  }

  VVA_FORCEINLINE VoxPix *queryVP (int x, int y, int z) noexcept {
    const uint32_t dofs = voxofs(x, y, z);
    return (dofs ? &data/*.ptr()*/[dofs] : nullptr);
  }

  // high byte is cull info
  // returns 0 if there is no such voxel
  VVA_FORCEINLINE uint8_t queryCull (int x, int y, int z) const noexcept {
    const uint32_t dofs = voxofs(x, y, z);
    return (dofs ? data/*.ptr()*/[dofs].cull : 0);
  }

  // only for existing voxels; won't remove empty voxels
  VVA_FORCEINLINE void setVoxelCull (int x, int y, int z, uint8_t cull) {
    VoxPix *vp = queryVP(x, y, z);
    if (vp) vp->cull = (uint8_t)(cull&0x3f);
  }


  void create3DBitmap (Vox3DBitmap &bmp);


  void removeVoxel (int x, int y, int z);
  void addVoxel (int x, int y, int z, uint32_t rgb, uint8_t cull);

  void checkInvariants ();

  // remove empty voxels from active voxels list
  void removeEmptyVoxels ();

  // remove inside voxels, leaving only contour
  void removeInsideFaces ();

  // if we have ANY voxel at the corresponding side, don't render that face
  // returns number of fixed voxels
  uint32_t fixFaceVisibility ();

  // this fills everything outside of the voxel, and
  // then resets culling bits for all invisible faces
  // i don't care about memory yet
  uint32_t hollowFill ();

  // main voxel optimisation entry point
  void optimise (bool doHollowFill);

public:
  static const int cullofs[6][3];
};


// ////////////////////////////////////////////////////////////////////////// //
struct /*__attribute__((packed))*/ VoxQuadVertex {
  float x, y, z;
  float dx, dy, dz;
  uint8_t qtype; // Xn_Yn_Zn
};


// quad is always one texel strip
struct __attribute__((packed)) VoxQuad {
  VoxQuadVertex vx[4];
  uint32_t cidx; // in colorpack's `citems`
  VoxQuadVertex normal;
  int type/* = Invalid*/;
  VoxWH16 wh; // width and height
  uint8_t cull; // for which face this quad was created?

  void calcNormal () noexcept;
};

/*
  this thing is used to create a quad mesh from a voxel data.
  it contains several conversion methods, from the most simple one
  "one quad for each visible voxel face", to the most complex one,
  that goes through each voxel plane, and joins individual voxel
  faces into a bigger quads (i.e. quads covering several faces at once).

  this doesn't directly calculates texture coords, tho: it is using an
  atlas, and records rect coords for each quad. real texture coords are
  calculated in GL mesh builder instead.
*/
struct VoxelMesh {
  // quad type
  enum {
    Invalid = -1,
    Point,
    XLong,
    YLong,
    ZLong,
    Quad,
  };

  enum {
    Cull_Right  = 0x01, // x axis
    Cull_Left   = 0x02, // x axis
    Cull_Near   = 0x04, // y axis
    Cull_Far    = 0x08, // y axis
    Cull_Top    = 0x10, // z axis
    Cull_Bottom = 0x20, // z axis

    Cull_XAxisMask = (Cull_Right|Cull_Left),
    Cull_YAxisMask = (Cull_Near|Cull_Far),
    Cull_ZAxisMask = (Cull_Top|Cull_Bottom),
  };

  enum {
    DMV_X = 0b100,
    DMV_Y = 0b010,
    DMV_Z = 0b001,
  };

  // bitmasks, `DMV_n` can be used to check for `0` or `1`
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

public:
  TArray<VoxQuad> quads;
  // voxel center point
  float cx, cy, cz;
  // color atlas
  VoxColorPack catlas;

private:
  static VoxQuadVertex genVertex (uint8_t type, const float x, const float y, const float z,
                                  const float xlen, const float ylen, const float zlen) noexcept
  {
    VoxQuadVertex vx;
    vx.qtype = type;
    vx.dx = vx.dy = vx.dz = 0.0f;
    vx.x = x;
    vx.y = y;
    vx.z = z;
    if (type&DMV_X) { vx.x += xlen; vx.dx = 1.0f; }
    if (type&DMV_Y) { vx.y += ylen; vx.dy = 1.0f; }
    if (type&DMV_Z) { vx.z += zlen; vx.dz = 1.0f; }
    return vx;
  }

  void setColors (VoxQuad &vq, const uint32_t *clrs, uint32_t wdt, uint32_t hgt);

  void addSlabFace (uint8_t cull, uint8_t dmv,
                    float x, float y, float z,
                    int len, const uint32_t *colors);

  void addCube (uint8_t cull, float x, float y, float z, uint32_t rgb);

  void addQuad (uint8_t cull,
                float x, float y, float z,
                int wdt, int hgt, // quad size
                const uint32_t *colors);

public:
  inline VoxelMesh () noexcept : quads(), cx(0.0f), cy(0.0f), cz(0.0f), catlas() {}
  inline ~VoxelMesh () noexcept { clear(); }

  void clear () noexcept;

  // optimisation levels:
  //   0: one quad per one voxel face
  //   1: merge vertical slabs
  //   2: merge vertical, top and bottom slabs
  //   3: merge vertical, top and bottom slabs, each in 2 directions
  //   4: create optimal number of quads, merging all possible voxel faces
  void createFrom (VoxelData &vox, int optlevel);

  void buildOpt0 (VoxelData &vox);
  void buildOpt1 (VoxelData &vox);
  void buildOpt2 (VoxelData &vox);
  void buildOpt3 (VoxelData &vox);
  void buildOpt4 (VoxelData &vox);

public:
  static const uint8_t quadFaces[6][4];
};


// ////////////////////////////////////////////////////////////////////////// //
/*
  this builds the OpenGL data structures, ready to be uploaded to the GPU.
  it also can perform t-junction fixing. it is using quad data from `VoxelMesh`
  as a source, and creates triangle fans from those, calucluating the proper
  texture mapping coordinates.
 */
struct GLVoxelMesh {
  // WARNING! DO NOT CHANGE ANY OF THE PUBLIC FIELDS MANUALLY!
  TArray<VVoxVertexEx> vertices;
  TArray<uint32_t> indicies;
  uint32_t breakIndex;
  uint32_t totaladded;

  TArray<uint32_t> img;
  uint32_t imgWidth, imgHeight;

private:
  enum {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_Z = 2,
  };

private:
  TMapNC<VVoxVertexEx, uint32_t> vertcache;
  float vmin[3]; // minimum vertex coords
  float vmax[3]; // maximum vertex coords

private:
  struct AddedVert {
    uint32_t vidx;
    int next; // next vertex in `addedlist`
  };

  struct VoxEdge {
    uint32_t v0, v1; // start and end vertex
    float dir; // by the axis, not normalized
    float clo, chi; // low and high coords
    int morefirst; // first added vertex; linked by `next`
    uint8_t axis; // AXIS_n

    inline bool hasMore () const noexcept { return (morefirst >= 0); }
    inline bool noMore () const noexcept { return (morefirst < 0); }
  };

  TArray<VoxEdge> edges;
  TArray<AddedVert> addedlist;

  Vox3DBitmap gridbmp;
  int gridmin[3];
  int gridmax[3];

private:
  uint32_t appendVertex (const VVoxVertexEx &gv);

private:
  inline void gridCoords (float fx, float fy, float fz, int *gx, int *gy, int *gz) const noexcept {
    int vx = (int)fx;
    int vy = (int)fy;
    int vz = (int)fz;
    vassert(vx >= gridmin[0] && vy >= gridmin[1] && vz >= gridmin[2]);
    vassert(vx <= gridmax[0] && vy <= gridmax[1] && vz <= gridmax[2]);
    *gx = vx-gridmin[0];
    *gy = vy-gridmin[1];
    *gz = vz-gridmin[2];
  }

  inline void putVertexToGrid (uint32_t vidx) noexcept {
    int vx, vy, vz;
    gridCoords(vertices[vidx].x, vertices[vidx].y, vertices[vidx].z, &vx, &vy, &vz);
    gridbmp.setPixel(vx, vy, vz);
  }

  inline uint32_t hasVertexAt (float fx, float fy, float fz) const noexcept {
    int vx, vy, vz;
    gridCoords(fx, fy, fz, &vx, &vy, &vz);
    return gridbmp.getPixel(vx, vy, vz);
  }

  inline void putEdgeToGrid (uint32_t eidx) noexcept {
    VoxEdge &e = edges[eidx];
    putVertexToGrid(e.v0);
    putVertexToGrid(e.v1);
  }

  void freeSortStructs ();
  void createGrid ();
  void createEdges ();
  void sortEdges ();

  void fixEdgeWithVert (VoxEdge &edge, float crd);
  void fixEdgeNew (uint32_t eidx);
  void rebuildEdges ();

  // t-junction fixer entry point
  // this will also convert vertex data to triangle strips
  void fixTJunctions ();

public:
  GLVoxelMesh ()
    : breakIndex(65535)
    , totaladded(0)
    , imgWidth(0)
    , imgHeight(0)
  {
      clear();
  }

  void clear ();

  // count the number of triangles in triangle fan data
  // can be used for informational messages
  // slow!
  uint32_t countTris ();

  // main entry point
  // `tjfix` is "fix t-junctions" flag
  // `BreakIndex` is OpenGL "break index" (uniqie index that finishes triangle fan)
  // on exit, `vertices` contains a list of vertices, and
  // `indicies` contains a list of vertex indicies, ready to be rendered as triangle fans
  // fans are separated by `BreakIndex` value
  void create (VoxelMesh &vox, bool tjfix, uint32_t BreakIndex);

  // call this after `create()`, to get triangle soup
  typedef void (*NewTriCB) (uint32_t v0, uint32_t v1, uint32_t v2, void *udata);
  void createTriangles (NewTriCB cb, void *udata);
};


// ////////////////////////////////////////////////////////////////////////// //
// stream interface for loaders
//

struct VoxByteStream {
  // read bytes to buffer
  // `buf` is never `nullptr`, `len` is never `0`
  // should either read exactly `len` bytes and return `true`,
  // or return `false` on any error
  // note that the caller may ask to read more bytes than there is in the stream
  // in this case, return `false`
  bool (*readBuf) (void *buf, uint32_t len, VoxByteStream *strm);

  // seek to the given byte
  // guaranteed to always seek forward
  // should return `false` on error
  // note that the caller may ask to seek outside the stread
  // in this case, return `false`
  bool (*seek) (uint32_t ofs, VoxByteStream *strm);

  // get total size of the stream
  // nobody knows what to do on error; return 0 or something
  uint32_t (*totalSize) (VoxByteStream *strm);

  void *udata;
};


// memory stream reader, for your convenience
struct VoxMemByteStream {
  VoxByteStream strm;
  // `udata` is data pointer
  uint32_t dataSize;
  uint32_t currOfs;
};


// returns `mst`, but properly casted
// there is no need to deinit this stream
VoxByteStream *vox_InitMemoryStream (VoxMemByteStream *mst, const void *buf, uint32_t buflen);


// ////////////////////////////////////////////////////////////////////////// //
// loaders
//
// WARNING! passed `strm` must not be used before passing
//          (i.e. its position should be at the very first stream byte)
//

enum VoxFmt {
  VoxFmt_Unknown,
  VoxFmt_KV6,
  VoxFmt_Vxl,
};

// detect voxel file format by the first 4 file bytes
// KVX format has no signature, so it cannot be reliably detected
VoxFmt vox_detectFormat (const uint8_t bytes[4]);

// WARNING! loaders perform very little input file validation!

// load KVX model
// WARNING! does no format detection checks!
// `defpal` is the default palette in `r, g, b` triplets
// palette colors should be in [0..255] range
bool vox_loadKVX (VoxByteStream &strm, VoxelData &vox, const uint8_t defpal[768]);
bool vox_loadKV6 (VoxByteStream &strm, VoxelData &vox);
bool vox_loadVxl (VoxByteStream &strm, VoxelData &vox);
// raw voxel cube with dimensions
bool vox_loadVox (VoxByteStream &strm, VoxelData &vox, const uint8_t defpal[768]);


#endif
