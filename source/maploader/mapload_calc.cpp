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
//**
//**    Build nodes using ajbsp.
//**
//**************************************************************************
#include "../gamedefs.h"
#include "../text.h"


// ////////////////////////////////////////////////////////////////////////// //
static VCvarI nodes_builder_type("nodes_builder_type", "0", "Which internal node builder to use (0:auto; 1:ajbsp; 2:zdbsp)?", CVAR_Archive|CVAR_NoShadow);
// default nodes builder for UDMF is still AJBSP, because it seems that i fixed UDMF bugs
static VCvarI nodes_builder_normal("nodes_builder_normal", "1", "Which internal node builder to use for non-UDMF maps (0:auto; 1:ajbsp; 2:zdbsp)?", CVAR_Archive|CVAR_NoShadow);
static VCvarI nodes_builder_udmf("nodes_builder_udmf", "1", "Which internal node builder to use for UDMF maps (0:auto; 1:ajbsp; 2:zdbsp)?", CVAR_Archive|CVAR_NoShadow);


//==========================================================================
//
//  VLevel::GetNodesBuilder
//
//  valid only after `LevelFlags` are set
//
//==========================================================================
int VLevel::GetNodesBuilder () const {
  if (LevelFlags&LF_ForceUseZDBSP) return BSP_ZD;
  int nbt = nodes_builder_type;
  if (nbt <= 0) nbt = (LevelFlags&LF_TextMap ? nodes_builder_udmf : nodes_builder_normal);
  if (nbt == 2) return BSP_ZD;
  switch (nbt) {
    case 1: return BSP_AJ;
    case 2: return BSP_ZD;
  }
  // something strange, use old heuristic
  return (LevelFlags&LF_TextMap ? BSP_ZD : BSP_AJ);
}


//==========================================================================
//
//  VLevel::BuildNodes
//
//==========================================================================
void VLevel::BuildNodes () {
#ifdef CLIENT
  R_OSDMsgShowSecondary("BUILDING NODES");
  R_PBarReset();
#endif
  switch (GetNodesBuilder()) {
    case BSP_AJ:
      GCon->Log("Selected nodes builder: AJBSP");
      BuildNodesAJ();
      break;
    case BSP_ZD:
      GCon->Log("Selected nodes builder: ZDBSP");
      BuildNodesZD();
      break;
    default: Sys_Error("cannot determine nodes builder (this is internal engine bug!)");
  }
#ifdef CLIENT
  R_PBarUpdate("BSP", 42, 42, true); // final update
#endif
}
