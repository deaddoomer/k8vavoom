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
//**	Copyright (C) 1999-2002 J�nis Legzdi��
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
//**************************************************************************

#ifndef __VCC_H__
#define __VCC_H__

// HEADER FILES ------------------------------------------------------------

#include "cmdlib.h"
#include "../../source/progdefs.h"

using namespace VavoomUtils;

#include "array.h"
#include "names.h"
#include "name.h"

// MACROS ------------------------------------------------------------------

//	Number of elements in an array.
#define ARRAY_COUNT(array)				((int)(sizeof(array) / sizeof((array)[0])))

//	Offset of a struct member.
#define STRUCT_OFFSET(struc, member)	((int)&((struc *)NULL)->member)

#define MAX_FILE_NAME_LENGTH	512
#define MAX_QUOTED_LENGTH		256
#define MAX_IDENTIFIER_LENGTH	64
#define	MAX_PARAMS				8
#define MAX_LOCAL_DEFS			64
#define	MAX_FUNCTIONS			8192
#define MAX_CONSTANTS			10000

#define PF_VARARGS				0x8000
#define PF_COUNT_MASK			0x7fff

#define REF_CPP		// C++ style references

#define VTABLE_OFFS				4

// TYPES -------------------------------------------------------------------

enum
{
	ev_void,
	ev_int,
	ev_float,
	ev_name,
	ev_string,
	ev_function,
	ev_pointer,
	ev_reference,
	ev_array,
	ev_struct,
	ev_vector,
	ev_class,
	ev_method,
	ev_classid,

	NUM_BASIC_TYPES
};

enum error_t
{
	ERR_NONE,
	//  Atmi�as k��das
	ERR_TOO_MANY_STRINGS,
	ERR_BREAK_OVERFLOW,
	ERR_CONTINUE_OVERFLOW,
	ERR_CASE_OVERFLOW,
	ERR_PARAMS_OVERFLOW,
    ERR_LOCALS_OVERFLOW,
	ERR_STATEMENTS_OVERFLOW,
	//  Failu k��das
    ERR_CANT_OPEN_FILE,
	ERR_CANT_OPEN_DBGFILE,
	//  Sadal��anas k��das
	ERR_INCL_NESTING_TOO_DEEP,
	ERR_BAD_RADIX_CONSTANT,
	ERR_STRING_TOO_LONG,
	ERR_EOF_IN_STRING,
    ERR_NEW_LINE_INSIDE_QUOTE,
	ERR_UNKNOWN_ESC_CHAR,
	ERR_IDENTIFIER_TOO_LONG,
	ERR_BAD_CHARACTER,
	ERR_UNTERM_COMMENT,
	ERR_TOO_MENY_CONSTANTS,
	//  Sintakses k��das
	ERR_BAD_CONST_EXPR,
	ERR_BAD_EXPR,
	ERR_MISSING_LPAREN,
	ERR_MISSING_RPAREN,
	ERR_MISSING_LBRACE,
	ERR_MISSING_RBRACE,
	ERR_MISSING_COLON,
	ERR_MISSING_SEMICOLON,
	ERR_BAD_ARG_COUNT,
    ERR_VOID_VALUE,
    ERR_PARAM_TYPE,
	ERR_ILLEGAL_EXPR_IDENT,
	ERR_BAD_ASSIGNEMENT,
	ERR_MISPLACED_BREAK,
	ERR_MISPLACED_CONTINUE,
	ERR_UNEXPECTED_EOF,
	ERR_BAD_DO_STATEMENT,
	ERR_NO_RET_VALUE,
	ERR_VOID_RET,
	ERR_MULTIPLE_DEFAULT,
	ERR_INVALID_STATEMENT,
	ERR_INVALID_IDENTIFIER,
	ERR_VOID_VAR,
	ERR_REDEFINED_IDENTIFIER,
    ERR_TYPE_MISTMATCH,
	ERR_BAD_VAR_TYPE,
	ERR_FUNCTION_REDECLARED,
	ERR_INVALID_DECLARATOR,
	ERR_INVALID_DIRECTIVE,
	ERR_STRING_LIT_NOT_FOUND,
	ERR_UNDEFINED_FUNCTIONS,
	ERR_BAD_INCDEC,
	ERR_END_OF_NON_VOID_FUNCTION,
    ERR_NOT_A_STRUCT,
    ERR_NOT_A_FIELD,
	ERR_MISSING_RFIGURESCOPE,
	ERR_BAD_ARRAY,
    ERR_EXPR_TYPE_MISTMATCH,
    ERR_POINTER_TO_POINTER,

