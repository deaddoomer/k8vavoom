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
#ifndef VAVOOM_PSIM_TRACE_INTERNAL_HEADER
#define VAVOOM_PSIM_TRACE_INTERNAL_HEADER


// ////////////////////////////////////////////////////////////////////////// //
struct InterceptionList {
  VV_DISABLE_COPY(InterceptionList)

  intercept_t *list;
  unsigned alloted;
  unsigned used;

  inline InterceptionList () noexcept : list(nullptr), alloted(0), used(0) {}
  inline ~InterceptionList () noexcept { clear(); }

  inline void clear () noexcept {
    if (list) Z_Free(list);
    list = nullptr;
    alloted = used = 0;
  }

  inline void reset () noexcept { used = 0; }

  VVA_CHECKRESULT inline bool isEmpty () const noexcept { return (used == 0); }

  VVA_CHECKRESULT inline unsigned count () const noexcept { return used; }

  VVA_CHECKRESULT inline intercept_t *insert (const float frac) noexcept {
    if (used == alloted) {
      alloted += 1024;
      list = (intercept_t *)Z_Realloc(list, sizeof(list[0])*alloted);
    }

    intercept_t *res;
    unsigned ipos = used;
    if (ipos > 0) {
      while (ipos > 0 && frac < list[ipos-1].frac) --ipos;
      // here we should insert at `ipos` position
      if (ipos == used) {
        // as last
        res = &list[used++];
      } else {
        // make room
        memmove((void *)(list+ipos+1), (void *)(list+ipos), (used-ipos)*sizeof(list[0]));
        ++used;
        res = &list[ipos];
      }
    } else {
      res = &list[used++];
    }
    res->frac = frac;
    res->Flags = 0u;
    return res;
  }
};


#endif
