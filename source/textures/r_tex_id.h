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
#ifndef VAVOOM_TEX_ID_HEADER
#define VAVOOM_TEX_ID_HEADER

#include "../../libs/core/core.h"
#include "../gamedefs_fwd.h"


// ////////////////////////////////////////////////////////////////////////// //
struct VTextureID {
public:
  vint32 id;

  inline VTextureID () noexcept : id (-1) {}
  inline VTextureID (const VTextureID &b) noexcept : id(b.id) {}
  // temp
  inline VTextureID (vint32 aid) noexcept : id(aid) {}

  inline void operator = (const VTextureID &b) noexcept { id = b.id; }
  inline operator vint32 () const noexcept { return id; }

  void Serialise (VStream &strm) const;
  void Serialise (VStream &strm);

private:
  void save (VStream &strm) const;
  void load (VStream &strm);
};

static_assert(sizeof(VTextureID) == sizeof(vint32), "invalid VTextureID size");

static inline VVA_OKUNUSED VStream &operator << (VStream &strm, const VTextureID &tid) { tid.Serialise(strm); return strm; }
static inline VVA_OKUNUSED VStream &operator << (VStream &strm, VTextureID &tid) { tid.Serialise(strm); return strm; }


#endif
