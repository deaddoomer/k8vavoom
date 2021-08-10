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
#include "server/server.h"
#include "mapinfo.h"
#include "host.h"
#include "filesys/files.h"
#include "sound/sound.h"
#ifdef CLIENT
# include "text.h"
# include "client/client.h"
#endif

extern VCvarB sv_cheats;

VCmdBuf GCmdBuf;

bool VCommand::execLogInit = true;

bool VCommand::ParsingKeyConf;

bool VCommand::Initialised = false;
VStr VCommand::Original;

bool VCommand::rebuildCache = true;
TMap<VStrCI, VCommand *> VCommand::locaseCache;

TArray<VStr> VCommand::Args;
VCommand::ECmdSource VCommand::Source;
VBasePlayer *VCommand::Player;

TArray<VStr> VCommand::AutoCompleteTable;
static TMap<VStr, bool> AutoCompleteTableBSet; // quicksearch

VCommand *VCommand::Cmds = nullptr;
//VCommand::VAlias *VCommand::Alias = nullptr;
TArray<VCommand::VAlias> VCommand::AliasList;
TMap<VStrCI, int> VCommand::AliasMap;

bool VCommand::cliInserted = false;
VStr VCommand::cliPreCmds;

void (*VCommand::onShowCompletionMatch) (bool isheader, VStr s);

VStr VCommand::CurrKeyConfKeySection;
VStr VCommand::CurrKeyConfWeaponSection;

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

static bool wasRunCliCommands = false;


//==========================================================================
//
//  sortCmpVStrCI
//
//==========================================================================
static int sortCmpVStrCI (const void *a, const void *b, void * /*udata*/) {
  if (a == b) return 0;
  VStr *sa = (VStr *)a;
  VStr *sb = (VStr *)b;
  return sa->ICmp(*sb);
}


//==========================================================================
//
//  isBoolTrueStr
//
//==========================================================================
static bool isBoolTrueStr (VStr s) {
  if (s.isEmpty()) return false;
  /*
  s = s.xstrip();
  if (s.isEmpty()) return false;
  if (s.strEquCI("false")) return false;
  if (s.strEquCI("ona")) return false;
  if (s.strEquCI("0")) return false;
  if (s.indexOf('.') >= 0) {
    float f = -1.0f;
    if (s.convertFloat(&f)) return (f != 0.0f);
  } else {
    int n = -1;
    if (s.convertInt(&n)) return (n != 0);
  }
  return true;
  */
  return VCvar::ParseBool(*s);
}


//**************************************************************************
//
//  Commands, alias
//
//**************************************************************************

//==========================================================================
//
//  CheatAllowed
//
//==========================================================================
static bool CheatAllowed (VBasePlayer *Player, bool allowDead=false) {
  if (!Player) return false;
  if (sv.intermission) {
    Player->Printf("You are not in game!");
    return false;
  }
  // client will forward them anyway
  if (GGameInfo->NetMode == NM_Client) return true;
  if (GGameInfo->NetMode == NM_ListenServer || GGameInfo->NetMode == NM_DedicatedServer) {
    if (!sv_cheats) {
      Player->Printf("You cannot cheat in a network game!");
      return false;
    }
    return true;
  }
  // if not a network server, cheats are always allowed
  //return (GGameInfo->NetMode >= NM_Standalone);
  /*
  if (GGameInfo->NetMode >= NM_DedicatedServer) {
    Player->Printf("You cannot cheat in a network game!");
    return false;
  }
  if (GGameInfo->WorldInfo->Flags&VWorldInfo::WIF_SkillDisableCheats) {
    Player->Printf("You are too good to cheat!");
    //k8: meh, if i want to cheat, i want to cheat!
    //return false;
    return true;
  }
  */
  if (!allowDead && Player->Health <= 0) {
    // dead players can't cheat
    Player->Printf("You must be alive to cheat");
    return false;
  }
  return true;
}


//==========================================================================
//
//  VCommand::VCommand
//
//==========================================================================
VCommand::VCommand (const char *name) {
  Next = Cmds;
  Name = name;
  Cmds = this;
  if (Initialised) AddToAutoComplete(Name);
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
VStr VCommand::AutoCompleteArg (const TArray<VStr> &/*args*/, int /*aidx*/) {
  return VStr::EmptyString;
}


//==========================================================================
//
//  VCommand::Init
//
//==========================================================================
void VCommand::Init () {
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) AddToAutoComplete(cmd->Name);

  // add configuration file execution
  GCmdBuf.Insert("exec k8vavoom_startup.vs\n__run_cli_commands__\n");

  Initialised = true;
}


//==========================================================================
//
//  VCommand::InsertCLICommands
//
//==========================================================================
void VCommand::InsertCLICommands () {
  VStr cstr;

  if (!cliPreCmds.isEmpty()) {
    cstr = cliPreCmds;
    if (!cstr.endsWith("\n")) cstr += '\n';
    cliPreCmds.clear();
  }

  // add console commands from command line
  // these are params, that start with + and continue until the end or until next param that starts with - or +
  bool in_cmd = false;
  for (int i = 1; i < GArgs.Count(); ++i) {
    if (in_cmd) {
      // check for number
      if (GArgs[i] && (GArgs[i][0] == '-' || GArgs[i][0] == '+')) {
        float v;
        if (VStr::convertFloat(GArgs[i], &v)) {
          cstr += ' ';
          cstr += '"';
          cstr += VStr(GArgs[i]).quote();
          cstr += '"';
          continue;
        }
      }
      if (!GArgs[i] || GArgs[i][0] == '-' || GArgs[i][0] == '+') {
        in_cmd = false;
        //GCmdBuf << "\n";
        cstr += '\n';
      } else {
        //GCmdBuf << " \"" << VStr(GArgs[i]).quote() << "\"";
        cstr += ' ';
        cstr += '"';
        cstr += VStr(GArgs[i]).quote();
        cstr += '"';
        continue;
      }
    }
    if (GArgs[i][0] == '+') {
      in_cmd = true;
      //GCmdBuf << (GArgs[i]+1);
      cstr += (GArgs[i]+1);
    }
  }
  //if (in_cmd) GCmdBuf << "\n";
  if (in_cmd) cstr += '\n';

  //GCon->Logf("===\n%s\n===", *cstr);

  if (!cstr.isEmpty()) GCmdBuf.Insert(cstr);
}


