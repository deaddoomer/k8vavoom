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
#include "gamedefs.h"
#include "net/network.h"


VCmdBuf GCmdBuf;

bool VCommand::ParsingKeyConf;

bool VCommand::Initialised = false;
VStr VCommand::Original;

TArray<VStr> VCommand::Args;
VCommand::ECmdSource VCommand::Source;
VBasePlayer *VCommand::Player;

TArray<VStr> VCommand::AutoCompleteTable;

VCommand *VCommand::Cmds = nullptr;
VCommand::VAlias *VCommand::Alias = nullptr;

void (*VCommand::onShowCompletionMatch) (bool isheader, const VStr &s);

static const char *KeyConfCommands[] = {
  "alias",
  "bind",
  "defaultbind",
  "addkeysection",
  "addmenukey",
  "addslotdefault",
  "weaponsection",
  "setslot",
  "addplayerclass",
  "clearplayerclasses"
};


//**************************************************************************
//
//  Commands, alias
//
//**************************************************************************

//==========================================================================
//
//  VCommand::VCommand
//
//==========================================================================
VCommand::VCommand (const char *name) {
  guard(VCommand::VCommand);
  Next = Cmds;
  Name = name;
  Cmds = this;
  if (Initialised) AddToAutoComplete(Name);
  unguard;
}


//==========================================================================
//
//  VCommand::~VCommand
//
//==========================================================================
VCommand::~VCommand () {
}


//==========================================================================
//
//  VCommand::AutoCompleteArg
//
//  return non-empty string to replace arg
//
//==========================================================================
VStr VCommand::AutoCompleteArg (const TArray<VStr> &args, int aidx) {
  return VStr::EmptyString;
}


//==========================================================================
//
//  VCommand::Init
//
//==========================================================================
void VCommand::Init () {
  guard(VCommand::Init);
  bool in_cmd = false;

  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) AddToAutoComplete(cmd->Name);

  // add configuration file execution
  GCmdBuf << "exec startup.vs\n";

  // add console commands from command line
  // these are params, that start with + and continue until the end or until next param that starts with - or +
  for (int i = 1; i < GArgs.Count(); ++i) {
    if (in_cmd) {
      if (!GArgs[i] || GArgs[i][0] == '-' || GArgs[i][0] == '+') {
        in_cmd = false;
        GCmdBuf << "\n";
      } else {
        GCmdBuf << " \"" << VStr(GArgs[i]).quote() << "\"";
        continue;
      }
    }
    if (GArgs[i][0] == '+') {
      in_cmd = true;
      GCmdBuf << (GArgs[i]+1);
    }
  }
  if (in_cmd) GCmdBuf << "\n";

  Initialised = true;
  unguard;
}


//==========================================================================
//
//  VCommand::WriteAlias
//
//==========================================================================
void VCommand::WriteAlias (FILE *f) {
  guard(VCommand::WriteAlias);
  for (VAlias *a = Alias; a; a = a->Next) {
    fprintf(f, "alias %s \"%s\"\n", *a->Name, *a->CmdLine.quote());
  }
  unguard;
}


//==========================================================================
//
//  VCommand::Shutdown
//
//==========================================================================
void VCommand::Shutdown () {
  guard(VCommand::Shutdown);
  for (VAlias *a = Alias; a;) {
    VAlias *Next = a->Next;
    delete a;
    a = Next;
  }
  AutoCompleteTable.Clear();
  Args.Clear();
  Original.Clean();
  unguard;
}


//==========================================================================
//
//  VCommand::ProcessKeyConf
//
//==========================================================================
void VCommand::ProcessKeyConf () {
  guard(VCommand::ProcessKeyConf);
  // enable special mode for console commands
  ParsingKeyConf = true;

  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_keyconf) {
      // read it
      VStream *Strm = W_CreateLumpReaderNum(Lump);
      char *Buf = new char[Strm->TotalSize()+1];
      Strm->Serialise(Buf, Strm->TotalSize());
      Buf[Strm->TotalSize()] = 0;
      delete Strm;
      Strm = nullptr;

      // parse it
      VCmdBuf CmdBuf;
      CmdBuf << Buf;
      CmdBuf.Exec();
      delete[] Buf;
      Buf = nullptr;
    }
  }

  // back to normal console command execution
  ParsingKeyConf = false;
  unguard;
}


