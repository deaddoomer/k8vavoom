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
#include "../../gamedefs.h"
#include "../sound.h"
#include "../snd_local.h"


VCvarB snd_sf2_autoload("snd_sf2_autoload", true, "Automatically load SF2 from a set of predefined directories?", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);
VCvarS snd_sf2_file("snd_sf2_file", "", "Use this soundfont file.", CVAR_Archive|CVAR_PreInit|CVAR_NoShadow);


TArray<VStr> sf2FileList;
static bool diskScanned = false;

// for SF2 VC API
static TArray<VStr> sf2KnownFiles;
static bool sf2KnownScanned = false;


static const char *SF2SearchPathes[] = {
  "!",
  "!/sf2",
  "!/dls",
  "!/soundfonts",
#if defined(_WIN32)
  "!/share",
  "!/share/sf2",
  "!/share/dls",
  "!/share/soundfonts",
#endif
#if !defined(_WIN32) && !defined(__SWITCH__)
  "~/.k8vavoom",
  "~/.k8vavoom/sf2",
  "~/.k8vavoom/dls",
  "~/.k8vavoom/soundfonts",

  "/opt/vavoom/sf2",
  "/opt/vavoom/dls",
  "/opt/vavoom/soundfonts",

  "/opt/vavoom/share",
  "/opt/vavoom/share/sf2",
  "/opt/vavoom/share/dls",
  "/opt/vavoom/share/soundfonts",

  "/opt/vavoom/share/k8vavoom",
  "/opt/vavoom/share/k8vavoom/sf2",
  "/opt/vavoom/share/k8vavoom/dls",
  "/opt/vavoom/share/k8vavoom/soundfonts",

  "/usr/local/share/k8vavoom",
  "/usr/local/share/k8vavoom/sf2",
  "/usr/local/share/k8vavoom/dls",
  "/usr/local/share/k8vavoom/soundfonts",

  "/usr/share/k8vavoom",
  "/usr/share/k8vavoom/sf2",
  "/usr/share/k8vavoom/dls",
  "/usr/share/k8vavoom/soundfonts",

  "!/../share",
  "!/../share/sf2",
  "!/../share/dls",
  "!/../share/soundfonts",

  "!/../share/k8vavoom",
  "!/../share/k8vavoom/sf2",
  "!/../share/k8vavoom/dls",
  "!/../share/k8vavoom/soundfonts",
#endif
  nullptr,
};


//==========================================================================
//
//  SF2_NeedDiskScan
//
//==========================================================================
bool SF2_NeedDiskScan () {
  return !diskScanned;
}


//==========================================================================
//
//  SF2_SetDiskScanned
//
//==========================================================================
void SF2_SetDiskScanned (bool v) {
  diskScanned = !v;
}


//==========================================================================
//
//  SF2_ScanDiskBanks
//
//  this fills `sf2FileList`
//
//==========================================================================
void SF2_ScanDiskBanks () {
  if (sf2FileList.length() == 0 || sf2FileList[0] != snd_sf2_file.asStr()) {
    diskScanned = false;
  }

  if (diskScanned) return;

  // try to find sf2 in binary dir
  diskScanned = true;

  TMap<VStr, bool> knownMap;
  VStr sf2x = snd_sf2_file.asStr();

  // collect banks
  sf2FileList.reset();
  if (!sf2x.isEmpty()) {
    sf2FileList.append(sf2x);
    knownMap.put(sf2x, true);
  }

  if (snd_sf2_autoload) {
    for (const char **sfdir = SF2SearchPathes; *sfdir; ++sfdir) {
      VStr dirname = VStr(*sfdir);
      if (dirname.isEmpty()) continue;
      if (dirname[0] == '!') { dirname.chopLeft(1); dirname = GParsedArgs.getBinDir()+dirname; }
      #if !defined(_WIN32) && !defined(__SWITCH__)
      else if (dirname[0] == '~') {
        const char *home = getenv("HOME");
        if (!home || !home[0]) continue;
        dirname.chopLeft(1);
        dirname = VStr(home)+dirname;
      }
      #endif
      //GCon->Logf("Timidity: scanning '%s'", *dirname);
      auto dir = Sys_OpenDir(dirname);
      for (;;) {
        auto fname = Sys_ReadDir(dir);
        if (fname.isEmpty()) break;
        VStr ext = fname.extractFileExtension();
        if (ext.strEquCI(".sf2") || ext.strEquCI(".dls")) {
          VStr nn = dirname+"/"+fname;
          if (!knownMap.put(nn, true)) sf2FileList.append(nn);
        }
      }
      Sys_CloseDir(dir);
    }
  }

#if defined(__SWITCH__)
  // try "./gzdoom.sf2"
  if (Sys_FileExists("./gzdoom.sf2")) {
    bool found = false;
    for (auto &&fn : sf2FileList) {
      if (fn.strEquCI("./gzdoom.sf2")) {
        found = true;
        break;
      }
    }
    if (!found) {
      VStr nn = "./gzdoom.sf2";
      if (!knownMap.put(nn, true)) sf2FileList.append(nn);
    }
  }
#endif

#ifdef _WIN32
  {
    /*static*/ const char *shitdozeShit[] = {
      "ct4mgm.sf2",
      "ct2mgm.sf2",
      "drivers\\gm.dls",
      nullptr,
    };
    bool delimeterPut = false;
    for (const char **ssp = shitdozeShit; *ssp; ++ssp) {
      static char sysdir[65536];
      memset(sysdir, 0, sizeof(sysdir));
      if (!GetSystemDirectoryA(sysdir, sizeof(sysdir)-1)) break;
      //VStr gmpath = VStr(getenv("WINDIR"))+"/system32/drivers/gm.dls";
      VStr gmpath = VStr(sysdir)+"\\"+(*ssp);
      //GCon->Logf("::: trying <%s> :::", *gmpath);
      if (Sys_FileExists(*gmpath)) {
        bool found = false;
        for (auto &&fn : sf2FileList) {
          if (fn.strEquCI(gmpath)) {
            found = true;
            break;
          }
        }
        if (!found && !knownMap.put(gmpath, true)) {
          if (!delimeterPut) sf2FileList.append(""); // delimiter
          delimeterPut = true;
          sf2FileList.append(gmpath);
        }
      }
    }
  }
#endif
}