// ////////////////////////////////////////////////////////////////////////// //
static int vapcmp (const void *aa, const void *bb, void * /*udata*/) {
  const VCommand::VAlias *a = *(const VCommand::VAlias **)aa;
  const VCommand::VAlias *b = *(const VCommand::VAlias **)bb;
  if (a == b) return 0;
  return a->Name.ICmp(b->Name);
}

static int vstrptrcmpci (const void *aa, const void *bb, void * /*udata*/) {
  const VStr *a = (const VStr *)aa;
  const VStr *b = (const VStr *)bb;
  if (a == b) return 0;
  return a->ICmp(*b);
}


//==========================================================================
//
//  VCommand::WriteAlias
//
//==========================================================================
void VCommand::WriteAlias (VStream *st) {
  // build list
  TArray<VAlias *> alist;
  for (auto &&al : AliasList) if (al.Save) alist.append(&al);
  if (alist.length() == 0) return;
  // sort list
  timsort_r(alist.ptr(), alist.length(), sizeof(VAlias *), &vapcmp, nullptr);
  // write list
  for (auto &&al : alist) {
    st->writef("alias %s \"%s\"\n", *al->Name, *al->CmdLine.quote());
  }
}


//==========================================================================
//
//  VCommand::Shutdown
//
//==========================================================================
void VCommand::Shutdown () {
  AliasMap.clear();
  AliasList.clear();
  AutoCompleteTable.Clear();
  AutoCompleteTableBSet.clear();
  Args.Clear();
  Original.Clean();
  locaseCache.clear();
}


//==========================================================================
//
//  VCommand::LoadKeyconfLump
//
//==========================================================================
void VCommand::LoadKeyconfLump (int Lump) {
  if (Lump < 0) return;
  // read it
  VStream *Strm = W_CreateLumpReaderNum(Lump);
  if (!Strm) return;

  VStr buf;
  buf.setLength(Strm->TotalSize(), 0);
  Strm->Serialize(buf.getMutableCStr(), buf.length());
  if (Strm->IsError()) buf.clear();
  VStream::Destroy(Strm);

  GCon->Logf(NAME_Init, "loading keyconf from '%s'...", *W_FullLumpName(Lump));

  // parse it
  VCmdBuf CmdBuf;
  TArray<VStr> lines;
  TArray<VStr> args;
  buf.split('\n', lines);
  for (auto &&s : lines) {
    s = s.xstrip();
    if (s.length() == 0 || s[0] == '#' || s[0] == '/') continue;
    args.reset();
    s.tokenise(args);
    if (args.length() == 0) continue;
    /*
    if (args[0].strEquCI("defaultbind")) {
      GCon->Logf(NAME_Warning, "ignored keyconf command: %s", *s);
    } else
    */
    {
      CmdBuf << s << "\n";
    }
  }

  // enable special mode for console commands
  ParsingKeyConf = true;
  CmdBuf.Exec();
  // back to normal console command execution
  ParsingKeyConf = false;
}


//==========================================================================
//
//  VCommand::ProcessKeyConf
//
//==========================================================================
/*
void VCommand::ProcessKeyConf () {
  for (auto &&it : WadNSNameIterator(NAME_keyconf, WADNS_Global)) {
    const int Lump = it.lump;
    LoadKeyconfLump(Lump);
  }
}
*/


//==========================================================================
//
//  VCommand::AddToAutoComplete
//
//==========================================================================
void VCommand::AddToAutoComplete (const char *string) {
  if (!string || !string[0] || string[0] == '_' || (vuint8)string[0] < 32 || (vuint8)string[0] >= 127) return;

  VStr vs(string);
  VStr vslow = vs.toLowerCase();

  if (AutoCompleteTableBSet.has(vslow)) return;

  AutoCompleteTableBSet.put(vslow, true);
  AutoCompleteTable.Append(vs);

  // alphabetic sort
  for (int i = AutoCompleteTable.length()-1; i && AutoCompleteTable[i-1].ICmp(AutoCompleteTable[i]) > 0; --i) {
    VStr swap = AutoCompleteTable[i];
    AutoCompleteTable[i] = AutoCompleteTable[i-1];
    AutoCompleteTable[i-1] = swap;
  }
}


//==========================================================================
//
//  VCommand::RemoveFromAutoComplete
//
//==========================================================================
void VCommand::RemoveFromAutoComplete (const char *string) {
  if (!string || !string[0] || string[0] == '_') return;

  VStr vs(string);
  VStr vslow = vs.toLowerCase();

  if (!AutoCompleteTableBSet.has(vslow)) return; // nothing to do

  AutoCompleteTableBSet.del(vslow);
  for (int f = 0; f < AutoCompleteTable.length(); ++f) {
    if (AutoCompleteTable[f].strEquCI(vs)) {
      AutoCompleteTable.removeAt(f);
      --f;
    }
  }
}


//==========================================================================
//
//  acPartialQuote
//
//==========================================================================
static VStr acPartialQuote (VStr s, bool doQuoting, bool hasSpacedMatch=false) {
  if (!doQuoting) return s;
  if (!hasSpacedMatch && !s.needQuoting()) return s;
  //return VStr("\"")+s.quote();
  // close quote, autocompleter can cope with that
  if (hasSpacedMatch && !s.needQuoting()) return VStr("\"")+s+"\"";
  return s.quote(true);
}


