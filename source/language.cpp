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
#include "gamedefs.h"
#include "language.h"


// ////////////////////////////////////////////////////////////////////////// //
VLanguage GLanguage;


//==========================================================================
//
//  VLanguage::VLanguage
//
//==========================================================================
VLanguage::VLanguage () noexcept {
}


//==========================================================================
//
//  VLanguage::~VLanguage
//
//==========================================================================
VLanguage::~VLanguage () noexcept {
}


//==========================================================================
//
//  VLanguage::FreeNonDehackedStrings
//
//==========================================================================
void VLanguage::FreeNonDehackedStrings () {
  for (auto &&it : table.first()) {
    if (it.getValue().PassNum != 0) it.removeCurrentNoAdvance();
  }
  table.compact();
}


//==========================================================================
//
//  VLanguage::LoadStrings
//
//==========================================================================
void VLanguage::LoadStrings (const char *LangId) {
  FreeNonDehackedStrings();
  for (auto &&it : WadNSNameIterator(NAME_language, WADNS_Global)) {
    const int Lump = it.lump;
    int j = 1;
    if (VStr::Cmp(LangId, "**") != 0) {
      ParseLanguageScript(Lump, "*", true, j++);
      ParseLanguageScript(Lump, LangId, true, j++);
      ParseLanguageScript(Lump, LangId, false, j++);
    }
    ParseLanguageScript(Lump, "**", true, j++);
  }
}


//==========================================================================
//
//  VLanguage::ParseLanguageScript
//
//==========================================================================
void VLanguage::ParseLanguageScript (vint32 Lump, const char *InCode, bool ExactMatch, vint32 PassNum) {
  char Code[4];
  Code[0] = VStr::ToLower(InCode[0]);
  Code[1] = VStr::ToLower(InCode[0] ? InCode[1] : 0);
  Code[2] = (ExactMatch && InCode[0] && InCode[1] ? VStr::ToLower(InCode[2]) : 0);
  Code[3] = 0;

  VScriptParser *sc = VScriptParser::NewWithLump(Lump);
  sc->SetCMode(true);

  bool GotLanguageCode = false;
  bool Skip = false;
  bool Finished = false;

  while (!sc->AtEnd()) {
    if (sc->Check("[")) {
      // language identifiers
      Skip = true;
      while (!sc->Check("]")) {
        sc->ExpectString();
        size_t Len = sc->String.Length();
        char CurCode[4];
        if (Len != 2 && Len != 3) {
          if (Len == 1 && sc->String[0] == '*') {
            CurCode[0] = '*';
            CurCode[1] = 0;
            CurCode[2] = 0;
          } else if (Len == 7 && !sc->String.ICmp("default")) {
            CurCode[0] = '*';
            CurCode[1] = '*';
            CurCode[2] = 0;
          } else {
            sc->Error(va("Language code must be 2 or 3 characters long, %s is %u characters long", *sc->String, (unsigned)Len));
            // shut up compiler
            CurCode[0] = 0;
            CurCode[1] = 0;
            CurCode[2] = 0;
          }
        } else {
          CurCode[0] = VStr::ToLower(sc->String[0]);
          CurCode[1] = VStr::ToLower(sc->String[1]);
          CurCode[2] = (ExactMatch ? VStr::ToLower(sc->String[2]) : 0);
          CurCode[3] = 0;
        }
        if (Code[0] == CurCode[0] && Code[1] == CurCode[1] && Code[2] == CurCode[2]) {
          Skip = false;
        }
        if (!GotLanguageCode && !Skip) {
          GCon->Logf(NAME_Dev, "parsing language script '%s' for language '%s'", *W_FullLumpName(Lump), Code);
        }
        GotLanguageCode = true;
      }
    } else {
      if (!GotLanguageCode) {
        // skip old binary LANGUAGE lumps
        if (!sc->IsText()) {
          if (!Finished) GCon->Logf("Skipping binary LANGUAGE lump");
          Finished = true;
          return;
        }
        sc->Error("Found a string without language specified");
      }

      // parse string definitions
      if (Skip) {
        // we are skipping this language
        sc->ExpectString();
        //sc->Expect("=");
        //sc->ExpectString();
        while (!sc->Check(";")) sc->ExpectString();
        continue;
      }

      sc->ExpectString();

      if (sc->String == "$") {
        //GCon->Logf(NAME_Warning, "%s: conditionals in language script is not supported yet", *sc->GetLoc().toStringNoCol());
        sc->Expect("ifgame");
        sc->Expect("(");
        VStr gmname;
        if (!sc->Check(")")) {
          sc->ExpectString();
          gmname = sc->String.xstrip();
          if (!sc->Check(")")) {
            GCon->Logf(NAME_Error, "%s: invalid conditional in language script", *sc->GetLoc().toStringNoCol());
            while (!sc->Check(")")) sc->ExpectString();
          }
        } else {
          GCon->Logf(NAME_Warning, "%s: empty conditionals in language script", *sc->GetLoc().toStringNoCol());
        }
        if (!gmname.isEmpty() && !game_name.asStr().startsWithCI(gmname)) {
          //if (!gmname.isEmpty()) GCon->Logf(NAME_Init, "%s: skipped conditional for game '%s'", *sc->GetLoc().toStringNoCol(), *gmname);
          sc->ExpectString();
          sc->Expect("=");
          while (!sc->Check(";")) sc->ExpectString();
          continue;
        }
        sc->ExpectString();
      }

      VName Key(*sc->String, VName::AddLower);
      sc->Expect("=");
      sc->ExpectString();
      VStr Value = HandleEscapes(sc->String);
      while (!sc->Check(";")) {
        sc->ExpectString();
        Value += HandleEscapes(sc->String);
      }

      // check for replacement
      VLangEntry *vep = table.get(Key);
      if (!vep || vep->PassNum >= PassNum) {
        VLangEntry Entry;
        Entry.Value = Value;
        Entry.PassNum = PassNum;
        table.put(Key, Entry);
        //GCon->Logf(NAME_Dev, "  [%s]=[%s]", *VStr(Key).quote(), *Value.quote());
      }
    }
  }

  delete sc;
}


