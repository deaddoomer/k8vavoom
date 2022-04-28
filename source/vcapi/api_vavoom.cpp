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
#include "../gamedefs.h"
#include "../host.h"
#ifdef CLIENT
# include "../drawer.h"
# include "../input.h"
# include "../automap.h"
# include "../client/client.h"
#else
# include "../render/r_public.h"
#endif
# include "../client/cl_local.h"  /* for dlight_t */
#include "../language.h"
#include "../infostr.h"
#include "../mapinfo.h"
#include "../lockdefs.h"
#include "../psim/p_decal.h"
#include "../psim/p_entity.h"
#include "../psim/p_player.h"
#include "../filesys/files.h"
#include "../sound/sound.h"
#include "../decorate/vc_decorate.h"


VStream *VPackage::OpenFileStreamRO (VStr fname) { return FL_OpenFileRead(fname); }

void __attribute__((noreturn)) __declspec(noreturn) VPackage::HostErrorBuiltin (VStr msg) { Host_Error("%s", *msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::SysErrorBuiltin (VStr msg) { Sys_Error("%s", *msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::AssertErrorBuiltin (VStr msg) { Sys_Error("Assertion failure: %s", *msg); }

void __attribute__((noreturn)) __declspec(noreturn) VPackage::ExecutorAbort (const char *msg) { Host_Error("%s", msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::IOError (const char *msg) { Host_Error("I/O Error: %s", msg); }

void __attribute__((noreturn)) __declspec(noreturn) VPackage::CompilerFatalError (const char *msg) { Sys_Error("%s", msg); }
void __attribute__((noreturn)) __declspec(noreturn) VPackage::InternalFatalError (const char *msg) { Sys_Error("%s", msg); }


//==========================================================================
//
//  VPackage::LoadObject
//
//==========================================================================
void VPackage::LoadObject (TLocation l) {
  // main engine
  for (unsigned pidx = 0; ; ++pidx) {
    const char *pif = GetPkgImportFile(pidx);
    if (!pif) break;
    VStr mainVC = va("progs/%s/%s", *Name, pif);
    if (FL_FileExists(*mainVC)) {
      // compile package
      //fprintf(stderr, "Loading package '%s' (%s)\n", *Name, *mainVC);
      VStream *Strm = VPackage::OpenFileStreamRO(*mainVC);
      LoadSourceObject(Strm, mainVC, l);
      return;
    }
  }
  Sys_Error("Progs package %s not found", *Name);
}


//native static final bool IsIWadClass (class C) [property(class) IWad];
IMPLEMENT_FREE_FUNCTION(VObject, IsIWadClass) {
  VClass *Class;
  vobjGetParam(Class);
  if (Class) {
    // heuristic; let's assume that IWAD is "basepak" for now
    RET_BOOL(Class->Loc.GetSourceFile().toLowerCase().indexOf("/basepak.pk3") >= 0);
  } else {
    RET_BOOL(false);
  }
}


IMPLEMENT_FREE_FUNCTION(VObject, CvarUnlatchAll) {
  if (GGameInfo && GGameInfo->NetMode < NM_DedicatedServer) {
    VCvar::Unlatch();
  }
}


// native static final float GetLightMaxDist ();
IMPLEMENT_FREE_FUNCTION(VObject, GetLightMaxDist) {
  #ifdef CLIENT
  RET_FLOAT(VRenderLevelDrawer::GetLightMaxDist());
  #else
  RET_FLOAT(2048); // arbitrary
  #endif
}

// native static final float GetDynLightMaxDist ();
IMPLEMENT_FREE_FUNCTION(VObject, GetDynLightMaxDist) {
  #ifdef CLIENT
  RET_FLOAT(VRenderLevelDrawer::GetLightMaxDistDef());
  #else
  RET_FLOAT(1024); // arbitrary
  #endif
}

// native static final float GetLightMaxDistDef (float defval);
IMPLEMENT_FREE_FUNCTION(VObject, GetLightMaxDistDef) {
  float defval;
  vobjGetParam(defval);
  #ifdef CLIENT
  RET_FLOAT(VRenderLevelDrawer::GetLightMaxDistDef(defval));
  #else
  RET_FLOAT(defval);
  #endif
}


// native static final bool CheckViewOrgDistance (const TVec org, const float dist);
IMPLEMENT_FREE_FUNCTION(VObject, CheckViewOrgDistance) {
  TVec org;
  float dist;
  vobjGetParam(org, dist);
  #ifdef CLIENT
  if (cl) {
    RET_BOOL((cl->ViewOrg-org).lengthSquared() < dist*dist);
  } else {
    // no camera
    RET_BOOL(false);
  }
  #else
  // for server, always failed (there is no camera)
  RET_BOOL(false);
  #endif
}

// native static final bool CheckViewOrgDistance2D (const TVec org, const float dist);
IMPLEMENT_FREE_FUNCTION(VObject, CheckViewOrgDistance2D) {
  TVec org;
  float dist;
  vobjGetParam(org, dist);
  #ifdef CLIENT
  if (cl) {
    RET_BOOL((cl->ViewOrg-org).length2DSquared() < dist*dist);
  } else {
    // no camera
    RET_BOOL(false);
  }
  #else
  // for server, always failed (there is no camera)
  RET_BOOL(false);
  #endif
}


//**************************************************************************
//
//  Texture utils
//
//**************************************************************************
IMPLEMENT_FREE_FUNCTION(VObject, CheckTextureNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.CheckNumForName(name, TEXTYPE_Wall, true));
}

IMPLEMENT_FREE_FUNCTION(VObject, TextureNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.NumForName(name, TEXTYPE_Wall, false));
}

IMPLEMENT_FREE_FUNCTION(VObject, CheckFlatNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.CheckNumForName(name, TEXTYPE_Flat, true));
}

IMPLEMENT_FREE_FUNCTION(VObject, FlatNumForName) {
  P_GET_NAME(name);
  RET_INT(GTextureManager.NumForName(name, TEXTYPE_Flat, false));
}

IMPLEMENT_FREE_FUNCTION(VObject, TextureHeight) {
  P_GET_INT(pic);
  RET_FLOAT(GTextureManager.TextureHeight(pic));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetTextureName) {
  P_GET_INT(Handle);
  RET_NAME(GTextureManager.GetTextureName(Handle));
}


//==========================================================================
//
//  Console command functions
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, Cmd_CheckParm) {
  P_GET_STR(str);
  RET_INT(VCommand::CheckParm(*str));
}

IMPLEMENT_FREE_FUNCTION(VObject, Cmd_GetArgC) {
  RET_INT(VCommand::GetArgC());
}

IMPLEMENT_FREE_FUNCTION(VObject, Cmd_GetArgV) {
  P_GET_INT(idx);
  RET_STR(VCommand::GetArgV(idx));
}

IMPLEMENT_FREE_FUNCTION(VObject, CmdBuf_AddText) {
  GCmdBuf << VObject::PF_FormatString();
}


IMPLEMENT_FREE_FUNCTION(VObject, KBCheatClearAll) {
#ifdef CLIENT
  VInputPublic::KBCheatClearAll();
#endif
}

IMPLEMENT_FREE_FUNCTION(VObject, KBCheatAppend) {
  P_GET_STR(concmd);
  P_GET_STR(keys);
#ifdef CLIENT
  VInputPublic::KBCheatAppend(keys, concmd);
#endif
}


IMPLEMENT_FREE_FUNCTION(VObject, AreStateSpritesPresent) {
  P_GET_PTR(VState, State);
  RET_BOOL(State ? R_AreSpritesPresent(State->SpriteIndex) : false);
}


// R_IsSpritePresent(string sprname)
IMPLEMENT_FREE_FUNCTION(VObject, R_IsSpritePresent) {
  P_GET_STR(str);
  RET_BOOL(R_IsSpritePresent(*str));
}


//native static final int R_GetBloodTranslation (int color, optional bool allowadd/*=false*/);
IMPLEMENT_FREE_FUNCTION(VObject, R_GetBloodTranslation) {
  vint32 color;
  VOptParamBool allowadd(false);
  vobjGetParam(color, allowadd);
  RET_INT(R_GetBloodTranslation(color, allowadd));
}


//native static final int R_FindNamedTranslation (string name);
IMPLEMENT_FREE_FUNCTION(VObject, R_FindNamedTranslation) {
  VStr name;
  vobjGetParam(name);
  RET_INT(R_FindTranslationByName(name));
}


//native static final int R_CreateDesaturatedTranslation (int AStart, int AEnd, float rs, float gs, float bs, float re, float ge, float be);
IMPLEMENT_FREE_FUNCTION(VObject, R_CreateDesaturatedTranslation) {
  int AStart, AEnd;
  float rs, gs, bs, re, ge, be;
  vobjGetParam(AStart, AEnd, rs, gs, bs, re, ge, be);
  RET_INT(R_CreateDesaturatedTranslation(AStart, AEnd, rs, gs, bs, re, ge, be));
}


//native static final int R_CreateBlendedTranslation (int AStart, int AEnd, int r, int g, int b);
IMPLEMENT_FREE_FUNCTION(VObject, R_CreateBlendedTranslation) {
  int AStart, AEnd;
  int r, g, b;
  vobjGetParam(AStart, AEnd, r, g, b);
  RET_INT(R_CreateBlendedTranslation(AStart, AEnd, r, g, b));
}


//native static final int R_CreateTintedTranslation (int AStart, int AEnd, int r, int g, int b, int amount);
IMPLEMENT_FREE_FUNCTION(VObject, R_CreateTintedTranslation) {
  int AStart, AEnd;
  int r, g, b;
  int amount;
  vobjGetParam(AStart, AEnd, r, g, b, amount);
  RET_INT(R_CreateTintedTranslation(AStart, AEnd, r, g, b, amount));
}


//native static final void R_GetGamePalette (ref array!ColorRGBA pal);
IMPLEMENT_FREE_FUNCTION(VObject, R_GetGamePalette) {
  TArray<VColorRGBA> *pal256;
  vobjGetParam(pal256);
  if (pal256) {
    if (pal256->length() != 256) pal256->setLength(256);
    R_GetGamePalette(pal256->ptr());
  }
}


//native static final void R_GetTranslatedPalette (int transnum, ref array!ColorRGBA pal);
IMPLEMENT_FREE_FUNCTION(VObject, R_GetTranslatedPalette) {
  int transnum;
  TArray<VColorRGBA> *pal256;
  vobjGetParam(transnum, pal256);
  if (pal256) {
    if (pal256->length() != 256) pal256->setLength(256);
    R_GetTranslatedPalette(transnum, pal256->ptr());
  }
}


//native static final int R_CreateColorTranslation (const ref array!ColorRGBA pal);
IMPLEMENT_FREE_FUNCTION(VObject, R_CreateColorTranslation) {
  TArray<VColorRGBA> *pal256;
  vobjGetParam(pal256);
  if (pal256) {
    if (pal256->length() < 256) {
      VColorRGBA pal[256];
      R_GetGamePalette(pal256->ptr());
      for (unsigned int f = 0; f < (unsigned)pal256->length(); ++f) pal[f] = pal256->ptr()[f];
      RET_INT(R_CreateColorTranslation(pal));
    } else {
      RET_INT(R_CreateColorTranslation(pal256->ptr()));
    }
  } else {
    RET_INT(0);
  }
}


// native static final int BoxOnLineSide2D (const ref GameObject::line_t line, const TVec bmin, const TVec bmax);
IMPLEMENT_FREE_FUNCTION(VObject, BoxOnLineSide2D) {
  P_GET_VEC(bmax);
  P_GET_VEC(bmin);
  P_GET_PTR(line_t, ld);
  // 2d bbox points are in this strange order
  const float tmbox[4] = { bmax.y, bmin.y, bmin.x, bmax.x };
  RET_INT((ld ? ld->Box2DSide(tmbox) : 0));
}

// native static final bool BoxLineHit2D (const ref GameObject::line_t line, const TVec bmin, const TVec bmax);
IMPLEMENT_FREE_FUNCTION(VObject, BoxLineHit2D) {
  P_GET_VEC(bmax);
  P_GET_VEC(bmin);
  P_GET_PTR(line_t, ld);
  // 2d bbox points are in this strange order
  const float tmbox[4] = { bmax.y, bmin.y, bmin.x, bmax.x };
  RET_BOOL((ld ? ld->Box2DHit(tmbox) : false));
}


//==========================================================================
//
//  Misc
//
//==========================================================================
IMPLEMENT_FREE_FUNCTION(VObject, Info_ValueForKey) {
  P_GET_STR(key);
  P_GET_STR(info);
  RET_STR(Info_ValueForKey(info, key));
}

IMPLEMENT_FREE_FUNCTION(VObject, WadLumpPresent) {
  P_GET_NAME(name);
  //fprintf(stderr, "*** <%s> : %d (%d)\n", *name, W_CheckNumForName(name), W_CheckNumForName(name, WADNS_Graphics));
  RET_BOOL(W_CheckNumForName(name) >= 0 || W_CheckNumForName(name, WADNS_Graphics) >= 0);
}

IMPLEMENT_FREE_FUNCTION(VObject, FindAnimDoor) {
  P_GET_INT(BaseTex);
  RET_PTR(R_FindAnimDoor(BaseTex));
}

IMPLEMENT_FREE_FUNCTION(VObject, IsAnimatedTexture) {
  P_GET_INT(texid);
  RET_BOOL(R_IsAnimatedTexture(texid));
}

IMPLEMENT_FREE_FUNCTION(VObject, HasLangString) {
  P_GET_STR(Id);
  RET_BOOL(GLanguage.HasTranslation(*Id));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetLangString) {
  P_GET_STR(Id);
  RET_STR(GLanguage[*Id]);
}

//native static final string TranslateString (string Id); // *WITH* leading '$'
IMPLEMENT_FREE_FUNCTION(VObject, TranslateString) {
  VStr id;
  vobjGetParam(id);
  RET_STR(GLanguage.Translate(id));
}

IMPLEMENT_FREE_FUNCTION(VObject, GetLockDef) {
  P_GET_INT(Lock);
  RET_PTR(GetLockDef(Lock));
}

//native static final int ParseColor (string Name, optional bool retZeroIfInvalid/*=false*/, optional bool showError/*=true*/);
IMPLEMENT_FREE_FUNCTION(VObject, ParseColor) {
  VStr Name;
  VOptParamBool retZeroIfInvalid(false);
  VOptParamBool showError(true);
  vobjGetParam(Name, retZeroIfInvalid, showError);
  RET_INT(M_ParseColor(*Name, retZeroIfInvalid, showError));
}

IMPLEMENT_FREE_FUNCTION(VObject, TextColorString) {
  char buf[24];
  P_GET_INT(Color);
  /*if (Color == -1) {
    buf[0] = TEXT_COLOR_ESCAPE;
    buf[1] = '-';
    buf[2] = 0;
  } else*/ if ((Color&0xff000000) == 0xff000000) {
    snprintf(buf, sizeof(buf), "%c[#%04x]", TEXT_COLOR_ESCAPE, Color&0xffffff);
  } else {
    buf[0] = TEXT_COLOR_ESCAPE;
    buf[1] = (Color < CR_BRICK || Color >= NUM_TEXT_COLORS ? '-' : (char)(Color+'A'));
    buf[2] = 0;
  }
  RET_STR(VStr(buf));
}

IMPLEMENT_FREE_FUNCTION(VObject, StartTitleMap) {
  RET_BOOL(Host_StartTitleMap());
}

IMPLEMENT_FREE_FUNCTION(VObject, IsAutoloadingMapFromCLI) {
  RET_BOOL(Host_IsCLIMapStartFound());
}

IMPLEMENT_FREE_FUNCTION(VObject, LoadBinaryLump) {
  P_GET_PTR(TArrayNC<vuint8>, Array);
  P_GET_NAME(LumpName);
  W_LoadLumpIntoArray(LumpName, *Array);
}

IMPLEMENT_FREE_FUNCTION(VObject, IsMapPresent) {
  P_GET_NAME(MapName);
  RET_BOOL(IsMapPresent(MapName));
}

IMPLEMENT_FREE_FUNCTION(VObject, HasDecal) {
  VName name;
  vobjGetParam(name);
  RET_BOOL(VDecalDef::hasDecal(name));
}


// native static final int W_IterateNS (int Prev, EWadNamespace NS);
IMPLEMENT_FREE_FUNCTION(VObject, W_IterateNS) {
  int prev, wadns;
  vobjGetParam(prev, wadns);
  RET_INT(W_IterateNS(prev, EWadNamespace(wadns)));
}

// native static final int W_IterateFile (int Prev, string Name);
IMPLEMENT_FREE_FUNCTION(VObject, W_IterateFile) {
  int prev;
  VStr name;
  vobjGetParam(prev, name);
  RET_INT(W_IterateFile(prev, name));
}

// native static final int W_IterateDirectory (int Prev, string Name, bool allowSubdirs=true);
IMPLEMENT_FREE_FUNCTION(VObject, W_IterateDirectory) {
  int prev;
  VStr name;
  VOptParamBool allowSubdirs(true);
  vobjGetParam(prev, name, allowSubdirs);
  RET_INT(W_IterateDirectory(prev, name, allowSubdirs));
}

// native static final int W_LumpLength (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_LumpLength) {
  P_GET_INT(lump);
  RET_INT(W_LumpLength(lump));
}

// native static final name W_LumpName (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_LumpName) {
  P_GET_INT(lump);
  RET_NAME(W_LumpName(lump));
}

