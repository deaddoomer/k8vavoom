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
//**
//**  Template for mapping kays to values.
//**
//**************************************************************************

//k8: don't even fuckin' ask me. shitplusplus is not a language,
//k8: it is a stinking pile of shit.

#define VV_SHITPP_FUCKED_TMAP_TRAIT
//#define VV_SHITPP_FUCKED_TMAP_TRAIT_DEBUG

#ifdef VV_SHITPP_FUCKED_TMAP_TRAIT
template <class T> struct HasTypeHash {
  typedef char fuckit[1];
  typedef fuckit cantfuckit[2];
  template <typename U, U u> struct fuckitfuck;
  // it must be const and noexcept
  template <typename C> static fuckit &assprobe (fuckitfuck<uint32_t (C::*)() const noexcept, &C::TypeHash>*) { /*static*/ fuckit f; return f; }
  template <typename> static cantfuckit &assprobe (...) { /*static*/ cantfuckit f; return f; }
  static constexpr bool value = (sizeof(assprobe<T>(0)) == sizeof(fuckit));
};

template<bool B, class T=void> struct fuck_enable_if {};
template<class T> struct fuck_enable_if<true, T> { typedef T type; };

#ifdef VV_SHITPP_FUCKED_TMAP_TRAIT_DEBUG
# include <cstdlib>
# include <cxxabi.h>
# define VV_SHITPP_PRINT_TPN(msg_)  do { \
    const char *tpn = typeid(TKT).name(); \
    char *dmn = abi::__cxa_demangle(tpn, nullptr, nullptr, nullptr); \
    if (dmn) { \
      printf("%s: tkt=%s\n", msg_, dmn); \
      ::free(dmn); \
    } else { \
      printf("%s: tkt=%s\n", msg_, tpn); \
    } \
  } while (0)
#endif
#endif


#define TMap_Class_Name  TMap
#include "map_impl.h"
#undef TMap_Class_Name

#define TMap_Class_Name  TMapNC
#define TMAP_NO_CLEAR
#include "map_impl.h"
#undef TMAP_NO_CLEAR
#undef TMap_Class_Name
