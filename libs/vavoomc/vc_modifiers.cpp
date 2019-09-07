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
//**  Copyright (C) 2018-2019 Ketmar Dark
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
#include "vc_local.h"


//==========================================================================
//
//  TModifiers::Parse
//
//  Parse supported modifiers.
//
//==========================================================================
int TModifiers::Parse (VLexer &Lex) {
  struct Mod {
    EToken token;
    int flag;
  };

  static const Mod mods[] = {
    {TK_Native, Native},
    {TK_Static, Static},
    {TK_Abstract, Abstract},
    {TK_Private, Private},
    {TK_Protected, Protected},
    {TK_ReadOnly, ReadOnly},
    {TK_Transient, Transient},
    {TK_Final, Final},
    {TK_Optional, Optional},
    {TK_Out, Out},
    {TK_Spawner, Spawner},
    {TK_Override, Override},
    {TK_Ref, Ref},
    {TK_Const, Const},
    {TK_Repnotify, Repnotify},
    {TK_Scope, Scope},
    {TK_Published, Published},
    {TK_EOF, 0},
  };

  int Modifiers = 0;
  for (;;) {
    bool wasHit = false;
    for (const Mod *mod = mods; mod->flag; ++mod) {
      if (Lex.Check(mod->token)) {
        if (Modifiers&mod->flag) ParseError(Lex.Location, "duplicate modifier");
        Modifiers |= mod->flag;
        wasHit = true;
        break;
      } else if (Lex.Check(TK_LBracket)) {
        // [opt]
        bool rbParsed = false;
        for (;;) {
          if (Lex.Check(TK_RBracket)) { rbParsed = true; break; }
          if (Lex.Check(TK_Decorate)) {
            if (Modifiers&DecVisible) ParseError(Lex.Location, "duplicate modifier '%s'", *Lex.Name);
            Modifiers |= DecVisible;
          } else {
            Lex.Expect(TK_Identifier);
            if (Lex.Name == "internal") {
              if (Modifiers&Internal) ParseError(Lex.Location, "duplicate modifier '%s'", *Lex.Name);
              Modifiers |= Internal;
            } else {
              ParseError(Lex.Location, "unknown modifier '%s'", *Lex.Name);
            }
          }
          if (!Lex.Check(TK_Comma)) break;
        }
        if (!rbParsed) Lex.Expect(TK_RBracket);
      }
    }
    if (!wasHit) break;
  }
  return Modifiers;
}


//==========================================================================
//
//  TModifiers::Name
//
//  Return string representation of a modifier.
//
//==========================================================================
const char *TModifiers::Name (int Modifier) {
  switch (Modifier) {
    case Native: return "native";
    case Static: return "static";
    case Abstract: return "abstract";
    case Private: return "private";
    case ReadOnly: return "readonly";
    case Transient: return "transient";
    case Final: return "final";
    case Optional: return "optional";
    case Out: return "out";
    case Spawner: return "spawner";
    case Override: return "override";
    case Ref: return "ref";
    case Protected: return "protected";
    case Const: return "const";
    case Repnotify: return "repnotify";
    case Scope: return "scope";
    case Internal: return "[internal]";
    case Published: return "published";
    case DecVisible: return "[decorate]";
  }
  return "";
}


//==========================================================================
//
//  TModifiers::ShowBadAttributes
//
//==========================================================================
void TModifiers::ShowBadAttributes (int Modifiers, const TLocation &l) {
  if (!Modifiers) return;
  for (int i = 0; i < 31; ++i) if (Modifiers&(1<<i)) ParseError(l, "`%s` modifier is not allowed", Name(1<<i));
}


//==========================================================================
//
//  TModifiers::Check
//
//  Verify that modifiers are valid in current context.
//
//==========================================================================
int TModifiers::Check (int Modifers, int Allowed, const TLocation &l) {
  // check for conflicting protection
  static const int ProtAttrs[] = { Private, Protected, Published, 0 };
  int protcount = 0;
  for (int f = 0; ProtAttrs[f]; ++f) if (Modifers&ProtAttrs[f]) ++protcount;
  if (protcount > 1) ParseError(l, "conflicting protection modifiers"); // TODO: print modifiers
  ShowBadAttributes(Modifers&~Allowed, l);
  return Modifers&Allowed;
}


//==========================================================================
//
//  TModifiers::MethodAttr
//
//  Convert modifiers to method attributes.
//
//==========================================================================
int TModifiers::MethodAttr (int Modifiers) {
  int Attributes = 0;
  if (Modifiers&Native) Attributes |= FUNC_Native;
  if (Modifiers&Static) Attributes |= FUNC_Static|FUNC_Final; // anyway for now
  if (Modifiers&Final) Attributes |= FUNC_Final;
  if (Modifiers&Spawner) Attributes |= FUNC_Spawner;
  if (Modifiers&Override) Attributes |= FUNC_Override;
  if (Modifiers&Private) Attributes |= FUNC_Private|FUNC_Final; // private methods can always be marked as `final`
  if (Modifiers&Protected) Attributes |= FUNC_Protected;
  if (Modifiers&DecVisible) Attributes |= FUNC_Decorate;
  return Attributes;
}


//==========================================================================
//
//  TModifiers::ClassAttr
//
//  Convert modifiers to class attributes.
//
//==========================================================================
int TModifiers::ClassAttr (int Modifiers) {
  int Attributes = 0;
  if (Modifiers&Native) Attributes |= CLASS_Native;
  if (Modifiers&Abstract) Attributes |= CLASS_Abstract;
  if (Modifiers&Transient) Attributes |= CLASS_Transient;
  if (Modifiers&DecVisible) Attributes |= CLASS_DecorateVisible;
  return Attributes;
}


//==========================================================================
//
//  TModifiers::FieldAttr
//
//  Convert modifiers to field attributes.
//
//==========================================================================
int TModifiers::FieldAttr (int Modifiers) {
  int Attributes = 0;
  if (Modifiers&Native) Attributes |= FIELD_Native;
  if (Modifiers&Transient) Attributes |= FIELD_Transient;
  if (Modifiers&Private) Attributes |= FIELD_Private;
  if (Modifiers&ReadOnly) Attributes |= FIELD_ReadOnly;
  if (Modifiers&Protected) Attributes |= FIELD_Protected;
  if (Modifiers&Repnotify) Attributes |= FIELD_Repnotify;
  if (Modifiers&Internal) Attributes |= FIELD_Internal;
  if (Modifiers&Published) Attributes |= FIELD_Published;
  return Attributes;
}


//==========================================================================
//
//  TModifiers::PropAttr
//
//  Convert modifiers to property attributes.
//
//==========================================================================
int TModifiers::PropAttr (int Modifiers) {
  int Attributes = 0;
  if (Modifiers&Native) Attributes |= PROP_Native;
  if (Modifiers&Final) Attributes |= PROP_Final;
  if (Modifiers&Static) Attributes |= PROP_Final; // anyway for now
  if (Modifiers&Protected) Attributes |= PROP_Protected;
  return Attributes;
}


//==========================================================================
//
//  TModifiers::ParmAttr
//
//  Convert modifiers to method parameter attributes.
//
//==========================================================================
int TModifiers::ParmAttr (int Modifiers) {
  int Attributes = 0;
  if (Modifiers&Optional) Attributes |= FPARM_Optional;
  if (Modifiers&Out) Attributes |= FPARM_Out;
  if (Modifiers&Ref) Attributes |= FPARM_Ref;
  if (Modifiers&Const) Attributes |= FPARM_Const;
  return Attributes;
}
