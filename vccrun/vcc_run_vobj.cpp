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

//native static final int fsysAppendDir (string path, optional string pfx);
IMPLEMENT_FREE_FUNCTION(VObject, fsysAppendDir) {
  VOptParamStr pfx;
  VStr fname;
  vobjGetParam(fname, pfx);
  //fprintf(stderr, "pakid(%d)=%d; fname=<%s>\n", (int)specified_pakid, pakid, *fname);
  if (pfx.specified) {
    RET_INT(fsysAppendDir(fname, pfx));
  } else {
    RET_INT(fsysAppendDir(fname));
  }
}


// append archive to the list of archives
// it will be searched in the current dir, and then in `fsysBaseDir`
// returns pack id or 0
//native static final int fsysAppendPak (string fname, optional int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysAppendPak) {
  VStr fname;
  VOptParamInt pakid(-1);
  vobjGetParam(fname, pakid);
  //fprintf(stderr, "pakid(%d)=%d; fname=<%s>\n", (int)specified_pakid, pakid, *fname);
  if (pakid.specified) {
    RET_INT(fsysAppendPak(fname, pakid));
  } else {
    RET_INT(fsysAppendPak(fname));
  }
}

// remove given pack from pack list
//native static final void fsysRemovePak (int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysRemovePak) {
  int pakid;
  vobjGetParam(pakid);
  fsysRemovePak(pakid);
}

// remove all packs from pakid and later
//native static final void fsysRemovePaksFrom (int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysRemovePaksFrom) {
  int pakid;
  vobjGetParam(pakid);
  fsysRemovePaksFrom(pakid);
}

// 0: no such pack
//native static final int fsysFindPakByPrefix (string pfx);
IMPLEMENT_FREE_FUNCTION(VObject, fsysFindPakByPrefix) {
  VStr pfx;
  vobjGetParam(pfx);
  RET_BOOL(fsysFindPakByPrefix(pfx));
}

//native static final bool fsysFileExists (string fname, optional int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysFileExists) {
  VStr fname;
  VOptParamInt pakid(-1);
  vobjGetParam(fname, pakid);
  if (pakid.specified) {
    RET_BOOL(fsysFileExists(fname, pakid));
  } else {
    RET_BOOL(fsysFileExists(fname));
  }
}

// find file with any extension
//native static final string fsysFileFindAnyExt (string fname, optional int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysFileFindAnyExt) {
  VStr fname;
  VOptParamInt pakid(-1);
  vobjGetParam(fname, pakid);
  if (pakid.specified) {
    RET_STR(fsysFileFindAnyExt(fname, pakid));
  } else {
    RET_STR(fsysFileFindAnyExt(fname));
  }
}


// return pack file path for the given pack id (or empty string)
//native static final string fsysGetPakPath (int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysGetPakPath) {
  int pakid;
  vobjGetParam(pakid);
  RET_STR(fsysGetPakPath(pakid));
}

// return pack prefix for the given pack id (or empty string)
//native static final string fsysGetPakPrefix (int pakid);
IMPLEMENT_FREE_FUNCTION(VObject, fsysGetPakPrefix) {
  int pakid;
  vobjGetParam(pakid);
  RET_STR(fsysGetPakPrefix(pakid));
}


//native static final int fsysGetLastPakId ();
IMPLEMENT_FREE_FUNCTION(VObject, fsysGetLastPakId) {
  RET_INT(fsysGetLastPakId());
}


IMPLEMENT_FREE_FUNCTION(VObject, get_fsysKillCommonZipPrefix) {
  RET_BOOL(fsysKillCommonZipPrefix);
}

IMPLEMENT_FREE_FUNCTION(VObject, set_fsysKillCommonZipPrefix) {
  bool v;
  vobjGetParam(v);
  fsysKillCommonZipPrefix = v;
}


// native final void appSetName (string appname);
IMPLEMENT_FREE_FUNCTION(VObject, appSetName) {
  VStr aname;
  vobjGetParam(aname);
  appName = aname;
}


//native final static bool appSaveOptions (Object optobj, optional string optfile, optional bool packit);
IMPLEMENT_FREE_FUNCTION(VObject, appSaveOptions) {
  VObject *optobj;
  VOptParamStr optfile;
  VOptParamBool packit(true);
  vobjGetParam(optobj, optfile, packit);
  (void)packit;
  fprintf(stderr, "appSaveOptions: not implemented!\n");
  RET_BOOL(false);
}


//native final static spawner Object appLoadOptions (class cls, optional string optfile);
IMPLEMENT_FREE_FUNCTION(VObject, appLoadOptions) {
  VClass *cls;
  VOptParamStr optfile;
  vobjGetParam(cls, optfile);
  fprintf(stderr, "appSaveOptions: not implemented!\n");
  RET_REF(nullptr);
}


