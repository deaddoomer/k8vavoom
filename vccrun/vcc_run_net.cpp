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
// directly included from "vcc_run.cpp"
//**************************************************************************


//==========================================================================
//
//  onExecuteNetMethod
//
//==========================================================================
static bool onExecuteNetMethod (VObject *aself, VMethod *func) {
  // find initial version of the method
  VMethod *Base = func;
  while (Base->SuperMethod) Base = Base->SuperMethod;

  // execute it's replication condition method
  vassert(Base->ReplCond);
  P_PASS_REF(aself);
  if (!VObject::ExecuteFunction(Base->ReplCond).getBool()) {
    //fprintf(stderr, "rpc call to `%s` (%s) is not done\n", aself->GetClass()->GetName(), *func->GetFullName());
    return false;
  }

  // clean up parameters
  func->CleanupParams();

  fprintf(stderr, "rpc call to `%s` (%s) is DONE!\n", aself->GetClass()->GetName(), *func->GetFullName());

  // it's been handled here
  return true;
}
