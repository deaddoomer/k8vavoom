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
//**  Copyright (C) 2018-2020 Ketmar Dark
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

#include "build.h"    //  Build settings
#include "common.h"   //  Common types
#include "language.h" //  Localisation
#include "misc.h"   //  Misc utilites
#include "infostr.h"  //  Info strings
#include "debug.h"    //  Debug file
#include "system.h"   //  System specific routines
#include "filesys/files.h"    //  File I/O routines
#include "../libs/vavoomc/vc_public.h"
#include "vc_dehacked.h"//  DeHackEd support
#include "scripts.h"  //  Script parsing
#include "input.h"    //  Input from keyboard, mouse and joystick
#include "video.h"    //  Graphics
#include "screen.h"
#include "automap.h"
#include "p_gameobject.h"
#include "textures/r_tex_id.h"
#include "ntvalueioex.h"
#include "level.h"    //  Level data
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
#include "save.h"
#include "server.h"
#include "qs_data.h"
#include "p_clip.h"
#include "p_decal.h"
#include "p_worldinfo.h"
#include "p_thinker.h"
#include "p_levelinfo.h"
#include "p_entity.h"
#include "vc_decorate.h"//  Decorate scripts
#include "p_playerreplicationinfo.h"
#include "p_player.h"
#include "p_gameinfo.h"
#include "p_world.h"
#include "client.h"

#endif