//==========================================================================
//
//  VCommand::AutoCompleteFromList
//
//==========================================================================
VStr VCommand::AutoCompleteFromList (VStr prefix, const TArray <VStr> &list, bool unchangedAsEmpty, bool doSortHint, bool doQuoting) {
  if (list.length() == 0) return (unchangedAsEmpty ? VStr::EmptyString : acPartialQuote(prefix, doQuoting));

  VStr bestmatch;
  int matchcount = 0;

  // first, get longest match
  for (auto &&mt : list) {
    if (mt.length() < prefix.length()) continue;
    if (VStr::NICmp(*prefix, *mt, prefix.length()) != 0) continue;
    ++matchcount;
    if (bestmatch.length() < mt.length()) bestmatch = mt;
  }

  if (matchcount == 0) return (unchangedAsEmpty ? VStr::EmptyString : acPartialQuote(prefix, doQuoting)); // alas
  if (matchcount == 1) { if (doQuoting) bestmatch = bestmatch.quote(true); bestmatch += " "; return bestmatch; } // done

  // trim match; check for spaces too
  bool hasSpacedMatch = false;
  for (auto &&mt : list) {
    if (mt.length() < prefix.length()) continue;
    if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
    // cannot be longer than this
    if (!hasSpacedMatch && (mt.indexOf(' ') >= 0 || mt.indexOf('\t') >= 0)) hasSpacedMatch = true;
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

  /*
  GCon->Logf("prefix: <%s>", *prefix);
  GCon->Logf("bestmatch: <%s>", *bestmatch);
  GCon->Logf("hasSpacedMatch: %d", (int)hasSpacedMatch);
  */

  // if match equals to prefix, this is second tab tap, so show all possible matches
  if (bestmatch == prefix) {
    // show all possible matches
    if (onShowCompletionMatch) {
      onShowCompletionMatch(true, "=== possible matches ===");
      bool skipPrint = false;
      if (doSortHint && list.length() > 1) {
        bool needSorting = false;
        for (int f = 1; f < list.length(); ++f) if (list[f-1].ICmp(list[f]) > 0) { needSorting = true; break; }
        if (needSorting) {
          TArray<VStr> sortedlist;
          sortedlist.resize(list.length());
          for (int f = 0; f < list.length(); ++f) sortedlist.append(list[f]);
          timsort_r(sortedlist.ptr(), sortedlist.length(), sizeof(VStr), &vstrptrcmpci, nullptr);
          for (int f = 0; f < sortedlist.length(); ++f) {
            VStr mt = sortedlist[f];
            if (mt.length() < prefix.length()) continue;
            if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
            onShowCompletionMatch(false, mt);
          }
          skipPrint = true;
        }
      }
      if (!skipPrint) {
        for (int f = 0; f < list.length(); ++f) {
          VStr mt = list[f];
          if (mt.length() < prefix.length()) continue;
          if (VStr::NICmp(*prefix, *mt, prefix.Length()) != 0) continue;
          onShowCompletionMatch(false, mt);
        }
      }
    }
    return (unchangedAsEmpty ? VStr::EmptyString : acPartialQuote(prefix, doQuoting, hasSpacedMatch));
  }

  // found extended match
  return acPartialQuote(bestmatch, doQuoting, hasSpacedMatch);
}


//==========================================================================
//
//  findPlayer
//
//==========================================================================
static VBasePlayer *findPlayer () {
  if (sv.intermission) return nullptr;
  if (GGameInfo->NetMode < NM_Standalone) return nullptr; // not playing
  #ifdef CLIENT
  if (GGameInfo->NetMode == NM_Client) return (cl && cl->Net ? cl : nullptr);
  #endif
  // find any active player
  for (int f = 0; f < MAXPLAYERS; ++f) {
    VBasePlayer *plr = GGameInfo->Players[f];
    if (!plr) continue;
    if ((plr->PlayerFlags&VBasePlayer::PF_IsBot) ||
        !(plr->PlayerFlags&VBasePlayer::PF_Spawned))
    {
      continue;
    }
    if (plr->PlayerState != PST_LIVE || plr->Health <= 0) continue;
    return plr;
  }
  return nullptr;
}


//==========================================================================
//
//  VCommand::GetAutoComplete
//
//  if returned string ends with space, this is the only match
//
//==========================================================================
VStr VCommand::GetAutoComplete (VStr prefix) {
  if (prefix.length() == 0) return prefix; // oops

  //GCon->Logf("000: PFX=<%s>", *prefix);

  TArray<VStr> args;
  prefix.tokenize(args);
  int aidx = args.length();
  if (aidx == 0) return prefix; // wtf?!

  //GCon->Logf("001: len=%d; [$-1]=<%s>", args.length(), *args[args.length()-1]);

  bool endsWithBlank = ((vuint8)prefix[prefix.length()-1] <= ' ');

  if (aidx == 1 && !endsWithBlank) {
    VBasePlayer *plr = findPlayer();
    if (plr) {
      auto otbllen = AutoCompleteTable.length();
      plr->ListConCommands(AutoCompleteTable, prefix);
      //GCon->Logf("***PLR: pfx=<%s>; found=%d", *prefix, AutoCompleteTable.length()-otbllen);
      if (AutoCompleteTable.length() > otbllen) {
        // copy and sort
        TArray<VStr> newlist;
        newlist.setLength(AutoCompleteTable.length());
        for (int f = 0; f < AutoCompleteTable.length(); ++f) newlist[f] = AutoCompleteTable[f];
        AutoCompleteTable.setLength(otbllen, false); // don't resize
        timsort_r(newlist.ptr(), newlist.length(), sizeof(VStr), &sortCmpVStrCI, nullptr);
        return AutoCompleteFromList(prefix, newlist);
      }
    }
    return AutoCompleteFromList(prefix, AutoCompleteTable);
  }

  // autocomplete new arg?
  if (aidx > 1 && !endsWithBlank) --aidx; // nope, last arg

  // check for command
  rebuildCommandCache();

  auto cptr = locaseCache.find(args[0]);
  if (cptr) {
    VCommand *cmd = *cptr;
    VStr ac = cmd->AutoCompleteArg(args, aidx);
    if (ac.length()) {
      // autocompleted, rebuild string
      //if (aidx < args.length()) args[aidx] = ac; else args.append(ac);
      //!bool addSpace = ((vuint8)ac[ac.length()-1] <= ' ');
      //!if (addSpace) ac.chopRight(1);
      VStr res;
      for (int f = 0; f < aidx; ++f) {
        res += args[f].quote(true); // add quote chars if necessary
        res += ' ';
      }
      /*!
      if (ac.length()) {
        ac = ac.quote(true);
        if (!addSpace && ac[ac.length()-1] == '"') ac.chopRight(1);
      }
      res += ac;
      if (addSpace) res += ' ';
      !*/
      res += ac;
      return res;
    }
    // cannot complete, nothing's changed
    return prefix;
  }

  // try player
  {
    VBasePlayer *plr = findPlayer();
    if (plr) {
      TArray<VStr> aclist;
      if (plr->ExecConCommandAC(args, endsWithBlank, aclist)) {
        if (aclist.length() == 0) return prefix; // nothing's found
        // rebuild string
        VStr res;
        for (int f = 0; f < aidx; ++f) {
          res += args[f].quote(true); // add quote chars if necessary
          res += ' ';
        }
        // several matches
        // sort
        timsort_r(aclist.ptr(), aclist.length(), sizeof(VStr), &sortCmpVStrCI, nullptr);
        //for (int f = 0; f < aclist.length(); ++f) GCon->Logf(" %d:<%s>", f, *aclist[f]);
        VStr ac = AutoCompleteFromList((endsWithBlank ? VStr() : args[args.length()-1]), aclist, false, true, true);
        /*!
        bool addSpace = ((vuint8)ac[ac.length()-1] <= ' ');
        if (addSpace) ac.chopRight(1);
        if (ac.length()) {
          ac = ac.quote(true);
          if (!addSpace && ac[ac.length()-1] == '"') ac.chopRight(1);
        }
        if (ac.length()) {
          res += ac;
          if (addSpace) res += ' ';
        }
        !*/
        res += ac;
        return res;
      }
    }
  }

  // Cvar
  if (aidx == 1) {
    // show cvar help, why not?
    if (onShowCompletionMatch) {
      VCvar *var = VCvar::FindVariable(*args[0]);
      if (var) {
        VStr help = var->GetHelp();
        if (help.length() && !VStr(help).startsWithNoCase("no help yet")) {
          onShowCompletionMatch(false, args[0]+": "+help);
        }
      }
    }
  }

  // nothing's found
  return prefix;
}


//**************************************************************************
//
//  Parsing of a command, command arg handling
//
//**************************************************************************


//==========================================================================
//
//  VCommand::SubstituteArgs
//
//  substiture "${1}" and such args; used for aliases
//  use "${1q}" to insert properly quoted argument (WITHOUT quotation marks)
//
//==========================================================================
VStr VCommand::SubstituteArgs (VStr str) {
  if (str.isEmpty()) return VStr::EmptyString;
  if (str.indexOf('$') < 0) return str;
  const char *s = *str;
  VStr res;
  while (*s) {
    const char ch = *s++;
    if (ch == '$' && (*s != '(' || *s == '{')) {
      const char *sstart = s-1;
      const char ech = (*s == '{' ? '}' : ')');
      ++s;
      if (!s[0]) break;
      const char *se = s;
      while (*se && *se != ech) ++se;
      if (se != s) {
        VStr cvn(s, (int)(ptrdiff_t)(se-s));
        cvn = cvn.xstrip();
        if (cvn.length() && cvn[0] >= '0' && cvn[0] <= '9') {
          int n = -1;
          bool quoted = false;
          if (cvn.length() > 1 && cvn[cvn.length()-1] == 'q') {
            quoted = true;
            cvn.chopRight(1);
          }
          if (cvn.convertInt(&n)) {
            if (n >= 0 && n < Args.length()) { // overflow protection
              if (quoted) res += Args[n].quote(); else res += Args[n];
            }
            s = se+(*se != 0);
            continue;
          }
        }
      }
      if (*se) ++se;
      res += VStr(sstart, (int)(ptrdiff_t)(se-sstart));
      s = se;
    } else {
      res += ch;
      if (ch == '\\' && *s) res += *s++;
    }
  }
  return res;
}


//==========================================================================
//
//  VCommand::ExpandSigil
//
//==========================================================================
VStr VCommand::ExpandSigil (VStr str) {
  if (str.isEmpty()) return VStr::EmptyString;
  if (str.indexOf('$') < 0) return str;
  const char *s = *str;
  VStr res;
  while (*s) {
    const char ch = *s++;
    if (ch == '$' && (*s != '(' || *s == '{')) {
      const char ech = (*s == '{' ? '}' : ')');
      ++s;
      if (!s[0]) break;
      const char *se = s;
      while (*se && *se != ech) ++se;
      if (se != s) {
        VStr cvn(s, (int)(ptrdiff_t)(se-s));
        cvn = cvn.xstrip();
        VCvar *cv = VCvar::FindVariable(*cvn);
        if (cv) res += cv->asStr();
      }
      s = se+(*se != 0);
    } else {
      res += ch;
      if (ch == '\\' && *s) res += *s++;
    }
  }
  return res;
}


//==========================================================================
//
//  VCommand::TokeniseString
//
//==========================================================================
void VCommand::TokeniseString (VStr str) {
  Original = str;
  Args.reset();
  str.tokenize(Args);
  // expand sigils (except the command name)
  //HACK: for "alias" command, don't perform any expanding
  if (!Args[0].strEquCI("alias")) {
    for (int f = 1; f < Args.length(); ++f) Args[f] = ExpandSigil(Args[f]);
  }
}


//==========================================================================
//
//  VCommand::rebuildCommandCache
//
//==========================================================================
void VCommand::rebuildCommandCache () {
  if (!rebuildCache) return;
  rebuildCache = false;
  locaseCache.clear();
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) {
    locaseCache.put(cmd->Name, cmd);
  }
}


