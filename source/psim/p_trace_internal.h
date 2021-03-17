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
//#include "../gamedefs.h"
//#include "../server/sv_local.h"


// ////////////////////////////////////////////////////////////////////////// //
struct PlaneHitInfo {
  TVec linestart;
  TVec lineend;
  bool bestIsSky;
  bool wasHit;
  float besthtime;

  inline PlaneHitInfo (const TVec &alinestart, const TVec &alineend) noexcept
    : linestart(alinestart)
    , lineend(alineend)
    , bestIsSky(false)
    , wasHit(false)
    , besthtime(9999.0f)
  {}

  inline TVec getPointAtTime (const float time) const noexcept __attribute__((always_inline)) {
    return linestart+(lineend-linestart)*time;
  }

  inline TVec getHitPoint () const noexcept __attribute__((always_inline)) {
    return linestart+(lineend-linestart)*(wasHit ? besthtime : 0.0f);
  }

  inline void update (const TSecPlaneRef &plane) noexcept {
    const float d1 = plane.PointDistance(linestart);
    if (d1 < 0.0f) return; // don't shoot back side

    const float d2 = plane.PointDistance(lineend);
    if (d2 >= 0.0f) return; // didn't hit plane

    // d1/(d1-d2) -- from start
    // d2/(d2-d1) -- from end

    const float time = d1/(d1-d2);
    if (time < 0.0f || time > 1.0f) return; // hit time is invalid

    if (!wasHit || time < besthtime) {
      bestIsSky = (plane.splane->pic == skyflatnum);
      besthtime = time;
    }

    wasHit = true;
  }

  inline void update (sec_plane_t &plane, bool flip=false) noexcept __attribute__((always_inline)) {
    TSecPlaneRef pp(&plane, flip);
    update(pp);
  }

  // this requires non-slope, and will clamp to minz/maxz
  inline void updatePObj (sec_plane_t &plane, const float minz, const float maxz) noexcept __attribute__((always_inline)) {
    if (plane.isSlope() || minz >= maxz) return;
    sec_plane_t pl = plane;
    if (pl.normal.z > 0.0f) {
      // this is a ceiling
      pl.dist = min2(pl.dist, maxz);
    } else {
      // this is a floor
      pl.dist = -min2(-pl.dist, minz);
    }
    TSecPlaneRef pp(&pl, false);
    update(pp);
  }
};