    NUM_ERRORS
};

enum tokenType_t
{
	TK_NONE,
	TK_EOF,				//	Sasniegtas faila biegas
	TK_IDENTIFIER, 		//	Identifik�tors, v�rt�ba: tk_String
	TK_PUNCT, 			//	speci�lais simbols, v�rt�ba: tk_String
	TK_NAME,
	TK_KEYWORD,			//	atsl�gv�rds, v�rt�ba: tk_String
	TK_STRING,			//	simbolu virkne, v�rt�ba: tk_String
	TK_INTEGER,			//	vesels skaitlis, v�rt�ba: tk_Number
	TK_FLOAT,			//	peldo�� komata skaitlis, v�rt�ba: tk_Float
};

enum Keyword
{
	KW_STATES = 1,
	KW_MOBJINFO,
	KW_ADDFIELDS,
	KW_BREAK,
	KW_CASE,
	KW_CLASS,
	KW_CLASSID,
	KW_CONTINUE,
	KW_DEFAULT,
	KW_DEFAULTPROPERTIES,
	KW_DO,
	KW_ELSE,
	KW_ENUM,
	KW_FLOAT,
	KW_FOR,
	KW_FUNCTION,
	KW_IF,
   	KW_INT,
	KW_NAME,
	KW_NATIVE,
	KW_NONE,
	KW_NULL,
	KW_RETURN,
	KW_SELF,
	KW_STRING,
	KW_STRUCT,
	KW_SWITCH,
	KW_THIS,
	KW_TYPEDEF,
	KW_VECTOR,
   	KW_VOID,
	KW_WHILE,
};

enum Punctuation
{
	PU_VARARGS = 1,
	PU_LSHIFT_ASSIGN,
	PU_RSHIFT_ASSIGN,
	PU_ADD_ASSIGN,
	PU_MINUS_ASSIGN,
	PU_MULTIPLY_ASSIGN,
	PU_DIVIDE_ASSIGN,
	PU_MOD_ASSIGN,
	PU_AND_ASSIGN,
	PU_OR_ASSIGN,
	PU_XOR_ASSIGN,
	PU_EQ,
	PU_NE,
	PU_LE,
	PU_GE,
	PU_AND_LOG,
	PU_OR_LOG,
	PU_LSHIFT,
	PU_RSHIFT,
	PU_INC,
	PU_DEC,
	PU_MINUS_GT,
	PU_DCOLON,
	PU_LT,
	PU_GT,
	PU_QUEST,
	PU_AND,
	PU_OR,
	PU_XOR,
	PU_TILDE,
	PU_NOT,
	PU_PLUS,
	PU_MINUS,
	PU_ASTERISK,
	PU_SLASH,
	PU_PERCENT,
	PU_LPAREN,
	PU_RPAREN,
	PU_DOT,
	PU_COMMA,
	PU_SEMICOLON,
	PU_COLON,
	PU_ASSIGN,
	PU_LINDEX,
	PU_RINDEX,
	PU_LBRACE,
	PU_RBRACE,
};

class TType;

struct field_t
{
	FName		Name;
	int			ofs;
	TType		*type;
	int			func_num;	// Method's function
};

class TType
{
public:
	TType(void) {}
	TType(int Atype, TType *Anext, TType *Aaux_type, int Asize) :
		type(Atype), next(Anext), aux_type(Aaux_type), size(Asize)
	{ }

	FName		Name;
	int			type;
	TType		*next;
	TType		*aux_type;
	int			size;

	//	Structure fields
	int			numfields;
	field_t		*fields;
	//	Addfield info
	int			available_size;
	int			available_ofs;

