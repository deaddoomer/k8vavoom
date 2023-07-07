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
//**
//**  Main game header file.
//**
//**************************************************************************
#ifndef GAMEDEFS_HEADER
#define GAMEDEFS_HEADER

#ifdef CLIENT
# ifdef USE_GLAD
#  include "glad.h"
# else
#  include <GL/gl.h>
# endif
#else
# define GLuint  vuint32
#endif

#include "../libs/core/core.h"

#include "gamedefs_build.h"
#include "gamedefs_fwd.h"
#include "utils/misc.h"
#include "../libs/vavoomc/vc_public.h"

#include "console.h"
#include "cmd.h"

#include "utils/scripts.h"
#include "psim/p_gameobject.h"
#include "level/level.h"
#include "textures/r_tex_public.h"
#include "psim/p_gameinfo.h"

#endif