//==========================================================================
//
//  VCommand::AddToAutoComplete
//
//==========================================================================
void VCommand::AddToAutoComplete (const char *string) {
  guard(VCommand::AddToAutoComplete);

  if (!string || !string[0] || string[0] == '_') return;

  for (int i = 0; i < AutoCompleteTable.length(); ++i) {
    if (AutoCompleteTable[i].ICmp(string) == 0) return; //Sys_Error("C_AddToAutoComplete: %s is allready registered.", string);
  }

  AutoCompleteTable.Append(VStr(string));

  // alphabetic sort
  for (int i = AutoCompleteTable.Num()-1; i && AutoCompleteTable[i-1].ICmp(AutoCompleteTable[i]) > 0; --i) {
    VStr swap = AutoCompleteTable[i];
    AutoCompleteTable[i] = AutoCompleteTable[i-1];
    AutoCompleteTable[i-1] = swap;
  }

  unguard;
}


//==========================================================================
//
//  VCommand::acCommand
//
//  autocomplete command
//  prefix should not be empty, and should not end with space
//
//==========================================================================
VStr VCommand::acCommand (const VStr &prefix) {
  VStr bestmatch;
  int matchcount = 0;

  // first, get longest match
  for (int f = 0; f < AutoCompleteTable.length(); ++f) {
    VStr mt = AutoCompleteTable[f];
    if (mt.length() < prefix.length()) continue;
    if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
    ++matchcount;
    if (bestmatch.length() < mt.length()) bestmatch = mt;
  }

  if (matchcount == 0) return prefix; // alas
  if (matchcount == 1) { bestmatch += " "; return bestmatch; } // done

  // trim match
  for (int f = 0; f < AutoCompleteTable.length(); ++f) {
    VStr mt = AutoCompleteTable[f];
    if (mt.length() < prefix.length()) continue;
    if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
    // cannot be longer than this
    if (bestmatch.length() > mt.length()) bestmatch = bestmatch.left(mt.length());
    int mlpos = 0;
    while (mlpos < bestmatch.length()) {
      if (VStr::upcase1251(bestmatch[mlpos]) != VStr::upcase1251(mt[mlpos])) {
        bestmatch = bestmatch.left(mlpos);
        break;
      }
      ++mlpos;
    }
  }

  // if match equals to prefix, this is second tab tap, so show all possible matches
  if (bestmatch == prefix) {
    // show all possible matches
    if (onShowCompletionMatch) {
      onShowCompletionMatch(true, "=== possible matches ===");
      for (int f = 0; f < AutoCompleteTable.length(); ++f) {
        VStr mt = AutoCompleteTable[f];
        if (mt.length() < prefix.length()) continue;
        if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
        onShowCompletionMatch(false, mt);
      }
    }
    return prefix;
  }

  // found extended match
  return bestmatch;
}


//==========================================================================
//
//  VCommand::GetAutoComplete
//
//  if returned string ends with space, this is the only match
//
//==========================================================================
VStr VCommand::GetAutoComplete (const VStr &prefix) {
  guard(VCommand::GetAutoComplete);

  if (prefix.length() == 0) return prefix; // oops

  TArray<VStr> args;
  prefix.tokenize(args);
  int aidx = args.length();
  if (aidx == 0) return prefix; // wtf?!

  bool endsWithBlank = ((vuint8)prefix[prefix.length()-1] <= ' ');

  if (aidx == 1 && !endsWithBlank) return acCommand(prefix);

  // autocomplete new arg?
  if (aidx > 1 && !endsWithBlank) --aidx; // nope, last arg

  // check for command
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) {
    if (args[0].ICmp(cmd->Name) == 0) {
      VStr ac = cmd->AutoCompleteArg(args, aidx);
      if (ac.length()) {
        // autocompleted, rebuild string
        //if (aidx < args.length()) args[aidx] = ac; else args.append(ac);
        bool addSpace = ((vuint8)ac[ac.length()-1] <= ' ');
        if (addSpace) ac.chopRight(1);
        VStr res;
        for (int f = 0; f < aidx; ++f) {
          res += args[f].quote(true); // add quote chars if necessary
          res += ' ';
        }
        res += ac.quote(true);
        if (addSpace && ac.length()) res += ' ';
        return res;
      }
      // cannot complete, nothing's changed
      return prefix;
    }
  }

  // Cvar
  if (aidx == 1) {
    // show cvar help, why not?
    VCvar *var = VCvar::FindVariable(*args[0]);
    if (var) {
      VStr help = var->GetHelp();
      if (help.length() && !VStr(help).startsWithNoCase("no help yet")) {
        onShowCompletionMatch(false, args[0]+": "+help);
      }
      return prefix;
    }
  }

  // nothing's found
  return prefix;
  unguard;
}


//**************************************************************************
//
//  Parsing of a command, command arg handling
//
//**************************************************************************

//==========================================================================
//
//  VCommand::TokeniseString
//
//==========================================================================
void VCommand::TokeniseString (const VStr &str) {
  Original = str;
  Args.reset();
  str.tokenize(Args);
}