// native static final string W_RealLumpName (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_RealLumpName) {
  P_GET_INT(lump);
  RET_STR(W_RealLumpName(lump));
}

// native static final string W_FullLumpName (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_FullLumpName) {
  P_GET_INT(lump);
  RET_STR(W_FullLumpName(lump));
}

// native static final int W_LumpFile (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_LumpFile) {
  P_GET_INT(lump);
  RET_INT(W_LumpFile(lump));
}


// native static final bool W_IsIWADLump (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsIWADLump) { int lump; vobjGetParam(lump); RET_BOOL(W_IsIWADLump(lump)); }
// native static final bool W_IsIWADFile (int file);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsIWADFile) { int file; vobjGetParam(file); RET_BOOL(W_IsIWADFile(file)); }

// native static final bool W_IsWADLump (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsWADLump) { int lump; vobjGetParam(lump); RET_BOOL(W_IsWADLump(lump)); }
// native static final bool W_IsWADFile (int file);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsWADFile) { int file; vobjGetParam(file); RET_BOOL(W_IsWADFile(file)); }

// native static final bool W_IsPWADLump (int lump);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsPWADLump) { int lump; vobjGetParam(lump); RET_BOOL(W_IsUserWadLump(lump)); }
// native static final bool W_IsPWADFile (int file);
IMPLEMENT_FREE_FUNCTION(VObject, W_IsPWADFile) { int file; vobjGetParam(file); RET_BOOL(W_IsUserWadFile(file)); }