//==========================================================================
//
//  CountAllEntities
//
//==========================================================================
static VVA_OKUNUSED int cmpSF2FileNames (const void *aa, const void *bb, void *) {
  const VStr *a = (const VStr *)aa;
  const VStr *b = (const VStr *)bb;
  return a->ICmp(*b);
}


//==========================================================================
//
//  SF2_KnownScan
//
//==========================================================================
static void SF2_KnownScan () {
  if (sf2KnownScanned) return;
  sf2KnownScanned = true;

  TMap<VStr, bool> knownMap;
  VStr sf2x = snd_sf2_file.asStr();

  // collect banks
  sf2KnownFiles.reset();
  if (!sf2x.isEmpty()) {
    sf2KnownFiles.append(sf2x);
    knownMap.put(sf2x, true);
  }

  for (const char **sfdir = SF2SearchPathes; *sfdir; ++sfdir) {
    VStr dirname = VStr(*sfdir);
    if (dirname.isEmpty()) continue;
    if (dirname[0] == '!') { dirname.chopLeft(1); dirname = GParsedArgs.getBinDir()+dirname; }
    #if !defined(_WIN32) && !defined(__SWITCH__)
    else if (dirname[0] == '~') {
      const char *home = getenv("HOME");
      if (!home || !home[0]) continue;
      dirname.chopLeft(1);
      dirname = VStr(home)+dirname;
    }
    #endif
    //GCon->Logf("Timidity: scanning '%s'", *dirname);
    auto dir = Sys_OpenDir(dirname);
    for (;;) {
      auto fname = Sys_ReadDir(dir);
      if (fname.isEmpty()) break;
      VStr ext = fname.extractFileExtension();
      if (ext.strEquCI(".sf2") /*|| ext.strEquCI(".dls")*/) {
        VStr nn = dirname+"/"+fname;
        if (!knownMap.put(nn, true)) sf2KnownFiles.append(nn);
      }
    }
    Sys_CloseDir(dir);
  }

#if defined(__SWITCH__)
  // try "./gzdoom.sf2"
  if (Sys_FileExists("./gzdoom.sf2")) {
    bool found = false;
    for (auto &&fn : sf2KnownFiles) {
      if (fn.strEquCI("./gzdoom.sf2")) {
        found = true;
        break;
      }
    }
    if (!found) {
      VStr nn = "./gzdoom.sf2";
      if (!knownMap.put(nn, true)) sf2KnownFiles.append(nn);
    }
  }
#endif

  xxsort_r(sf2KnownFiles.ptr(), sf2KnownFiles.length(), sizeof(sf2KnownFiles[0]), &cmpSF2FileNames, nullptr);
}


//==========================================================================
//
//  SF2_GetCount
//
//==========================================================================
int SF2_GetCount () {
  SF2_KnownScan();
  return sf2KnownFiles.length();
}


//==========================================================================
//
//  SF2_GetName
//
//  get full soundfont name (to use with "snd_sf2_file")
//
//==========================================================================
VStr SF2_GetName (int idx) {
  SF2_KnownScan();
  return (idx >= 0 && idx < sf2KnownFiles.length() ? sf2KnownFiles[idx] : VStr::EmptyString);
}


//==========================================================================
//
//  SF2_GetHash
//
//  get soundfount hash to use with `FS_*()` API
//
//==========================================================================
VStr SF2_GetHash (int idx) {
  SF2_KnownScan();
  if (idx < 0 || idx >= sf2KnownFiles.length()) return VStr::EmptyString;
  VStr s = sf2KnownFiles[idx];
  VStr bn = s.extractFileName();
  if (bn.isEmpty()) bn = s;
  s = bn.toLowerCase();
  MD5Context md5ctx;
  md5ctx.Init();
  md5ctx.Update(*s, (unsigned)s.length());
  vuint8 md5digest[MD5Context::DIGEST_SIZE];
  md5ctx.Final(md5digest);
  return VStr::buf2hex(md5digest, MD5Context::DIGEST_SIZE);
}