//==========================================================================
//
//  VCommand::GetCommandType
//
//==========================================================================
int VCommand::GetCommandType (VStr cmd) {
  if (cmd.isEmpty()) return CT_UNKNOWN;

  rebuildCommandCache();

  auto cptr = locaseCache.find(cmd);
  if (cptr) return CT_COMMAND;

  VBasePlayer *plr = findPlayer();
  if (plr && plr->IsConCommand(cmd)) return CT_COMMAND;

  if (VCvar::HasVar(*cmd)) return CT_CVAR;

  auto idp = AliasMap.find(cmd);
  if (idp) return CT_ALIAS;

  return CT_UNKNOWN;
}


//==========================================================================
//
//  VCommand::ProcessSetCommand
//
//  "set [(type)] var value" (i.e. "set (int) myvar 0")
//    (type) is:
//      (int)
//      (float)
//      (bool)
//      (string)
//  default is "(string)"
//
//==========================================================================
void VCommand::ProcessSetCommand () {
  enum {
    SST_Int,
    SST_Float,
    SST_Bool,
    SST_Str,
  };

  if (Args.length() < 3 || Args.length() > 4) {
    if (host_initialised) GCon->Log(NAME_Error, "'set' command usage: \"set [(type)] name value\"");
    return;
  }

  int stype = SST_Str;
  int nidx = 1;

  if (Args.length() == 4) {
    nidx = 2;
         if (Args[1].strEquCI("(int)")) stype = SST_Int;
    else if (Args[1].strEquCI("(float)")) stype = SST_Float;
    else if (Args[1].strEquCI("(bool)")) stype = SST_Bool;
    else if (Args[1].strEquCI("(string)")) stype = SST_Str;
    else if (Args[1].strEquCI("(str)")) stype = SST_Str;
    else {
      if (host_initialised) GCon->Logf(NAME_Error, "invalid variable type `%s` in 'set'", *Args[1]);
      return;
    }
  }

  VStr vname = Args[nidx].xstrip();
  VStr vvalue = Args[nidx+1];

  if (vname.isEmpty()) {
    if (host_initialised) GCon->Log(NAME_Error, "'set' expects variable name");
    return;
  }

  float fv = 0.0f;
  int iv = 0;
  bool bv = false;

  switch (stype) {
    case SST_Int:
      if (!vvalue.convertInt(&iv)) {
        if (host_initialised) GCon->Logf(NAME_Error, "'set' expects int, but got `%s`", *vvalue);
        return;
      }
      break;
    case SST_Float:
      if (!vvalue.convertFloat(&fv)) {
        if (host_initialised) GCon->Logf(NAME_Error, "'set' expects float, but got `%s`", *vvalue);
        return;
      }
      break;
    case SST_Bool:
      bv = isBoolTrueStr(vvalue);
      break;
    case SST_Str:
      break;
    default: Sys_Error("VCommand::ProcessSetCommand: wtf vconvert?!");
  }

  VCvar *cv = VCvar::FindVariable(*vname);
  if (!cv) {
    switch (stype) {
      case SST_Int: cv = VCvar::CreateNewInt(VName(*vname), 0, "user-created variable", CVAR_User); break;
      case SST_Float: cv = VCvar::CreateNewFloat(VName(*vname), 0.0f, "user-created variable", CVAR_User); break;
      case SST_Bool: cv = VCvar::CreateNewBool(VName(*vname), false, "user-created variable", CVAR_User); break;
      case SST_Str: cv = VCvar::CreateNewStr(VName(*vname), "", "user-created variable", CVAR_User); break;
      default: Sys_Error("VCommand::ProcessSetCommand: wtf vcreate?!");
    }
  } else {
    if (cv->IsReadOnly()) {
      if (host_initialised) GCon->Logf(NAME_Error, "'set' tried to modify read-only cvar '%s'", *vname);
      return;
    }
  }
  vassert(cv);

  switch (stype) {
    case SST_Int: cv->SetInt(iv); break;
    case SST_Float: cv->SetFloat(fv); break;
    case SST_Bool: cv->SetBool(bv); break;
    case SST_Str: cv->SetStr(vvalue); break;
    default: Sys_Error("VCommand::ProcessSetCommand: wtf vset?!");
  }
}


