//**************************************************************************
//**
//**  ##   ##    ##    ##   ##   ####     ####   ###     ###
//**  ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**   ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**   ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**    ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**     #    ##    ##    #      ####     ####   ##       ##
//**
//**  $Id$
//**
//**  Copyright (C) 1999-2006 Jānis Legzdiņš
//**
//**  This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**  This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************

#include <signal.h>
#include <time.h>

#include "vcc_run.h"


// ////////////////////////////////////////////////////////////////////////// //
__attribute__((noreturn, format(printf, 1, 2))) void Host_Error (const char *error, ...) {
  fprintf(stderr, "FATAL: ");
  va_list argPtr;
  va_start(argPtr, error);
  vfprintf(stderr, error, argPtr);
  va_end(argPtr);
  fprintf(stderr, "\n");
  exit(1);
}


// ////////////////////////////////////////////////////////////////////////// //
class VVccLog : public VLogListener {
public:
  virtual void Serialise (const char* text, EName event) override {
    dprintf("%s", text);
  }
};


// ////////////////////////////////////////////////////////////////////////// //
class VFileReader : public VStream {
private:
  FILE* mFile;

public:
  VFileReader (FILE* InFile) : mFile(InFile) { bLoading = true; }

  virtual ~VFileReader () { Close(); }

  // stream interface
  virtual void Serialise (void* V, int Length) override {
    if (bError) return;
    if (!mFile || fread(V, Length, 1, mFile) != 1) bError = true;
  }

  virtual void Seek (int InPos) override {
    if (bError) return;
    if (!mFile || fseek(mFile, InPos, SEEK_SET)) bError = true;
  }

  virtual int Tell() override {
    return (mFile ? ftell(mFile) : 0);
  }

  virtual int TotalSize () override {
    if (bError || !mFile) return 0;
    auto curpos = ftell(mFile);
    if (fseek(mFile, 0, SEEK_END)) { bError = true; return 0; }
    auto size = ftell(mFile);
    if (fseek(mFile, curpos, SEEK_SET)) { bError = true; return 0; }
    return (int)size;
  }

  virtual bool AtEnd () override {
    if (bError || !mFile) return true;
    return !!feof(mFile);
  }

  virtual void Flush () override {
    if (!mFile && fflush(mFile)) bError = true;
  }

