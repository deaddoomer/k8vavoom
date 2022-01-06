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

// based on intel's slice-by-8 idea
// conforms to RFC 3720 (section B.4.)
// start with 0; pass returned value for continuous calculation
VVA_PURE vuint32 crc32cBuffer (vuint32 crc, const void *data, size_t length) noexcept;


// ////////////////////////////////////////////////////////////////////////// //
class TCRC16 {
private:
  enum {
    CRC_INIT_VALUE = 0xffffU,
    CRC_XOR_VALUE  = 0x0000U,
  };

private:
  static const vuint32 crc16Table[256];
  vuint16 curr;

public:
  inline TCRC16 () noexcept : curr(CRC_INIT_VALUE) {}
  inline TCRC16 (const TCRC16 &o) noexcept : curr(o.curr) {}
  inline void Init () noexcept { curr = CRC_INIT_VALUE; }
  inline void Reset () noexcept { curr = CRC_INIT_VALUE; }
  inline void reset () noexcept { curr = CRC_INIT_VALUE; }
  inline TCRC16 &operator += (vuint8 v) noexcept { curr = (curr<<8)^crc16Table[(curr>>8)^v]; return *this; }
  inline TCRC16 &put (const void *buf, size_t len) noexcept {
    const vuint8 *b = (const vuint8 *)buf;
    while (len--) {
      curr = (curr<<8)^crc16Table[(curr>>8)^(*b++)];
    }
    return *this;
  }
  inline vuint16 Result () const noexcept { return curr^CRC_XOR_VALUE; }
  inline operator vuint16 () const noexcept { return Result(); }
};



// ////////////////////////////////////////////////////////////////////////// //
class TCRC32 {
private:
  static const vuint32 crc32Table[16];
  vuint32 curr;

public:
  inline TCRC32 () noexcept : curr(/*0xffffffffU*/0) {}
  inline TCRC32 (const TCRC32 &o) noexcept : curr(o.curr) {}
  inline void Init () noexcept { curr = 0; }
  inline void Reset () noexcept { curr = 0; }
  inline void reset () noexcept { curr = 0; }
  inline TCRC32 &operator += (vuint8 b) noexcept { curr ^= b; curr = crc32Table[curr&0x0f]^(curr>>4); curr = crc32Table[curr&0x0f]^(curr>>4); return *this; }
  inline TCRC32 &put (const void *buf, size_t len) noexcept {
    const vuint8 *b = (const vuint8 *)buf;
    while (len--) {
      curr ^= *b++;
      curr = crc32Table[curr&0x0f]^(curr>>4);
      curr = crc32Table[curr&0x0f]^(curr>>4);
    }
    return *this;
  }
  inline vuint32 Result () const noexcept { return curr^0xffffffffU; }
  inline operator vuint32 () const noexcept { return Result(); }
};