//==========================================================================
//
//  VCommand::ExecuteString
//
//==========================================================================
void VCommand::ExecuteString (VStr Acmd, ECmdSource src, VBasePlayer *APlayer) {
  //GCon->Logf(NAME_Debug, "+++ command BEFORE tokenizing: <%s>; plr=%s\n", *Acmd, (APlayer ? APlayer->GetClass()->GetName() : "<none>"));

  TokeniseString(Acmd);
  Source = src;
  Player = APlayer;

  //GCon->Logf(NAME_Debug, "+++ command argc=%d (<%s>)\n", Args.length(), *Acmd);
  //for (int f = 0; f < Args.length(); ++f) GCon->Logf(NAME_Debug, "  #%d: <%s>\n", f, *Args[f]);

  if (Args.length() == 0) return;

  if (Args[0] == "__run_cli_commands__") {
    if (!wasRunCliCommands) {
      wasRunCliCommands = true;
      FL_ProcessPreInits(); // override configs
      FL_ClearPreInits();
      GSoundManager->InitThreads();
      if (!cliInserted) {
        #ifdef CLIENT
        if (!R_IsDrawerInited()) {
          wasRunCliCommands = false;
          GCmdBuf.Insert("wait\n__run_cli_commands__\n");
          return;
        }
        #endif
        execLogInit = false;
        cliInserted = true;
        InsertCLICommands();
      }
    }
    return;
  }

  VStr ccmd = Args[0];

  // conditionals
  if (ccmd.length() >= 3 && ccmd[0] == '$' && VStr::tolower(ccmd[1]) == 'i' && VStr::tolower(ccmd[2]) == 'f') {
    if (ccmd.strEquCI("$if") || ccmd.strEquCI("$ifnot")) {
      if (Args.length() < 3) return;
      const bool neg = (ccmd.length() > 3);
      const bool bvtrue = (isBoolTrueStr(Args[1]) ? !neg : neg);
      if (!bvtrue) return;
      // remove condition command and expression
      Args.removeAt(0);
      Args.removeAt(0);
    } else if (ccmd.strEquCI("$if_or") || ccmd.strEquCI("$ifnot_or")) {
      if (Args.length() < 4) return;
      const bool neg = (ccmd.length() > 6);
      const bool bvtrue = (isBoolTrueStr(Args[1]) || isBoolTrueStr(Args[2]) ? !neg : neg);
      if (!bvtrue) return;
      // remove condition command and expressions
      Args.removeAt(0);
      Args.removeAt(0);
      Args.removeAt(0);
    } else if (ccmd.strEquCI("$if_and") || ccmd.strEquCI("$ifnot_and")) {
      if (Args.length() < 4) return;
      const bool neg = (ccmd.length() > 7);
      const bool bvtrue = (isBoolTrueStr(Args[1]) && isBoolTrueStr(Args[2]) ? !neg : neg);
      if (!bvtrue) return;
      // remove condition command and expressions
      Args.removeAt(0);
      Args.removeAt(0);
      Args.removeAt(0);
    }
    ccmd = Args[0];
    if (ccmd.isEmpty()) return; // just in case
  }

  if (ParsingKeyConf) {
    // verify that it's a valid keyconf command
    bool Found = false;
    for (unsigned i = 0; i < ARRAY_COUNT(KeyConfCommands); ++i) {
      if (ccmd.strEquCI(KeyConfCommands[i])) {
        Found = true;
        break;
      }
    }
    if (!Found) {
      GCon->Logf(NAME_Warning, "Invalid KeyConf command: %s", *Acmd);
      return;
    }
  }

  if (ccmd.strEquCI("set")) {
    ProcessSetCommand();
    return;
  }

  // check for command
  rebuildCommandCache();

  auto cptr = locaseCache.find(ccmd);
  if (cptr) {
    if (cptr && !Player) Player = findPlayer(); // for local commands
    (*cptr)->Run();
    return;
  }

  // check for player command
  if (Source == SRC_Command) {
    VBasePlayer *plr = findPlayer();
    if (plr && plr->IsConCommand(ccmd)) {
      ForwardToServer();
      return;
    }
  } else if (Player && Player->IsConCommand(ccmd)) {
    if (CheatAllowed(Player)) Player->ExecConCommand();
    return;
  }

  // Cvar
  if (FL_HasPreInit(ccmd)) return;

  // this hack allows setting cheating variables from command line or autoexec
  {
    bool oldCheating = VCvar::GetCheating();
    bool cheatingChanged = false;
    VBasePlayer *plr = findPlayer();
    #ifdef CLIENT
    if (!plr || !cl || !cl->Net)
    #else
    if (!plr)
    #endif
    {
      cheatingChanged = true;
      VCvar::SetCheating(/*VCvar::GetBool("sv_cheats")*/true);
      //GCon->Log(NAME_Debug, "forced: cheating and unlatching");
    }
    //GCon->Logf("sv_cheats: %d; plr is %shere", (int)VCvar::GetBool("sv_cheats"), (plr ? "" : "not "));
    bool doneCvar = VCvar::Command(Args);
    if (cheatingChanged) VCvar::SetCheating(oldCheating);
    if (doneCvar) {
      if (cheatingChanged) VCvar::Unlatch();
      return;
    }
  }

  // check for command defined with ALIAS
  if (!ccmd.isEmpty()) {
    auto idp = AliasMap.find(ccmd);
    if (idp) {
      VAlias &al = AliasList[*idp];
      GCmdBuf.Insert("\n");
      GCmdBuf.Insert(SubstituteArgs(al.CmdLine));
      return;
    }
  }

  // unknown command
#ifndef CLIENT
  if (host_initialised)
#endif
    GCon->Logf(NAME_Error, "Unknown command '%s'", *ccmd);
}


