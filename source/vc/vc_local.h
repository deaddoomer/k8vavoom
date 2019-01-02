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
//**  Copyright (C) 2018-2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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
#ifndef VAVOOM_VAVOOMC_LOCAL_HEADER
#define VAVOOM_VAVOOMC_LOCAL_HEADER

#if !defined(IN_VCC) && !defined(VCC_STANDALONE_EXECUTOR)
# include "gamedefs.h"
# include "../progdefs.h"
# include "vc_emit_context.h"
# include "vc_expr_base.h"
# include "vc_expr_literal.h"
# include "vc_expr_unary_binary.h"
# include "vc_expr_cast.h"
# include "vc_expr_type.h"
# include "vc_expr_field.h"
# include "vc_expr_array.h"
# include "vc_expr_invoke.h"
# include "vc_expr_assign.h"
# include "vc_expr_local.h"
# include "vc_expr_misc.h"
# include "vc_statement.h"
# include "vc_error.h"
# include "vc_lexer.h"
# include "vc_modifiers.h"
# include "vc_parser.h"
#else
# if defined(IN_VCC)
#  include "../../utils/vcc/vcc.h"
# elif defined(VCC_STANDALONE_EXECUTOR)
#  include "../../vccrun/vcc_run.h"
# endif
#endif


#endif
