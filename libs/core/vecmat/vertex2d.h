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

// this is useful for vertex hashtables
struct __attribute__((packed)) Vertex2DInfo {
private:
  vfloat xy[2]; // k8vavoom works with floats
  int index; // vertex index in various structures

public:
  inline Vertex2DInfo () noexcept { memset(xy, 0, sizeof(xy)); index = 0; }
  inline Vertex2DInfo (const TVec &v, const int aindex) noexcept {
    xy[0] = v.x;
    xy[1] = v.y;
    index = aindex;
    // this is to remove sign from zeroes, so any zero will hash to the same value
    if (xy[0] == 0) memset(&xy[0], 0, sizeof(xy[0]));
    if (xy[1] == 0) memset(&xy[1], 0, sizeof(xy[1]));
  }
  inline Vertex2DInfo (const float vx, const float vy, const int aindex) noexcept {
    xy[0] = vx;
    xy[1] = vy;
    index = aindex;
    // this is to remove sign from zeroes, so any zero will hash to the same value
    if (xy[0] == 0) memset(&xy[0], 0, sizeof(xy[0]));
    if (xy[1] == 0) memset(&xy[1], 0, sizeof(xy[1]));
  }

  VVA_FORCEINLINE void operator = (const Vertex2DInfo &other) noexcept { if (&other != this) memcpy((void *)this, (const void *)&other, sizeof(Vertex2DInfo)); }

  inline bool operator == (const Vertex2DInfo &vi) const noexcept { return (memcmp(&xy[0], &vi.xy[0], sizeof(xy)) == 0); }
  inline bool operator != (const Vertex2DInfo &vi) const noexcept { return (memcmp(&xy[0], &vi.xy[0], sizeof(xy)) != 0); }

  inline int getIndex () const noexcept { return index; }
  inline float getX () const noexcept { return xy[0]; }
  inline float getY () const noexcept { return xy[1]; }

  inline const void *getHashData () const noexcept { return (const void *)(&xy[0]); }
  inline size_t getHashDataSize () const noexcept { return sizeof(xy); }
};
static_assert(sizeof(Vertex2DInfo) == sizeof(float)*2+sizeof(int), "oops");

VVA_FORCEINLINE VVA_PURE uint32_t GetTypeHash (const Vertex2DInfo &vi) noexcept { return joaatHashBuf(vi.getHashData(), vi.getHashDataSize()); }


struct VVoxVertex {
  float x, y, z;
  float uu, vv; // will be calculated after texture creation
  //vuint16 tu, tv;
  vuint32 rgb;

  inline VVoxVertex () noexcept : x(0.0f), y(0.0f), z(0.0f), uu(0.0f), vv(0.0f), rgb(0) {}

  inline bool operator == (const VVoxVertex &b) const noexcept {
    return (x == b.x && y == b.y && z == b.z && rgb == b.rgb);
  }

  inline TVec asTVec () const noexcept { return TVec(x, y, z); }
};

VVA_FORCEINLINE VVA_PURE uint32_t GetTypeHash (const VVoxVertex &vi) noexcept {
  uint32_t hash = 0U;
  hash = joaatHashPart(hash, &vi.x, sizeof(vi.x));
  hash = joaatHashPart(hash, &vi.y, sizeof(vi.y));
  hash = joaatHashPart(hash, &vi.z, sizeof(vi.z));
  hash = joaatHashPart(hash, &vi.rgb, sizeof(vi.rgb));
  return joaatHashFinalize(hash);
}


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

  inline TVec asTVec () const noexcept { return TVec(x, y, z); }
  inline TVec normalAsTVec () const noexcept { return TVec(nx, ny, nz); }
};

VVA_FORCEINLINE VVA_PURE uint32_t GetTypeHash (const VVoxVertexEx &vi) noexcept {
  uint32_t hash = 0U;
  hash = joaatHashPart(hash, &vi.x, sizeof(vi.x));
  hash = joaatHashPart(hash, &vi.y, sizeof(vi.y));
  hash = joaatHashPart(hash, &vi.z, sizeof(vi.z));
  hash = joaatHashPart(hash, &vi.s, sizeof(vi.s));
  hash = joaatHashPart(hash, &vi.t, sizeof(vi.t));
  hash = joaatHashPart(hash, &vi.nx, sizeof(vi.nx));
  hash = joaatHashPart(hash, &vi.ny, sizeof(vi.ny));
  hash = joaatHashPart(hash, &vi.nz, sizeof(vi.nz));
  return joaatHashFinalize(hash);
}