  virtual bool Close() override {
    if (mFile) { fclose(mFile); mFile = nullptr; }
    return !bError;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
static VStr SourceFileName;
static VStr ObjectFileName;

static int num_dump_asm;
static char *dump_asm_names[1024];
static bool DebugMode = false;
static FILE *DebugFile;

static VLexer Lex;
static VVccLog VccLog;


//==========================================================================
//
//  dprintf
//
//==========================================================================
__attribute__((format(printf, 1, 2))) int dprintf (const char *text, ...) {
  if (!DebugMode) return 0;

  va_list argPtr;
  FILE* fp = stderr; //(DebugFile ? DebugFile : stdout);
  va_start(argPtr, text);
  int ret = vfprintf(fp, text, argPtr);
  va_end(argPtr);
  fflush(fp);
  return ret;
}


//==========================================================================
//
//  Malloc
//
//==========================================================================
void* Malloc (size_t size) {
  if (!size) return nullptr;
  void *ptr = Z_Malloc(size);
  if (!ptr) FatalError("FATAL: couldn't alloc %d bytes", (int)size);
  memset(ptr, 0, size);
  return ptr;
}


//==========================================================================
//
//  Free
//
//==========================================================================
void Free (void* ptr) {
  if (ptr) Z_Free(ptr);
}


//==========================================================================
//
//  OpenFile
//
//==========================================================================
VStream* OpenFile (const VStr& Name) {
  FILE* file = fopen(*Name, "rb");
  return (file ? new VFileReader(file) : nullptr);
}


//==========================================================================
//
//  OpenDebugFile
//
//==========================================================================
static void OpenDebugFile (const VStr& name) {
  DebugFile = fopen(*name, "w");
  if (!DebugFile) FatalError("FATAL: can\'t open debug file \"%s\".", *name);
}


//==========================================================================
//
//  PC_DumpAsm
//
//==========================================================================
static void PC_DumpAsm (const char* name) {
  char buf[1024];
  char *cname;
  char *fname;

  snprintf(buf, sizeof(buf), "%s", name);

  //FIXME! PATH WITH DOTS!
  if (strstr(buf, ".")) {
    cname = buf;
    fname = strstr(buf, ".")+1;
    fname[-1] = 0;
  } else {
    dprintf("Dump ASM: Bad name %s\n", name);
    return;
  }

  for (int i = 0; i < VMemberBase::GMembers.Num(); ++i) {
    if (VMemberBase::GMembers[i]->MemberType == MEMBER_Method &&
        !VStr::Cmp(cname, *VMemberBase::GMembers[i]->Outer->Name) &&
        !VStr::Cmp(fname, *VMemberBase::GMembers[i]->Name))
    {
      ((VMethod*)VMemberBase::GMembers[i])->DumpAsm();
      return;
    }
  }

  dprintf("Dump ASM: %s not found!\n", name);
}


//==========================================================================
//
//  DumpAsm
//
//==========================================================================
static void DumpAsm () {
  for (int i = 0; i < num_dump_asm; ++i) PC_DumpAsm(dump_asm_names[i]);
}


//==========================================================================
//
//  DisplayUsage
//
//==========================================================================
static void DisplayUsage () {
  printf("\n");
  printf("VCC Version 1.%d. Copyright (c) 2000-2001 by JL, 2018 by Ketmar Dark. (" __DATE__ " " __TIME__ ")\n", PROG_VERSION);
  printf("Usage: vcc [options] source[.c] [object[.dat]]\n");
  printf("    -d<file>     Output debugging information into specified file\n");
  printf("    -a<function> Output function's ASM statements into debug file\n");
  printf("    -D<name>           Define macro\n");
  printf("    -I<directory>      Include files directory\n");
  printf("    -P<directory>      Package import files directory\n");
  exit(1);
}


//==========================================================================
//
//  ProcessArgs
//
//==========================================================================
static void ProcessArgs (int ArgCount, char **ArgVector) {
  int count = 0; // number of file arguments

  for (int i = 1; i < ArgCount; ++i) {
    char *text = ArgVector[i];
    if (*text == '-') {
      ++text;
      if (*text == 0) DisplayUsage();
      char option = *text++;
      switch (option) {
        case 'd':
          DebugMode = true;
          if (*text) OpenDebugFile(text);
          break;
        case 'a':
          if (!*text) DisplayUsage();
          dump_asm_names[num_dump_asm++] = text;
          break;
        case 'I':
          Lex.AddIncludePath(text);
          break;
        case 'D':
          Lex.AddDefine(text);
          break;
        case 'P':
          VMemberBase::StaticAddPackagePath(text);
          break;
        default:
          DisplayUsage();
          break;
      }
      continue;
    }
    ++count;
    switch (count) {
      case 1: SourceFileName = VStr(text).DefaultExtension(".vc"); break;
      case 2: ObjectFileName = VStr(text).DefaultExtension(".dat"); break;
      default: DisplayUsage(); break;
    }
  }

  if (count == 0) DisplayUsage();

  if (count == 1) ObjectFileName = SourceFileName.StripExtension()+".dat";

  /*
  if (!DebugFile) {
    VStr DbgFileName;
    DbgFileName = ObjectFileName.StripExtension()+".txt";
    OpenDebugFile(DbgFileName);
    DebugMode = true;
  }
  */

  SourceFileName = SourceFileName.FixFileSlashes();
  ObjectFileName = ObjectFileName.FixFileSlashes();
  dprintf("Main source file: %s\n", *SourceFileName);
  dprintf("  Resulting file: %s\n", *ObjectFileName);
}


//==========================================================================
//
//  initialize
//
//==========================================================================
static void initialize () {
  DebugMode = false;
  DebugFile = nullptr;
  num_dump_asm = 0;
  VName::StaticInit();
  //VMemberBase::StaticInit();
  VObject::StaticInit();
}


// ////////////////////////////////////////////////////////////////////////// //
int main (int argc, char **argv) {
  try {
    GLog.AddListener(&VccLog);

    int starttime;
    int endtime;

    M_InitByteOrder();

    starttime = time(0);

    initialize();

    ProcessArgs(argc, argv);

    PR_Init();

    VPackage *CurrentPackage = new VPackage(VName("vccrun"));

    VMemberBase::StaticLoadPackage(VName("engine"), TLocation());

    dprintf("Compiling '%s'...\n", *SourceFileName);
    Lex.OpenSource(SourceFileName);
    VParser Parser(Lex, CurrentPackage);

    Parser.Parse();

    int parsetime = time(0);
    dprintf("Parsed in %02d:%02d\n", (parsetime-starttime)/60, (parsetime-starttime)%60);

    CurrentPackage->Emit();
    int compiletime = time(0);
    dprintf("Compiled in %02d:%02d\n", (compiletime-parsetime)/60, (compiletime-parsetime)%60);

    /*
    CurrentPackage->WriteObject(*ObjectFileName);
    */

    DumpAsm();
    endtime = time(0);
    dprintf("Wrote in %02d:%02d\n", (endtime-compiletime)/60, (endtime-compiletime)%60);

    dprintf("Time elapsed: %02d:%02d\n", (endtime-starttime)/60, (endtime-starttime)%60);

    VObject::StaticExit();
    VName::StaticExit();
  } catch (VException& e) {
    FatalError("FATAL: %s", e.What());
  }

  return 0;
}
