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
#ifdef BUILTIN_OPCODE_INFO
# ifndef DECLARE_OPC_BUILTIN
#  define BUILTIN_OPCODE_INFO_DEFAULT
#  define DECLARE_OPC_BUILTIN(name)  OPC_Builtin_##name
enum {
# endif
# include "vc_progdefs_builtins.h"
# undef DECLARE_OPC_BUILTIN
# undef BUILTIN_OPCODE_INFO
# ifdef BUILTIN_OPCODE_INFO_DEFAULT
#  undef BUILTIN_OPCODE_INFO_DEFAULT
};
# endif

#elif defined(DICTDISPATCH_OPCODE_INFO)
# ifndef DECLARE_OPC_DICTDISPATCH
#  define DICTDISPATCH_OPCODE_INFO_DEFAULT
#  define DECLARE_OPC_DICTDISPATCH(name)  OPC_DictDispatch_##name
enum {
# endif
# include "vc_progdefs_dictdispatch.h"
# undef DECLARE_OPC_DICTDISPATCH
# undef DICTDISPATCH_OPCODE_INFO
# ifdef DICTDISPATCH_OPCODE_INFO_DEFAULT
#  undef DICTDISPATCH_OPCODE_INFO_DEFAULT
};
# endif

#elif defined(DYNARRDISPATCH_OPCODE_INFO)
# ifndef DECLARE_OPC_DYNARRDISPATCH
#  define DYNARRDISPATCH_OPCODE_INFO_DEFAULT
#  define DECLARE_OPC_DYNARRDISPATCH(name)  OPC_DynArrDispatch_##name
enum {
# endif
# include "vc_progdefs_dynarrdispatch.h"
# undef DECLARE_OPC_DYNARRDISPATCH
# undef DYNARRDISPATCH_OPCODE_INFO
# ifdef DYNARRDISPATCH_OPCODE_INFO_DEFAULT
#  undef DYNARRDISPATCH_OPCODE_INFO_DEFAULT
};
# endif

#else

#ifndef OPCODE_INFO

enum {
  OPCARGS_None,
  OPCARGS_Member,
  OPCARGS_BranchTargetB,
  OPCARGS_BranchTargetNB,
  //OPCARGS_BranchTargetS,
  OPCARGS_BranchTarget,
  OPCARGS_ByteBranchTarget,
  OPCARGS_ShortBranchTarget,
  OPCARGS_IntBranchTarget,
  OPCARGS_NameBranchTarget,
  OPCARGS_Byte,
  OPCARGS_Short,
  OPCARGS_Int,
  OPCARGS_Name,
  OPCARGS_NameS,
  OPCARGS_String,
  OPCARGS_FieldOffset,
  OPCARGS_FieldOffsetS,
  OPCARGS_VTableIndex,
  OPCARGS_VTableIndexB,
  OPCARGS_TypeSize,
  OPCARGS_TypeSizeB,
  OPCARGS_VTableIndex_Byte,
  OPCARGS_VTableIndexB_Byte,
  OPCARGS_FieldOffset_Byte,
  OPCARGS_FieldOffsetS_Byte,
  OPCARGS_Type,
  OPCARGS_A2DDimsAndSize,
  OPCARGS_Builtin,
  OPCARGS_BuiltinCVar,
  // used for call, int is argc
  OPCARGS_Member_Int,
  // used for delegate call, int is argc, type is delegate type
  OPCARGS_Type_Int,
  // used for dynarray sorting, int is delegate argc
  OPCARGS_ArrElemType_Int,
  OPCARGS_TypeDD, // dictdispatch
  OPCARGS_TypeAD, // dynarrdispatch
};


enum {
#define DECLARE_OPC(name, args)   OPC_##name
#endif
# include "vc_progdefs_opcodes.h"
#undef DECLARE_OPC
#ifndef OPCODE_INFO
  NUM_OPCODES

  // virtual opcodes: they will be converted to real bytecode opcodes by the method emitter
  //DECLARE_OPC(DoWriteOne, Type),
  //DECLARE_OPC(DoWriteFlush, None),
};

static_assert(NUM_OPCODES < 256, "VavoomC: too many opcodes!");



#endif

#endif
