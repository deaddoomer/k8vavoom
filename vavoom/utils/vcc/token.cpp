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

// HEADER FILES ------------------------------------------------------------

#include "vcc.h"

// MACROS ------------------------------------------------------------------

#define EOF_CHARACTER		127
#define NON_HEX_DIGIT		255

// TYPES -------------------------------------------------------------------

enum chr_t
{
	CHR_EOF,
	CHR_LETTER,
	CHR_NUMBER,
	CHR_QUOTE,
	CHR_SINGLE_QUOTE,
	CHR_SPECIAL
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void NextChr();

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char				tk_SourceName[MAX_FILE_NAME_LENGTH];
int 				tk_Line;

ETokenType	 		tk_Token;
char*				tk_String;
int					tk_StringI;
int 				tk_Number;
float				tk_Float;
EKeyword			tk_Keyword;
EPunctuation		tk_Punct;
VName				tk_Name;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static char			ASCIIToChrCode[256];
static byte			ASCIIToHexDigit[256];
static char			TokenStringBuffer[MAX_QUOTED_LENGTH];
static bool			IncLineNumber;
static bool			NewLine;
static bool			SourceOpen;
static char*		FileStart;
static char*		FilePtr;
static char*		FileEnd;
static char			Chr;

static char* Keywords[] =
{
	"",
	"__states__",
	"__mobjinfo__",
	"__scriptid__",
	"abstract",
	"addfields",
	"bool",
	"break",
	"case",
	"class",
	"classid",
	"const",
	"continue",
	"default",
	"defaultproperties",
	"delegate",
	"do",
	"else",
	"enum",
	"false",
	"float",
	"for",
	"if",
	"int",
	"name",
	"native",
	"none",
	"NULL",
	"private",
	"readonly",
	"return",
	"self",
	"state",
	"static",
	"string",
	"struct",
	"switch",
	"transient",
	"true",
	"vector",
	"void",
	"while",
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// TK_Init
//
//==========================================================================

void TK_Init()
{
	int i;

	for (i = 0; i < 256; i++)
	{
		ASCIIToChrCode[i] = CHR_SPECIAL;
		ASCIIToHexDigit[i] = NON_HEX_DIGIT;
	}
	for (i = '0'; i <= '9'; i++)
	{
		ASCIIToChrCode[i] = CHR_NUMBER;
		ASCIIToHexDigit[i] = i-'0';
	}
	for (i = 'A'; i <= 'F'; i++)
	{
		ASCIIToHexDigit[i] = 10+(i-'A');
	}
	for (i = 'a'; i <= 'f'; i++)
	{
		ASCIIToHexDigit[i] = 10+(i-'a');
	}
	for (i = 'A'; i <= 'Z'; i++)
	{
		ASCIIToChrCode[i] = CHR_LETTER;
	}
	for (i = 'a'; i <= 'z'; i++)
	{
		ASCIIToChrCode[i] = CHR_LETTER;
	}
	ASCIIToChrCode[(int)'\"'] = CHR_QUOTE;
	ASCIIToChrCode[(int)'\''] = CHR_SINGLE_QUOTE;
	ASCIIToChrCode[(int)'_'] = CHR_LETTER;
	ASCIIToChrCode[0] = CHR_EOF;
	ASCIIToChrCode[EOF_CHARACTER] = CHR_EOF;
	tk_String = TokenStringBuffer;
	IncLineNumber = false;
	SourceOpen = false;
}

//==========================================================================
//
// TK_OpenSource
//
//==========================================================================

void TK_OpenSource(void *buf, size_t size)
{
	//	Actually we don't need to close it.
	TK_CloseSource();

	//	Read file and prepare for compilation.
	SourceOpen = true;
	FileStart = (char *)buf;
	FileEnd = FileStart + size;
	FilePtr = FileStart;
	tk_Line = 1;
	tk_Token = TK_NONE;
	NewLine = true;
	NextChr();
}

//==========================================================================
//
//	TK_Restart
//
//==========================================================================

void TK_Restart()
{
	FilePtr = FileStart;
	tk_Line = 1;
	tk_Token = TK_NONE;
	NewLine = true;
	NextChr();
}

//==========================================================================
//
// TK_CloseSource
//
//==========================================================================

void TK_CloseSource()
{
	if (SourceOpen)
	{
		Free(FileStart);
		SourceOpen = false;
	}
}

//==========================================================================
//
// NextChr
//
//==========================================================================

static void NextChr()
{
	if (FilePtr >= FileEnd)
	{
		Chr = EOF_CHARACTER;
		return;
	}
	if (IncLineNumber)
	{
		tk_Line++;
		IncLineNumber = false;
	}
	Chr = *FilePtr++;
	if ((byte)Chr < ' ')
	{
		if (Chr == '\n')
		{
			IncLineNumber = true;
			NewLine = true;
		}
		Chr = ' ';
	}
}

//==========================================================================
//
// ProcessNumberToken
//
//==========================================================================

static void ProcessNumberToken()
{
	char c;

	tk_Token = TK_INTEGER;
	c = Chr;
	NextChr();
	tk_Number = c - '0';
	if (c == '0' && (Chr == 'x' || Chr == 'X'))
	{
		//  Heksadecim�la konstante
		NextChr();
		while (ASCIIToHexDigit[(byte)Chr] != NON_HEX_DIGIT)
		{
			tk_Number = (tk_Number << 4) + ASCIIToHexDigit[(byte)Chr];
			NextChr();
		}
		return;
	}
	while (ASCIIToChrCode[(byte)Chr] == CHR_NUMBER)
	{
		tk_Number = 10 * tk_Number + (Chr - '0');
		NextChr();
	}
	if (Chr == '.')
	{
		tk_Token = TK_FLOAT;
		NextChr(); // Point
		tk_Float = tk_Number;
		float	fmul = 0.1;
		while (ASCIIToChrCode[(byte)Chr] == CHR_NUMBER)
		{
			tk_Float += (Chr - '0') * fmul;
			fmul /= 10.0;
			NextChr();
		}
		return;
	}
	if (Chr == '_')
	{
		int radix;
		int digit;

		NextChr(); // Underscore
		radix = tk_Number;
		if (radix < 2 || radix > 36)
		{
			ERR_Exit(ERR_BAD_RADIX_CONSTANT, true, NULL);
		}
		tk_Number = 0;
		do
		{
			digit = toupper(Chr);
			if (digit < '0' || (digit > '9' && digit < 'A') || digit > 'Z')
			{
				digit = -1;
			}
			else if(digit > '9')
			{
				digit = 10 + digit - 'A';
			}
			else
			{
				digit -= '0';
			}
			if (digit >= radix)
			{
				digit = -1;
			}
			if (digit != -1)
			{
				tk_Number = radix * tk_Number + digit;
				NextChr();
			}
		} while (digit != -1);
	}
}

//==========================================================================
//
//	ProcessChar
//
//==========================================================================

static void ProcessChar()
{
	if (Chr == EOF_CHARACTER)
	{
		ERR_Exit(ERR_EOF_IN_STRING, true, NULL);
	}
	if (IncLineNumber)
	{
		ERR_Exit(ERR_NEW_LINE_INSIDE_QUOTE, true, NULL);
	}
	if (Chr == '\\')
	{
		//	Special symbol
		NextChr();
		if (Chr == EOF_CHARACTER)
		{
			ERR_Exit(ERR_EOF_IN_STRING, true, NULL);
		}
		if (IncLineNumber)
	   	{
			ERR_Exit(ERR_NEW_LINE_INSIDE_QUOTE, true, NULL);
		}
		if (Chr == 'n')
			Chr = '\n';
		else if (Chr == '\'')
			Chr = '\'';
		else if (Chr == '"')
			Chr = '"';
		else if (Chr == 't')
			Chr = '\t';
		else if (Chr == '\\')
			Chr = '\\';
		else
			ERR_Exit(ERR_UNKNOWN_ESC_CHAR, true, NULL);
	}
}

//==========================================================================
//
// ProcessQuoteToken
//
//==========================================================================

static void ProcessQuoteToken()
{
	int len;

	tk_Token = TK_STRING;
	len = 0;
	NextChr();
	while (Chr != '\"')
	{
		if (len >= MAX_QUOTED_LENGTH - 1)
		{
			ERR_Exit(ERR_STRING_TOO_LONG, true, NULL);
		}
		ProcessChar();
		TokenStringBuffer[len] = Chr;
		NextChr();
		len++;
	}
	TokenStringBuffer[len] = 0;
	NextChr();
	tk_StringI = FindString(tk_String);
}

//==========================================================================
//
// ProcessSingleQuoteToken
//
//==========================================================================

static void ProcessSingleQuoteToken()
{
	int len;

	tk_Token = TK_NAME;
	len = 0;
	NextChr();
	while (Chr != '\'')
	{
		if (len >= MAX_IDENTIFIER_LENGTH - 1)
		{
			ERR_Exit(ERR_STRING_TOO_LONG, true, NULL);
		}
		ProcessChar();
		TokenStringBuffer[len] = Chr;
		NextChr();
		len++;
	}
	TokenStringBuffer[len] = 0;
	NextChr();
	tk_Name = TokenStringBuffer;
}

//==========================================================================
//
// ProcessLetterToken
//
//==========================================================================

static void ProcessLetterToken()
{
	int		len;

	tk_Token = TK_IDENTIFIER;
	len = 0;
	while (ASCIIToChrCode[(byte)Chr] == CHR_LETTER
		|| ASCIIToChrCode[(byte)Chr] == CHR_NUMBER)
	{
		if (len == MAX_IDENTIFIER_LENGTH - 1)
		{
			ERR_Exit(ERR_IDENTIFIER_TOO_LONG, true, NULL);
		}
		TokenStringBuffer[len] = Chr;
		len++;
		NextChr();
	}
	TokenStringBuffer[len] = 0;

	register const char* s = tk_String;
	switch (s[0])
	{
	case '_':
		if (s[1] == '_')
		{
			if (s[2] == 'm' && s[3] == 'o' && s[4] == 'b' && s[5] == 'j' &&
				s[6] == 'i' && s[7] == 'n' && s[8] == 'f' && s[9] == 'o' &&
				s[10] == '_' && s[11] == '_' && s[12] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_MOBJINFO;
			}
			else if (s[2] == 's')
			{
				if (s[3] == 't' && s[4] == 'a' && s[5] == 't' && s[6] == 'e' &&
					s[7] == 's' && s[8] == '_' && s[9] == '_' && s[10] == 0)
				{
					tk_Token = TK_KEYWORD;
					tk_Keyword = KW_STATES;
				}
				else if (s[3] == 'c' && s[4] == 'r' && s[5] == 'i' &&
					s[6] == 'p' && s[7] == 't' && s[8] == 'i' && s[9] == 'd' &&
					s[10] == '_' && s[11] == '_' && s[12] == 0)
				{
					tk_Token = TK_KEYWORD;
					tk_Keyword = KW_SCRIPTID;
				}
			}
		}
		break;

	case 'a':
		if (s[1] == 'b' && s[2] == 's' && s[3] == 't' && s[4] == 'r' &&
			s[5] == 'a' && s[6] == 'c' && s[7] == 't' && s[8] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_ABSTRACT;
		}
		else if (s[1] == 'd' && s[2] == 'd' && s[3] == 'f' && s[4] == 'i' &&
			s[5] == 'e' && s[6] == 'l' && s[7] == 'd' && s[8] == 's' &&
			s[9] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_ADDFIELDS;
		}
		break;

	case 'b':
		if (s[1] == 'o' && s[2] == 'o' && s[3] == 'l' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_BOOL;
		}
		else if (s[1] == 'r' && s[2] == 'e' && s[3] == 'a' && s[4] == 'k' &&
			s[5] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_BREAK;
		}
		break;

	case 'c':
		if (s[1] == 'a' && s[2] == 's' && s[3] == 'e' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_CASE;
		}
		else if (s[1] == 'l' && s[2] == 'a' && s[3] == 's' && s[4] == 's')
		{
			if (s[5] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_CLASS;
			}
			else if (s[5] == 'i' && s[6] == 'd' && s[7] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_CLASSID;
			}
		}
		else if (s[1] == 'o' && s[2] == 'n')
		{
			if (s[3] == 's' && s[4] == 't' && s[5] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_CONST;
			}
			else if (s[3] == 't' && s[4] == 'i' && s[5] == 'n' &&
				s[6] == 'u' && s[7] == 'e' && s[8] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_CONTINUE;
			}
		}
		break;

	case 'd':
		if (s[1] == 'e')
		{
			if (s[2] == 'f' && s[3] == 'a' && s[4] == 'u' && s[5] == 'l' &&
				s[6] == 't')
			{
				if (s[7] == 0)
				{
					tk_Token = TK_KEYWORD;
					tk_Keyword = KW_DEFAULT;
				}
				else if (s[7] == 'p' && s[8] == 'r' && s[9] == 'o' &&
					s[10] == 'p' && s[11] == 'e' && s[12] == 'r' &&
					s[13] == 't' && s[14] == 'i' && s[15] == 'e' &&
					s[16] == 's' && s[17] == 0)
				{
					tk_Token = TK_KEYWORD;
					tk_Keyword = KW_DEFAULTPROPERTIES;
				}
			}
			else if (s[2] == 'l' && s[3] == 'e' && s[4] == 'g' &&
				s[5] == 'a' && s[6] == 't' && s[7] == 'e' && s[8] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_DELEGATE;
			}
		}
		else if (s[1] == 'o' && s[2] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_DO;
		}
		break;

	case 'e':
		if (s[1] == 'l' && s[2] == 's' && s[3] == 'e' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_ELSE;
		}
		else if (s[1] == 'n' && s[2] == 'u' && s[3] == 'm' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_ENUM;
		}
		break;

	case 'f':
		if (s[1] == 'a' && s[2] == 'l' && s[3] == 's' && s[4] == 'e' &&
			s[5] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_FALSE;
		}
		else if (s[1] == 'l' && s[2] == 'o' && s[3] == 'a' && s[4] == 't' &&
			s[5] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_FLOAT;
		}
		else if (s[1] == 'o' && s[2] == 'r' && s[3] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_FOR;
		}
		break;

	case 'i':
		if (s[1] == 'f' && s[2] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_IF;
		}
		else if (s[1] == 'n' && s[2] == 't' && s[3] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_INT;
		}
		break;

	case 'n':
		if (s[1] == 'a' && s[2] == 'm' && s[3] == 'e' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_NAME;
		}
		else if (s[1] == 'o' && s[2] == 'n' && s[3] == 'e' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_NONE;
		}
		else if (s[1] == 'a' && s[2] == 't' && s[3] == 'i' && s[4] == 'v' &&
			s[5] == 'e' && s[6] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_NATIVE;
		}
		break;

/*	case 'N':
		if (s[1] == 'U' && s[2] == 'L' &&
			s[3] == 'L' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_NULL;
		}
		break;*/

	case 'p':
		if (s[1] == 'r' && s[2] == 'i' && s[3] == 'v' && s[4] == 'a' &&
			s[5] == 't' && s[6] == 'e' && s[7] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_PRIVATE;
		}
		break;

	case 'r':
		if (s[1] == 'e')
		{
			if (s[2] == 'a' && s[3] == 'd' && s[4] == 'o' && s[5] == 'n' &&
				s[6] == 'l' && s[7] == 'y' && s[8] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_READONLY;
			}
			else if (s[2] == 't' && s[3] == 'u' && s[4] == 'r' &&
				s[5] == 'n' && s[6] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_RETURN;
			}
		}
		break;

	case 's':
		if (s[1] == 'e' && s[2] == 'l' && s[3] == 'f' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_SELF;
		}
		else if (s[1] == 't')
		{
			if (s[2] == 'a' && s[3] == 't')
			{
				if (s[4] == 'e' && s[5] == 0)
				{
					tk_Token = TK_KEYWORD;
					tk_Keyword = KW_STATE;
				}
				else if (s[4] == 'i' && s[5] == 'c' && s[6] == 0)
				{
					tk_Token = TK_KEYWORD;
					tk_Keyword = KW_STATIC;
				}
			}
			else if (s[2] == 'r')
			{
				if (s[3] == 'i' && s[4] == 'n' && s[5] == 'g' && s[6] == 0)
				{
					tk_Token = TK_KEYWORD;
					tk_Keyword = KW_STRING;
				}
				else if (s[3] == 'u' && s[4] == 'c' && s[5] == 't' &&
					s[6] == 0)
				{
					tk_Token = TK_KEYWORD;
					tk_Keyword = KW_STRUCT;
				}
			}
		}
		else if (s[1] == 'w' && s[2] == 'i' && s[3] == 't' && s[4] == 'c' &&
			s[5] == 'h' && s[6] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_SWITCH;
		}
		break;

	case 't':
		if (s[1] == 'r')
		{
			if (s[2] == 'a' && s[3] == 'n' && s[4] == 's' && s[5] == 'i' &&
				s[6] == 'e' && s[7] == 'n' && s[8] == 't' && s[9] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_TRANSIENT;
			}
			else if (s[2] == 'u' && s[3] == 'e' && s[4] == 0)
			{
				tk_Token = TK_KEYWORD;
				tk_Keyword = KW_TRUE;
			}
		}
		break;

	case 'v':
		if (s[1] == 'e' && s[2] == 'c' && s[3] == 't' && s[4] == 'o' &&
			s[5] == 'r' && s[6] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_VECTOR;
		}
		else if (s[1] == 'o' && s[2] == 'i' && s[3] == 'd' && s[4] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_VOID;
		}
		break;

	case 'w':
		if (s[1] == 'h' && s[2] == 'i' && s[3] == 'l' && s[4] == 'e' &&
			s[5] == 0)
		{
			tk_Token = TK_KEYWORD;
			tk_Keyword = KW_WHILE;
		}
		break;
	}
	if (s[0] == 'N' && s[1] == 'U' && s[2] == 'L' && s[3] == 'L' && s[4] == 0)
	{
		tk_Token = TK_KEYWORD;
		tk_Keyword = KW_NULL;
	}

	if (tk_Token == TK_IDENTIFIER)
	{
		tk_Name = tk_String;
	}
}

//==========================================================================
//
//	ProcessSpecialToken
//
//==========================================================================

static void ProcessSpecialToken()
{
	char c = Chr;
   	NextChr();
	tk_Token = TK_PUNCT;
	switch (c)
	{
	case '+':
		if (Chr == '=')
		{
			tk_Punct = PU_ADD_ASSIGN;
			NextChr();
		}
		else if (Chr == '+')
		{
			tk_Punct = PU_INC;
			NextChr();
		}
		else
		{
			tk_Punct = PU_PLUS;
		}
		break;

	case '-':
		if (Chr == '=')
		{
			tk_Punct = PU_MINUS_ASSIGN;
			NextChr();
		}
		else if (Chr == '-')
		{
			tk_Punct = PU_DEC;
			NextChr();
		}
		else if (Chr == '>')
		{
			tk_Punct = PU_MINUS_GT;
			NextChr();
		}
		else
		{
			tk_Punct = PU_MINUS;
		}
		break;

	case '*':
		if (Chr == '=')
		{
			tk_Punct = PU_MULTIPLY_ASSIGN;
			NextChr();
		}
		else
		{
			tk_Punct = PU_ASTERISK;
		}
		break;

	case '/':
		if (Chr == '=')
		{
			tk_Punct = PU_DIVIDE_ASSIGN;
			NextChr();
		}
		else
		{
			tk_Punct = PU_SLASH;
		}
		break;

	case '%':
		if (Chr == '=')
		{
			tk_Punct = PU_MOD_ASSIGN;
			NextChr();
		}
		else
		{
			tk_Punct = PU_PERCENT;
		}
		break;

	case '=':
		if (Chr == '=')
		{
			tk_Punct = PU_EQ;
			NextChr();
		}
		else
		{
			tk_Punct = PU_ASSIGN;
		}
		break;

	case '<':
		if (Chr == '<')
		{
			NextChr();
			if (Chr == '=')
			{
				tk_Punct = PU_LSHIFT_ASSIGN;
				NextChr();
			}
			else
			{
				tk_Punct = PU_LSHIFT;
			}
		}
		else if (Chr == '=')
		{
			tk_Punct = PU_LE;
			NextChr();
		}
		else
		{
			tk_Punct = PU_LT;
		}
		break;

	case '>':
		if (Chr == '>')
		{
			NextChr();
			if (Chr == '=')
			{
				tk_Punct = PU_RSHIFT_ASSIGN;
				NextChr();
			}
			else
			{
				tk_Punct = PU_RSHIFT;
			}
		}
		else if (Chr == '=')
		{
			tk_Punct = PU_GE;
			NextChr();
		}
		else
		{
			tk_Punct = PU_GT;
		}
		break;

	case '!':
		if (Chr == '=')
		{
			tk_Punct = PU_NE;
			NextChr();
		}
		else
		{
			tk_Punct = PU_NOT;
		}
		break;

	case '&':
		if (Chr == '=')
		{
			tk_Punct = PU_AND_ASSIGN;
			NextChr();
		}
		else if (Chr == '&')
		{
			tk_Punct = PU_AND_LOG;
			NextChr();
		}
		else
		{
			tk_Punct = PU_AND;
		}
		break;

	case '|':
		if (Chr == '=')
		{
			tk_Punct = PU_OR_ASSIGN;
			NextChr();
		}
		else if (Chr == '|')
		{
			tk_Punct = PU_OR_LOG;
			NextChr();
		}
		else
		{
			tk_Punct = PU_OR;
		}
		break;

	case '^':
		if (Chr == '=')
		{
			tk_Punct = PU_XOR_ASSIGN;
			NextChr();
		}
		else
		{
			tk_Punct = PU_XOR;
		}
		break;

	case '.':
		if (Chr == '.' && FilePtr[0] == '.')
		{
			tk_Punct = PU_VARARGS;
			NextChr();
			NextChr();
		}
		else
		{
			tk_Punct = PU_DOT;
		}
		break;

	case ':':
		if (Chr == ':')
		{
			tk_Punct = PU_DCOLON;
			NextChr();
		}
		else
		{
			tk_Punct = PU_COLON;
		}
		break;

	case '(':
		tk_Punct = PU_LPAREN;
		break;

	case ')':
		tk_Punct = PU_RPAREN;
		break;

	case '?':
		tk_Punct = PU_QUEST;
		break;

	case '~':
		tk_Punct = PU_TILDE;
		break;

	case ',':
		tk_Punct = PU_COMMA;
		break;

	case ';':
		tk_Punct = PU_SEMICOLON;
		break;

	case '[':
		tk_Punct = PU_LINDEX;
		break;

	case ']':
		tk_Punct = PU_RINDEX;
		break;

	case '{':
		tk_Punct = PU_LBRACE;
		break;

	case '}':
		tk_Punct = PU_RBRACE;
		break;

	default:
		ERR_Exit(ERR_BAD_CHARACTER, true, "Unknown punctuation \'%c\'", Chr);
	}
}

//==========================================================================
//
// ProcessFileName
//
//==========================================================================

static void ProcessFileName()
{
	int len = 0;
	NextChr();
	while (Chr != '\"')
	{
		if (len >= MAX_QUOTED_LENGTH - 1)
		{
			ERR_Exit(ERR_STRING_TOO_LONG, true, NULL);
		}
		if (Chr == EOF_CHARACTER)
		{
			ERR_Exit(ERR_EOF_IN_STRING, true, NULL);
		}
		if (IncLineNumber)
		{
			ERR_Exit(ERR_NEW_LINE_INSIDE_QUOTE, true, NULL);
		}
		TokenStringBuffer[len] = Chr;
		NextChr();
		len++;
	}
	TokenStringBuffer[len] = 0;
	NextChr();
}

//==========================================================================
//
// TK_NextToken
//
//==========================================================================

void TK_NextToken()
{
	do
	{
		TokenStringBuffer[0] = 0;
		while (Chr == ' ') NextChr();
		if (NewLine)
		{
			NewLine = false;
			if (Chr == '#')
			{
				if (!strncmp(FilePtr, "line", 4))
				{
					NextChr();
					NextChr();
					NextChr();
					NextChr();
				}
				NextChr();

				//	Read line number
				while (Chr == ' ') NextChr();
				if (ASCIIToChrCode[(byte)Chr] != CHR_NUMBER)
				{
					ERR_Exit(ERR_NONE, false, "Bad directive.");
				}
				ProcessNumberToken();
				tk_Line = tk_Number - 1;

				//	Read file name
				while (Chr == ' ') NextChr();
				if (ASCIIToChrCode[(byte)Chr] != CHR_QUOTE)
				{
					ERR_Exit(ERR_NONE, false, "Bad directive.");
				}
				ProcessFileName();
				strcpy(tk_SourceName, tk_String);

				//	Ignore flags
				while (!NewLine)
				{
					NextChr();
				}
				tk_Token = TK_NONE;
				continue;
			}
		}
		switch (ASCIIToChrCode[(byte)Chr])
		{
			case CHR_EOF:
				tk_Token = TK_EOF;
				break;
			case CHR_LETTER:
				ProcessLetterToken();
				break;
			case CHR_NUMBER:
				ProcessNumberToken();
				break;
			case CHR_QUOTE:
				ProcessQuoteToken();
				break;
			case CHR_SINGLE_QUOTE:
				ProcessSingleQuoteToken();
				break;
			default:
				ProcessSpecialToken();
				break;
		}
	} while (tk_Token == TK_NONE);
}

//==========================================================================
//
//  TK_Check
//
//	Return true and take next token if current matches string.
//  Return false and do nothing otherwise.
//
//==========================================================================

bool TK_Check(const char *string)
{
	if (tk_Token != TK_IDENTIFIER && tk_Token != TK_KEYWORD &&
		tk_Token != TK_PUNCT)
	{
		return false;
	}
	if (strcmp(string, TokenStringBuffer))
	{
		return false;
	}

	TK_NextToken();
	return true;
}

//==========================================================================
//
//	TK_Expect
//
//  Report error, if current token is not equals to string.
//  Take next token.
//
//==========================================================================

void TK_Expect(const char *string, ECompileError error)
{
	if (tk_Token != TK_IDENTIFIER && tk_Token != TK_KEYWORD &&
		tk_Token != TK_PUNCT)
	{
		ERR_Exit(error, true, "invalid token type");
	}
	if (strcmp(string, TokenStringBuffer))
	{
		ERR_Exit(error, true, "expected %s, found %s", string, TokenStringBuffer);
	}
	TK_NextToken();
}

//==========================================================================
//
//	TK_Expect
//
//  Report error, if current token is not equals to kwd.
//  Take next token.
//
//==========================================================================

void TK_Expect(EKeyword kwd, ECompileError error)
{
	if (tk_Token != TK_KEYWORD)
	{
		ERR_Exit(error, true, "invalid token type");
	}
	if (tk_Keyword != kwd)
	{
		ERR_Exit(error, true, "expected %s, found %s", Keywords[kwd], TokenStringBuffer);
	}
	TK_NextToken();
}

//==========================================================================
//
//	TK_Expect
//
//  Report error, if current token is not equals to punct.
//  Take next token.
//
//==========================================================================

void TK_Expect(EPunctuation punct, ECompileError error)
{
	if (tk_Token != TK_PUNCT || tk_Punct != punct)
	{
		ERR_Exit(error, true, NULL);
	}
	TK_NextToken();
}

//**************************************************************************
//
//	$Log$
//	Revision 1.29  2006/03/19 13:15:29  dj_jl
//	Per character keyword checks.
//
//	Revision 1.28  2006/03/13 21:24:21  dj_jl
//	Added support for read-only, private and transient fields.
//	
//	Revision 1.27  2006/03/12 20:04:50  dj_jl
//	States as objects, added state variable type.
//	
//	Revision 1.26  2006/02/28 19:17:20  dj_jl
//	Added support for constants.
//	
//	Revision 1.25  2006/02/27 21:23:55  dj_jl
//	Rewrote names class.
//	
//	Revision 1.24  2006/02/19 20:37:02  dj_jl
//	Implemented support for delegates.
//	
//	Revision 1.23  2006/02/19 14:37:36  dj_jl
//	Changed type handling.
//	
//	Revision 1.22  2006/02/17 19:25:00  dj_jl
//	Removed support for progs global variables and functions.
//	
//	Revision 1.21  2006/02/15 23:27:07  dj_jl
//	Added script ID class attribute.
//	
//	Revision 1.20  2005/04/28 07:14:03  dj_jl
//	Fixed some warnings.
//	
//	Revision 1.19  2003/03/08 12:47:52  dj_jl
//	Code cleanup.
//	
//	Revision 1.18  2002/09/07 16:36:38  dj_jl
//	Support bool in function args and return type.
//	Removed support for typedefs.
//	
//	Revision 1.17  2002/03/12 19:17:30  dj_jl
//	Added keyword abstract
//	
//	Revision 1.16  2002/02/16 16:28:36  dj_jl
//	Added support for bool variables
//	
//	Revision 1.15  2002/01/17 18:19:52  dj_jl
//	New style of adding to mobjinfo, some fixes
//	
//	Revision 1.14  2002/01/15 18:29:36  dj_jl
//	no message
//	
//	Revision 1.13  2002/01/11 08:17:31  dj_jl
//	Added name subsystem, removed support for unsigned ints
//	
//	Revision 1.12  2002/01/07 12:31:36  dj_jl
//	Changed copyright year
//	
//	Revision 1.11  2001/12/18 19:09:41  dj_jl
//	Some extra info in progs and other small changes
//	
//	Revision 1.10  2001/12/12 19:22:22  dj_jl
//	Support for method usage as state functions, dynamic cast
//	Added dynamic arrays
//	
//	Revision 1.9  2001/12/04 18:19:03  dj_jl
//	Escape character for \ symbol
//	
//	Revision 1.8  2001/12/03 19:25:44  dj_jl
//	Fixed calling of parent function
//	Added defaultproperties
//	Fixed vectors as arguments to methods
//	
//	Revision 1.7  2001/12/01 18:17:09  dj_jl
//	Fixed calling of parent method, speedup
//	
//	Revision 1.6  2001/11/09 14:42:29  dj_jl
//	References, beautification
//	
//	Revision 1.5  2001/10/02 17:44:52  dj_jl
//	Some optimizations
//	
//	Revision 1.4  2001/09/20 16:09:55  dj_jl
//	Added basic object-oriented support
//	
//	Revision 1.3  2001/08/21 17:52:54  dj_jl
//	Added support for real string pointers, beautification
//	
//	Revision 1.2  2001/07/27 14:27:56  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
