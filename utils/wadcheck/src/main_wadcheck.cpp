//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 2019 Ketmar Dark
//**
//**  This program is free software: you can redistribute it and/or modify
//**  it under the terms of the GNU General Public License as published by
//**  the Free Software Foundation, either version 3 of the License, or
//**  (at your option) any later version.
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

//#include <SDL.h>

#include "libs/core/src/core.h"
#include "fsys/files.h"


// ////////////////////////////////////////////////////////////////////////// //
static VStr getBaseWad (const VStr &s) {
  auto cpos = s.lastIndexOf(':');
  if (cpos <= 0) return VStr::EmptyString;
  return s.left(cpos);
}


static VStr getLumpName (const VStr &s) {
  auto cpos = s.lastIndexOf(':');
  if (cpos < 0) return s;
  return s.mid(cpos+1, s.length());
}


static VStr normalizeName (const VStr &s) {
  VStr bw = getBaseWad(s);
  VStr ln = getLumpName(s);
  if (bw.length()) {
    bw = bw.ExtractFileBaseName();
    if (bw.length()) bw += ':';
  }
  return bw+ln;
}


// ////////////////////////////////////////////////////////////////////////// //
struct FileHash {
  XXH64_hash_t hash;
  inline bool operator == (const FileHash &b) const { return (b.hash == hash); }
};

inline vuint32 GetTypeHash (const FileHash &fh) { return ((vuint32)fh.hash)^((vuint32)(fh.hash>>32)); }

struct LumpInfo {
  VStr name;
  vint32 size;
};

struct FileInfo {
  FileHash hash;
  TArray<LumpInfo> names;
};

static TMapDtor<FileHash, FileInfo> wadlist;


// ////////////////////////////////////////////////////////////////////////// //
static void appendHash (XXH64_hash_t hash, VStr fname, vint32 size) {
  fname = normalizeName(fname);
  FileHash fh;
  fh.hash = hash;
  auto fip = wadlist.find(fh);
  if (fip) {
    for (int f = 0; f < fip->names.length(); ++f) {
      if (fip->names[f].size != size) continue;
      if (fname.ICmp(fip->names[f].name) == 0) return;
    }
    LumpInfo li;
    li.name = fname;
    li.size = size;
    fip->names.append(li);
  } else {
    FileInfo fi;
    fi.hash = fh;
    LumpInfo li;
    li.name = fname;
    li.size = size;
    fi.names.append(li);
    wadlist.put(fh, fi);
  }
}


