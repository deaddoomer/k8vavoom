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
//**  Copyright (C) 2018-2021 Ketmar Dark
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
#include "gamedefs.h"
#include "psim/p_player.h"
#include "cvar.h"
#include "server/server.h"
#ifdef CLIENT
# include "client/client.h"
#endif
#include "infostr.h"


//==========================================================================
//
//  cv_userInfoSet
//
//==========================================================================
static void cv_userInfoSet (VCvar *cvar) {
  #ifdef CLIENT
  Info_SetValueForKey(cls.userinfo, cvar->GetName(), *cvar->asStr());
  if (cl) {
    if (GGameInfo->NetMode == NM_TitleMap ||
        GGameInfo->NetMode == NM_Standalone ||
        GGameInfo->NetMode == NM_ListenServer)
    {
      VCommand::ExecuteString(VStr("setinfo \"")+cvar->GetName()+"\" \""+cvar->asStr()+"\"\n", VCommand::SRC_Client, cl);
    } else {
      //if (cl->Net) cl->Net->SendCommand(VStr("setinfo \"")+cvar->GetName()+"\" \""+cvar->asStr()+"\"\n");
      CL_SendCommandToServer(VStr("setinfo \"")+cvar->GetName()+"\" \""+cvar->asStr().quote()+"\"\n");
    }
  }
  #endif
}


//==========================================================================
//
//  cv_serverInfoSet
//
//==========================================================================
static void cv_serverInfoSet (VCvar *cvar) {
  #ifdef SERVER
  Info_SetValueForKey(svs.serverinfo, cvar->GetName(), *cvar->asStr());
  if (GGameInfo && GGameInfo->NetMode != NM_None && GGameInfo->NetMode != NM_Client) {
    for (int i = 0; i < MAXPLAYERS; ++i) {
      if (GGameInfo->Players[i]) {
        GGameInfo->Players[i]->eventClientSetServerInfo(cvar->GetName(), cvar->asStr());
      }
    }
  }
  #endif
}


// ////////////////////////////////////////////////////////////////////////// //
//static TMapNC<VCvar *, CvarChangeHandler> chMap;
//static TMapNC<VStr, CvarChangeHandler> chMapPending;
static TMapNC<VCvar *, bool> acAdded; // already added to autocompletion list?


//==========================================================================
//
//  cv_created
//
//==========================================================================
static void cv_created (VCvar *cvar) {
  if (!acAdded.has(cvar)) {
    acAdded.put(cvar, true);
    if (!cvar->IsHidden()) VCommand::AddToAutoComplete(cvar->GetName());
  }
}


