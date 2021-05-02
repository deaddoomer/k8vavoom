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
//**  common header file
//**************************************************************************
#ifndef GAMECOMMON_HEADER
#define GAMECOMMON_HEADER

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

#include "build.h"
#include "common.h"
#include "misc.h"
#include "system.h"
#include "../libs/vavoomc/vc_public.h"

/*
#include "scripts.h"
#include "language.h"
#include "infostr.h"
#include "filesys/files.h"
#include "dehacked/vc_dehacked.h"
#include "input.h"
#include "video.h"
#include "screen.h"
#include "automap.h"
#include "psim/p_gameobject.h"
#include "textures/r_tex_id.h"
#include "ntvalueioex.h"
#include "level/level.h"
#include "level/beamclip.h"
#include "mapinfo.h"
#include "lockdefs.h"
#include "host.h"
#include "textures/r_tex_public.h"
#include "render/r_public.h"
#include "text.h"
#include "sound/sound.h"
#include "menu.h"
#include "console.h"
#include "cmd.h"
#include "sbar.h"
#include "chat.h"
#include "finale.h"
#include "server/server.h"
#include "server/sv_save.h"
#include "qs_data.h"
#include "psim/p_decal.h"
#include "psim/p_worldinfo.h"
#include "psim/p_thinker.h"
#include "psim/p_levelinfo.h"
#include "psim/p_entity.h"
#include "psim/p_playerreplicationinfo.h"
#include "psim/p_player.h"
#include "psim/p_gameinfo.h"
#include "psim/p_world.h"
#include "decorate/vc_decorate.h"
#include "client/client.h"
*/

#endif
