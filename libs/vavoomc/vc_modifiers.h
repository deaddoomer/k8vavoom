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

class TModifiers {
public:
  enum {
    Native    = (1<<0),
    Static    = (1<<1),
    Abstract  = (1<<2),
    Private   = (1<<3),
    ReadOnly  = (1<<4),
    Transient = (1<<5),
    Final     = (1<<6),
    Optional  = (1<<7),
    Out       = (1<<8),
    Spawner   = (1<<9),
    Override  = (1<<10),
    Ref       = (1<<11),
    Protected = (1<<12),
    Const     = (1<<13),
    Repnotify = (1<<14),
    Scope     = (1<<15),
    Internal  = (1<<16),
    Published = (1<<17),
    DecVisible= (1<<19),

    ClassSet = Native|Abstract|Transient|DecVisible,
    MethodSet = Native|Static|Final|Spawner|Override|Private|Protected|DecVisible,
    ClassFieldSet = Native|Transient|Private|ReadOnly|Protected|Repnotify|Internal|Published,
    StructFieldSet = Native|Transient|Private|ReadOnly|Internal|Protected|Published,
    PropertySet = Native|Final|Static|Protected|Private|Override|DecVisible,
    ParamSet = Optional|Out|Ref|Const|Scope,
    ConstSet = Published|DecVisible,
  };

  static void ShowBadAttributes (int Modifiers, const TLocation &l);

  static int Parse (VLexer &Lex, TArray<VName> *exatts=nullptr);
  static const char *Name (int Modifier);
  static int Check (int Modifers, int Allowed, const TLocation &l);
  static int MethodAttr (int Modifers);
  static int ClassAttr (int Modifers);
  static int FieldAttr (int Modifers);
  static int PropAttr (int Modifers);
  static int ParmAttr (int Modifers);
  static int ConstAttr (int Modifers);
};
