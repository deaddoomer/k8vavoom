//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2001 J�nis Legzdi��
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**	$Log$
//**	Revision 1.2  2001/07/27 14:27:54  dj_jl
//**	Update with Id-s and Log-s, some fixes
//**
//**************************************************************************

#ifndef __PROGDEFS_H
#define __PROGDEFS_H

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

#define PROG_MAGIC		"VPRG"
#define PROG_VERSION	8

// TYPES -------------------------------------------------------------------

enum
{
	OPC_DONE,
	OPC_RETURN,
	OPC_PUSHNUMBER,
	OPC_PUSHPOINTED,
    OPC_LOCALADDRESS,
    OPC_GLOBALADDRESS,
	OPC_ADD,
	OPC_SUBTRACT,
	OPC_MULTIPLY,
	OPC_DIVIDE,

	OPC_MODULUS,
	OPC_UDIVIDE,
	OPC_UMODULUS,
	OPC_EQ,
	OPC_NE,
	OPC_LT,
	OPC_GT,
	OPC_LE,
	OPC_GE,
	OPC_ULT,

	OPC_UGT,
	OPC_ULE,
	OPC_UGE,
	OPC_ANDLOGICAL,
	OPC_ORLOGICAL,
	OPC_NEGATELOGICAL,
	OPC_ANDBITWISE,
	OPC_ORBITWISE,
	OPC_XORBITWISE,
	OPC_LSHIFT,

	OPC_RSHIFT,
	OPC_URSHIFT,
	OPC_UNARYMINUS,
    OPC_BITINVERSE,
	OPC_CALL,
	OPC_GOTO,
	OPC_IFGOTO,
	OPC_IFNOTGOTO,
	OPC_CASEGOTO,
	OPC_DROP,

	OPC_ASSIGN,
	OPC_ADDVAR,
	OPC_SUBVAR,
	OPC_MULVAR,
	OPC_DIVVAR,
	OPC_MODVAR,
	OPC_UDIVVAR,
	OPC_UMODVAR,
    OPC_ANDVAR,
    OPC_ORVAR,

    OPC_XORVAR,
    OPC_LSHIFTVAR,
    OPC_RSHIFTVAR,
    OPC_URSHIFTVAR,
	OPC_PREINC,
	OPC_PREDEC,
    OPC_POSTINC,
    OPC_POSTDEC,
	OPC_IFTOPGOTO,
    OPC_IFNOTTOPGOTO,

	OPC_ASSIGN_DROP,
	OPC_ADDVAR_DROP,
	OPC_SUBVAR_DROP,
	OPC_MULVAR_DROP,
	OPC_DIVVAR_DROP,
	OPC_MODVAR_DROP,
	OPC_UDIVVAR_DROP,
	OPC_UMODVAR_DROP,
    OPC_ANDVAR_DROP,
    OPC_ORVAR_DROP,

    OPC_XORVAR_DROP,
    OPC_LSHIFTVAR_DROP,
    OPC_RSHIFTVAR_DROP,
    OPC_URSHIFTVAR_DROP,
	OPC_INC_DROP,
	OPC_DEC_DROP,
	OPC_FADD,
	OPC_FSUBTRACT,
	OPC_FMULTIPLY,
	OPC_FDIVIDE,

	OPC_FEQ,
	OPC_FNE,
	OPC_FLT,
	OPC_FGT,
	OPC_FLE,
	OPC_FGE,
	OPC_FUNARYMINUS,
	OPC_FADDVAR,
	OPC_FSUBVAR,
	OPC_FMULVAR,

	OPC_FDIVVAR,
	OPC_FADDVAR_DROP,
	OPC_FSUBVAR_DROP,
	OPC_FMULVAR_DROP,
	OPC_FDIVVAR_DROP,
	OPC_SWAP,
	OPC_ICALL,
	OPC_VPUSHPOINTED,
	OPC_VADD,
	OPC_VSUBTRACT,

	OPC_VPRESCALE,
	OPC_VPOSTSCALE,
	OPC_VISCALE,
	OPC_VEQ,
	OPC_VNE,
	OPC_VUNARYMINUS,
	OPC_VDROP,
	OPC_VASSIGN,
	OPC_VADDVAR,
	OPC_VSUBVAR,

	OPC_VSCALEVAR,
	OPC_VISCALEVAR,
	OPC_VASSIGN_DROP,
	OPC_VADDVAR_DROP,
	OPC_VSUBVAR_DROP,
	OPC_VSCALEVAR_DROP,
	OPC_VISCALEVAR_DROP,
	OPC_RETURNL,
	OPC_RETURNV,

	NUM_OPCODES
};

struct dprograms_t
{
	char	magic[4];		// "VPRG"
	int		version;

	int		ofs_strings;    //  pirm� virkne ir tuk�a
	int		num_strings;

	int		ofs_statements;
	int		num_statements;	//	Instrukcija 0 ir k��da

	int		ofs_globals;
	int		num_globals;

	int		ofs_functions;
	int		num_functions;	//  Funkcija 0 ir tuk�a
	
	int		ofs_globaldefs;
	int		num_globaldefs;

	int		pad1;	//  Rezerv�ts
    int		num_builtins;

	int		pad2;
	int		pad3;
};

struct dfunction_t
{
	int		s_name;
	int		first_statement;	//	Negat�vi skait�i ir ieb�v�t�s funkcijas
	short	num_parms;
	short	num_locals;
    int		type;
};

struct globaldef_t
{
	unsigned short	type;
	unsigned short	ofs;
	int				s_name;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------

#endif