// native static final int W_CheckNumForName (name Name, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FREE_FUNCTION(VObject, W_CheckNumForName) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_NAME(Name);
  RET_INT(W_CheckNumForName(Name, EWadNamespace(ns)));
}

// native static final int W_GetNumForName (name Name, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FREE_FUNCTION(VObject, W_GetNumForName) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_NAME(Name);
  RET_INT(W_GetNumForName(Name, EWadNamespace(ns)));
}

// native static final int W_CheckNumForNameInFile (name Name, int File, optional EWadNamespace NS /*= WADNS_Global*/);
IMPLEMENT_FREE_FUNCTION(VObject, W_CheckNumForNameInFile) {
  P_GET_INT_OPT(ns, WADNS_Global);
  P_GET_INT(File);
  P_GET_NAME(Name);
  RET_INT(W_CheckNumForNameInFile(Name, File, EWadNamespace(ns)));
}


// native static final bool FS_FileExists (string fname);
IMPLEMENT_FREE_FUNCTION(VObject, FS_FileExists) {
  P_GET_STR(fname);
  if (!FL_IsSafeDiskFileName(fname)) { RET_BOOL(false); return; }
  VStr diskName = FL_GetUserDataDir(false)+"/"+fname;
  VStream *st = FL_OpenSysFileRead(*diskName);
  RET_BOOL(!!st);
  VStream::Destroy(st);
}