//==========================================================================
//
//  VCommand::ExecuteString
//
//==========================================================================
void VCommand::ExecuteString (const VStr &Acmd, ECmdSource src, VBasePlayer *APlayer) {
  guard(VCommand::ExecuteString);

  //fprintf(stderr, "+++ command BEFORE tokenizing: <%s>\n", *Acmd);
  TokeniseString(Acmd);
  Source = src;
  Player = APlayer;

  //fprintf(stderr, "+++ command argc=%d (<%s>)\n", Args.length(), *Acmd);
  //for (int f = 0; f < Args.length(); ++f) fprintf(stderr, "  #%d: <%s>\n", f, *Args[f]);

  if (!Args.Num()) return;

  if (ParsingKeyConf) {
    // verify that it's a valid keyconf command
    bool Found = false;
    for (unsigned i = 0; i < ARRAY_COUNT(KeyConfCommands); ++i) {
      if (!Args[0].ICmp(KeyConfCommands[i])) {
        Found = true;
        break;
      }
    }
    if (!Found) {
      GCon->Logf("'%s' is not a valid KeyConf command!", *Args[0]);
      return;
    }
  }

  // check for command
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) {
    if (Args[0].ICmp(cmd->Name) == 0) {
      //fprintf(stderr, "+++ COMMAND FOUND: [%s] [%s] +++\n", *Args[0], cmd->Name);
      cmd->Run();
      return;
    }
  }

  // Cvar
  if (VCvar::Command(Args)) return;

  // command defined with ALIAS
  for (VAlias *a = Alias; a; a = a->Next) {
    if (!Args[0].ICmp(a->Name)) {
      GCmdBuf.Insert("\n");
      GCmdBuf.Insert(a->CmdLine);
      return;
    }
  }

  // unknown command
#ifndef CLIENT
  if (host_initialised)
#endif
    GCon->Logf("Unknown command '%s'", *Args[0]);
  unguard;
}


//==========================================================================
//
//  VCommand::ForwardToServer
//
//==========================================================================
void VCommand::ForwardToServer () {
  guard(VCommand::ForwardToServer);
#ifdef CLIENT
  if (!cl) {
    GCon->Log("You must be in a game to execute this command");
    return;
  }
  if (cl->Net) {
    //fprintf(stderr, "*** sending over the network: <%s>\n", *Original);
    cl->Net->SendCommand(Original);
  } else {
    //fprintf(stderr, "*** local executing: <%s>\n", *Original);
    VCommand::ExecuteString(Original, VCommand::SRC_Client, cl);
  }
#endif
  unguard;
}


//==========================================================================
//
//  VCommand::CheckParm
//
//==========================================================================
int VCommand::CheckParm (const char *check) {
  guard(VCommand::CheckParm);
  for (int i = 1; i < Args.Num(); ++i) {
    if (!Args[i].ICmp(check) == 0) return i;
  }
  return 0;
  unguard;
}


//==========================================================================
//
//  VCommand::GetArgC
//
//==========================================================================
int VCommand::GetArgC () {
  return Args.Num();
}


//==========================================================================
//
//  VCommand::GetArgV
//
//==========================================================================
VStr VCommand::GetArgV (int idx) {
  if (idx < 0 || idx >= Args.Num()) return VStr();
  return Args[idx];
}


//**************************************************************************
//
//  Command buffer
//
//**************************************************************************

//==========================================================================
//
//  VCmdBuf::Insert
//
//==========================================================================
void VCmdBuf::Insert (const char *text) {
  guard(VCmdBuf::Insert);
  Buffer = VStr(text)+Buffer;
  unguard;
}


//==========================================================================
//
//  VCmdBuf::Insert
//
//==========================================================================
void VCmdBuf::Insert (const VStr &text) {
  guard(VCmdBuf::Insert);
  Buffer = text+Buffer;
  unguard;
}


//==========================================================================
//
//  VCmdBuf::Print
//
//==========================================================================
void VCmdBuf::Print (const char *data) {
  guard(VCmdBuf::Print);
  Buffer += data;
  unguard;
}


//==========================================================================
//
//  VCmdBuf::Print
//
//==========================================================================
void VCmdBuf::Print (const VStr &data) {
  guard(VCmdBuf::Print);
  Buffer += data;
  unguard;
}


