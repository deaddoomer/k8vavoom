//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019-2022 Ketmar Dark
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libs/core.h"


//==========================================================================
//
//  usage
//
//==========================================================================
/*
static __attribute__((noreturn)) void usage () {
  GLog.Write("%s",
    "USAGE:\n"
    "  dunno\n"
    "");
  Z_ShuttingDown();
  Z_Exit(1);
}
*/


//==========================================================================
//
//  internalCheck
//
//==========================================================================
static int internalCheck (VStr fname, bool isWAD, int startfile) {
  if (!isWAD) {
    if (fname.endsWithCI(".zip") || fname.endsWithCI(".pk3")) {
      const bool ispk3 = fname.endsWithCI(".pk3");
      TArray<int> nlist;
      for (auto &&it : WadNSIterator(/*WADNS_Global*/WADNS_AllFiles)) {
        //GLog.Logf(NAME_Debug, "::: %s", *it.getRealName());
        VStr fn = it.getRealName();
        if (fn.endsWithCI(".wad") || (!ispk3 && fn.endsWithCI(".pk3"))) {
          //GLog.Logf(NAME_Debug, ":::%d: %s", it.lump, *it.getRealName());
          //nlist.append(fn);
          nlist.append(it.lump);
        }
      }
      if (nlist.length() == 0) return 0;
      // open nested files
      int count = 0;
      for (int nlump : nlist) {
        VStream *xst = W_CreateLumpReaderNum(nlump);
        if (!xst) continue;
        //GLog.Logf(NAME_Debug, "checking '%s'...", *W_FullLumpName(nlump));
        char sign[4];
        xst->Serialise(sign, 4);
        xst->Seek(0);
        if (xst->IsError()) {
          xst->Close();
          delete xst;
          continue;
        }
        const bool intrWAD = (memcmp(sign, "IWAD", 4) == 0 || memcmp(sign, "PWAD", 4) == 0);
        FSysAuxMark *mark = W_MarkAuxiliary();
        int cfx = W_AddAuxiliaryStream(xst, (intrWAD ? VFS_Wad : VFS_Archive));
        if (cfx >= 0) {
          int localcount = 0;
          //GLog.Logf(NAME_Debug, "fh=%d (%d)", cfx, W_LumpFile(cfx));
          for (auto &&it : WadMapIterator::FromWadFile(W_LumpFile(cfx))) {
            //GLog.Logf(NAME_Debug, "lump %d:<%s>: map %s", nlump, *W_FullLumpName(nlump), *it.getFullName());
            (void)it;
            ++count;
            ++localcount;
          }
          if (localcount) GLog.Logf("%-2d : %s", localcount, *W_FullLumpName(nlump));
        } else {
          xst->Close();
          delete xst;
        }
        W_ReleaseAuxiliary(mark);
      }
      return count;
    }
    return 0;
  } else {
    int count = 0;
    for (auto &&it : WadMapIterator::FromWadFile(W_LumpFile(startfile))) {
      //GLog.Logf(NAME_Debug, "lump %d:<%s>: map %s", nlump, *W_FullLumpName(nlump), *it.getFullName());
      (void)it;
      ++count;
    }
    return count;
  }
}


//==========================================================================
//
//  checkFile
//
//==========================================================================
static int checkFile (VStr fname) {
  if (fname.isEmpty()) return -1;
  if (!fname.endsWithCI(".zip") && !fname.endsWithCI(".pk3") &&
      !fname.endsWithCI(".wad") && !fname.endsWithCI(".iwad") &&
      !fname.endsWithCI(".ipk3"))
  {
    return -1;
  }
  if (Sys_DirExists(fname)) return -1;
  if (!Sys_FileExists(fname)) return -1;
  /*
  if (fname
  VFS_Wad, // caller is 100% sure that this is IWAD/PWAD
  VFS_Zip, // guess it, and allow nested files
  VFS_Archive, // guess it, but don't allow nested files
  */
  VStream *mainst = FL_OpenSysFileRead(fname);
  if (!mainst) return -1;
  if (mainst->TotalSize() < 8) {
    mainst->Close();
    delete mainst;
    return -1;
  }
  char sign[4];
  mainst->Serialise(sign, 4);
  mainst->Seek(0);
  if (mainst->IsError()) {
    mainst->Close();
    delete mainst;
    return -1;
  }
  const bool isWAD = (memcmp(sign, "IWAD", 4) == 0 || memcmp(sign, "PWAD", 4) == 0);
  /*const int aux =*/ W_StartAuxiliary();
  //GLog.Logf(NAME_Debug, "%s: isWAD=%d", *fname, (int)isWAD);
  const int stx = W_AddAuxiliaryStream(mainst, (isWAD ? VFS_Wad : VFS_Archive));
  if (stx == -1) {
    mainst->Close();
    delete mainst;
    return -1;
  }
  const int res = internalCheck(fname, isWAD, stx);
  W_CloseAuxiliary();
  return res;
}



//==========================================================================
//
//  main
//
//==========================================================================
int main (int argc, char **argv) {
  fsys_IgnoreZScript = 1; // no warnings
  fsys_EnableAuxSearch = true;
  fsys_WarningReportsEnabled = false;
  fsys_report_added_paks = false;

  GLogSkipLogTypeName = true;
  GLogTTYLog = true;

  if (argc < 2) {
    GLog.Logf("COUNTMAP build date: %s  %s", __DATE__, __TIME__);
    Z_ShuttingDown();
    return 0;
  }

  //GLog.Logf(NAME_Debug, "argc=%d", argc);
  //if (argc != 1) usage();

  int total = 0;
  int ftotal = 0;
  for (int fidx = 1; fidx < argc; ++fidx) {
    const int cnt = checkFile(argv[fidx]);
    if (cnt < 0) continue;
    GLog.Logf("%-2d : %s", cnt, argv[fidx]);
    total += cnt;
    if (cnt) ++ftotal;
  }
  GLog.Logf("%d maps in %d files", total, ftotal);

  Z_ShuttingDown();
  return 0;
}