// native static final string FS_ReadFileContents (string fname);
IMPLEMENT_FREE_FUNCTION(VObject, FS_ReadFileContents) {
  P_GET_STR(fname);
  if (!FL_IsSafeDiskFileName(fname)) { RET_STR(VStr::EmptyString); return; }
  VStr diskName = FL_GetUserDataDir(false)+"/"+fname;
  VStream *st = FL_OpenSysFileRead(*diskName);
  if (!st) { RET_STR(VStr::EmptyString); return; }
  VStr s;
  if (st->TotalSize() > 0) {
    s.setLength(st->TotalSize());
    st->Serialise(s.getMutableCStr(), s.length());
    if (st->IsError()) s.clear();
  }
  VStream::Destroy(st);
  RET_STR(s);
}

// native static final bool FS_WriteFileContents (string fname, string contents);
IMPLEMENT_FREE_FUNCTION(VObject, FS_WriteFileContents) {
  P_GET_STR(contents);
  P_GET_STR(fname);
  if (!FL_IsSafeDiskFileName(fname)) { RET_BOOL(false); return; }
  VStr diskName = FL_GetUserDataDir(true)+"/"+fname;
  VStream *st = FL_OpenSysFileWrite(*diskName);
  if (!st) { RET_BOOL(false); return; }
  if (contents.length()) st->Serialise(*contents, contents.length());
  bool ok = !st->IsError();
  VStream::Destroy(st);
  RET_BOOL(ok);
}