//==========================================================================
//
//  VCommand::ForwardToServer
//
//==========================================================================
void VCommand::ForwardToServer () {
#ifdef CLIENT
  if (!cl) {
    GCon->Log("You must be in a game to execute this command");
    return;
  }
  if (!CL_SendCommandToServer(Original)) {
    VCommand::ExecuteString(Original, VCommand::SRC_Client, cl);
  }
#else
  // for dedicated server, just execute it
  // yeah, it is marked "VCommand::SRC_Client", but this is just a bad name
  // actually, forwardig code only checks for `SRC_Command`, and forwards here
  VCommand::ExecuteString(Original, VCommand::SRC_Client, nullptr);
#endif
}


//==========================================================================
//
//  VCommand::CheckParm
//
//==========================================================================
int VCommand::CheckParm (const char *check) {
  if (check && check[0]) {
    for (int i = 1; i < Args.length(); ++i) {
      if (Args[i].strEquCI(check)) return i;
    }
  }
  return 0;
}


//==========================================================================
//
//  VCommand::GetArgC
//
//==========================================================================
int VCommand::GetArgC () {
  return Args.length();
}


//==========================================================================
//
//  VCommand::GetArgV
//
//==========================================================================
VStr VCommand::GetArgV (int idx) {
  if (idx < 0 || idx >= Args.length()) return VStr();
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
  if (text && text[0]) Buffer = VStr(text)+Buffer;
}


//==========================================================================
//
//  VCmdBuf::Insert
//
//==========================================================================
void VCmdBuf::Insert (VStr text) {
  if (text.length()) Buffer = text+Buffer;
}


//==========================================================================
//
//  VCmdBuf::Print
//
//==========================================================================
void VCmdBuf::Print (const char *data) {
  if (data && data[0]) Buffer += data;
}


//==========================================================================
//
//  VCmdBuf::Print
//
//==========================================================================
void VCmdBuf::Print (VStr data) {
  if (data.length()) Buffer += data;
}


//==========================================================================
//
//  VCmdBuf::checkWait
//
//  returns `false` (and resets `Wait`) if wait expired
//
//==========================================================================
bool VCmdBuf::checkWait () {
  if (!WaitEndTime) return false;
  const double currTime = Sys_Time();
  if (currTime >= WaitEndTime) {
    WaitEndTime = 0;
    return false;
  }
  return true;
}


