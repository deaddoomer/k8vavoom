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
#include "vcc_run.h"
#include "../libs/vavoomc/vc_local.h"


VStream *VPackage::OpenFileStreamRO (VStr fname) { return fsysOpenFileSimple(fname); }


//==========================================================================
//
//  VPackage::LoadObject
//
//==========================================================================
void VPackage::LoadObject (TLocation l) {
  //vdlogf("Loading package '%s'...", *Name);

  for (int i = 0; i < GPackagePath.length(); ++i) {
    for (unsigned pidx = 0; ; ++pidx) {
      const char *pif = GetPkgImportFile(pidx);
      if (!pif) break;
      VStr mainVC = GPackagePath[i]+"/"+Name+"/"+pif;
      //vdlogf("  <%s>", *mainVC);
      VStream *Strm = VPackage::OpenFileStreamRO(mainVC);
      if (Strm) { /*vdlogf("  '%s'", *mainVC);*/ LoadSourceObject(Strm, mainVC, l); return; }
    }
  }

  // if no package pathes specified, try "packages/"
  if (GPackagePath.length() == 0) {
    for (unsigned pidx = 0; ; ++pidx) {
      const char *pif = GetPkgImportFile(pidx);
      if (!pif) break;
      VStr mainVC = VStr("packages/")+Name+"/"+pif;
      VStream *Strm = VPackage::OpenFileStreamRO(mainVC);
      if (Strm) { /*vdlogf("  '%s'", *mainVC);*/ LoadSourceObject(Strm, mainVC, l); return; }
    }
  }

  ParseError(l, "Can't find package %s", *Name);
  BailOut();
}


//==========================================================================
//
//  VInvocation::MassageDecorateArg
//
//  this will try to coerce some decorate argument to something sensible
//
//==========================================================================
VExpression *VExpression::MassageDecorateArg (VEmitContext &ec, VInvocation *invokation, VState *CallerState, const char *funcName,
                                              int argnum, const VFieldType &destType, bool isOptional,
                                              const TLocation *aloc, bool *massaged)
{
  Sys_Error("VExpression::MassageDecorateArg: the thing that should not be!");
}


IMPLEMENT_FREE_FUNCTION(VObject, CvarUnlatchAll) {
  VCvar::Unlatch();
}


//**************************************************************************
//
//  Basic functions
//
//**************************************************************************
IMPLEMENT_FREE_FUNCTION(VObject, get_GC_ImmediateDelete) { RET_BOOL(VObject::GImmediadeDelete); }
IMPLEMENT_FREE_FUNCTION(VObject, set_GC_ImmediateDelete) { bool val; vobjGetParam(val); VObject::GImmediadeDelete = val; }


// native static final void ccmdClearText ();
IMPLEMENT_FREE_FUNCTION(VObject, ccmdClearText) {
  ccmdClearText();
}

// native static final void ccmdClearCommand ();
IMPLEMENT_FREE_FUNCTION(VObject, ccmdClearCommand) {
  ccmdClearCommand();
}

// native static final CCResult ccmdParseOne ();
IMPLEMENT_FREE_FUNCTION(VObject, ccmdParseOne) {
  RET_INT(ccmdParseOne());
}

// native static final int ccmdGetArgc ();
IMPLEMENT_FREE_FUNCTION(VObject, ccmdGetArgc) {
  RET_INT(ccmdGetArgc());
}

// native static final string ccmdGetArgv (int idx);
IMPLEMENT_FREE_FUNCTION(VObject, ccmdGetArgv) {
  int idx;
  vobjGetParam(idx);
  RET_STR(ccmdGetArgv(idx));
}

// native static final int ccmdTextSize ();
IMPLEMENT_FREE_FUNCTION(VObject, ccmdTextSize) {
  RET_INT(ccmdTextSize());
}

// native static final void ccmdPrepend (string str);
IMPLEMENT_FREE_FUNCTION(VObject, ccmdPrepend) {
  VStr str;
  vobjGetParam(str);
  ccmdPrepend(str);
}

// native static final void ccmdPrependQuoted (string str);
IMPLEMENT_FREE_FUNCTION(VObject, ccmdPrependQuoted) {
  VStr str;
  vobjGetParam(str);
  ccmdPrependQuoted(str);
}

// native static final void ccmdAppend (string str);
IMPLEMENT_FREE_FUNCTION(VObject, ccmdAppend) {
  VStr str;
  vobjGetParam(str);
  ccmdAppend(str);
}

// native static final void ccmdAppendQuoted (string str);
IMPLEMENT_FREE_FUNCTION(VObject, ccmdAppendQuoted) {
  VStr str;
  vobjGetParam(str);
  ccmdAppendQuoted(str);
}