#ifdef CLIENT
//k8: sorry!
extern int screenblocks;
#endif

// native static final int R_GetScreenBlocks ();
IMPLEMENT_FREE_FUNCTION(VObject, R_GetScreenBlocks) {
#ifdef CLIENT
  RET_INT(screenblocks);
#else
  // `13` is "fullscreen, no status bar"
  RET_INT(13);
#endif
}


// get number of known soundfonts
//native static final int SF2_GetCount ();
IMPLEMENT_FREE_FUNCTION(VObject, SF2_GetCount) {
#ifdef CLIENT
  RET_INT(SF2_GetCount());
#else
  RET_INT(0);
#endif
}

// get full soundfont name (to use with "snd_sf2_file")
//native static final string SF2_GetName (int idx);
IMPLEMENT_FREE_FUNCTION(VObject, SF2_GetName) {
  int idx;
  vobjGetParam(idx);
#ifdef CLIENT
  RET_STR(SF2_GetName(idx));
#else
  RET_STR(VStr::EmptyString);
#endif
}

// get short soundfont name (to use in menu)
//native static final string SF2_GetShortName (int idx);
IMPLEMENT_FREE_FUNCTION(VObject, SF2_GetShortName) {
  int idx;
  vobjGetParam(idx);
#ifdef CLIENT
  VStr s = SF2_GetName(idx);
  VStr bn = s.ExtractFileName();
  if (bn.isEmpty()) bn = s;
  if (bn.length() > 4 && bn.endsWithCI(".sf2")) bn.chopRight(4);
  int f = 0;
  while (f < bn.length()) {
    if (bn[f] == '_' || (vuint8)bn[f] < 32) bn.getMutableCStr()[f] = ' ';
    if ((vuint8)bn[f] >= 127) bn.getMutableCStr()[f] = '-';
    if (f > 0 && bn[f] == ' ' && bn[f-1] == ' ') {
      bn = bn.left(f)+bn.mid(f+1, bn.length());
    } else {
      ++f;
    }
  }
  bn = bn.xstrip();
  if (bn.isEmpty()) bn = "wtf?";
  RET_STR(bn);
#else
  RET_STR(VStr::EmptyString);
#endif
}