//static final void PostQuitEvent (int code);
IMPLEMENT_FREE_FUNCTION(VObject, PostQuitEvent) {
  int code;
  vobjGetParam(code);
  ::PostQuitEvent(code);
}


//static final int RunMainEventLoop (optional ref event_t qevent);
IMPLEMENT_FREE_FUNCTION(VObject, RunMainEventLoop) {
  VOptParamPtr<event_t> qevt;
  vobjGetParam(qevt);
  RET_INT(::VCC_RunEventLoop(qevt.value));
}


// timer API for non-SDL apps
// `vccSetTimer` and `vccSetInterval` returns timer id, or 0 on error
//native static final int vccSetTimer (int mstimeout);
IMPLEMENT_FREE_FUNCTION(VObject, vccSetTimer) {
  int mstimeout;
  vobjGetParam(mstimeout);
  RET_INT(::VCC_SetTimer(mstimeout));
}

//native static final int vccSetInterval (int mstimeout);
IMPLEMENT_FREE_FUNCTION(VObject, vccSetInterval) {
  int mstimeout;
  vobjGetParam(mstimeout);
  RET_INT(::VCC_SetInterval(mstimeout));
}

//native static final void vccCancelInterval (int iid);
IMPLEMENT_FREE_FUNCTION(VObject, vccCancelInterval) {
  int mstimeout;
  vobjGetParam(mstimeout);
  ::VCC_CancelInterval(mstimeout);
}


// returns `false` if TTY is not available at all
//native static bool ttyIsAvailable () noexcept;
IMPLEMENT_FREE_FUNCTION(VObject, ttyIsAvailable) {
  RET_BOOL(ttyIsAvailable());
}

// returns `true` if TTY is good and supports fancy features
// if TTY is not good, no other API will work, and calling 'em is UB
//native static bool ttyIsGood () noexcept;
IMPLEMENT_FREE_FUNCTION(VObject, ttyIsGood) {
  RET_BOOL(ttyIsGood());
}

// switch TTY to raw or to normal mode
// returns `false` if failed
// WARNING! calls are not counted! i.e. two disables and then one enable will enable
//native static bool ttySetRawMode (bool enable) noexcept;
IMPLEMENT_FREE_FUNCTION(VObject, ttySetRawMode) {
  bool enable;
  vobjGetParam(enable);
  //if (!enable) ttySetWaitKey(true);
  ttySetWaitKey(!enable);
  RET_BOOL(ttySetRawMode(enable));
  //if (!enable) ttySetWaitKey(false);
}

// returns current TTY mode as was previously set by `ttySetRawMode()`
//native static bool ttyIsInRawMode () noexcept;
IMPLEMENT_FREE_FUNCTION(VObject, ttyIsInRawMode) {
  RET_BOOL(ttyIsInRawMode());
}

// returns TTY size
//native static int ttyGetWidth () noexcept;
IMPLEMENT_FREE_FUNCTION(VObject, ttyGetWidth) {
  RET_INT(ttyGetWidth());
}

//native static int ttyGetHeight () noexcept;
IMPLEMENT_FREE_FUNCTION(VObject, ttyGetHeight) {
  RET_INT(ttyGetHeight());
}

//native static final void ttyRawWrite (string format, ...) [printf,1];
IMPLEMENT_FREE_FUNCTION(VObject, ttyRawWrite) {
  VStr s = VObject::PF_FormatString();
  if (!s.isEmpty()) ttyRawWrite("%s", *s);
}

//native static final void ttyBeep ();
IMPLEMENT_FREE_FUNCTION(VObject, ttyBeep) {
  ttyBeep();
}

//native static final void ttyEnableBracketedPaste ();
IMPLEMENT_FREE_FUNCTION(VObject, ttyEnableBracketedPaste) {
  ttyEnableBracketedPaste();
}

//native static final void ttyDisableBracketedPaste ();
IMPLEMENT_FREE_FUNCTION(VObject, ttyDisableBracketedPaste) {
  ttyDisableBracketedPaste();
}

//native static final void ttyEnableFocusReports ();
IMPLEMENT_FREE_FUNCTION(VObject, ttyEnableFocusReports) {
  ttyEnableFocusReports();
}

//native static final void ttyDisableFocusReports ();
IMPLEMENT_FREE_FUNCTION(VObject, ttyDisableFocusReports) {
  ttyDisableFocusReports();
}

//native static final void ttyEnableMouseReports ();
IMPLEMENT_FREE_FUNCTION(VObject, ttyEnableMouseReports) {
  ttyEnableMouseReports();
}

//native static final void ttyDisableMouseReports ();
IMPLEMENT_FREE_FUNCTION(VObject, ttyDisableMouseReports) {
  ttyDisableMouseReports();
}