//==========================================================================
//
//  VCmdBuf::Exec
//
//==========================================================================
void VCmdBuf::Exec () {
  //GCon->Logf(NAME_Debug, "=============================\nEXECBUF: \"%s\"\n----------------", *Buffer.quote());
  while (Buffer.length()) {
    if (checkWait()) {
      //GCon->Logf(NAME_Debug, "*** WAIT HIT! bufleft: \"%s\"", *Buffer.quote());
      break;
    }

    int quotes = 0;
    bool comment = false;
    int len = 0;
    int stpos = -1;
    const int blen = Buffer.length();
    const char *bs = *Buffer;

    for (; len < blen; ++len) {
      const char ch = *bs++;
      // EOL?
      if (ch == '\n') {
        if (stpos >= 0) break;
        vassert(!quotes);
        comment = false;
        continue;
      }
      // in the comment?
      if (comment) continue;
      // inside the quotes?
      if (quotes) {
             if (ch == quotes) quotes = 0;
        else if (ch == '\\') { ++bs; ++len; } // skip escaping (it is safe to not check `len` here)
        continue;
      }
      // separator?
      if (ch == ';') {
        if (stpos >= 0) break;
        continue;
      }
      // comment? (it is safe to check `*bs` here)
      if (ch == '/' && *bs == '/') {
        if (stpos >= 0) break; // comment will be skipped on the next iteration
        comment = true;
        continue;
      }
      // if non-blank char, mark string start
      if ((vuint8)ch > ' ' && stpos < 0) stpos = len;
      if (ch == '"' || ch == '\'') quotes = ch; // quoting
      // tokenizer doesn't allow esaping outside of quotes
      //else if (ch == '\\') { ++bs; ++len; } // skip escaping (it is safe to not check `len` here)
    }

    if (stpos < 0) {
      // no non-blanks
      if (len > 0) Buffer.chopLeft(len);
      //GCon->Logf(NAME_Debug, "***   CHOPPED: \"%s\"", *Buffer.quote());
      continue;
    }
    vassert(len);

    VStr ParsedCmd(*Buffer+stpos, len-stpos);
    Buffer.chopLeft(len);

    //GCon->Logf(NAME_Debug, "***   CMD: \"%s\"", *ParsedCmd.quote());
    //GCon->Logf(NAME_Debug, "***   chopped: \"%s\"", *Buffer.quote());
    // safety net
    const int prevlen = Buffer.length();
    VCommand::ExecuteString(ParsedCmd, VCommand::SRC_Command, nullptr);

    if (host_request_exit) return;

    // check length
    const int currlen = Buffer.length();
    if (currlen > prevlen && currlen > 1024*1024*16) Sys_Error("console command buffer overflow (probably invalid alias or something)");
  }
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
  const char *prefix = (Args.length() > 1 ? *Args[1] : "");
  int pref_len = VStr::Length(prefix);
  int count = 0;
  for (VCommand *cmd = Cmds; cmd; cmd = cmd->Next) {
    if (pref_len && VStr::NICmp(cmd->Name, prefix, pref_len)) continue;
    GCon->Logf(" %s", cmd->Name);
    ++count;
  }
  GCon->Logf("%d commands.", count);
}


//==========================================================================
//
//  VCommand::rebuildAliasMap
//
//==========================================================================
void VCommand::rebuildAliasMap () {
  AliasMap.reset();
  for (auto &&it : AliasList.itemsIdx()) {
    VAlias &al = it.value();
    VStr aliasName = al.Name/*.toLowerCase()*/;
    if (aliasName.length() == 0) continue; // just in case
    AliasMap.put(aliasName, it.index());
  }
}


//==========================================================================
//
//  Alias_f
//
//==========================================================================
COMMAND(Alias) {
  if (Args.length() == 1) {
    GCon->Logf("\034K%s", "Current aliases:");
    for (auto &&al : AliasList) {
      GCon->Logf("\034D  %s: %s%s", *al.Name, *al.CmdLine, (al.Save ? "" : " (temporary)"));
    }
    return;
  }

  //VStr aliasName = Args[1].toLowerCase();
  auto idxp = AliasMap.find(/*aliasName*/Args[1]);

  if (Args.length() == 2) {
    if (!idxp) {
      GCon->Logf("no named alias '%s' found", *Args[1]);
    } else {
      const VAlias &al = AliasList[*idxp];
      GCon->Logf("alias %s: %s%s", *Args[1], *al.CmdLine, (al.Save ? "" : " (temporary)"));
    }
    return;
  }

  VStr cmd;
  for (int f = 2; f < Args.length(); ++f) {
    if (Args[f].isEmpty()) continue;
    if (cmd.length()) cmd += ' ';
    cmd += Args[f];
  }
  cmd = cmd.xstrip(); // why not?

  if (cmd.isEmpty()) {
    if (Args.length() != 3 || !Args[2].isEmpty()) {
      GCon->Logf("invalid alias defintion for '%s' (empty command)", *Args[1]);
      return;
    }
    // remove alias
    if (!idxp) {
      GCon->Logf("no named alias '%s' found", *Args[1]);
    } else {
      VAlias &al = AliasList[*idxp];
      if (ParsingKeyConf && al.Save) {
        GCon->Logf("cannot remove permanent alias '%s' from keyconf", *Args[1]);
      } else {
        AliasList.removeAt(*idxp);
        rebuildAliasMap();
        if (GetCommandType(Args[1]) == CT_UNKNOWN) RemoveFromAutoComplete(*Args[1]);
        GCon->Logf("removed alias '%s'", *Args[1]);
      }
    }
    return;
  }

  if (idxp) {
    // redefine alias
    VAlias &al = AliasList[*idxp];
    if (ParsingKeyConf && al.Save) {
      // add new temporary alias below
    } else {
      // redefine alias
      al.CmdLine = cmd;
      GCon->Logf("redefined alias '%s'", *Args[1]);
      return;
    }
  }

  // new alias
  VAlias &nal = AliasList.alloc();
  nal.Name = Args[1];
  nal.CmdLine = cmd;
  nal.Save = !ParsingKeyConf;

  if (ParsingKeyConf) {
    if (ParsingKeyConf) {
      GCon->Logf(NAME_Init, "defined temporary alias '%s': %s", *Args[1], *cmd);
    } else {
      GCon->Logf("defined alias '%s': %s", *Args[1], *cmd);
    }
  }

  rebuildAliasMap();
  AddToAutoComplete(*Args[1]);
}