// get soundfount hash to use with `FS_*()` API
//native static final string SF2_GetHash (int idx);
IMPLEMENT_FREE_FUNCTION(VObject, SF2_GetHash) {
  int idx;
  vobjGetParam(idx);
#ifdef CLIENT
  RET_STR(SF2_GetHash(idx));
#else
  RET_STR(VStr::EmptyString);
#endif
}


//native static final void AM_DrawAtWidget (Widget w, float xc, float yc, float scale, float angle, float plrangle, float alpha);
IMPLEMENT_FREE_FUNCTION(VObject, AM_DrawAtWidget) {
#ifdef CLIENT
  VWidget *w;
#else
  void *w;
#endif
  float xc, yc, scale, angle, plrangle, alpha;
  vobjGetParam(w, xc, yc, scale, angle, plrangle, alpha);
#ifdef CLIENT
  AM_DrawAtWidget(w, xc, yc, scale, angle, plrangle, alpha);
#endif
}


//native static final bool AM_GetPlayerPos (out float xc, out float yc, out float angle);
IMPLEMENT_FREE_FUNCTION(VObject, AM_GetPlayerPos) {
  float *xc;
  float *yc;
  float *angle;
  vobjGetParam(xc, yc, angle);
#ifdef CLIENT
  if (GGameInfo->NetMode != NM_TitleMap) {
    if (cl && cl->MO /*&& cl->MO == cl->Camera*/) {
      *xc = cl->MO->Origin.x;
      *yc = cl->MO->Origin.y;
      *angle = AngleMod(cl->MO->Angles.yaw-90.0f);
      RET_BOOL(true);
      return;
    }
  }
#endif
  *xc = 0.0f;
  *yc = 0.0f;
  *angle = 0.0f;
  RET_BOOL(false);
}