//==========================================================================
//
//  VCmdBuf::Exec
//
//==========================================================================
void VCmdBuf::Exec () {
  guard(VCmdBuf::Exec);
  int len;
  int quotes;
  bool comment;
  VStr ParsedCmd;

  do {
    quotes = 0;
    comment = false;
    ParsedCmd.Clean();

    for (len = 0; len < Buffer.length(); ++len) {
      if (Buffer[len] == '\n') break;
      if (comment) continue;
      if (Buffer[len] == ';' && !(quotes&1)) break;
      if (Buffer[len] == '/' && Buffer[len+1] == '/' && !(quotes&1)) {
        // comment, all till end is ignored
        comment = true;
        continue;
      }
      // screened char in string?
      if ((quotes&1) && Buffer[len] == '\\' && Buffer.length()-len > 1 && (Buffer[len+1] == '"' || Buffer[len+1] == '\\')) {
        ParsedCmd += Buffer[len++];
      } else {
        if (Buffer[len] == '\"') ++quotes;
      }
      ParsedCmd += Buffer[len];
    }

    if (len < Buffer.Length()) ++len; // skip seperator symbol

    Buffer = VStr(Buffer, len, Buffer.Length()-len);

    VCommand::ExecuteString(ParsedCmd, VCommand::SRC_Command, nullptr);

    if (host_request_exit) return;

    if (Wait) {
      // skip out while text still remains in buffer, leaving it for next frame
      Wait = false;
      break;
    }
  } while (len);
  unguard;
}


//**************************************************************************
//
//  Some commands
//
//**************************************************************************

//==========================================================================
//
//  COMMAND CmdList
//
//==========================================================================
COMMAND(CmdList) {
  guard(COMMAND CmdList);
  const char *prefix = (Args.Num() > 1 ? *Args[1] : "");
  int pref_len = VStr::Length(prefix);
  int count = 0;
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) {
    if (pref_len && VStr::NICmp(cmd->Name, prefix, pref_len)) continue;
    GCon->Logf(" %s", cmd->Name);
    ++count;
  }
  GCon->Logf("%d commands.", count);
  unguard;
}


//==========================================================================
//
//  Alias_f
//
//==========================================================================
COMMAND(Alias) {
  guard(COMMAND Alias);
  VCommand::VAlias *a;
  VStr tmp;
  int i;
  int c;

  if (Args.Num() == 1) {
    GCon->Log("Current alias:");
    for (a = VCommand::Alias; a; a = a->Next) GCon->Log(a->Name+": "+a->CmdLine);
    return;
  }

  c = Args.Num();
  for (i = 2; i < c; ++i) {
    if (i != 2) tmp += " ";
    tmp += Args[i];
  }

  for (a = VCommand::Alias; a; a = a->Next) {
    if (!a->Name.ICmp(Args[1])) break;
  }

  if (!a) {
    a = new VAlias;
    a->Name = Args[1];
    a->Next = VCommand::Alias;
    a->Save = !ParsingKeyConf;
    VCommand::Alias = a;
  }
  a->CmdLine = tmp;
  unguard;
}


//==========================================================================
//
//  Echo_f
//
//==========================================================================
COMMAND(Echo) {
  guard(COMMAND Echo);
  if (Args.Num() < 2) return;

  VStr Text = Args[1];
  for (int i = 2; i < Args.Num(); ++i) {
    Text += " ";
    Text += Args[i];
  }
  Text = Text.EvalEscapeSequences();
#ifdef CLIENT
  if (cl) {
    cl->Printf("%s", *Text);
  }
  else
#endif
  {
    GCon->Log(Text);
  }
  unguard;
}


//==========================================================================
//
//  Exec_f
//
//==========================================================================
COMMAND(Exec) {
  guard(COMMAND Exec);
  if (Args.length() < 2 || Args.length() > 3) {
    GCon->Log("Exec <filename> : execute script file");
    return;
  }

  VStream *Strm = nullptr;

  // try disk file
  VStr dskname = Host_GetConfigDir()+"/"+Args[1];
  if (Sys_FileExists(dskname)) {
    Strm = FL_OpenSysFileRead(dskname);
    if (Strm) GCon->Logf("Executing '%s'...", *dskname);
  }

  // try wad file
  if (!Strm && FL_FileExists(Args[1])) {
    Strm = FL_OpenFileRead(Args[1]);
    if (Strm) {
      GCon->Logf("Executing '%s'...", *Args[1]);
      //GCon->Logf("<%s>", *Strm->GetName());
    }
  }

  if (!Strm) {
    if (Args.length() == 2) GCon->Logf("Can't find '%s'...", *Args[1]);
    return;
  }

  //GCon->Logf("Executing '%s'...", *Args[1]);

  int flsize = Strm->TotalSize();
  if (flsize == 0) { delete Strm; return; }

  char *buf = new char[flsize+2];
  Strm->Serialise(buf, flsize);
  if (Strm->IsError()) {
    delete Strm;
    delete[] buf;
    GCon->Logf("Error reading '%s'!", *Args[1]);
    return;
  }
  delete Strm;

  if (buf[flsize-1] != '\n') buf[flsize++] = '\n';
  buf[flsize] = 0;

  GCmdBuf.Insert(buf);

  delete[] buf;
  unguard;
}


//==========================================================================
//
//  COMMAND Wait
//
//==========================================================================
COMMAND(Wait) {
  GCmdBuf.Wait = true;
}
