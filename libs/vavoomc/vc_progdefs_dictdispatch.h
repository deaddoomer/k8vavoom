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
//** directly included from "vc_progdefs.h"
//**************************************************************************
  DECLARE_OPC_DICTDISPATCH(Clear),
  DECLARE_OPC_DICTDISPATCH(Reset),
  DECLARE_OPC_DICTDISPATCH(Length),
  DECLARE_OPC_DICTDISPATCH(Capacity),
  DECLARE_OPC_DICTDISPATCH(Find),
  DECLARE_OPC_DICTDISPATCH(Put),
  DECLARE_OPC_DICTDISPATCH(Delete),
  DECLARE_OPC_DICTDISPATCH(ClearPointed),
  DECLARE_OPC_DICTDISPATCH(FirstIndex),
  DECLARE_OPC_DICTDISPATCH(IsValidIndex),
  DECLARE_OPC_DICTDISPATCH(NextIndex),
  DECLARE_OPC_DICTDISPATCH(DelAndNextIndex),
  DECLARE_OPC_DICTDISPATCH(GetKeyAtIndex),
  DECLARE_OPC_DICTDISPATCH(GetValueAtIndex),
  DECLARE_OPC_DICTDISPATCH(Compact),
  DECLARE_OPC_DICTDISPATCH(Rehash),
  DECLARE_OPC_DICTDISPATCH(DictToBool),