	//  Class stuff
	int			classid;
	int			num_methods;
	int			vtable;

	//	Function params
	int			num_params;
	int			params_size;
	TType		*param_types[MAX_PARAMS];
};

class TFunction
{
public:
	FName		Name;
	TType		*OuterClass;
	int			first_statement;	//	Negat�vi skait�i ir ieb�v�t�s funkcijas
	int			num_locals;
	int			flags;
	TType		*type;
};

class TGlobalDef
{
public:
	FName		Name;
	int			ofs;
	TType		*type;
};

struct localvardef_t
{
	FName		Name;
	int			ofs;
	TType		*type;
};

struct constant_t
{
	FName		Name;
	int			value;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// -- Common --

void ERR_Exit(error_t error, boolean info, char *text, ...) __attribute__((noreturn));
void ParseError(error_t error);
void ParseError(error_t error, const char *text, ...) __attribute__ ((format(printf, 2, 3)));
void ParseError(const char *text, ...) __attribute__ ((format(printf, 1, 2)));
void ParseWarning(const char *text, ...) __attribute__ ((format(printf, 1, 2)));
void BailOut(void) __attribute__((noreturn));
void ERR_RemoveErrorFile(void);

int dprintf(const char *text, ...);

void TK_Init(void);
void TK_OpenSource(void *buf, size_t size);
void TK_Restart(void);
void TK_CloseSource(void);
void TK_NextToken(void);
bool TK_Check(const char *string);
void TK_Expect(const char *string, error_t error);
void TK_Expect(Keyword kwd, error_t error);
void TK_Expect(Punctuation punct, error_t error);

void PC_Init(void);
int FindString(const char *str);
int *AddStatement(int statement);
int *AddStatement(int statement, int parm1);
int *AddStatement(int statement, int parm1, int parm2);
int UndoStatement(void);
void PC_WriteObject(char *name);
void PC_DumpAsm(char* name);

int EvalConstExpression(int type);
float ConstFloatExpression(void);

void PA_Parse(void);
void ParseLocalVar(TType *type);
void ParseMethodDef(TType *t, field_t *fi, field_t *otherfield,
	TType *class_type);
int ParseStateCode(TType *class_type);
void ParseDefaultProperties(field_t *method, TType *class_type);

int CheckForFunction(TType *InClass, FName Name);
int CheckForGlobalVar(FName Name);
int CheckForLocalVar(FName Name);
int CheckForConstant(FName Name);
void AddConstant(FName Name, int value);

void InitTypes(void);
TType *CheckForType(void);
TType *CheckForType(FName Name);
TType *FindType(TType *type);
TType *MakePointerType(TType *type);
TType *MakeReferenceType(TType *type);
TType *MakeArrayType(TType *type, int elcount);
int TypeSize(TType *t);
void TypeCheckPassable(TType *type);
void TypeCheck1(TType *t);
void TypeCheck2(TType *t);
void TypeCheck3(TType *t1, TType *t2);
void ParseStruct(void);
void ParseClass(void);
void AddFields(void);
void ParseVector(void);
field_t* ParseField(TType *t);
field_t* CheckForField(TType *, bool = true);
field_t* CheckForField(FName, TType *, bool = true);
field_t* FindConstructor(TType *t);
void ParseTypeDef(void);
void AddVirtualTables(void);

void InitInfoTables(void);
void ParseStates(TType *class_type);
void ParseMobjInfo(void);
void AddInfoTables(void);

TType *ParseExpression(bool = false);

// PUBLIC DATA DECLARATIONS ------------------------------------------------

extern bool				ClassAddfields;
extern char				tk_SourceName[MAX_FILE_NAME_LENGTH];
extern int				tk_IncludedLines;
extern tokenType_t		tk_Token;
extern int				tk_Line;
extern int				tk_Number;
extern float			tk_Float;
extern char*			tk_String;
extern int				tk_StringI;
extern Keyword			tk_Keyword;
extern Punctuation		tk_Punct;
extern FName			tk_Name;

extern int*				CodeBuffer;
extern int				CodeBufferSize;

extern int*				globals;
extern byte*			globalinfo;
extern int				numglobals;

extern TGlobalDef*		globaldefs;
extern int				numglobaldefs;

extern TFunction*		functions;
extern int				numfunctions;
extern int				numbuiltins;

extern constant_t		Constants[MAX_CONSTANTS];
extern int				numconstants;

extern TType			*ThisType;
extern TType			*SelfType;

extern char*    		strings;
extern int              strofs;

extern int				NumErrors;

extern localvardef_t	localdefs[MAX_LOCAL_DEFS];

extern TType			type_void;
extern TType			type_int;
extern TType			type_float;
extern TType			type_name;
extern TType			type_string;
extern TType			type_function;
extern TType			type_state;
extern TType			type_mobjinfo;
extern TType			type_void_ptr;
extern TType			type_vector;
extern TType			type_classid;
extern TType			type_class;
extern TType			type_none_ref;

extern int				numclasses;

extern TType			**classtypes;

extern int				CurrentPass;

// INLINE CODE -------------------------------------------------------------

//==========================================================================
//
//	PassFloat
//
//==========================================================================

inline int PassFloat(float f)
{
	union
    {
    	float	f;
        int		i;
	} v;

    v.f = f;
    return v.i;
}

inline bool TK_Check(int s_string)
{
	if (tk_Token == TK_IDENTIFIER && tk_StringI == s_string)
	{
		TK_NextToken();
		return true;
	}
	return false;
}

inline bool TK_Check(FName Name)
{
	if (tk_Token == TK_IDENTIFIER && tk_Name == Name)
	{
		TK_NextToken();
		return true;
	}
	return false;
}

inline bool TK_Check(Keyword kwd)
{
	if (tk_Token == TK_KEYWORD && tk_Keyword == kwd)
	{
		TK_NextToken();
		return true;
	}
	return false;
}

inline bool TK_Check(Punctuation punct)
{
	if (tk_Token == TK_PUNCT && tk_Punct == punct)
	{
		TK_NextToken();
		return true;
	}
	return false;
}

#endif

//**************************************************************************
//
//	$Log$
//	Revision 1.18  2002/01/12 18:06:34  dj_jl
//	New style of state functions, some other changes
//
//	Revision 1.17  2002/01/11 08:17:31  dj_jl
//	Added name subsystem, removed support for unsigned ints
//	
//	Revision 1.16  2002/01/07 12:31:36  dj_jl
//	Changed copyright year
//	
//	Revision 1.15  2001/12/27 17:44:02  dj_jl
//	Removed support for C++ style constructors and destructors, some fixes
//	
//	Revision 1.14  2001/12/18 19:09:41  dj_jl
//	Some extra info in progs and other small changes
//	
//	Revision 1.13  2001/12/12 19:22:22  dj_jl
//	Support for method usage as state functions, dynamic cast
//	Added dynamic arrays
//	
//	Revision 1.12  2001/12/03 19:25:44  dj_jl
//	Fixed calling of parent function
//	Added defaultproperties
//	Fixed vectors as arguments to methods
//	
//	Revision 1.11  2001/12/01 18:17:09  dj_jl
//	Fixed calling of parent method, speedup
//	
//	Revision 1.10  2001/11/09 14:42:29  dj_jl
//	References, beautification
//	
//	Revision 1.9  2001/10/27 07:54:59  dj_jl
//	Added support for constructors and destructors
//	
//	Revision 1.8  2001/10/09 17:31:55  dj_jl
//	Addfields to class disabled by default
//	
//	Revision 1.7  2001/10/02 17:44:52  dj_jl
//	Some optimizations
//	
//	Revision 1.6  2001/09/24 17:31:38  dj_jl
//	Some fixes
//	
//	Revision 1.5  2001/09/20 16:09:55  dj_jl
//	Added basic object-oriented support
//	
//	Revision 1.4  2001/09/12 17:29:54  dj_jl
//	VavoomUtils namespace usage
//	
//	Revision 1.3  2001/08/21 17:52:54  dj_jl
//	Added support for real string pointers, beautification
//	
//	Revision 1.2  2001/07/27 14:27:56  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