//==========================================================================
//
//  Cvars_Init
//
//==========================================================================
void Cvars_Init () {
  VCvar::Init();
  VCvar::AddAllVarsToAutocomplete(&VCommand::AddToAutoComplete);
  VCvar::CreatedCB = &cv_created; // this adds to autocompletion
  //VCvar::ChangedCB = &cv_changed;
  VCvar::UserInfoSetCB = &cv_userInfoSet;
  VCvar::ServerInfoSetCB = &cv_serverInfoSet;
  VCvar::SendAllUserInfos();
  VCvar::SendAllServerInfos();
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND CvarList
//
// This is slightly more complicated, as we want nicely sorted list.
// It can be fairly slow, we don't care.
COMMAND(CvarList) {
  vuint32 count = VCvar::countCVars();
  VCvar **list = VCvar::getSortedList();
  bool showValues = (Args.length() > 1 && (Args[1].ICmp("values") == 0 || Args[1].ICmp("value") == 0));
  for (vuint32 n = 0; n < count; ++n) {
    VCvar *cvar = list[n];
    if (showValues) {
      GCon->Logf("  %s \"%s\"", cvar->GetName(), *VStr(cvar->asStr()).quote());
    } else {
      GCon->Logf("%s: %s", cvar->GetName(), cvar->GetHelp());
    }
  }
  GCon->Logf("%u variables.", count);
  delete[] list;
}


//==========================================================================
//
//  DoCvarCompletions
//
//==========================================================================
static VStr DoCvarCompletions (const TArray<VStr> &args, int aidx) {
  TArray<VStr> list;
  VStr prefix = (aidx < args.length() ? args[aidx] : VStr());
  if (aidx == 1) {
    VCvar::GetPrefixedList(list, prefix);
    if (list.length() > 127) {
      GCon->Logf("\034[Gold]There are %d matches; please, be more specific.", list.length());
      return VStr::EmptyString;
    } else {
      return VCommand::AutoCompleteFromListCmd(prefix, list);
    }
  } else {
    return VStr::EmptyString;
  }
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_whatis
//
// Show short description for a cvar.
COMMAND_WITH_AC(cvar_whatis) {
  if (Args.length() != 2) {
    GCon->Logf("Show short cvar description.");
    GCon->Logf("usage: cvar_whatis varname");
  } else {
    VCvar *cvar = VCvar::FindVariable(*(Args[1]));
    if (cvar) {
      GCon->Logf("\034[Green]%s: \034[Gold]%s", cvar->GetName(), cvar->GetHelp());
    } else {
      GCon->Logf("Unknown cvar: '%s'", *(Args[1]));
    }
  }
}
COMMAND_AC(cvar_whatis) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND toggle
//
// Toggles boolean cvar
COMMAND_WITH_AC(toggle) {
  if (Args.length() != 2) {
    GCon->Log("Toggles boolean cvar.");
    GCon->Log("usage: toggle varname");
    return;
  }
  VCvar *cvar = VCvar::FindVariable(*Args[1]);
  if (!cvar) {
    GCon->Logf(NAME_Error, "Unknown cvar: '%s'", *Args[1]);
    return;
  }
  cvar->Set(cvar->asBool() ? 0 : 1);
}
COMMAND_AC(toggle) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
static void cvarDoAddSubF (TArray<VStr> &Args, bool asAdd) {
  VCvar *cvar = VCvar::FindVariable(*Args[1]);
  if (!cvar) {
    GCon->Logf(NAME_Error, "Unknown cvar: '%s'", *Args[1]);
    return;
  }

  if (cvar->GetType() != VCvar::CVType::Float) {
    GCon->Logf(NAME_Error, "cvar '%s' is not float", *Args[1]);
    return;
  }

  float delta = 0.0f;
  if (!Args[2].convertFloat(&delta) || !isFiniteF(delta)) {
    GCon->Logf(NAME_Error, "'%s' is not a float value", *Args[2]);
    return;
  }

  bool hasLimit = false;
  float limit = 0.0f;
  if (Args.length() > 3) {
    if (!Args[3].convertFloat(&limit) || !isFiniteF(limit)) {
      GCon->Logf(NAME_Error, "'%s' is not a float value", *Args[3]);
      return;
    }
    hasLimit = true;
  }

  float val = cvar->asFloat();
  if (asAdd) {
    val += delta;
    if (hasLimit && val > limit) val = limit;
  } else {
    val -= delta;
    if (hasLimit && val < limit) val = limit;
  }
  cvar->Set(val);
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_add_f name value [limit]
//
// add value to float cvar
COMMAND_WITH_AC(cvar_add_f) {
  if (Args.length() < 3) {
    GCon->Log("Add value to float cvar.");
    GCon->Log("usage: cvar_add_f varname value [limit]");
    return;
  }
  cvarDoAddSubF(Args, true);
}
COMMAND_AC(cvar_add_f) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_sub_f name value [limit]
//
// subtract value from float cvar
COMMAND_WITH_AC(cvar_sub_f) {
  if (Args.length() < 3) {
    GCon->Log("Subtract value from float cvar.");
    GCon->Log("usage: cvar_sub_f varname value [limit]");
    return;
  }
  cvarDoAddSubF(Args, false);
}
COMMAND_AC(cvar_sub_f) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
static void cvarDoAddSubI (TArray<VStr> &Args, bool asAdd) {
  VCvar *cvar = VCvar::FindVariable(*Args[1]);
  if (!cvar) {
    GCon->Logf(NAME_Error, "Unknown cvar: '%s'", *Args[1]);
    return;
  }
  if (cvar->GetType() != VCvar::CVType::Int) {
    GCon->Logf(NAME_Error, "cvar '%s' is not int", *Args[1]);
    return;
  }

  int delta = 0;
  if (!Args[2].convertInt(&delta)) {
    GCon->Logf(NAME_Error, "'%s' is not an int value", *Args[2]);
    return;
  }

  bool hasLimit = false;
  int limit = 0;
  if (Args.length() > 3) {
    if (!Args[3].convertInt(&limit)) {
      GCon->Logf(NAME_Error, "'%s' is not an int value", *Args[3]);
      return;
    }
    hasLimit = true;
  }

  bool hasWrap = false;
  int wrap = 0;
  if (Args.length() > 4) {
    if (!Args[4].convertInt(&wrap)) {
      GCon->Logf(NAME_Error, "'%s' is not an int value", *Args[4]);
      return;
    }
    hasWrap = true;
  }

  int val = cvar->asInt();
  if (asAdd) {
    val += delta;
    if (hasLimit && val > limit) val = (hasWrap ? wrap : limit);
  } else {
    val -= delta;
    if (hasLimit && val < limit) val = (hasWrap ? wrap : limit);
  }
  cvar->Set(val);
}


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_add_i name value [limit] [lowrap]
//
// add value to int cvar
COMMAND_WITH_AC(cvar_add_i) {
  if (Args.length() < 3) {
    GCon->Log("Add value to integer cvar.");
    GCon->Log("usage: cvar_add_i varname value [limit] [lowrap]");
    return;
  }
  cvarDoAddSubI(Args, true);
}
COMMAND_AC(cvar_add_i) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_sub_i name value [limit] [hiwrap]
//
// subtract value from int cvar
COMMAND_WITH_AC(cvar_sub_i) {
  if (Args.length() < 3) {
    GCon->Log("Subtract value from integer cvar.");
    GCon->Log("usage: cvar_sub_i varname value [limit] [hiwrap]");
    return;
  }
  cvarDoAddSubI(Args, false);
}
COMMAND_AC(cvar_sub_i) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_reset
//
// Resets cvar to default value
COMMAND_WITH_AC(cvar_reset) {
  if (Args.length() != 2) {
    GCon->Logf("Resets cvar to default value.");
    GCon->Logf("usage: cvar_reset varname");
  } else {
    VCvar *cvar = VCvar::FindVariable(*(Args[1]));
    if (cvar) {
      GCon->Logf("%s: default is %s", cvar->GetName(), *VStr(cvar->GetDefault()).quote(true, true));
      cvar->SetStr(nullptr); // reset to default
      //GCon->Logf("%s: value is %s (int:%d)", cvar->GetName(), *cvar->asStr().quote(true, true), cvar->asInt());
    } else {
      GCon->Logf("Unknown cvar: '%s'", *Args[1]);
    }
  }
}
COMMAND_AC(cvar_reset) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_show_default
//
// Show default cvar value
COMMAND_WITH_AC(cvar_show_default) {
  if (Args.length() != 2) {
    GCon->Logf("Show default cvar value.");
    GCon->Logf("usage: cvar_show_default varname");
  } else {
    VCvar *cvar = VCvar::FindVariable(*Args[1]);
    if (cvar) {
      GCon->Logf("  %s \"%s\"", *(VStr(cvar->GetName()).quote(true)), *(VStr(cvar->GetDefault()).quote()));
    } else {
      GCon->Logf("Unknown cvar: '%s'", *Args[1]);
    }
  }
}
COMMAND_AC(cvar_show_default) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_increment
//
// increment cvar
COMMAND_WITH_AC(cvar_increment) {
  if (Args.length() < 2 || Args.length() > 3) {
    GCon->Logf("Increment cvar.");
    GCon->Logf("usage: cvar_increment varname [value]");
  } else {
    float vv = 1.0f;
    if (Args.length() > 2) {
      if (!Args[2].convertFloat(&vv)) { GCon->Logf(NAME_Error, "invalid number: %s", *Args[2]); return; }
    }
    VCvar *cvar = VCvar::FindVariable(*Args[1]);
    if (cvar) {
      cvar->Set(cvar->asFloat()+vv);
    } else {
      GCon->Logf("Unknown cvar: '%s'", *Args[1]);
    }
  }
}
COMMAND_AC(cvar_increment) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvar_decrement
//
// increment cvar
COMMAND_WITH_AC(cvar_decrement) {
  if (Args.length() < 2 || Args.length() > 3) {
    GCon->Logf("decrement cvar.");
    GCon->Logf("usage: cvar_decrement varname [value]");
  } else {
    float vv = 1.0f;
    if (Args.length() > 2) {
      if (!Args[2].convertFloat(&vv)) { GCon->Logf(NAME_Error, "invalid number: %s", *Args[2]); return; }
    }
    VCvar *cvar = VCvar::FindVariable(*Args[1]);
    if (cvar) {
      cvar->Set(cvar->asFloat()-vv);
    } else {
      GCon->Logf("Unknown cvar: '%s'", *Args[1]);
    }
  }
}
COMMAND_AC(cvar_decrement) { return DoCvarCompletions(args, aidx); }


// ////////////////////////////////////////////////////////////////////////// //
// COMMAND cvarinfovar
//
// create temp variable from user mod
COMMAND(CvarInfoVar) {
  if (Args.length() < 3) {
    GCon->Logf("usage: cvarinfovar [flags-and-type] varname varvalue");
  } else {
    VStr vname = Args[Args.length()-2];
    VStr vvalue = Args[Args.length()-1];
    if (vname.length() == 0) return;
    VCvar *cvar = VCvar::FindVariable(*vname);
    if (cvar) {
      // just set value
      cvar->Set(vvalue);
      return;
    }
    int flags = CVAR_FromMod|CVAR_Archive|CVAR_AlwaysArchive;
    bool isint = false;
    bool isfloat = false;
    bool isstr = false;
    bool isbool = false;
    const char *cvtname = "int";
    for (int f = 1; f < Args.length()-2; ++f) {
           if (Args[f].ICmp("noarchive") == 0) flags &= ~(CVAR_Archive|CVAR_AlwaysArchive);
      else if (Args[f].ICmp("cheat") == 0) flags |= CVAR_Cheat;
      else if (Args[f].ICmp("latch") == 0) {/*flags |= CVAR_Latch;*/} //k8: this seems to be unnecessary
      else if (Args[f].ICmp("server") == 0) {/*flags |= CVAR_ServerInfo;*/} //k8: this seems to be unnecessary
      else if (Args[f].ICmp("user") == 0) {}
      else if (Args[f].strEquCI("int")) { cvtname = "int"; isint = true; }
      else if (Args[f].strEquCI("float")) { cvtname = "float"; isfloat = true; }
      else if (Args[f].strEquCI("bool")) { cvtname = "bool"; isbool = true; }
      else if (Args[f].strEquCI("string")) { cvtname = "string"; isstr = true; }
      else { GCon->Logf("invalid cvarinfo flag '%s'", *Args[f]); return; }
    }
    if (!isint && !isfloat && !isstr && !isbool) isint = true;
    VCvar *cv = VCvar::CreateNew(*vname, vvalue, va("cvarinfo %s variable", cvtname), flags);
    if (cv) {
           if (isstr) cv->SetType(VCvar::String);
      else if (isbool) cv->SetType(VCvar::Bool);
      else if (isfloat) cv->SetType(VCvar::Float);
      else if (isint) cv->SetType(VCvar::Int);
    }
  }
}