//==========================================================================
//
//  Echo_f
//
//==========================================================================
COMMAND(Echo) {
  if (Args.length() < 2) return;

  VStr Text = Args[1];
  for (int i = 2; i < Args.length(); ++i) {
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
}


//==========================================================================
//
//  COMMAND Exec
//
//==========================================================================
COMMAND(Exec) {
  if (Args.length() < 2 || Args.length() > 3) {
    GCon->Log((execLogInit ? NAME_Init : NAME_Log), "Exec <filename> : execute script file");
    return;
  }

  VStream *Strm = nullptr;

  // try disk file
  VStr dskname = Host_GetConfigDir()+"/"+Args[1];
  if (Sys_FileExists(dskname)) {
    Strm = FL_OpenSysFileRead(dskname);
    if (Strm) GCon->Logf((execLogInit ? NAME_Init : NAME_Log), "Executing '%s'", *dskname);
  }

  // try wad file
  if (!Strm && FL_FileExists(Args[1])) {
    Strm = FL_OpenFileRead(Args[1]);
    if (Strm) {
      GCon->Logf((execLogInit ? NAME_Init : NAME_Log), "Executing '%s'", *Args[1]);
      //GCon->Logf("<%s>", *Strm->GetName());
    }
  }

  if (!Strm) {
    if (Args.length() == 2) GCon->Logf(NAME_Warning, "Can't find '%s'", *Args[1]);
    return;
  }

  //GCon->Logf("Executing '%s'", *Args[1]);

  int flsize = Strm->TotalSize();
  if (flsize == 0) { VStream::Destroy(Strm); return; }

  char *buf = new char[flsize+2];
  Strm->Serialise(buf, flsize);
  if (Strm->IsError()) {
    VStream::Destroy(Strm);
    delete[] buf;
    GCon->Logf(NAME_Warning, "Error reading '%s'!", *Args[1]);
    return;
  }
  VStream::Destroy(Strm);

  if (buf[flsize-1] != '\n') buf[flsize++] = '\n';
  buf[flsize] = 0;

  GCmdBuf.Insert(buf);

  delete[] buf;
}


//==========================================================================
//
//  COMMAND Wait
//
//==========================================================================
COMMAND(Wait) {
  int msecs = 0;
  if (Args.length() > 1) {
    if (!VStr::convertInt(*Args[1], &msecs)) msecs = 0;
  }

  // negative means "milliseconds"
  if (msecs < 0) {
    // no more than 3 seconds
    if (msecs < -3000) msecs = -3000;
    msecs = -msecs;
    GCmdBuf.WaitEndTime = Sys_Time()+((double)msecs/1000.0);
    return;
  }

  // positive means "tics" (roughly)
  if (msecs > 0) {
    // no more than 3 seconds
    if (msecs > 3*35) msecs = 10*35;
    GCmdBuf.WaitEndTime = Sys_Time()+((double)msecs/35.0+0.000001); // ticks (roughly)
    return;
  }

  // error or zero
  GCmdBuf.WaitEndTime = Sys_Time()+(1.0/35.0+0.000001); // roughly one tick
  return;
}


//==========================================================================
//
//  __k8_run_first_map
//
//  used for "-k8runmap" if mapinfo found
//
//==========================================================================
COMMAND(__k8_run_first_map) {
  if (P_GetNumEpisodes() == 0) {
    GCon->Logf("ERROR: No eposode info found!");
    return;
  }

  VName startMap = NAME_None;
  //int bestmdlump = -1;

  for (int ep = 0; ep < P_GetNumEpisodes(); ++ep) {
    VEpisodeDef *edef = P_GetEpisodeDef(ep);
    if (!edef) continue; // just in case

    //GCon->Logf(NAME_Debug, "ep=%d: Name=<%s>; TeaserName=<%s>; Text=%s; MapinfoSourceLump=%d (%d) <%s>", ep, *edef->Name, *edef->TeaserName, *edef->Text.quote(true), edef->MapinfoSourceLump, W_IsUserWadLump(edef->MapinfoSourceLump), *W_FullLumpName(edef->MapinfoSourceLump));

    VName map = edef->Name;
    if (map == NAME_None || !IsMapPresent(map)) {
      //GCon->Logf(NAME_Debug, "  ep=%d; map '%s' is not here!", ep, *edef->Name);
      map = edef->TeaserName;
      if (map == NAME_None || !IsMapPresent(map)) continue;
    }
    /*
    else {
      GCon->Logf(NAME_Debug, "  ep=%d; map '%s' is here!", ep, *edef->Name);
    }
    */

    const VMapInfo &mi = P_GetMapInfo(map);
    //GCon->Logf(NAME_Debug, "  ep=%d; map '%s': LumpName=<%s> (%s); LevelNum=%d", ep, *map, *mi.LumpName, *mi.Name, mi.LevelNum); mi.dump("!!!");

    //k8: i don't care about levelnum here
    /*
    if (mi.LevelNum == 0) {
      //GCon->Logf(NAME_Debug, "  ep=%d; map '%s' has zero levelnum!", ep, *map);
      continue;
    }
    */

    if (!W_IsUserWadLump(mi.MapinfoSourceLump)) continue; // ignore non-user maps

    // don't trust wad file order, use episode order instead
    //if (mi.MapinfoSourceLump <= bestmdlump) continue;
    //GCon->Logf(NAME_Debug, "MapinfoSourceLump=%d; name=<%s>; index=%d", mi.MapinfoSourceLump, *map, mi.LevelNum);

    //bestmdlump = mi.MapinfoSourceLump;
    startMap = map;
    break;
  }

  if (startMap == NAME_None && fsys_PWadMaps.length()) {
    GCon->Log(NAME_Init, "cannot find starting map from mapinfo, taking from pwad map list");
    startMap = VName(*fsys_PWadMaps[0].mapname);
  }

  if (startMap == NAME_None) {
    GCon->Log(NAME_Warning, "Starting map not found!");
    return;
  }

  GCon->Logf(NAME_Init, "autostart map '%s'", *startMap);

  Host_CLIMapStartFound();
  GCmdBuf.Insert(va("map \"%s\"\n", *VStr(startMap).quote()));
}