static void checkHash (XXH64_hash_t hash, VStr fname, vint32 size) {
  VStr origName = fname;
  fname = normalizeName(fname);
  FileHash fh;
  fh.hash = hash;
  auto fip = wadlist.find(fh);
  if (!fip) return;
  bool wasHeader = false;
  for (int f = 0; f < fip->names.length(); ++f) {
    if (fip->names[f].size != size) continue;
    if (!wasHeader) {
      wasHeader = true;
      //GLog.WriteLine("LUMP HIT FOR '%s'", *origName);
      fprintf(stderr, "LUMP HIT FOR '%s'\n", *origName);
    }
    //GLog.WriteLine("  %s", *(fip->names[f].name));
    fprintf(stderr, "  %s\n", *(fip->names[f].name));
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static const char *DBSignature = "WLHDv0.1";

static void writeList (VStream *fo) {
  TArray<VStr> strpool;
  TMap<VStr, int> spmap;

  for (auto it = wadlist.first(); it; ++it) {
    const FileInfo &fi = it.getValue();
    for (int f = 0; f < fi.names.length(); ++f) {
      VStr s = fi.names[f].name;
      VStr bw = getBaseWad(s); //.toLowerCase();
      auto ip = spmap.find(bw);
      if (!ip) {
        auto idx = strpool.length();
        strpool.append(bw);
        spmap.put(bw, idx);
      }
      bw = getLumpName(s); //.toLowerCase();
      ip = spmap.find(bw);
      if (!ip) {
        auto idx = strpool.length();
        strpool.append(bw);
        spmap.put(bw, idx);
      }
    }
  }

  fo->Serialise(DBSignature, 8);
  // write list of names
  vint32 nlen = strpool.length();
  *fo << STRM_INDEX(nlen);
  for (int f = 0; f < strpool.length(); ++f) *fo << strpool[f];
  // list of lumps
  vint32 count = wadlist.length();
  *fo << STRM_INDEX(count);
  for (auto it = wadlist.first(); it; ++it) {
    const FileInfo &fi = it.getValue();
    //fo->Serialise(&fi.hash.hash, (int)sizeof(fi.hash.hash));
    //XXH64_hash_t hash = fi.hash.hash;
    //*fo << hash;
    *fo << fi.hash.hash;
    // list of names and sizes
    vint32 ncount = fi.names.length();
    *fo << STRM_INDEX(ncount);
    for (int f = 0; f < fi.names.length(); ++f) {
      VStr s = fi.names[f].name;
      VStr bw = getBaseWad(s);
      auto ip = spmap.find(bw);
      check(ip);
      *fo << STRM_INDEX(*ip);
      VStr ln = getLumpName(s);
      ip = spmap.find(ln);
      check(ip);
      *fo << STRM_INDEX(*ip);
      // size
      vint32 size = fi.names[f].size;
      *fo << STRM_INDEX(size);
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static void readList (VStream *fo) {
  char sign[8];
  fo->Serialise(sign, 8);
  if (memcmp(sign, DBSignature, 8) != 0) Sys_Error("invalid database signature");
  // read names
  vint32 scount = 0;
  *fo << STRM_INDEX(scount);
  if (fo->IsError()) Sys_Error("error reading database");
  if (scount < 0 || scount > 1024*1024*32) Sys_Error("database corrupted");
  TArray<VStr> strpool;
  strpool.setLength(scount);
  for (int f = 0; f < scount; ++f) {
    *fo << strpool[f];
    if (fo->IsError()) Sys_Error("error reading database");
  }
  // read list
  vint32 count = 0;
  *fo << STRM_INDEX(count);
  if (fo->IsError()) Sys_Error("error reading database");
  if (count < 0 || count > 1024*1024*32) Sys_Error("database corrupted");
  while (count--) {
    XXH64_hash_t hash;
    //fo->Serialise(&hash, (int)sizeof(hash));
    *fo << hash;
    if (fo->IsError()) Sys_Error("error reading database");
    vint32 ncount = 0;
    *fo << STRM_INDEX(ncount);
    if (fo->IsError()) Sys_Error("error reading database");
    if (ncount < 1 || ncount > 1024*1024) Sys_Error("database corrupted");
    while (ncount-- > 0) {
      vint32 bwidx, lnidx;
      *fo << STRM_INDEX(bwidx) << STRM_INDEX(lnidx);
      if (fo->IsError()) Sys_Error("error reading database");
      VStr bw = strpool[bwidx];
      VStr ln = strpool[lnidx];
      if (bw.length()) bw += ':';
      vint32 size;
      *fo << STRM_INDEX(size);
      if (fo->IsError()) Sys_Error("error reading database");
      appendHash(hash, bw+ln, size);
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
static __attribute__((noreturn)) void usage () {
  GLog.Write("%s",
    "USAGE:\n"
    "  wadcheck [--db dbfilename] --register wad/pk3\n"
    "  wadcheck [--db dbfilename] wad/pk3\n"
    "");
  exit(1);
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  GLog.WriteLine("WADCHECK build date: %s  %s", __DATE__, __TIME__);

  VStr dbname = ".wadhash.wdb";
  if (argc > 1 && strcmp(argv[1], "--db") == 0) {
    if (argc < 3) usage();
    dbname = VStr(argv[2]);
    if (dbname.length() == 0) usage();
    for (int f = 3; f < argc; ++f) argv[f-2] = argv[f];
    argc -= 2;
    dbname = dbname.DefaultExtension(".wdb");
  }

  if (argc < 2) usage();

  // prepend binary path to db name
  if (!dbname.IsAbsolutePath()) {
    dbname = VStr(VArgs::GetBinaryDir())+"/"+dbname;
  }
  GLog.WriteLine("using database file '%s'", *dbname);

  {
    VStream *fi = FL_OpenSysFileRead(dbname);
    if (fi) {
      readList(fi);
      delete fi;
    }
  }

  if (strcmp(argv[1], "--register") == 0) {
    for (int f = 2; f < argc; ++f) W_AddFile(argv[f]);
    GLog.WriteLine("calculating hashes...");
    for (int lump = W_IterateNS(-1, WADNS_Any); lump >= 0; lump = W_IterateNS(lump, WADNS_Any)) {
      if (W_LumpLength(lump) < 8) continue;
      TArray<vuint8> data;
      W_LoadLumpIntoArray(lump, data);
      XXH64_hash_t hash = XXH64(data.ptr(), data.length(), 0x29a);
      //GLog.WriteLine("calculated hash for '%s': 0x%16llx", *W_FullLumpName(lump), hash);
      appendHash(hash, W_FullLumpName(lump), W_LumpLength(lump));
    }

    {
      VStream *fo = FL_OpenSysFileWrite(dbname);
      writeList(fo);
      delete fo;
    }
  } else {
    if (wadlist.length() == 0) Sys_Error("database not found");

    for (int f = 1; f < argc; ++f) W_AddFile(argv[f]);

    for (int lump = W_IterateNS(-1, WADNS_Any); lump >= 0; lump = W_IterateNS(lump, WADNS_Any)) {
      if (W_LumpLength(lump) < 8) continue;
      TArray<vuint8> data;
      W_LoadLumpIntoArray(lump, data);
      XXH64_hash_t hash = XXH64(data.ptr(), data.length(), 0x29a);
      checkHash(hash, W_FullLumpName(lump), W_LumpLength(lump));
    }
  }

  //W_AddFile("/home/ketmar/DooMz/wads/doom2.wad");

  return 0;
}