// native static final bool AM_Active { get; set; }
IMPLEMENT_FREE_FUNCTION(VObject, get_AM_Active) {
#ifdef CLIENT
  RET_BOOL(AM_IsActive());
#else
  RET_BOOL(false);
#endif
}

IMPLEMENT_FREE_FUNCTION(VObject, set_AM_Active) {
  bool flag;
  vobjGetParam(flag);
#ifdef CLIENT
  AM_SetActive(flag);
#else
  (void)flag;
#endif
}


//native static final bool AM_IsOverlay { get; }
IMPLEMENT_FREE_FUNCTION(VObject, get_AM_IsOverlay) {
#ifdef CLIENT
  RET_BOOL(AM_IsOverlay());
#else
  RET_BOOL(false);
#endif
}


// native int dlight_t.GetFlags () const;
/*
IMPLEMENT_FREE_STRUCT_FUNCTION(Object, dlight_t, GetFlags) {
  dlight_t *lt;
  vobjGetParam(lt);
  if (lt) {
    RET_INT(lt->flags);
  } else {
    RET_INT(0);
  }
}
*/


// return flags as `LIGHTFLAG_xxx` bitset
//native static final int GetLightEffectLightFlags (const ref LightEffectDef lt);
IMPLEMENT_FREE_FUNCTION(VEntity, GetLightEffectLightFlags) {
  VLightEffectDef *lt;
  vobjGetParam(lt);
  vuint32 res = 0;
  if (lt) {
    if (lt->IsNoSelfShadow()) res |= dlight_t::NoSelfShadow;
    if (lt->IsNoShadow()) res |= dlight_t::NoShadow;
    if (lt->IsNoSelfLight()) res |= dlight_t::NoSelfLight;
    if (lt->IsNoActorLight()) res |= dlight_t::NoActorLight;
    if (lt->IsNoActorShadow()) res |= dlight_t::NoActorShadow;
    if (lt->IsAdditive()) res |= dlight_t::Additive;
    if (lt->IsSubtractive()) res |= dlight_t::Subtractive;
    if (lt->IsDisabled()) res |= dlight_t::Disabled;
    if (lt->IsNoGeoClip()) res |= dlight_t::NoGeoClip;
  }
  RET_INT((vint32)res);
}


//native static final int FindLineSpecialByName (string s, optional out VLineSpecInfo info);
IMPLEMENT_FREE_FUNCTION(VObject, FindLineSpecialByName) {
  VStr s;
  VOptParamPtr<VLineSpecInfo> info;
  vobjGetParam(s, info);
  const VLineSpecInfo *li = FindLineSpecialByName(s);
  if (li) {
    if (info.specified) *info.value = *li;
    RET_INT(li->Index);
  } else {
    if (info.specified) info.value->clear();
    RET_INT(0);
  }
}

//native static final string FindLineSpecialNameByNumber (int num, optional out VLineSpecInfo info);
IMPLEMENT_FREE_FUNCTION(VObject, FindLineSpecialNameByNumber) {
  int num;
  VOptParamPtr<VLineSpecInfo> info;
  vobjGetParam(num, info);
  const VLineSpecInfo *li = FindLineSpecialByNumber(num);
  if (li) {
    if (info.specified) *info.value = *li;
    RET_STR(li->Name);
  } else {
    if (info.specified) info.value->clear();
    RET_STR(VStr());
  }
}

//native static final bool GetLineSpecialInfoByNumber (int num, optional out VLineSpecInfo info);
IMPLEMENT_FREE_FUNCTION(VObject, GetLineSpecialInfoByNumber) {
  int num;
  VOptParamPtr<VLineSpecInfo> info;
  vobjGetParam(num, info);
  const VLineSpecInfo *li = FindLineSpecialByNumber(num);
  if (li) {
    if (info.specified) *info.value = *li;
    RET_BOOL(true);
  } else {
    if (info.specified) info.value->clear();
    RET_BOOL(false);
  }
}

//native static final int FindScriptLineSpecialByName (string s);
IMPLEMENT_FREE_FUNCTION(VObject, FindScriptLineSpecialByName) {
  VStr s;
  vobjGetParam(s);
  RET_INT(FindScriptLineSpecialByName(s));
}
