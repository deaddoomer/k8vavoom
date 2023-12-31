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
  VC_LEXER_DEFTOKEN(NoToken, "")
  VC_LEXER_DEFTOKEN(EOF,           "<END-OF-FILE>")
  VC_LEXER_DEFTOKEN(Identifier,    "<IDENTIFIER>")
  VC_LEXER_DEFTOKEN(NameLiteral,   "<NAME-LITERAL>")
  VC_LEXER_DEFTOKEN(StringLiteral, "<STRING-LITERAL>")
  VC_LEXER_DEFTOKEN(IntLiteral,    "<INT-LITERAL>")
  VC_LEXER_DEFTOKEN(FloatLiteral,  "<FLOAT-LITERAL>")

  // keywords
  VC_LEXER_DEFTOKEN(Abstract,          "abstract")
  VC_LEXER_DEFTOKEN(Alias,             "alias")
  VC_LEXER_DEFTOKEN(Array,             "array")
  VC_LEXER_DEFTOKEN(Assert,            "assert")
  VC_LEXER_DEFTOKEN(Auto,              "auto")
  VC_LEXER_DEFTOKEN(BitEnum,           "bitenum")
  VC_LEXER_DEFTOKEN(Bool,              "bool")
  VC_LEXER_DEFTOKEN(Break,             "break")
  VC_LEXER_DEFTOKEN(Byte,              "byte")
  VC_LEXER_DEFTOKEN(Case,              "case")
  VC_LEXER_DEFTOKEN(Cast,              "cast")
  VC_LEXER_DEFTOKEN(Class,             "class")
  VC_LEXER_DEFTOKEN(Const,             "const")
  VC_LEXER_DEFTOKEN(Continue,          "continue")
  VC_LEXER_DEFTOKEN(Decorate,          "decorate")
  VC_LEXER_DEFTOKEN(Default,           "default")
  VC_LEXER_DEFTOKEN(DefaultProperties, "defaultproperties")
  VC_LEXER_DEFTOKEN(Delegate,          "delegate")
  VC_LEXER_DEFTOKEN(Delete,            "delete")
  VC_LEXER_DEFTOKEN(Dictionary,        "dictionary")
  VC_LEXER_DEFTOKEN(Do,                "do")
  VC_LEXER_DEFTOKEN(Else,              "else")
  VC_LEXER_DEFTOKEN(Enum,              "enum")
  VC_LEXER_DEFTOKEN(False,             "false")
  VC_LEXER_DEFTOKEN(Final,             "final")
  VC_LEXER_DEFTOKEN(Float,             "float")
  VC_LEXER_DEFTOKEN(For,               "for")
  VC_LEXER_DEFTOKEN(Foreach,           "foreach")
  //VC_LEXER_DEFTOKEN(Game,              "game") // demoted to simple identifier
  VC_LEXER_DEFTOKEN(Get,               "get")
  VC_LEXER_DEFTOKEN(Goto,              "goto")
  VC_LEXER_DEFTOKEN(If,                "if")
  VC_LEXER_DEFTOKEN(Inline,            "inline")
  VC_LEXER_DEFTOKEN(Import,            "import")
  VC_LEXER_DEFTOKEN(Int,               "int")
  VC_LEXER_DEFTOKEN(IsA,               "isa")
  VC_LEXER_DEFTOKEN(Iterator,          "iterator")
  VC_LEXER_DEFTOKEN(Name,              "name")
  VC_LEXER_DEFTOKEN(Native,            "native")
  VC_LEXER_DEFTOKEN(None,              "none")
  VC_LEXER_DEFTOKEN(Null,              "null")
  VC_LEXER_DEFTOKEN(Optional,          "optional")
  VC_LEXER_DEFTOKEN(Out,               "out")
  VC_LEXER_DEFTOKEN(Override,          "override")
  VC_LEXER_DEFTOKEN(Private,           "private")
  VC_LEXER_DEFTOKEN(Protected,         "protected")
  VC_LEXER_DEFTOKEN(Published,         "published")
  VC_LEXER_DEFTOKEN(ReadOnly,          "readonly")
  VC_LEXER_DEFTOKEN(Ref,               "ref")
  VC_LEXER_DEFTOKEN(Reliable,          "reliable")
  VC_LEXER_DEFTOKEN(Replication,       "replication")
  VC_LEXER_DEFTOKEN(Repnotify,         "repnotify")
  VC_LEXER_DEFTOKEN(Return,            "return")
  VC_LEXER_DEFTOKEN(Scope,             "scope")
  VC_LEXER_DEFTOKEN(Self,              "self")
  VC_LEXER_DEFTOKEN(Set,               "set")
  VC_LEXER_DEFTOKEN(Spawner,           "spawner")
  VC_LEXER_DEFTOKEN(State,             "state")
  VC_LEXER_DEFTOKEN(States,            "states")
  VC_LEXER_DEFTOKEN(Static,            "static")
  VC_LEXER_DEFTOKEN(String,            "string")
  VC_LEXER_DEFTOKEN(Struct,            "struct")
  VC_LEXER_DEFTOKEN(Switch,            "switch")
  VC_LEXER_DEFTOKEN(Transient,         "transient")
  VC_LEXER_DEFTOKEN(True,              "true")
  VC_LEXER_DEFTOKEN(UByte,             "ubyte")
  VC_LEXER_DEFTOKEN(UInt,              "uint")
  VC_LEXER_DEFTOKEN(Unreliable,        "unreliable")
  VC_LEXER_DEFTOKEN(Vector,            "vector")
  VC_LEXER_DEFTOKEN(Void,              "void")
  VC_LEXER_DEFTOKEN(While,             "while")
  VC_LEXER_DEFTOKEN(With,              "with")

  VC_LEXER_DEFTOKEN(MobjInfo, "__mobjinfo__")
  VC_LEXER_DEFTOKEN(ScriptId, "__scriptid__")

  //WARNING! the following tokens should be sorted by string length!
  // punctuation
  VC_LEXER_DEFTOKEN(URShiftAssign,  ">>>=")
  VC_LEXER_DEFTOKEN(URShift,        ">>>")
  VC_LEXER_DEFTOKEN(VarArgs,        "...")
  VC_LEXER_DEFTOKEN(LShiftAssign,   "<<=")
  VC_LEXER_DEFTOKEN(RShiftAssign,   ">>=")
  VC_LEXER_DEFTOKEN(DotDot,         "..")
  VC_LEXER_DEFTOKEN(AddAssign,      "+=")
  VC_LEXER_DEFTOKEN(MinusAssign,    "-=")
  VC_LEXER_DEFTOKEN(MultiplyAssign, "*=")
  VC_LEXER_DEFTOKEN(DivideAssign,   "/=")
  VC_LEXER_DEFTOKEN(ModAssign,      "%=")
  VC_LEXER_DEFTOKEN(AndAssign,      "&=")
  VC_LEXER_DEFTOKEN(OrAssign,       "|=")
  VC_LEXER_DEFTOKEN(XOrAssign,      "^=")
  VC_LEXER_DEFTOKEN(CatAssign,      "~=")
  VC_LEXER_DEFTOKEN(Equals,         "==")
  VC_LEXER_DEFTOKEN(NotEquals,      "!=")
  VC_LEXER_DEFTOKEN(LessEquals,     "<=")
  VC_LEXER_DEFTOKEN(GreaterEquals,  ">=")
  VC_LEXER_DEFTOKEN(AndLog,         "&&")
  VC_LEXER_DEFTOKEN(OrLog,          "||")
  VC_LEXER_DEFTOKEN(LShift,         "<<")
  VC_LEXER_DEFTOKEN(RShift,         ">>")
  VC_LEXER_DEFTOKEN(Inc,            "++")
  VC_LEXER_DEFTOKEN(Dec,            "--")
  VC_LEXER_DEFTOKEN(Arrow,          "->")
  VC_LEXER_DEFTOKEN(DColon,         "::")
  VC_LEXER_DEFTOKEN(Less,           "<")
  VC_LEXER_DEFTOKEN(Greater,        ">")
  VC_LEXER_DEFTOKEN(Quest,          "?")
  VC_LEXER_DEFTOKEN(And,            "&")
  VC_LEXER_DEFTOKEN(Or,             "|")
  VC_LEXER_DEFTOKEN(XOr,            "^")
  VC_LEXER_DEFTOKEN(Tilde,          "~")
  VC_LEXER_DEFTOKEN(Not,            "!")
  VC_LEXER_DEFTOKEN(Plus,           "+")
  VC_LEXER_DEFTOKEN(Minus,          "-")
  VC_LEXER_DEFTOKEN(Asterisk,       "*")
  VC_LEXER_DEFTOKEN(Slash,          "/")
  VC_LEXER_DEFTOKEN(Percent,        "%")
  VC_LEXER_DEFTOKEN(LParen,         "(")
  VC_LEXER_DEFTOKEN(RParen,         ")")
  VC_LEXER_DEFTOKEN(Dot,            ".")
  VC_LEXER_DEFTOKEN(Comma,          ",")
  VC_LEXER_DEFTOKEN(Semicolon,      ";")
  VC_LEXER_DEFTOKEN(Colon,          ":")
  VC_LEXER_DEFTOKEN(Assign,         "=")
  VC_LEXER_DEFTOKEN(LBracket,       "[")
  VC_LEXER_DEFTOKEN(RBracket,       "]")
  VC_LEXER_DEFTOKEN(LBrace,         "{")
  VC_LEXER_DEFTOKEN(RBrace,         "}")
  VC_LEXER_DEFTOKEN(Dollar,         "$")