//==========================================================================
//
//  VLanguage::HandleEscapes
//
//==========================================================================
VStr VLanguage::HandleEscapes (VStr Src) {
  bool hasWork = false;
  for (size_t i = Src.Length(); i > 0; --i) if (Src[i-1] == '\\') { hasWork = true; break; }
  if (!hasWork) return VStr(Src);
  VStr Ret;
  for (int i = 0; i < Src.Length(); ++i) {
    char c = Src[i];
    if (c == '\\') {
      ++i;
      c = Src[i];
           if (c == 'n') c = '\n';
      else if (c == 'r') c = '\r';
      else if (c == 't') c = '\t';
      else if (c == 'c') c = -127;
      else if (c == '\n') continue;
    }
    Ret += c;
  }
  return Ret;
}


//==========================================================================
//
//  VLanguage::Find
//
//==========================================================================
VStr VLanguage::Find (VName Key, bool *found) const {
  if (Key == NAME_None) {
    if (found) *found = true;
    return VStr();
  }
  const VLangEntry *vep = table.get(Key);
  if (vep) {
    if (found) *found = true;
    return vep->Value;
  }
  // try lowercase
  for (const char *s = *Key; *s; ++s) {
    if (*s >= 'A' && *s <= 'Z') {
      // found uppercase letter, try lowercase name
      VName loname = VName(*Key, VName::FindLower);
      if (loname != NAME_None) {
        vep = table.get(loname);
        if (vep) {
          if (found) *found = true;
          return vep->Value;
        }
      }
      break;
    }
  }
  if (found) *found = false;
  return VStr();
}


//==========================================================================
//
//  VLanguage::Find
//
//==========================================================================
VStr VLanguage::Find (const char *s, bool *found) const {
  if (!s) s = "";
  if (!s[0]) {
    if (found) *found = true;
    return VStr();
  }
  VName loname = VName(s, VName::FindLower);
  if (loname != NAME_None) {
    const VLangEntry *vep = table.get(loname);
    if (vep) {
      if (found) *found = true;
      return vep->Value;
    }
  }
  if (found) *found = false;
  return VStr();
}


//==========================================================================
//
//  VLanguage::operator[]
//
//==========================================================================
VStr VLanguage::operator [] (VName Key) const {
  bool found = false;
  VStr res = Find(Key, &found);
  if (found) return res;
  return VStr(Key);
}


//==========================================================================
//
//  VLanguage::operator[]
//
//==========================================================================
VStr VLanguage::operator [] (const char *s) const {
  bool found = false;
  VStr res = Find(s, &found);
  if (found) return res;
  return (s ? VStr(s) : VStr::EmptyString);
}


//==========================================================================
//
//  VLanguage::Translate
//
// WITH '$'!
//
//==========================================================================
VStr VLanguage::Translate (const VStr &s) const {
  if (s.length() < 2 || s[0] != '$') return s;
  bool found = false;
  VStr res = Find(*s+1, &found);
  if (!found) res = s;
  return res;
}


//==========================================================================
//
//  VLanguage::HasTranslation
//
//==========================================================================
bool VLanguage::HasTranslation (VName s) const {
  bool found = false;
  (void)Find(s, &found);
  return found;
}


//==========================================================================
//
//  VLanguage::HasTranslation
//
//==========================================================================
bool VLanguage::HasTranslation (const char *s) const {
  bool found = false;
  (void)Find(s, &found);
  return found;
}


//==========================================================================
//
//  VLanguage::GetStringId
//
//==========================================================================
VName VLanguage::GetStringId (VStr Str) {
  for (auto it = table.first(); it; ++it) {
    if (it.getValue().Value == Str) return it.GetKey();
  }
  return NAME_None;
}


//==========================================================================
//
//  VLanguage::GetStringIdCI
//
//==========================================================================
VName VLanguage::GetStringIdCI (VStr Str) {
  for (auto it = table.first(); it; ++it) {
    if (it.getValue().Value.strEquCI(Str)) return it.GetKey();
  }
  return NAME_None;
}


//==========================================================================
//
//  VLanguage::ReplaceString
//
//==========================================================================
void VLanguage::ReplaceString (VName Key, VStr Value) {
  VLangEntry Entry;
  Entry.Value = Value;
  Entry.PassNum = 0;
  table.put(Key, Entry);
}
