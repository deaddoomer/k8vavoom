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
#ifndef VAVOOM_LANGUAGE_HEADER
#define VAVOOM_LANGUAGE_HEADER


class VLanguage {
private:
  struct VLangEntry {
    vint32 PassNum;
    VStr Value;
  };

  TMap<VName, VLangEntry> table;

  void FreeNonDehackedStrings ();
  void ParseLanguageScript (vint32 Lump, const char *InCode, bool ExactMatch, vint32 PassNum);
  VStr HandleEscapes (VStr Src);

public:
  VLanguage () noexcept;
  ~VLanguage () noexcept;

  void LoadStrings (const char *LangId);

  VStr Find (VName Key, bool *found=nullptr) const;
  VStr Find (const char *s, bool *found=nullptr) const;

  VStr operator [] (VName Key) const;
  VStr operator [] (const char *s) const;
  VStr operator [] (const VStr &s) const;

  // WITH '$'!
  VStr Translate (const VStr &s) const;

  bool HasTranslation (VName s) const;
  bool HasTranslation (const char *s) const;

  VName GetStringId (VStr Str);
  VName GetStringIdCI (VStr Str);
  void ReplaceString (VName, VStr);
};


extern VLanguage GLanguage;


#endif
