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

#include "vc_local.h"
#include "sv_local.h"


static VCvarB dbg_show_decorate_unsupported("dbg_show_decorate_unsupported", false, "Show unsupported decorate props/flags?", 0);
VCvarB dbg_show_missing_class("dbg_show_missing_class", false, "Show missing classes?", 0);
VCvarB decorate_fail_on_unknown("decorate_fail_on_unknown", false, "Fail on unknown decorate properties?", 0);


enum {
  PROPS_HASH_SIZE = 256,
  FLAGS_HASH_SIZE = 256,
};

enum {
  OLDDEC_Decoration,
  OLDDEC_Breakable,
  OLDDEC_Projectile,
  OLDDEC_Pickup,
};

enum {
  BOUNCE_None,
  BOUNCE_Doom,
  BOUNCE_Heretic,
  BOUNCE_Hexen
};

enum {
  PROP_Int,
  PROP_IntTrunced,
  PROP_IntConst,
  PROP_IntUnsupported,
  PROP_IntIdUnsupported,
  PROP_BitIndex,
  PROP_Float,
  PROP_FloatUnsupported,
  PROP_Speed,
  PROP_Tics,
  PROP_TicsSecs,
  PROP_Percent,
  PROP_FloatClamped,
  PROP_FloatClamped2,
  PROP_FloatOpt2,
  PROP_Name,
  PROP_NameLower,
  PROP_Str,
  PROP_StrUnsupported,
  PROP_Class,
  PROP_Power_Class,
  PROP_BoolConst,
  PROP_State,
  PROP_Game,
  PROP_SpawnId,
  PROP_ConversationId,
  PROP_PainChance,
  PROP_DamageFactor,
  PROP_MissileDamage,
  PROP_VSpeed,
  PROP_RenderStyle,
  PROP_Translation,
  PROP_BloodColour,
  PROP_BloodType,
  PROP_StencilColour,
  PROP_Monster,
  PROP_Projectile,
  PROP_BounceType,
  PROP_ClearFlags,
  PROP_DropItem,
  PROP_States,
  PROP_SkipSuper,
  PROP_Args,
  PROP_LowMessage,
  PROP_PowerupColour,
  PROP_ColourRange,
  PROP_DamageScreenColour,
  PROP_HexenArmor,
  PROP_StartItem,
  PROP_MorphStyle,
  PROP_SkipLineUnsupported,
};

enum {
  FLAG_Bool,
  FLAG_Unsupported,
  FLAG_Byte,
  FLAG_Float,
  FLAG_Name,
  FLAG_Class,
  FLAG_NoClip,
};

struct VClassFixup {
  int Offset;
  VStr Name;
  VClass *ReqParent;
  VClass *Class;
};


struct VPropDef {
  vuint8 Type;
  int HashNext;
  VName Name;
  VField *Field;
  VField *Field2;
  VName PropName;
  union {
    int IConst;
    float FMin;
  };
  float FMax;
  VStr CPrefix;

  void SetField (VClass *, const char *);
  void SetField2 (VClass *, const char *);
};


struct VFlagDef {
  vuint8 Type;
  int HashNext;
  VName Name;
  VField *Field;
  VField *Field2;
  union {
    vuint8 BTrue;
    float FTrue;
  };
  VName NTrue;
  union {
    vuint8 BFalse;
    float FFalse;
  };
  VName NFalse;

  void SetField (VClass *, const char *);
  void SetField2 (VClass *, const char *);
};


struct VFlagList {
  VClass *Class;

  TArray<VPropDef> Props;
  int PropsHash[PROPS_HASH_SIZE];

  TArray<VFlagDef> Flags;
  int FlagsHash[FLAGS_HASH_SIZE];

  VPropDef &NewProp (vuint8, VXmlNode *);
  VFlagDef &NewFlag (vuint8, VXmlNode *);
};


//==========================================================================
//
//  VDecorateInvocation
//
//==========================================================================
class VDecorateInvocation : public VExpression {
public:
  VName Name;
  int NumArgs;
  VExpression *Args[VMethod::MAX_PARAMS+1];

  VDecorateInvocation (VName, const TLocation &, int, VExpression **);
  virtual ~VDecorateInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

protected:
  VDecorateInvocation () : Name(NAME_None), NumArgs(0) {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
static VExpression *ParseExpressionPriority13 (VScriptParser *sc);


// ////////////////////////////////////////////////////////////////////////// //
TArray<VLineSpecInfo> LineSpecialInfos;


// ////////////////////////////////////////////////////////////////////////// //
static VPackage *DecPkg;

static VClass *ActorClass;
static VClass *FakeInventoryClass;
static VClass *InventoryClass;
static VClass *AmmoClass;
static VClass *BasicArmorPickupClass;
static VClass *BasicArmorBonusClass;
static VClass *HealthClass;
static VClass *PowerupGiverClass;
static VClass *PuzzleItemClass;
static VClass *WeaponClass;
static VClass *WeaponPieceClass;
static VClass *PlayerPawnClass;
static VClass *MorphProjectileClass;

static VMethod *FuncA_Scream;
static VMethod *FuncA_NoBlocking;
static VMethod *FuncA_ScreamAndUnblock;
static VMethod *FuncA_ActiveSound;
static VMethod *FuncA_ActiveAndUnblock;
static VMethod *FuncA_ExplodeParms;
static VMethod *FuncA_FreezeDeath;
static VMethod *FuncA_FreezeDeathChunks;

static TArray<VFlagList> FlagList;


//==========================================================================
//
//  ParseDecorateDef
//
//==========================================================================
static void ParseDecorateDef (VXmlDocument &Doc) {
  guard(ParseDecorateDef);
  for (VXmlNode *N = Doc.Root.FindChild("class"); N; N = N->FindNext()) {
    VStr ClassName = N->GetAttribute("name");
    VFlagList &Lst = FlagList.Alloc();
    Lst.Class = VClass::FindClass(*ClassName);
    for (int i = 0; i < PROPS_HASH_SIZE; ++i) Lst.PropsHash[i] = -1;
    for (int i = 0; i < FLAGS_HASH_SIZE; ++i) Lst.FlagsHash[i] = -1;
    for (VXmlNode *PN = N->FirstChild; PN; PN = PN->NextSibling) {
      if (PN->Name == "prop_int") {
        VPropDef &P = Lst.NewProp(PROP_Int, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_int_trunced") {
        VPropDef &P = Lst.NewProp(PROP_IntTrunced, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_int_const") {
        VPropDef &P = Lst.NewProp(PROP_IntConst, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        P.IConst = atoi(*PN->GetAttribute("value")); //FIXME
      } else if (PN->Name == "prop_int_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_IntUnsupported, PN);
      } else if (PN->Name == "prop_float_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_FloatUnsupported, PN);
      } else if (PN->Name == "prop_int_id_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_IntIdUnsupported, PN);
      } else if (PN->Name == "prop_skip_line_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_SkipLineUnsupported, PN);
      } else if (PN->Name == "prop_bit_index") {
        VPropDef &P = Lst.NewProp(PROP_BitIndex, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_float") {
        VPropDef &P = Lst.NewProp(PROP_Float, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_speed") {
        VPropDef &P = Lst.NewProp(PROP_Speed, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_tics") {
        VPropDef &P = Lst.NewProp(PROP_Tics, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_tics_secs") {
        VPropDef &P = Lst.NewProp(PROP_TicsSecs, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_percent") {
        VPropDef &P = Lst.NewProp(PROP_Percent, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_float_clamped") {
        VPropDef &P = Lst.NewProp(PROP_FloatClamped, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        P.FMin = atof(*PN->GetAttribute("min")); //FIXME
        P.FMax = atof(*PN->GetAttribute("max")); //FIXME
      } else if (PN->Name == "prop_float_clamped_2") {
        VPropDef &P = Lst.NewProp(PROP_FloatClamped2, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        P.SetField2(Lst.Class, *PN->GetAttribute("property2"));
        P.FMin = atof(*PN->GetAttribute("min")); //FIXME
        P.FMax = atof(*PN->GetAttribute("max")); //FIXME
      } else if (PN->Name == "prop_float_optional_2") {
        VPropDef &P = Lst.NewProp(PROP_FloatOpt2, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        P.SetField2(Lst.Class, *PN->GetAttribute("property2"));
      } else if (PN->Name == "prop_name") {
        VPropDef &P = Lst.NewProp(PROP_Name, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_name_lower") {
        VPropDef &P = Lst.NewProp(PROP_NameLower, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_string") {
        VPropDef &P = Lst.NewProp(PROP_Str, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_string_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_StrUnsupported, PN);
      } else if (PN->Name == "prop_class") {
        VPropDef &P = Lst.NewProp(PROP_Class, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        if (PN->HasAttribute("prefix")) P.CPrefix = PN->GetAttribute("prefix");
      } else if (PN->Name == "prop_power_class") {
        VPropDef &P = Lst.NewProp(PROP_Power_Class, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        if (PN->HasAttribute("prefix")) P.CPrefix = PN->GetAttribute("prefix");
      } else if (PN->Name == "prop_bool_const") {
        VPropDef &P = Lst.NewProp(PROP_BoolConst, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        P.IConst = !PN->GetAttribute("value").ICmp("true");
      } else if (PN->Name == "prop_state") {
        VPropDef &P = Lst.NewProp(PROP_State, PN);
        P.PropName = *PN->GetAttribute("property");
      } else if (PN->Name == "prop_game") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_Game, PN);
      } else if (PN->Name == "prop_spawn_id") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_SpawnId, PN);
      } else if (PN->Name == "prop_conversation_id") {
        VPropDef &P = Lst.NewProp(PROP_ConversationId, PN);
        P.SetField(Lst.Class, "ConversationID");
      } else if (PN->Name == "prop_pain_chance") {
        VPropDef &P = Lst.NewProp(PROP_PainChance, PN);
        P.SetField(Lst.Class, "PainChance");
      } else if (PN->Name == "prop_damage_factor") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_DamageFactor, PN);
      } else if (PN->Name == "prop_missile_damage") {
        VPropDef &P = Lst.NewProp(PROP_MissileDamage, PN);
        P.SetField(Lst.Class, "MissileDamage");
      } else if (PN->Name == "prop_vspeed") {
        VPropDef &P = Lst.NewProp(PROP_VSpeed, PN);
        P.SetField(Lst.Class, "Velocity");
      } else if (PN->Name == "prop_render_style") {
        VPropDef &P = Lst.NewProp(PROP_RenderStyle, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_translation") {
        VPropDef &P = Lst.NewProp(PROP_Translation, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_blood_colour") {
        VPropDef &P = Lst.NewProp(PROP_BloodColour, PN);
        P.SetField(Lst.Class, "BloodColour");
        P.SetField2(Lst.Class, "BloodTranslation");
      } else if (PN->Name == "prop_blood_type") {
        VPropDef &P = Lst.NewProp(PROP_BloodType, PN);
        P.SetField(Lst.Class, "BloodType");
        P.SetField2(Lst.Class, "BloodSplatterType");
      } else if (PN->Name == "prop_stencil_colour") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_StencilColour, PN);
      } else if (PN->Name == "prop_monster") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_Monster, PN);
      } else if (PN->Name == "prop_projectile") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_Projectile, PN);
      } else if (PN->Name == "prop_bouncetype") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_BounceType, PN);
      } else if (PN->Name == "prop_clear_flags") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_ClearFlags, PN);
      } else if (PN->Name == "prop_drop_item") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_DropItem, PN);
      } else if (PN->Name == "prop_states") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_States, PN);
      } else if (PN->Name == "prop_skip_super") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_SkipSuper, PN);
      } else if (PN->Name == "prop_args") {
        VPropDef &P = Lst.NewProp(PROP_Args, PN);
        P.SetField(Lst.Class, "Args");
        P.SetField2(Lst.Class, "bArgsDefined");
      } else if (PN->Name == "prop_low_message") {
        VPropDef &P = Lst.NewProp(PROP_LowMessage, PN);
        P.SetField(Lst.Class, "LowHealth");
        P.SetField2(Lst.Class, "LowHealthMessage");
      } else if (PN->Name == "prop_powerup_colour") {
        VPropDef &P = Lst.NewProp(PROP_PowerupColour, PN);
        P.SetField(Lst.Class, "BlendColour");
      } else if (PN->Name == "prop_colour_range") {
        VPropDef &P = Lst.NewProp(PROP_ColourRange, PN);
        P.SetField(Lst.Class, "TranslStart");
        P.SetField2(Lst.Class, "TranslEnd");
      } else if (PN->Name == "prop_damage_screen_colour") {
        VPropDef &P = Lst.NewProp(PROP_DamageScreenColour, PN);
        P.SetField(Lst.Class, "DamageScreenColour");
      } else if (PN->Name == "prop_hexen_armor") {
        VPropDef &P = Lst.NewProp(PROP_HexenArmor, PN);
        P.SetField(Lst.Class, "HexenArmor");
      } else if (PN->Name == "prop_start_item") {
        VPropDef &P = Lst.NewProp(PROP_StartItem, PN);
        P.SetField(Lst.Class, "DropItemList");
      } else if (PN->Name == "prop_morph_style") {
        VPropDef &P = Lst.NewProp(PROP_MorphStyle, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "flag") {
        VFlagDef &F = Lst.NewFlag(FLAG_Bool, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "flag_unsupported") {
        /*VFlagDef &F =*/(void)Lst.NewFlag(FLAG_Unsupported, PN);
      } else if (PN->Name == "flag_byte") {
        VFlagDef &F = Lst.NewFlag(FLAG_Byte, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
        F.BTrue = atoi(*PN->GetAttribute("true_value")); //FIXME
        F.BFalse = atoi(*PN->GetAttribute("false_value")); //FIXME
      } else if (PN->Name == "flag_float") {
        VFlagDef &F = Lst.NewFlag(FLAG_Float, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
        F.FTrue = atof(*PN->GetAttribute("true_value")); //FIXME
        F.FFalse = atof(*PN->GetAttribute("false_value")); //FIXME
      } else if (PN->Name == "flag_name") {
        VFlagDef &F = Lst.NewFlag(FLAG_Name, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
        F.NTrue = *PN->GetAttribute("true_value");
        F.NFalse = *PN->GetAttribute("false_value");
      } else if (PN->Name == "flag_class") {
        VFlagDef &F = Lst.NewFlag(FLAG_Class, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
        F.NTrue = *PN->GetAttribute("true_value");
        F.NFalse = *PN->GetAttribute("false_value");
      } else if (PN->Name == "flag_noclip") {
        VFlagDef &F = Lst.NewFlag(FLAG_NoClip, PN);
        F.SetField(Lst.Class, "bColideWithThings");
        F.SetField2(Lst.Class, "bColideWithWorld");
      } else {
        Sys_Error("Unknown XML node %s", *PN->Name);
      }
    }
  }
  unguard;
}


//==========================================================================
//
//  VPropDef::SetField
//
//==========================================================================
void VPropDef::SetField (VClass *Class, const char *FieldName) {
  guard(VPropDef::SetField);
  Field = Class->FindFieldChecked(FieldName);
  unguard;
}


//==========================================================================
//
//  VPropDef::SetField2
//
//==========================================================================
void VPropDef::SetField2 (VClass *Class, const char *FieldName) {
  guard(VPropDef::SetField2);
  Field2 = Class->FindFieldChecked(FieldName);
  unguard;
}


//==========================================================================
//
//  VFlagDef::SetField
//
//==========================================================================
void VFlagDef::SetField (VClass *Class, const char *FieldName) {
  guard(VFlagDef::SetField);
  Field = Class->FindFieldChecked(FieldName);
  unguard;
}


//==========================================================================
//
//  VFlagDef::SetField2
//
//==========================================================================
void VFlagDef::SetField2 (VClass *Class, const char *FieldName) {
  guard(VFlagDef::SetField2);
  Field2 = Class->FindFieldChecked(FieldName);
  unguard;
}


//==========================================================================
//
//  VFlagList::NewProp
//
//==========================================================================
VPropDef &VFlagList::NewProp (vuint8 Type, VXmlNode *PN) {
  guard(VFlagList::NewProp);
  VPropDef &P = Props.Alloc();
  P.Type = Type;
  P.Name = *PN->GetAttribute("name").ToLower();
  int HashIndex = GetTypeHash(P.Name)&(PROPS_HASH_SIZE-1);
  P.HashNext = PropsHash[HashIndex];
  PropsHash[HashIndex] = Props.Num()-1;
  return P;
  unguard;
}


//==========================================================================
//
//  VFlagList::NewFlag
//
//==========================================================================
VFlagDef &VFlagList::NewFlag (vuint8 Type, VXmlNode *PN) {
  guard(VFlagList::NewFlag);
  VFlagDef &F = Flags.Alloc();
  F.Type = Type;
  F.Name = *PN->GetAttribute("name").ToLower();
  int HashIndex = GetTypeHash(F.Name)&(FLAGS_HASH_SIZE-1);
  F.HashNext = FlagsHash[HashIndex];
  FlagsHash[HashIndex] = Flags.Num()-1;
  return F;
  unguard;
}


//==========================================================================
//
//  VDecorateSingleName::VDecorateSingleName
//
//==========================================================================
VDecorateSingleName::VDecorateSingleName (const VStr &AName, const TLocation &ALoc)
  : VExpression(ALoc)
  , Name(AName)
{
}


//==========================================================================
//
//  VDecorateSingleName::VDecorateSingleName
//
//==========================================================================
VDecorateSingleName::VDecorateSingleName () {
}


//==========================================================================
//
//  VDecorateSingleName::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateSingleName::SyntaxCopy () {
  auto res = new VDecorateSingleName();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateSingleName::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateSingleName::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateSingleName *)e;
  res->Name = Name;
}


//==========================================================================
//
//  VDecorateSingleName::DoResolve
//
//==========================================================================
VExpression *VDecorateSingleName::DoResolve (VEmitContext &ec) {
  guard(VDecorateSingleName::DoResolve);
  VName CheckName = va("decorate_%s", *Name.ToLower());
  if (ec.SelfClass) {
    VConstant *Const = ec.SelfClass->FindConstant(CheckName);
    if (Const) {
      VExpression *e = new VConstantValue(Const, Loc);
      delete this;
      return e->Resolve(ec);
    }

    VProperty *Prop = ec.SelfClass->FindProperty(CheckName);
    if (Prop) {
      if (!Prop->GetFunc) {
        ParseError(Loc, "Property %s cannot be read", *Name);
        delete this;
        return nullptr;
      }
      VExpression *e = new VInvocation(nullptr, Prop->GetFunc, nullptr, false, false, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
  }

  CheckName = *Name.ToLower();
  // look only for constants defined in DECORATE scripts
  VConstant *Const = ec.Package->FindConstant(CheckName);
  if (Const) {
    VExpression *e = new VConstantValue(Const, Loc);
    delete this;
    return e->Resolve(ec);
  }

  ParseError(Loc, "Illegal expression identifier %s", *Name);
  delete this;
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VDecorateSingleName::Emit
//
//==========================================================================
void VDecorateSingleName::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen");
}


//==========================================================================
//
//  VDecorateSingleName::IsDecorateSingleName
//
//==========================================================================
bool VDecorateSingleName::IsDecorateSingleName () const {
  return true;
}


//==========================================================================
//
//  VDecorateInvocation::VDecorateInvocation
//
//==========================================================================
VDecorateInvocation::VDecorateInvocation (VName AName, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VExpression(ALoc)
  , Name(AName)
  , NumArgs(ANumArgs)
{
  memset(Args, 0, sizeof(Args));
  for (int i = 0; i < NumArgs; ++i) Args[i] = AArgs[i];
}


//==========================================================================
//
//  VDecorateInvocation::~VDecorateInvocation
//
//==========================================================================
VDecorateInvocation::~VDecorateInvocation () {
  for (int i = 0; i < NumArgs; ++i) {
    if (Args[i]) {
      delete Args[i];
      Args[i] = nullptr;
    }
  }
}


//==========================================================================
//
//  VDecorateInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateInvocation::SyntaxCopy () {
  auto res = new VDecorateInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateInvocation::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateInvocation *)e;
  memset(res->Args, 0, sizeof(res->Args));
  res->Name = Name;
  res->NumArgs = NumArgs;
  for (int f = 0; f < NumArgs; ++f) res->Args[f] = (Args[f] ? Args[f]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDecorateInvocation::DoResolve
//
//==========================================================================
VExpression *VDecorateInvocation::DoResolve (VEmitContext &ec) {
  guard(VDecorateInvocation::DoResolve);
  if (ec.SelfClass) {
    // first try with decorate_ prefix, then without.
    VMethod *M = ec.SelfClass->FindMethod(va("decorate_%s", *Name));
    if (!M) M = ec.SelfClass->FindMethod(Name);
    if (M) {
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  ParseError(Loc, "Unknown method %s", *Name);
  delete this;
  return nullptr;
  unguard;
}


//==========================================================================
//
//  VDecorateInvocation::Emit
//
//==========================================================================
void VDecorateInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen");
}


//==========================================================================
//
//  GetClassFieldFloat
//
//==========================================================================
static float GetClassFieldFloat (VClass *Class, VName FieldName) {
  guard(GetClassFieldFloat);
  VField *F = Class->FindFieldChecked(FieldName);
  return F->GetFloat((VObject*)Class->Defaults);
  unguard;
}


//==========================================================================
//
//  GetClassFieldVec
//
//==========================================================================
__attribute__((unused)) static TVec GetClassFieldVec (VClass *Class, VName FieldName) {
  guard(GetClassFieldVec);
  VField *F = Class->FindFieldChecked(FieldName);
  return F->GetVec((VObject*)Class->Defaults);
  unguard;
}


//==========================================================================
//
//  GetClassDropItems
//
//==========================================================================
static TArray<VDropItemInfo> &GetClassDropItems (VClass *Class) {
  guard(GetClassDropItems);
  VField *F = Class->FindFieldChecked("DropItemList");
  return *(TArray<VDropItemInfo>*)F->GetFieldPtr((VObject*)Class->Defaults);
  unguard;
}


//==========================================================================
//
//  GetClassDamageFactors
//
//==========================================================================
static TArray<VDamageFactor> &GetClassDamageFactors (VClass *Class) {
  guard(GetClassDamageFactors);
  VField *F = Class->FindFieldChecked("DamageFactors");
  return *(TArray<VDamageFactor>*)F->GetFieldPtr((VObject*)Class->Defaults);
  unguard;
}


//==========================================================================
//
//  GetClassPainChances
//
//==========================================================================
static TArray<VPainChanceInfo> &GetClassPainChances (VClass *Class) {
  guard(GetClassPainChances);
  VField *F = Class->FindFieldChecked("PainChances");
  return *(TArray<VPainChanceInfo>*)F->GetFieldPtr((VObject*)Class->Defaults);
  unguard;
}


//==========================================================================
//
//  SetClassFieldInt
//
//==========================================================================
static void SetClassFieldInt (VClass *Class, VName FieldName, int Value, int Idx=0) {
  guard(SetClassFieldInt);
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetInt((VObject*)Class->Defaults, Value, Idx);
  unguard;
}


//==========================================================================
//
//  SetClassFieldByte
//
//==========================================================================
static void SetClassFieldByte (VClass *Class, VName FieldName, vuint8 Value) {
  guard(SetClassFieldByte);
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetByte((VObject*)Class->Defaults, Value);
  unguard;
}


//==========================================================================
//
//  SetClassFieldFloat
//
//==========================================================================
static void SetClassFieldFloat (VClass *Class, VName FieldName, float Value, int Idx=0) {
  guard(SetClassFieldFloat);
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetFloat((VObject*)Class->Defaults, Value, Idx);
  unguard;
}


//==========================================================================
//
//  SetClassFieldBool
//
//==========================================================================
static void SetClassFieldBool (VClass *Class, VName FieldName, int Value) {
  guard(SetClassFieldBool);
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetBool((VObject*)Class->Defaults, Value);
  unguard;
}


//==========================================================================
//
//  SetClassFieldName
//
//==========================================================================
static void SetClassFieldName (VClass *Class, VName FieldName, VName Value) {
  guard(SetClassFieldName);
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetName((VObject*)Class->Defaults, Value);
  unguard;
}


//==========================================================================
//
//  SetClassFieldStr
//
//==========================================================================
static void SetClassFieldStr (VClass *Class, VName FieldName, const VStr &Value) {
  guard(SetClassFieldStr);
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetStr((VObject*)Class->Defaults, Value);
  unguard;
}


//==========================================================================
//
//  SetClassFieldVec
//
//==========================================================================
__attribute__((unused)) static void SetClassFieldVec (VClass *Class, VName FieldName, const TVec &Value) {
  guard(SetClassFieldVec);
  VField *F = Class->FindFieldChecked(FieldName);
  F->SetVec((VObject*)Class->Defaults, Value);
  unguard;
}


//==========================================================================
//
//  SetFieldByte
//
//==========================================================================
__attribute__((unused)) static void SetFieldByte (VObject *Obj, VName FieldName, vuint8 Value) {
  guard(SetFieldByte);
  VField *F = Obj->GetClass()->FindFieldChecked(FieldName);
  F->SetByte(Obj, Value);
  unguard;
}


//==========================================================================
//
//  SetFieldFloat
//
//==========================================================================
__attribute__((unused)) static void SetFieldFloat (VObject *Obj, VName FieldName, float Value, int Idx=0) {
  guard(SetFieldFloat);
  VField *F = Obj->GetClass()->FindFieldChecked(FieldName);
  F->SetFloat(Obj, Value, Idx);
  unguard;
}


//==========================================================================
//
//  SetFieldBool
//
//==========================================================================
__attribute__((unused)) static void SetFieldBool (VObject *Obj, VName FieldName, int Value) {
  guard(SetFieldBool);
  VField *F = Obj->GetClass()->FindFieldChecked(FieldName);
  F->SetBool(Obj, Value);
  unguard;
}


//==========================================================================
//
//  SetFieldName
//
//==========================================================================
__attribute__((unused)) static void SetFieldName (VObject *Obj, VName FieldName, VName Value) {
  guard(SetFieldName);
  VField *F = Obj->GetClass()->FindFieldChecked(FieldName);
  F->SetName(Obj, Value);
  unguard;
}


//==========================================================================
//
//  SetFieldClass
//
//==========================================================================
__attribute__((unused)) static void SetFieldClass (VObject *Obj, VName FieldName, VClass *Value) {
  guard(SetFieldClass);
  VField *F = Obj->GetClass()->FindFieldChecked(FieldName);
  F->SetClass(Obj, Value);
  unguard;
}


//==========================================================================
//
//  AddClassFixup
//
//==========================================================================
static void AddClassFixup (VClass *Class, VName FieldName, const VStr &ClassName, TArray<VClassFixup> &ClassFixups) {
  guard(AddClassFixup);
  VField *F = Class->FindFieldChecked(FieldName);
  VClassFixup &CF = ClassFixups.Alloc();
  CF.Offset = F->Ofs;
  CF.Name = ClassName;
  CF.ReqParent = F->Type.Class;
  CF.Class = Class;
  unguard;
}


//==========================================================================
//
//  AddClassFixup
//
//==========================================================================
static void AddClassFixup (VClass *Class, VField *Field, const VStr &ClassName, TArray<VClassFixup> &ClassFixups) {
  guard(AddClassFixup);
  VClassFixup &CF = ClassFixups.Alloc();
  CF.Offset = Field->Ofs;
  CF.Name = ClassName;
  CF.ReqParent = Field->Type.Class;
  CF.Class = Class;
  unguard;
}


//==========================================================================
//
//  SkipBlock
//
//==========================================================================
static void SkipBlock (VScriptParser *sc, int Level) {
  while (!sc->AtEnd() && Level > 0) {
         if (sc->Check("{")) ++Level;
    else if (sc->Check("}")) --Level;
    else sc->GetString();
  }
}


//==========================================================================
//
//  ParseMethodCall
//
//==========================================================================
static VExpression *ParseMethodCall (VScriptParser *sc, VName Name, TLocation Loc) {
  guard(ParseMethodCall);
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = 0;
  if (!sc->Check(")")) {
    do {
      Args[NumArgs] = ParseExpressionPriority13(sc);
      if (NumArgs == VMethod::MAX_PARAMS) ParseError(sc->GetLoc(), "Too many arguments"); else ++NumArgs;
    } while (sc->Check(","));
    sc->Expect(")");
  }
  return new VDecorateInvocation(Name, Loc, NumArgs, Args);
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority0
//
//==========================================================================
static VExpression *ParseExpressionPriority0 (VScriptParser *sc) {
  guard(ParseExpressionPriority0);
  TLocation l = sc->GetLoc();

  // check for quoted strings first, since these could also have numbers...
  if (sc->CheckQuotedString()) {
    int Val = DecPkg->FindString(*sc->String);
    return new VStringLiteral(sc->String, Val, l);
  }

  if (sc->CheckNumber()) {
    vint32 Val = sc->Number;
    return new VIntLiteral(Val, l);
  }

  if (sc->CheckFloat()) {
    float Val = sc->Float;
    return new VFloatLiteral(Val, l);
  }

  if (sc->Check("false")) return new VIntLiteral(0, l);
  if (sc->Check("true")) return new VIntLiteral(1, l);

  if (sc->Check("(")) {
    VExpression *op = ParseExpressionPriority13(sc);
    if (!op) ParseError(l, "Expression expected");
    sc->Expect(")");
    return op;
  }

  if (sc->CheckIdentifier()) {
    VStr Name = sc->String;
    if (Name.ICmp("args") == 0) {
      if (sc->GetString()) {
        if (sc->String == "[") {
          Name = VStr("GetArg");
          //fprintf(stderr, "*** ARGS ***\n");
          VExpression *Args[1];
          Args[0] = ParseExpressionPriority13(sc);
          if (!Args[0]) ParseError(l, "`args` index expression expected");
          sc->Expect("]");
          return new VDecorateInvocation(VName(*Name), l, 1, Args);
        }
        sc->UnGet();
      }
    }
    // skip random generator ID
    if ((!Name.ICmp("random") || !Name.ICmp("random2") || !Name.ICmp("frandom")) && sc->Check("[")) {
      sc->ExpectString();
      sc->Expect("]");
    }
    if (sc->Check("(")) return ParseMethodCall(sc, *Name.ToLower(), l);
    return new VDecorateSingleName(Name, l);
  }

  return nullptr;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority1
//
//==========================================================================
static VExpression *ParseExpressionPriority1 (VScriptParser *sc) {
  guard(ParseExpressionPriority1);
  VExpression *op = ParseExpressionPriority0(sc);
  TLocation l = sc->GetLoc();
  if (!op) return nullptr;
  bool done = false;
  do {
    if (sc->Check("[")) {
      VExpression *ind = ParseExpressionPriority13(sc);
      sc->Expect("]");
      op = new VArrayElement(op, ind, l);
    } else {
      done = true;
    }
  } while (!done);
  return op;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority2
//
//==========================================================================
static VExpression *ParseExpressionPriority2 (VScriptParser *sc) {
  guard(ParseExpressionPriority2);
  VExpression *op;
  TLocation l = sc->GetLoc();

  if (sc->Check("+")) {
    op = ParseExpressionPriority2(sc);
    return new VUnary(VUnary::Plus, op, l);
  }

  if (sc->Check("-")) {
    op = ParseExpressionPriority2(sc);
    return new VUnary(VUnary::Minus, op, l);
  }

  if (sc->Check("!")) {
    op = ParseExpressionPriority2(sc);
    return new VUnary(VUnary::Not, op, l);
  }

  if (sc->Check("~")) {
    op = ParseExpressionPriority2(sc);
    return new VUnary(VUnary::BitInvert, op, l);
  }

  return ParseExpressionPriority1(sc);
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority3
//
//==========================================================================
static VExpression *ParseExpressionPriority3 (VScriptParser *sc) {
  guard(ParseExpressionPriority3);
  VExpression *op1 = ParseExpressionPriority2(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("*")) {
      VExpression *op2 = ParseExpressionPriority2(sc);
      op1 = new VBinary(VBinary::Multiply, op1, op2, l);
    } else if (sc->Check("/")) {
      VExpression *op2 = ParseExpressionPriority2(sc);
      op1 = new VBinary(VBinary::Divide, op1, op2, l);
    } else if (sc->Check("%")) {
      VExpression *op2 = ParseExpressionPriority2(sc);
      op1 = new VBinary(VBinary::Modulus, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority4
//
//==========================================================================
static VExpression *ParseExpressionPriority4 (VScriptParser *sc) {
  guard(ParseExpressionPriority4);
  VExpression *op1 = ParseExpressionPriority3(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("+")) {
      VExpression *op2 = ParseExpressionPriority3(sc);
      op1 = new VBinary(VBinary::Add, op1, op2, l);
    } else if (sc->Check("-")) {
      VExpression *op2 = ParseExpressionPriority3(sc);
      op1 = new VBinary(VBinary::Subtract, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority5
//
//==========================================================================
static VExpression *ParseExpressionPriority5 (VScriptParser *sc) {
  guard(ParseExpressionPriority5);
  VExpression *op1 = ParseExpressionPriority4(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("<<")) {
      VExpression *op2 = ParseExpressionPriority4(sc);
      op1 = new VBinary(VBinary::LShift, op1, op2, l);
    } else if (sc->Check(">>")) {
      VExpression *op2 = ParseExpressionPriority4(sc);
      op1 = new VBinary(VBinary::RShift, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority6
//
//==========================================================================
static VExpression *ParseExpressionPriority6 (VScriptParser *sc) {
  guard(ParseExpressionPriority6);
  VExpression *op1 = ParseExpressionPriority5(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("<")) {
      VExpression *op2 = ParseExpressionPriority5(sc);
      op1 = new VBinary(VBinary::Less, op1, op2, l);
    } else if (sc->Check("<=")) {
      VExpression *op2 = ParseExpressionPriority5(sc);
      op1 = new VBinary(VBinary::LessEquals, op1, op2, l);
    } else if (sc->Check(">")) {
      VExpression *op2 = ParseExpressionPriority5(sc);
      op1 = new VBinary(VBinary::Greater, op1, op2, l);
    } else if (sc->Check(">=")) {
      VExpression *op2 = ParseExpressionPriority5(sc);
      op1 = new VBinary(VBinary::GreaterEquals, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority7
//
//==========================================================================
static VExpression *ParseExpressionPriority7 (VScriptParser *sc) {
  guard(ParseExpressionPriority7);
  VExpression *op1 = ParseExpressionPriority6(sc);
  if (!op1) return nullptr;
  bool done = false;
  do {
    TLocation l = sc->GetLoc();
    if (sc->Check("==")) {
      VExpression *op2 = ParseExpressionPriority6(sc);
      op1 = new VBinary(VBinary::Equals, op1, op2, l);
    } else if (sc->Check("!=")) {
      VExpression *op2 = ParseExpressionPriority6(sc);
      op1 = new VBinary(VBinary::NotEquals, op1, op2, l);
    } else {
      done = true;
    }
  } while (!done);
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority8
//
//==========================================================================
static VExpression *ParseExpressionPriority8 (VScriptParser *sc) {
  guard(ParseExpressionPriority8);
  VExpression *op1 = ParseExpressionPriority7(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("&")) {
    VExpression *op2 = ParseExpressionPriority7(sc);
    op1 = new VBinary(VBinary::And, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority9
//
//==========================================================================
static VExpression *ParseExpressionPriority9 (VScriptParser *sc) {
  guard(ParseExpressionPriority9);
  VExpression *op1 = ParseExpressionPriority8(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("^")) {
    VExpression *op2 = ParseExpressionPriority8(sc);
    op1 = new VBinary(VBinary::XOr, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority10
//
//==========================================================================
static VExpression *ParseExpressionPriority10 (VScriptParser *sc) {
  guard(ParseExpressionPriority10);
  VExpression *op1 = ParseExpressionPriority9(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("|")) {
    VExpression *op2 = ParseExpressionPriority9(sc);
    op1 = new VBinary(VBinary::Or, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority11
//
//==========================================================================
static VExpression *ParseExpressionPriority11 (VScriptParser *sc) {
  guard(ParseExpressionPriority11);
  VExpression *op1 = ParseExpressionPriority10(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("&&")) {
    VExpression *op2 = ParseExpressionPriority10(sc);
    op1 = new VBinaryLogical(VBinaryLogical::And, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  ParseExpressionPriority12
//
//==========================================================================
static VExpression *ParseExpressionPriority12 (VScriptParser *sc) {
  guard(ParseExpressionPriority12);
  VExpression *op1 = ParseExpressionPriority11(sc);
  if (!op1) return nullptr;
  TLocation l = sc->GetLoc();
  while (sc->Check("||")) {
    VExpression *op2 = ParseExpressionPriority11(sc);
    op1 = new VBinaryLogical(VBinaryLogical::Or, op1, op2, l);
    l = sc->GetLoc();
  }
  return op1;
  unguard;
}


//==========================================================================
//
//  VParser::ParseExpressionPriority13
//
//==========================================================================
static VExpression *ParseExpressionPriority13 (VScriptParser *sc) {
  guard(ParseExpressionPriority13);
  VExpression *op = ParseExpressionPriority12(sc);
  if (!op) return nullptr;
  TLocation l = sc->GetLoc();
  if (sc->Check("?")) {
    VExpression *op1 = ParseExpressionPriority13(sc);
    sc->Expect(":");
    VExpression *op2 = ParseExpressionPriority13(sc);
    op = new VConditional(op, op1, op2, l);
  }
  return op;
  unguard;
}


//==========================================================================
//
//  ParseExpression
//
//==========================================================================
static VExpression *ParseExpression (VScriptParser *sc) {
  guard(ParseExpression);
  return ParseExpressionPriority13(sc);
  unguard;
}


//==========================================================================
//
//  ParseConst
//
//==========================================================================
static void ParseConst (VScriptParser *sc) {
  guard(ParseConst);

  sc->SetCMode(true);
  sc->Expect("int");
  sc->ExpectString();
  TLocation Loc = sc->GetLoc();
  VStr Name = sc->String.ToLower();
  sc->Expect("=");

  VExpression *Expr = ParseExpression(sc);
  if (!Expr) {
    sc->Error("Constant value expected");
  } else {
    VEmitContext ec(DecPkg);
    Expr = Expr->Resolve(ec);
    if (Expr) {
      int Val = Expr->GetIntConst();
      delete Expr;
      Expr = nullptr;
      VConstant *C = new VConstant(*Name, DecPkg, Loc);
      C->Type = TYPE_Int;
      C->Value = Val;
    }
  }
  sc->Expect(";");
  sc->SetCMode(false);

  unguard;
}


//==========================================================================
//
//  ParseAction
//
//==========================================================================
static void ParseAction (VScriptParser *sc, VClass *Class) {
  guard(ParseAction);
  sc->Expect("native");
  // find the method. First try with decorate_ prefix, then without
  sc->ExpectIdentifier();
  VMethod *M = Class->FindMethod(va("decorate_%s", *sc->String));
  if (!M) M = Class->FindMethod(*sc->String);
  if (!M) sc->Error(va("Method %s not found in class %s", *sc->String, Class->GetName()));
  if (M && M->ReturnType.Type != TYPE_Void) sc->Error(va("State action %s doesn't return void", *sc->String));
  VDecorateStateAction &A = Class->DecorateStateActions.Alloc();
  A.Name = *sc->String.ToLower();
  A.Method = M;
  // skip arguments, right now I don't care bout them
  sc->Expect("(");
  while (!sc->Check(")")) sc->ExpectString();
  sc->Expect(";");
  unguard;
}


//==========================================================================
//
//  ParseClass
//
//==========================================================================
static void ParseClass (VScriptParser *sc) {
  guard(ParseClass);
  sc->SetCMode(true);
  // get class name and find the class
  sc->ExpectString();
  VClass *Class = VClass::FindClass(*sc->String);
  if (!Class) sc->Error("Class not found");
  // I don't care about parent class name because in Vavoom it can be different
  sc->Expect("extends");
  sc->ExpectString();
  sc->Expect("native");
  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("action")) {
      ParseAction(sc, Class);
    } else {
      sc->Error("Unknown class property");
    }
  }
  sc->SetCMode(false);
  unguard;
}


//==========================================================================
//
//  ParseEnum
//
//==========================================================================
static void ParseEnum (VScriptParser *sc) {
  guard(ParseEnum);
  GCon->Logf("Enum");
  sc->Expect("{");
  SkipBlock(sc, 1);
  unguard;
}


//==========================================================================
//
//  ParseFlag
//
//==========================================================================
static bool ParseFlag (VScriptParser *sc, VClass *Class, bool Value, TArray<VClassFixup> &ClassFixups) {
  guard(ParseFlag);
  auto floc = sc->GetLoc(); // for warnings
  // get full name of the flag
  sc->ExpectIdentifier();
  VName FlagName(*sc->String.ToLower());
  VName ClassFilter(NAME_None);
  if (sc->Check(".")) {
    sc->ExpectIdentifier();
    ClassFilter = FlagName;
    FlagName = *sc->String.ToLower();
  }
  VObject *DefObj = (VObject*)Class->Defaults;

  for (int j = 0; j < FlagList.Num(); ++j) {
    VFlagList &ClassDef = FlagList[j];
    if (ClassFilter != NAME_None && ClassDef.Class->LowerCaseName != ClassFilter) continue;
    if (!Class->IsChildOf(ClassDef.Class)) continue;
    for (int i = ClassDef.FlagsHash[GetTypeHash(FlagName)&(FLAGS_HASH_SIZE-1)]; i != -1; i = ClassDef.Flags[i].HashNext) {
      const VFlagDef &F = ClassDef.Flags[i];
      if (FlagName == F.Name) {
        switch (F.Type) {
          case FLAG_Bool: F.Field->SetBool(DefObj, Value); break;
          case FLAG_Unsupported: if (dbg_show_decorate_unsupported) GCon->Logf("%s: Unsupported flag %s in %s", *floc.toStringNoCol(), *FlagName, Class->GetName()); break;
          case FLAG_Byte: F.Field->SetByte(DefObj, Value ? F.BTrue : F.BFalse); break;
          case FLAG_Float: F.Field->SetFloat(DefObj, Value ? F.FTrue : F.FFalse); break;
          case FLAG_Name: F.Field->SetName(DefObj, Value ? F.NTrue : F.NFalse); break;
          case FLAG_Class: AddClassFixup(Class, F.Field, (Value ? *F.NTrue : *F.NFalse), ClassFixups); break;
          case FLAG_NoClip: F.Field->SetBool(DefObj, !Value); F.Field2->SetBool(DefObj, !Value); break;
        }
        return true;
      }
    }
  }
  if (decorate_fail_on_unknown) {
    sc->Error(va("Unknown flag \"%s\"", *FlagName));
    return false;
  }
  GCon->Logf("WARNING: %s: Unknown flag \"%s\"", *floc.toStringNoCol(), *FlagName);
  /*
  if (!sc->IsAtEol()) {
    sc->Crossed = false;
    while (!sc->AtEnd() && !sc->Crossed) sc->GetString();
  } else {
    sc->GetString();
  }
  */
  return true;
  unguard;
}


//==========================================================================
//
//  ParseStateString
//
//==========================================================================
static VStr ParseStateString (VScriptParser *sc) {
  guard(ParseStateString);
  VStr StateStr;

  if (!sc->CheckQuotedString()) sc->ExpectIdentifier();
  StateStr = sc->String;

  if (sc->Check("::")) {
    sc->ExpectIdentifier();
    StateStr += "::";
    StateStr += sc->String;
  }

  if (sc->Check(".")) {
    sc->ExpectIdentifier();
    StateStr += ".";
    StateStr += sc->String;
  }

  return StateStr;
  unguard;
}


//==========================================================================
//
//  ParseStates
//
//==========================================================================
static bool ParseStates (VScriptParser *sc, VClass *Class, TArray<VState*> &States) {
  guard(ParseStates);
  VState *PrevState = nullptr;
  VState *LastState = nullptr;
  VState *LoopStart = nullptr;
  int NewLabelsStart = Class->StateLabelDefs.Num();

  sc->Expect("{");
  // disable escape sequences in states
  sc->SetEscape(false);
  while (!sc->Check("}")) {
    TLocation TmpLoc = sc->GetLoc();
    VStr TmpName = ParseStateString(sc);

    // goto command
    if (!TmpName.ICmp("Goto")) {
      VName GotoLabel = *ParseStateString(sc);
      int GotoOffset = 0;
      if (sc->Check("+")) {
        sc->ExpectNumber();
        GotoOffset = sc->Number;
      }
      if (!LastState && NewLabelsStart == Class->StateLabelDefs.Num()) sc->Error("Goto before first state");
      if (LastState) {
        LastState->GotoLabel = GotoLabel;
        LastState->GotoOffset = GotoOffset;
      }
      for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) {
        Class->StateLabelDefs[i].GotoLabel = GotoLabel;
        Class->StateLabelDefs[i].GotoOffset = GotoOffset;
      }
      NewLabelsStart = Class->StateLabelDefs.Num();
      PrevState = nullptr;
      continue;
    }

    // stop command
    if (!TmpName.ICmp("Stop")) {
      if (!LastState && NewLabelsStart == Class->StateLabelDefs.Num()) {
        sc->Error("Stop before first state");
        continue;
      }
      if (LastState) LastState->NextState = nullptr;
      for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) Class->StateLabelDefs[i].State = nullptr;
      NewLabelsStart = Class->StateLabelDefs.Num();
      PrevState = nullptr;
      continue;
    }

    // wait command
    if (!TmpName.ICmp("Wait") || !TmpName.ICmp("Fail")) {
      if (!LastState) {
        sc->Error(va("%s before first state", *TmpName));
        continue;
      }
      LastState->NextState = LastState;
      PrevState = nullptr;
      continue;
    }

    // loop command
    if (!TmpName.ICmp("Loop")) {
      if (!LastState) {
        sc->Error("Loop before first state");
        continue;
      }
      LastState->NextState = LoopStart;
      PrevState = nullptr;
      continue;
    }

    // check for label
    if (sc->Check(":")) {
      LastState = nullptr;
      VStateLabelDef &Lbl = Class->StateLabelDefs.Alloc();
      Lbl.Loc = TmpLoc;
      Lbl.Name = TmpName;
      continue;
    }

    VState *State = new VState(va("S_%d", States.Num()), Class, TmpLoc);
    States.Append(State);

    // sprite name
    if (TmpName.Length() != 4) sc->Error("Invalid sprite name");
    if (TmpName == "####" || TmpName == "----") {
      State->SpriteName = NAME_None; // don't change
    } else {
      State->SpriteName = *TmpName.ToLower();
    }

    // frame
    sc->ExpectString();
    char FChar = VStr::ToUpper(sc->String[0]);
    if (FChar == '#' || FChar == '-') {
      State->Frame = VState::FF_DONTCHANGE;
    } else {
      if (FChar < 'A' || FChar > ']') sc->Error(va("Frames must be A-Z, [, \\ or ], got <%c>", FChar));
      State->Frame = FChar - 'A';
    }
    VStr FramesString = sc->String;

    // tics
    if (!sc->GetString()) sc->Error("decorate: tics expected");
    // `random(a, b)`?
    if (sc->String.ICmp("random") == 0) {
      sc->Expect("(");
      sc->ExpectNumberWithSign();
      State->Arg1 = sc->Number;
      sc->Expect(",");
      sc->ExpectNumberWithSign();
      State->Arg2 = sc->Number;
      sc->Expect(")");
      State->Time = float(State->Arg1)/35.0f;
    } else {
      // number
      sc->UnGet();
      sc->ExpectNumberWithSign();
      if (sc->Number < 0) {
        State->Time = sc->Number;
      } else {
        State->Time = float(sc->Number)/35.0f;
      }
    }

    bool NeedsUnget = true;
    while (sc->GetString() && !sc->Crossed) {
      // check for bright parameter
      if (!sc->String.ICmp("Bright")) {
        State->Frame |= VState::FF_FULLBRIGHT;
        continue;
      }
      // check for canrise parameter
      if (!sc->String.ICmp("CanRaise")) {
        GCon->Logf("%s: unsupported DECORATE 'CanRaise' attribute", *sc->GetLoc().toStringNoCol());
        State->Frame |= VState::FF_CANRAISE;
        continue;
      }

      //FIXME: check for light parameter (unsupported for now)
      if (!sc->String.ICmp("Light")) {
        //LIGHT(UNMNRALR)
        GCon->Logf("%s: unsupported DECORATE 'Light' attribute", *sc->GetLoc().toStringNoCol());
        if (!sc->Crossed) {
          if (sc->Check("(")) {
            while (!sc->IsAtEol()) {
              if (sc->Check(")")) break;
              sc->GetString();
            }
          }
        }
        continue;
      }

      // check for other parameters
      if (!sc->String.ICmp("Fast") || !sc->String.ICmp("CanRaise") || !sc->String.ICmp("NoDelay") || !sc->String.ICmp("Slow")) {
        GCon->Logf("%s: unsupported DECORATE state keyword: '%s'", *sc->GetLoc().toStringNoCol(), *sc->String);
        continue;
      }

      // check for offsets
      if (!sc->String.ICmp("Offset")) {
        sc->Expect("(");
        sc->ExpectNumberWithSign();
        State->Misc1 = sc->Number;
        sc->Expect(",");
        sc->ExpectNumberWithSign();
        State->Misc2 = sc->Number;
        sc->Expect(")");
        continue;
      }

      // get function name and parse arguments
      VStr FuncName = sc->String;
      VStr FuncNameLower = sc->String.ToLower();
      VExpression *Args[VMethod::MAX_PARAMS+1];
      int NumArgs = 0;
      if (sc->Check("(")) {
        if (!sc->Check(")")) {
          do {
            Args[NumArgs] = ParseExpressionPriority13(sc);
            if (NumArgs == VMethod::MAX_PARAMS) ParseError(sc->GetLoc(), "Too many arguments"); else ++NumArgs;
          } while (sc->Check(","));
          sc->Expect(")");
        }
      }

      // find the state action method. First check action specials, then state actions
      VMethod *Func = nullptr;
      for (int i = 0; i < LineSpecialInfos.Num(); ++i) {
        if (LineSpecialInfos[i].Name == FuncNameLower) {
          Func = Class->FindMethodChecked("A_ExecActionSpecial");
          if (NumArgs > 5) {
            sc->Error("Too many arguments");
          } else {
            // add missing arguments
            while (NumArgs < 5) {
              Args[NumArgs] = new VIntLiteral(0, sc->GetLoc());
              ++NumArgs;
            }
            // add action special number argument
            Args[5] = new VIntLiteral(LineSpecialInfos[i].Number, sc->GetLoc());
            ++NumArgs;
          }
          break;
        }
      }
      if (!Func) {
        VDecorateStateAction *Act = Class->FindDecorateStateAction(*FuncNameLower);
        Func = (Act ? Act->Method : nullptr);
      }
      //fprintf(stderr, "<%s>\n", *FuncNameLower);
      if (!Func) {
        GCon->Logf("ERROR: Unknown state action %s in %s", *FuncName, Class->GetName());
      } else if (Func->NumParams || NumArgs || FuncNameLower == "a_explode") {
        VInvocation *Expr = new VInvocation(nullptr, Func, nullptr, false, false, sc->GetLoc(), NumArgs, Args);
        Expr->CallerState = State;
        Expr->MultiFrameState = (FramesString.Length() > 1);
        VExpressionStatement *Stmt = new VExpressionStatement(Expr);
        VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
        M->Flags = FUNC_Final;
        M->ReturnType = TYPE_Void;
        M->Statement = Stmt;
        M->ParamsSize = 1;
        Class->AddMethod(M);
        State->Function = M;
      } else {
        State->Function = Func;
      }

      // if state function is not assigned, it means something is wrong
      // in that case we need to free argument expressions
      if (!State->Function) {
        for (int i = 0; i < NumArgs; ++i) {
          if (Args[i]) {
            delete Args[i];
            Args[i] = nullptr;
          }
        }
      }
      NeedsUnget = false;
      break;
    }
    if (NeedsUnget) sc->UnGet();

    // link previous state
    if (PrevState) PrevState->NextState = State;

    // assign state to the labels
    for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) {
      Class->StateLabelDefs[i].State = State;
      LoopStart = State;
    }
    NewLabelsStart = Class->StateLabelDefs.Num();
    PrevState = State;
    LastState = State;

    for (int i = 1; i < FramesString.Length(); ++i) {
      char FSChar = VStr::ToUpper(FramesString[i]);
      vint32 frm;
      if (FSChar == '#' || FSChar == '-') {
        frm = VState::FF_DONTCHANGE;
      } else {
        if (FSChar < 'A' || FSChar > ']') sc->Error(va("Frames must be A-Z, [, \\ or ], got <%c>", FSChar));
        frm = FSChar-'A';
      }

      // create a new state
      VState *s2 = new VState(va("S_%d", States.Num()), Class, sc->GetLoc());
      States.Append(s2);
      s2->SpriteName = State->SpriteName;
      s2->Frame = (State->Frame&VState::FF_FULLBRIGHT)|frm;
      s2->Time = State->Time;
      s2->Misc1 = State->Misc1;
      s2->Misc2 = State->Misc2;
      s2->Function = State->Function;

      // link previous state
      PrevState->NextState = s2;
      PrevState = s2;
      LastState = s2;
    }
  }
  // re-enable escape sequences
  sc->SetEscape(true);
  return true;
  unguard;
}


//==========================================================================
//
//  ParseParentState
//
//  This is for compatibility with old WADs.
//
//==========================================================================
static void ParseParentState (VScriptParser *sc, VClass *Class, const char *LblName) {
  guard(ParseParentState);
  TLocation TmpLoc = sc->GetLoc();
  VState *State = nullptr;
  // if there's a string token on next line, it gets eaten: is this a bug?
  if (sc->GetString() && !sc->Crossed) {
    sc->UnGet();
    if (sc->Check("0")) {
      State = nullptr;
    } else if (sc->Check("parent")) {
      // find state in parent class
      sc->ExpectString();
      VStateLabel *SLbl = Class->ParentClass->FindStateLabel(*sc->String);
      State = (SLbl ? SLbl->State : nullptr);

      // check for offset
      int Offs = 0;
      if (sc->Check("+")) {
        sc->ExpectNumber();
        Offs = sc->Number;
      }

      if (!State && Offs) {
        sc->Error(va("Attempt to get invalid state from actor %s", Class->GetSuperClass()->GetName()));
      } else if (State) {
        State = State->GetPlus(Offs, true);
      }
    } else {
      sc->Error("Invalid state assignment");
    }
  } else {
    State = nullptr;
  }

  VStateLabelDef &Lbl = Class->StateLabelDefs.Alloc();
  Lbl.Loc = TmpLoc;
  Lbl.Name = LblName;
  Lbl.State = State;
  unguard;
}


//==========================================================================
//
//  ParseActor
//
//==========================================================================
static void ParseActor (VScriptParser *sc, TArray<VClassFixup> &ClassFixups) {
  guard(ParseActor);
  // parse actor name
  // in order to allow dots in actor names, this is done in non-C mode,
  // so we have to do a little bit more complex parsing
  sc->ExpectString();
  VStr NameStr;
  VStr ParentStr;
  int ColonPos = sc->String.IndexOf(':');
  if (ColonPos >= 0) {
    // there's a colon inside, so split up the string
    NameStr = VStr(sc->String, 0, ColonPos);
    ParentStr = VStr(sc->String, ColonPos+1, sc->String.Length()-ColonPos-1);
  } else {
    NameStr = sc->String;
  }

  if (GArgs.CheckParm("-debug_decorate")) sc->Message(va("Parsing class %s", *NameStr));

  VClass *DupCheck = VClass::FindClassLowerCase(*NameStr.ToLower());
  if (DupCheck != nullptr && DupCheck->MemberType == MEMBER_Class) {
    sc->Message(va("Warning: Redeclared class %s", *NameStr));
  }

  if (ColonPos < 0) {
    // there's no colon, check if next string starts with it
    sc->ExpectString();
    if (sc->String[0] == ':') {
      ColonPos = 0;
      ParentStr = VStr(sc->String, 1, sc->String.Length()-1);
    } else {
      sc->UnGet();
    }
  }

  // if we got colon but no parent class name, then get it
  if (ColonPos >= 0 && ParentStr.IsEmpty()) {
    sc->ExpectString();
    ParentStr = sc->String;
  }

  VClass *ParentClass = ActorClass;
  if (ParentStr.IsNotEmpty()) {
    ParentClass = VClass::FindClassLowerCase(*ParentStr.ToLower());
    if (ParentClass == nullptr || ParentClass->MemberType != MEMBER_Class) {
      sc->Error(va("Parent class %s not found", *ParentStr));
    }
    if (ParentClass != nullptr && !ParentClass->IsChildOf(ActorClass)) {
      sc->Error(va("Parent class %s is not an actor class", *ParentStr));
    }
  }

  VClass *Class = ParentClass->CreateDerivedClass(*NameStr, DecPkg, sc->GetLoc());
  DecPkg->ParsedClasses.Append(Class);

  if (Class) {
    // copy class fixups of the parent class
    for (int i = 0; i < ClassFixups.Num(); ++i) {
      VClassFixup &CF = ClassFixups[i];
      if (CF.Class == ParentClass) {
        VClassFixup &NewCF = ClassFixups.Alloc();
        NewCF.Offset = CF.Offset;
        NewCF.Name = CF.Name;
        NewCF.ReqParent = CF.ReqParent;
        NewCF.Class = Class;
      }
    }
  }

  VClass *ReplaceeClass = nullptr;
  if (sc->Check("replaces")) {
    sc->ExpectString();
    ReplaceeClass = VClass::FindClassLowerCase(*sc->String.ToLower());
    if (ReplaceeClass == nullptr || ReplaceeClass->MemberType != MEMBER_Class) {
      sc->Error(va("Replaced class %s not found", *sc->String));
    }
    if (ReplaceeClass != nullptr && !ReplaceeClass->IsChildOf(ActorClass)) {
      sc->Error(va("Replaced class %s is not an actor class", *sc->String));
    }
  }

  // time to switch to the C mode
  sc->SetCMode(true);

  int GameFilter = 0;
  int DoomEdNum = -1;
  int SpawnNum = -1;
  TArray<VState*> States;
  bool DropItemsDefined = false;
  VObject *DefObj = (VObject*)Class->Defaults;

  if (sc->CheckNumber()) {
    if (sc->Number < -1 || sc->Number > 32767) sc->Error("DoomEdNum is out of range [-1, 32767]");
    DoomEdNum = sc->Number;
  }

  sc->Expect("{");
  while (!sc->Check("}")) {
    if (sc->Check("+")) {
      if (!ParseFlag(sc, Class, true, ClassFixups)) return;
      continue;
    }

    if (sc->Check("-")) {
      if (!ParseFlag(sc, Class, false, ClassFixups)) return;
      continue;
    }

    if (sc->Check("action")) {
      ParseAction(sc, Class);
      continue;
    }

    // get full name of the property
    auto prloc = sc->GetLoc(); // for error messages
    sc->ExpectIdentifier();
    VStr Prop = sc->String;
    while (sc->Check(".")) {
      sc->ExpectIdentifier();
      Prop += ".";
      Prop += sc->String;
    }
    VName PropName = *Prop.ToLower();

    bool FoundProp = false;
    for (int j = 0; j < FlagList.Num() && !FoundProp; ++j) {
      VFlagList &ClassDef = FlagList[j];
      if (!Class->IsChildOf(ClassDef.Class)) continue;
      for (int i = ClassDef.PropsHash[GetTypeHash(PropName)&(PROPS_HASH_SIZE-1)]; i != -1; i = ClassDef.Props[i].HashNext) {
        VPropDef &P = FlagList[j].Props[i];
        if (PropName != P.Name) continue;
        switch (P.Type) {
          case PROP_Int:
            sc->ExpectNumberWithSign();
            P.Field->SetInt(DefObj, sc->Number);
            break;
          case PROP_IntTrunced:
            sc->ExpectFloatWithSign();
            P.Field->SetInt(DefObj, (int)sc->Float);
            break;
          case PROP_IntConst:
            P.Field->SetInt(DefObj, P.IConst);
            break;
          case PROP_IntUnsupported:
            //FIXME
            sc->CheckNumberWithSign();
            if (dbg_show_decorate_unsupported) GCon->Logf("%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
            break;
          case PROP_IntIdUnsupported:
            //FIXME
            {
              bool oldcm = sc->IsCMode();
              sc->SetCMode(true);
              sc->CheckNumberWithSign();
              sc->Expect(",");
              sc->ExpectIdentifier();
              if (sc->Check(",")) sc->ExpectIdentifier();
              sc->SetCMode(oldcm);
              if (dbg_show_decorate_unsupported) GCon->Logf("%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
            }
            break;
          case PROP_BitIndex:
            sc->ExpectNumber();
            P.Field->SetInt(DefObj, 1 << (sc->Number - 1));
            break;
          case PROP_Float:
            sc->ExpectFloatWithSign();
            P.Field->SetFloat(DefObj, sc->Float);
            break;
          case PROP_FloatUnsupported:
            //FIXME
            sc->ExpectFloatWithSign();
            if (dbg_show_decorate_unsupported) GCon->Logf("%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
            break;
          case PROP_Speed:
            sc->ExpectFloatWithSign();
            P.Field->SetFloat(DefObj, sc->Float * 35.0);
            break;
          case PROP_Tics:
            sc->ExpectNumberWithSign();
            P.Field->SetFloat(DefObj, sc->Number / 35.0);
            break;
          case PROP_TicsSecs:
            sc->ExpectNumberWithSign();
            P.Field->SetFloat(DefObj, sc->Number >= 0 ?
              sc->Number / 35.0 : sc->Number);
            break;
          case PROP_Percent:
            sc->ExpectFloat();
            P.Field->SetFloat(DefObj, MID(0, sc->Float, 100) / 100.0);
            break;
          case PROP_FloatClamped:
            sc->ExpectFloatWithSign();
            P.Field->SetFloat(DefObj, MID(P.FMin, sc->Float, P.FMax));
            break;
          case PROP_FloatClamped2:
            sc->ExpectFloatWithSign();
            P.Field->SetFloat(DefObj, MID(P.FMin, sc->Float, P.FMax));
            P.Field2->SetFloat(DefObj, MID(P.FMin, sc->Float, P.FMax));
            break;
          case PROP_FloatOpt2:
            sc->ExpectFloat();
            P.Field->SetFloat(DefObj, sc->Float);
            P.Field2->SetFloat(DefObj, sc->Float);
            if (sc->Check(",")) {
              sc->ExpectFloat();
              P.Field2->SetFloat(DefObj, sc->Float);
            } else if (sc->CheckFloat()) {
              P.Field2->SetFloat(DefObj, sc->Float);
            }
            break;
          case PROP_Name:
            sc->ExpectString();
            P.Field->SetName(DefObj, *sc->String);
            break;
          case PROP_NameLower:
            sc->ExpectString();
            P.Field->SetName(DefObj, *sc->String.ToLower());
            break;
          case PROP_Str:
            sc->ExpectString();
            P.Field->SetStr(DefObj, sc->String);
            break;
          case PROP_StrUnsupported:
            //FIXME
            sc->ExpectString();
            if (dbg_show_decorate_unsupported) GCon->Logf("%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
            break;
          case PROP_Class:
            sc->ExpectString();
            AddClassFixup(Class, P.Field, P.CPrefix + sc->String, ClassFixups);
            break;
          case PROP_Power_Class:
            // This is a very inconvenient shit!
            // but ZDoom had to prepend "power" to the name...
            sc->ExpectString();
            AddClassFixup(Class, P.Field, sc->String.StartsWith("Power") || sc->String.StartsWith("power") ?
                sc->String : P.CPrefix+sc->String, ClassFixups);
            break;
          case PROP_BoolConst:
            P.Field->SetBool(DefObj, P.IConst);
            break;
          case PROP_State:
            ParseParentState(sc, Class, *P.PropName);
            break;
          case PROP_Game:
                 if (sc->Check("Doom")) GameFilter |= GAME_Doom;
            else if (sc->Check("Heretic")) GameFilter |= GAME_Heretic;
            else if (sc->Check("Hexen")) GameFilter |= GAME_Hexen;
            else if (sc->Check("Strife")) GameFilter |= GAME_Strife;
            else if (sc->Check("Raven")) GameFilter |= GAME_Raven;
            else if (sc->Check("Any")) GameFilter |= GAME_Any;
            else if (GameFilter) sc->Error("Unknown game filter");
            break;
          case PROP_SpawnId:
            sc->ExpectNumber();
            SpawnNum = sc->Number;
            break;
          case PROP_ConversationId:
            sc->ExpectNumber();
            P.Field->SetInt(DefObj, sc->Number);
            if (sc->Check(",")) {
              sc->ExpectNumberWithSign();
              sc->Expect(",");
              sc->ExpectNumberWithSign();
            }
            break;
          case PROP_PainChance:
            if (sc->CheckNumber()) {
              P.Field->SetFloat(DefObj, float(sc->Number)/256.0);
            } else {
              sc->ExpectString();
              VName DamageType = (sc->String.ICmp("Normal") ? NAME_None : VName(*sc->String));
              sc->Expect(",");
              sc->ExpectNumber();

              // check pain chances array for replacements
              TArray<VPainChanceInfo> &PainChances = GetClassPainChances(Class);
              VPainChanceInfo *PC = nullptr;
              for (i = 0; i < PainChances.Num(); ++i) {
                if (PainChances[i].DamageType == DamageType) {
                  PC = &PainChances[i];
                  break;
                }
              }
              if (!PC) {
                PC = &PainChances.Alloc();
                PC->DamageType = DamageType;
              }
              PC->Chance = float(sc->Number)/256.0;
            }
            break;
          case PROP_DamageFactor:
            {
              VName DamageType = NAME_None;
              // Check if we only have a number instead of a string, since
              // there are some custom WAD files that don't specify a DamageType,
              // but specify a DamageFactor
              if (!sc->CheckFloat()) {
                sc->ExpectString();
                DamageType = !sc->String.ICmp("Normal") ? NAME_None : VName(*sc->String);
                sc->Expect(",");
                sc->ExpectFloat();
              }

              // check damage factors array for replacements
              TArray<VDamageFactor> DamageFactors = GetClassDamageFactors(Class);
              VDamageFactor *DF = nullptr;
              for (i = 0; i < DamageFactors.Num(); ++i) {
                if (DamageFactors[i].DamageType == DamageType) {
                  DF = &DamageFactors[i];
                  break;
                }
              }
              if (!DF) {
                DF = &DamageFactors.Alloc();
                DF->DamageType = DamageType;
              }
              DF->Factor = sc->Float;
            }
            break;
          case PROP_MissileDamage:
            if (sc->Check("(")) {
              VExpression *Expr = ParseExpression(sc);
              if (!Expr) {
                ParseError(sc->GetLoc(), "Damage expression expected");
              } else {
                VMethod *M = new VMethod("GetMissileDamage", Class, sc->GetLoc());
                M->Flags = FUNC_Override;
                M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Int, sc->GetLoc());
                M->ReturnType = TYPE_Int;
                M->NumParams = 2;
                M->Params[0].Name = "Mask";
                M->Params[0].Loc = sc->GetLoc();
                M->Params[0].TypeExpr = new VTypeExprSimple(TYPE_Int, sc->GetLoc());
                M->Params[1].Name = "Add";
                M->Params[1].Loc = sc->GetLoc();
                M->Params[1].TypeExpr = new VTypeExprSimple(TYPE_Int, sc->GetLoc());
                M->Statement = new VReturn(Expr, sc->GetLoc());
                Class->AddMethod(M);
                M->Define();
              }
              sc->Expect(")");
            } else {
              sc->ExpectNumber();
              P.Field->SetInt(DefObj, sc->Number);
            }
            break;
          case PROP_VSpeed:
            {
              sc->ExpectFloatWithSign();
              TVec Val = P.Field->GetVec(DefObj);
              Val.z = sc->Float * 35.0;
              P.Field->SetVec(DefObj, Val);
            }
            break;
          case PROP_RenderStyle:
            {
              int RenderStyle = 0;
                   if (sc->Check("None")) RenderStyle = STYLE_None;
              else if (sc->Check("Normal")) RenderStyle = STYLE_Normal;
              else if (sc->Check("Fuzzy")) RenderStyle = STYLE_Fuzzy;
              else if (sc->Check("SoulTrans")) RenderStyle = STYLE_SoulTrans;
              else if (sc->Check("OptFuzzy")) RenderStyle = STYLE_OptFuzzy;
              else if (sc->Check("Translucent")) RenderStyle = STYLE_Translucent;
              else if (sc->Check("Add")) RenderStyle = STYLE_Add;
              else if (sc->Check("Stencil")) { if (dbg_show_decorate_unsupported) GCon->Logf("%s: Render style 'Stencil' in '%s' is not yet supported", *prloc.toStringNoCol(), Class->GetName()); } //FIXME
              else if (sc->Check("Shaded")) RenderStyle = STYLE_Fuzzy; //FIXME -- This is an aproximated style... but it's not the desired one!
              else sc->Error("Bad render style");
              P.Field->SetByte(DefObj, RenderStyle);
            }
            break;
          case PROP_Translation:
            P.Field->SetInt(DefObj, R_ParseDecorateTranslation(sc, (GameFilter&GAME_Strife ? 7 : 3)));
            break;
          case PROP_BloodColour:
            {
              vuint32 Col;
              if (sc->CheckNumber()) {
                int r = MID(0, sc->Number, 255);
                sc->Check(",");
                sc->ExpectNumber();
                int g = MID(0, sc->Number, 255);
                sc->Check(",");
                sc->ExpectNumber();
                int b = MID(0, sc->Number, 255);
                Col = 0xff000000 | (r << 16) | (g << 8) | b;
              } else {
                sc->ExpectString();
                Col = M_ParseColour(sc->String);
              }
              P.Field->SetInt(DefObj, Col);
              P.Field2->SetInt(DefObj, R_GetBloodTranslation(Col));
            }
            break;
          case PROP_BloodType:
            sc->ExpectString();
            AddClassFixup(Class, P.Field, sc->String, ClassFixups);
            if (sc->Check(",")) sc->ExpectString();
            AddClassFixup(Class, P.Field2, sc->String, ClassFixups);
            if (sc->Check(",")) sc->ExpectString();
            AddClassFixup(Class, "AxeBloodType", sc->String, ClassFixups);
            break;
          case PROP_StencilColour:
            //FIXME
            if (sc->CheckNumber()) {
              sc->ExpectNumber();
              sc->ExpectNumber();
            } else {
              sc->ExpectString();
            }
            if (dbg_show_decorate_unsupported) GCon->Logf("%s: Property 'StencilColor' in '%s' is not yet supported", *prloc.toStringNoCol(), Class->GetName());
            break;
          case PROP_Monster:
            SetClassFieldBool(Class, "bShootable", true);
            SetClassFieldBool(Class, "bCountKill", true);
            SetClassFieldBool(Class, "bSolid", true);
            SetClassFieldBool(Class, "bActivatePushWall", true);
            SetClassFieldBool(Class, "bActivateMCross", true);
            SetClassFieldBool(Class, "bPassMobj", true);
            SetClassFieldBool(Class, "bMonster", true);
            SetClassFieldBool(Class, "bCanUseWalls", true);
            break;
          case PROP_Projectile:
            SetClassFieldBool(Class, "bNoBlockmap", true);
            SetClassFieldBool(Class, "bNoGravity", true);
            SetClassFieldBool(Class, "bDropOff", true);
            SetClassFieldBool(Class, "bMissile", true);
            SetClassFieldBool(Class, "bActivateImpact", true);
            SetClassFieldBool(Class, "bActivatePCross", true);
            SetClassFieldBool(Class, "bNoTeleport", true);
            if (GGameInfo->Flags & VGameInfo::GIF_DefaultBloodSplatter) SetClassFieldBool(Class, "bBloodSplatter", true);
            break;
          case PROP_BounceType:
            if (sc->Check("None")) {
              SetClassFieldByte(Class, "BounceType", BOUNCE_None);
            } else if (sc->Check("Doom")) {
              SetClassFieldByte(Class, "BounceType", BOUNCE_Doom);
              SetClassFieldBool(Class, "bBounceWalls", true);
              SetClassFieldBool(Class, "bBounceFloors", true);
              SetClassFieldBool(Class, "bBounceCeilings", true);
              SetClassFieldBool(Class, "bBounceOnActors", true);
              SetClassFieldBool(Class, "bBounceAutoOff", true);
            } else if (sc->Check("Heretic")) {
              SetClassFieldByte(Class, "BounceType", BOUNCE_Heretic);
              SetClassFieldBool(Class, "bBounceFloors", true);
              SetClassFieldBool(Class, "bBounceCeilings", true);
            } else if (sc->Check("Hexen")) {
              SetClassFieldByte(Class, "BounceType", BOUNCE_Hexen);
              SetClassFieldBool(Class, "bBounceWalls", true);
              SetClassFieldBool(Class, "bBounceFloors", true);
              SetClassFieldBool(Class, "bBounceCeilings", true);
              SetClassFieldBool(Class, "bBounceOnActors", true);
            } else if (sc->Check("DoomCompat")) {
              SetClassFieldByte(Class, "BounceType", BOUNCE_Doom);
            } else if (sc->Check("HereticCompat")) {
              SetClassFieldByte(Class, "BounceType", BOUNCE_Heretic);
            } else if (sc->Check("HexenCompat")) {
              SetClassFieldByte(Class, "BounceType", BOUNCE_Hexen);
            } else if (sc->Check("Grenade")) {
              // bounces on walls and flats like ZDoom bounce
              SetClassFieldByte(Class, "BounceType", BOUNCE_Doom);
              SetClassFieldBool(Class, "bBounceOnActors", false);
            } else if (sc->Check("Classic")) {
              // bounces on flats only, but does not die when bouncing
              SetClassFieldByte(Class, "BounceType", BOUNCE_Heretic);
              SetClassFieldBool(Class, "bMBFBounce", true);
            }
            break;
          case PROP_ClearFlags:
            for (j = 0; j < FlagList.Num(); ++j) {
              if (FlagList[j].Class != ActorClass) continue;
              for (i = 0; i < FlagList[j].Flags.Num(); ++i) {
                VFlagDef &F = FlagList[j].Flags[i];
                switch (F.Type) {
                  case FLAG_Bool: F.Field->SetBool(DefObj, false); break;
                }
              }
            }
            SetClassFieldByte(Class, "BounceType", BOUNCE_None);
            SetClassFieldBool(Class, "bColideWithThings", true);
            SetClassFieldBool(Class, "bColideWithWorld", true);
            SetClassFieldBool(Class, "bPickUp", false);
            break;
          case PROP_DropItem:
            {
              if (!DropItemsDefined) {
                GetClassDropItems(Class).Clear();
                DropItemsDefined = true;
              }
              sc->ExpectString();
              VDropItemInfo DI;
              DI.TypeName = *sc->String.ToLower();
              DI.Type = nullptr;
              DI.Amount = 0;
              DI.Chance = 1.0;
              bool HaveChance = false;
              if (sc->Check(",")) {
                sc->ExpectNumber();
                HaveChance = true;
              } else {
                HaveChance = sc->CheckNumber();
              }
              if (HaveChance) {
                DI.Chance = float(sc->Number)/255.0;
                if (sc->Check(",")) {
                  sc->ExpectNumber();
                  DI.Amount = sc->Number;
                } else if (sc->CheckNumber()) {
                  DI.Amount = sc->Number;
                }
              }
              GetClassDropItems(Class).Insert(0, DI);
            }
            break;
          case PROP_States:
            if (!ParseStates(sc, Class, States)) return;
            break;
          case PROP_SkipSuper:
            {
              // preserve items that should not be copied
              TArray<VDamageFactor> DamageFactors = GetClassDamageFactors(Class);
              TArray<VPainChanceInfo> PainChances = GetClassPainChances(Class);
              // copy default properties
              ActorClass->CopyObject(ActorClass->Defaults, Class->Defaults);
              // copy state labels
              Class->StateLabels = ActorClass->StateLabels;
              Class->ClassFlags |= CLASS_SkipSuperStateLabels;
              // drop items are reset back to the list of the parent class
              GetClassDropItems(Class) = GetClassDropItems(Class->ParentClass);
              // restore items that should not be copied
              GetClassDamageFactors(Class) = DamageFactors;
              GetClassPainChances(Class) = PainChances;
            }
            break;
          case PROP_Args:
            for (i = 0; i < 5; ++i) {
              sc->ExpectNumber();
              P.Field->SetInt(DefObj, sc->Number, i);
              if (i < 4 && !sc->Check(",")) break;
            }
            P.Field2->SetBool(DefObj, true);
            break;
          case PROP_LowMessage:
            sc->ExpectNumber();
            P.Field->SetInt(DefObj, sc->Number);
            sc->Expect(",");
            sc->ExpectString();
            P.Field2->SetStr(DefObj, sc->String);
            break;
          case PROP_PowerupColour:
                 if (sc->Check("InverseMap")) P.Field->SetInt(DefObj, 0x00123456);
            else if (sc->Check("GoldMap")) P.Field->SetInt(DefObj, 0x00123457);
            else if (sc->Check("RedMap")) P.Field->SetInt(DefObj, 0x00123458);
            else if (sc->Check("GreenMap")) P.Field->SetInt(DefObj, 0x00123459);
            else {
              int a, r, g, b;
              if (sc->CheckNumber()) {
                r = MID(0, sc->Number, 255);
                sc->Check(",");
                sc->ExpectNumber();
                g = MID(0, sc->Number, 255);
                sc->Check(",");
                sc->ExpectNumber();
                b = MID(0, sc->Number, 255);
              } else {
                vuint32 Col;
                sc->ExpectString();
                Col = M_ParseColour(sc->String);
                r = (Col >> 16) & 255;
                g = (Col >> 8) & 255;
                b = Col & 255;
              }
              sc->ResetCrossed();
              sc->Check(",");
              // alpha may be missing
              if (!sc->Crossed) {
                sc->ExpectFloat();
                a = MID(0, int(sc->Float * 255), 254);
                if (a > 250) a = 250;
              } else {
                a = 88; // default alpha, around 0.(3)
              }
              P.Field->SetInt(DefObj, (r << 16) | (g << 8) | b | (a << 24));
            }
            break;
          case PROP_ColourRange:
            sc->ExpectNumber();
            P.Field->SetInt(DefObj, sc->Number);
            sc->Check(",");
            sc->ExpectNumber();
            P.Field2->SetInt(DefObj, sc->Number);
            break;
          case PROP_DamageScreenColour:
            {
              // first number is ignored. Is it a bug?
              int Col;
              if (sc->CheckNumber()) {
                sc->ExpectNumber();
                int r = MID(sc->Number, 0, 255);
                sc->Check(",");
                sc->ExpectNumber();
                int g = MID(sc->Number, 0, 255);
                sc->Check(",");
                sc->ExpectNumber();
                int b = MID(sc->Number, 0, 255);
                Col = 0xff000000 | (r << 16) | (g << 8) | b;
              } else {
                sc->ExpectString();
                Col = M_ParseColour(sc->String);
                while (sc->Check(",")) {
                  sc->ExpectFloat();
                }
              }
              P.Field->SetInt(DefObj, Col);
            }
            break;
          case PROP_HexenArmor:
            sc->ExpectFloat();
            P.Field->SetFloat(DefObj, sc->Float, 0);
            sc->Expect(",");
            sc->ExpectFloat();
            P.Field->SetFloat(DefObj, sc->Float, 1);
            sc->Expect(",");
            sc->ExpectFloat();
            P.Field->SetFloat(DefObj, sc->Float, 2);
            sc->Expect(",");
            sc->ExpectFloat();
            P.Field->SetFloat(DefObj, sc->Float, 3);
            sc->Expect(",");
            sc->ExpectFloat();
            P.Field->SetFloat(DefObj, sc->Float, 4);
            break;
          case PROP_StartItem:
            {
              TArray<VDropItemInfo> &DropItems = *(TArray<VDropItemInfo>*)P.Field->GetFieldPtr(DefObj);
              if (!DropItemsDefined) {
                DropItems.Clear();
                DropItemsDefined = true;
              }
              sc->ExpectString();
              VDropItemInfo DI;
              DI.TypeName = *sc->String.ToLower();
              DI.Type = nullptr;
              DI.Amount = 0;
              DI.Chance = 1.0;
              if (sc->Check(",")) {
                sc->ExpectNumber();
                DI.Amount = sc->Number;
              } else if (sc->CheckNumber()) {
                DI.Amount = sc->Number;
              }
              DropItems.Insert(0, DI);
            }
            break;
          case PROP_MorphStyle:
            if (sc->CheckNumber()) {
              P.Field->SetInt(DefObj, sc->Number);
            } else {
              bool HaveParen = sc->Check("(");
              int Val = 0;
              do {
                     if (sc->Check("MRF_ADDSTAMINA")) Val |= 1;
                else if (sc->Check("MRF_FULLHEALTH")) Val |= 2;
                else if (sc->Check("MRF_UNDOBYTOMEOFPOWER")) Val |= 4;
                else if (sc->Check("MRF_UNDOBYCHAOSDEVICE")) Val |= 8;
                else if (sc->Check("MRF_FAILNOTELEFRAG")) Val |= 16;
                else if (sc->Check("MRF_FAILNOLAUGH")) Val |= 32;
                else if (sc->Check("MRF_WHENINVULNERABLE")) Val |= 64;
                else if (sc->Check("MRF_LOSEACTUALWEAPON")) Val |= 128;
                else if (sc->Check("MRF_NEWTIDBEHAVIOUR")) Val |= 256;
                else if (sc->Check("MRF_UNDOBYDEATH")) Val |= 512;
                else if (sc->Check("MRF_UNDOBYDEATHFORCED")) Val |= 1024;
                else if (sc->Check("MRF_UNDOBYDEATHSAVES")) Val |= 2048;
                else sc->Error("Bad morph style");
              }
              while (sc->Check("|"));
              if (HaveParen) sc->Expect(")");
              P.Field->SetInt(DefObj, Val);
            }
            break;
          case PROP_SkipLineUnsupported:
            {
              if (dbg_show_decorate_unsupported) GCon->Logf("%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
              sc->ResetCrossed();
              while (sc->GetString()) {
                if (sc->Crossed) { sc->UnGet(); break; }
              }
            }
            break;
        }
        FoundProp = true;
        break;
      }
    }
    if (FoundProp) continue;

    if (decorate_fail_on_unknown) {
      sc->Error(va("Unknown property \"%s\"", *Prop));
    } else {
      GCon->Logf("WARNING: %s: Unknown property \"%s\"", *prloc.toStringNoCol(), *Prop);
    }
    // skip this line
    if (!sc->IsAtEol()) {
      sc->ResetCrossed();
      while (!sc->AtEnd() && !sc->Crossed) sc->GetString();
    } else {
      sc->GetString();
    }
  }

  sc->SetCMode(false);

  Class->EmitStateLabels();

  // set up linked list of states
  if (States.Num()) {
    Class->States = States[0];
    for (int i = 0; i < States.Num() - 1; ++i) States[i]->Next = States[i+1];
    for (int i = 0; i < States.Num(); ++i) {
      if (States[i]->GotoLabel != NAME_None) {
        States[i]->NextState = Class->ResolveStateLabel(States[i]->Loc, States[i]->GotoLabel, States[i]->GotoOffset);
      }
    }
  }

  if (DoomEdNum > 0) {
    mobjinfo_t &MI = VClass::GMobjInfos.Alloc();
    MI.Class = Class;
    MI.DoomEdNum = DoomEdNum;
    MI.GameFilter = GameFilter;
  }
  if (SpawnNum > 0) {
    mobjinfo_t &SI = VClass::GScriptIds.Alloc();
    SI.Class = Class;
    SI.DoomEdNum = SpawnNum;
    SI.GameFilter = GameFilter;
  }
  if (ReplaceeClass) {
    ReplaceeClass->Replacement = Class;
    Class->Replacee = ReplaceeClass;
  }
  unguard;
}


//==========================================================================
//
//  ParseOldDecStates
//
//==========================================================================
static void ParseOldDecStates (VScriptParser *sc, TArray<VState *> &States, VClass *Class) {
  guard(ParseOldDecStates);
  TArray<VStr> Tokens;
  sc->String.Split(",\t\r\n", Tokens);
  for (int TokIdx = 0; TokIdx < Tokens.Num(); ++TokIdx) {
    const char *pFrame = *Tokens[TokIdx];
    int DurColon = Tokens[TokIdx].IndexOf(':');
    float Duration = 4;
    if (DurColon >= 0) {
      Duration = atoi(pFrame);
      if (Duration < 1 || Duration > 65534) sc->Error ("Rates must be in the range [0,65534]");
      pFrame = *Tokens[TokIdx]+DurColon+1;
    }

    bool GotState = false;
    while (*pFrame) {
      char cc = *pFrame;
      if (cc == '#') cc = 'A'; // hideous destructor hack
      if (cc == ' ') {
      } else if (cc == '*') {
        if (!GotState) sc->Error("* must come after a frame");
        States[States.Num()-1]->Frame |= VState::FF_FULLBRIGHT;
      } else if (cc < 'A' || cc > ']') {
        sc->Error("Frames must be A-Z, [, \\, or ]");
      } else {
        GotState = true;
        VState *State = new VState(va("S_%d", States.Num()), Class, sc->GetLoc());
        States.Append(State);
        State->Frame = cc-'A';
        State->Time = float(Duration)/35.0;
      }
      ++pFrame;
    }
  }
  unguard;
}


//==========================================================================
//
//  ParseOldDecoration
//
//==========================================================================
static void ParseOldDecoration (VScriptParser *sc, int Type) {
  guard(ParseOldDecoration);
  // get name of the class
  sc->ExpectString();
  VName ClassName = *sc->String;

  // create class
  VClass *Class = Type == OLDDEC_Pickup ?
    FakeInventoryClass->CreateDerivedClass(ClassName, DecPkg, sc->GetLoc()) :
    ActorClass->CreateDerivedClass(ClassName, DecPkg, sc->GetLoc());
  DecPkg->ParsedClasses.Append(Class);
  if (Type == OLDDEC_Breakable) SetClassFieldBool(Class, "bShootable", true);
  if (Type == OLDDEC_Projectile) {
    SetClassFieldBool(Class, "bMissile", true);
    SetClassFieldBool(Class, "bDropOff", true);
  }

  // parse game filters
  int GameFilter = 0;
  while (!sc->Check("{")) {
         if (sc->Check("Doom")) GameFilter |= GAME_Doom;
    else if (sc->Check("Heretic")) GameFilter |= GAME_Heretic;
    else if (sc->Check("Hexen")) GameFilter |= GAME_Hexen;
    else if (sc->Check("Strife")) GameFilter |= GAME_Strife;
    else if (sc->Check("Raven")) GameFilter |= GAME_Raven;
    else if (sc->Check("Any")) GameFilter |= GAME_Any;
    else if (GameFilter) sc->Error("Unknown game filter");
    else sc->Error("Unknown identifier");
  }

  int DoomEdNum = -1;
  int SpawnNum = -1;
  VName Sprite("tnt1");
  VName DeathSprite(NAME_None);
  TArray<VState*> States;
  int SpawnStart = 0;
  int SpawnEnd = 0;
  int DeathStart = 0;
  int DeathEnd = 0;
  bool DiesAway = false;
  bool SolidOnDeath = false;
  float DeathHeight = 0.0;
  int BurnStart = 0;
  int BurnEnd = 0;
  bool BurnsAway = false;
  bool SolidOnBurn = false;
  float BurnHeight = 0.0;
  int IceStart = 0;
  int IceEnd = 0;
  bool GenericIceDeath = false;
  bool Explosive = false;

  while (!sc->Check("}")) {
    if (sc->Check("DoomEdNum")) {
      sc->ExpectNumber();
      if (sc->Number < -1 || sc->Number > 32767) sc->Error("DoomEdNum is out of range [-1, 32767]");
      DoomEdNum = sc->Number;
    } else if (sc->Check("SpawnNum")) {
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > 255) sc->Error("SpawnNum is out of range [0, 255]");
      SpawnNum = sc->Number;
    } else if (sc->Check("Sprite")) {
      // spawn state
      sc->ExpectString();
      if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 characters long");
      Sprite = *sc->String.ToLower();
    } else if (sc->Check("Frames")) {
      sc->ExpectString();
      SpawnStart = States.Num();
      ParseOldDecStates(sc, States, Class);
      SpawnEnd = States.Num();
    } else if ((Type == OLDDEC_Breakable || Type == OLDDEC_Projectile) && sc->Check("DeathSprite")) {
      // death states
      sc->ExpectString();
      if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 characters long");
      DeathSprite = *sc->String.ToLower();
    } else if ((Type == OLDDEC_Breakable || Type == OLDDEC_Projectile) && sc->Check("DeathFrames")) {
      sc->ExpectString();
      DeathStart = States.Num();
      ParseOldDecStates(sc, States, Class);
      DeathEnd = States.Num();
    } else if (Type == OLDDEC_Breakable && sc->Check("DiesAway")) {
      DiesAway = true;
    } else if (Type == OLDDEC_Breakable && sc->Check("BurnDeathFrames")) {
      sc->ExpectString();
      BurnStart = States.Num();
      ParseOldDecStates(sc, States, Class);
      BurnEnd = States.Num();
    } else if (Type == OLDDEC_Breakable && sc->Check("BurnsAway")) {
      BurnsAway = true;
    } else if (Type == OLDDEC_Breakable && sc->Check("IceDeathFrames")) {
      sc->ExpectString();
      IceStart = States.Num();
      ParseOldDecStates(sc, States, Class);

      // make a copy of the last state for A_FreezeDeathChunks
      VState *State = new VState(va("S_%d", States.Num()), Class, sc->GetLoc());
      States.Append(State);
      State->Frame = States[States.Num()-2]->Frame;

      IceEnd = States.Num();
    } else if (Type == OLDDEC_Breakable && sc->Check("GenericIceDeath")) {
      GenericIceDeath = true;
    }
    // misc properties
    else if (sc->Check("Radius")) {
      sc->ExpectFloat();
      SetClassFieldFloat(Class, "Radius", sc->Float);
    } else if (sc->Check("Height")) {
      sc->ExpectFloat();
      SetClassFieldFloat(Class, "Height", sc->Float);
    } else if (sc->Check("Mass")) {
      sc->ExpectFloat();
      SetClassFieldFloat(Class, "Mass", sc->Float);
    } else if (sc->Check("Scale")) {
      sc->ExpectFloat();
      SetClassFieldFloat(Class, "ScaleX", sc->Float);
      SetClassFieldFloat(Class, "ScaleY", sc->Float);
    } else if (sc->Check("Alpha")) {
      sc->ExpectFloat();
      SetClassFieldFloat(Class, "Alpha", MID(0.0, sc->Float, 1.0));
    } else if (sc->Check("RenderStyle")) {
      int RenderStyle = 0;
           if (sc->Check("STYLE_None")) RenderStyle = STYLE_None;
      else if (sc->Check("STYLE_Normal")) RenderStyle = STYLE_Normal;
      else if (sc->Check("STYLE_Fuzzy")) RenderStyle = STYLE_Fuzzy;
      else if (sc->Check("STYLE_SoulTrans")) RenderStyle = STYLE_SoulTrans;
      else if (sc->Check("STYLE_OptFuzzy")) RenderStyle = STYLE_OptFuzzy;
      else if (sc->Check("STYLE_Translucent")) RenderStyle = STYLE_Translucent;
      else if (sc->Check("STYLE_Add")) RenderStyle = STYLE_Add;
      else sc->Error("Bad render style");
      SetClassFieldByte(Class, "RenderStyle", RenderStyle);
    } else if (sc->Check("Translation1")) {
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > 2) sc->Error("Translation1 is out of range [0, 2]");
      SetClassFieldInt(Class, "Translation", (TRANSL_Standard<<TRANSL_TYPE_SHIFT)+sc->Number);
    } else if (sc->Check("Translation2")) {
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > MAX_LEVEL_TRANSLATIONS) sc->Error(va("Translation2 is out of range [0, %d]", MAX_LEVEL_TRANSLATIONS));
      SetClassFieldInt(Class, "Translation", (TRANSL_Level<<TRANSL_TYPE_SHIFT)+sc->Number);
    }
    // breakable decoration properties
    else if (Type == OLDDEC_Breakable && sc->Check("Health")) {
      sc->ExpectNumber();
      SetClassFieldInt(Class, "Health", sc->Number);
    } else if (Type == OLDDEC_Breakable && sc->Check("DeathHeight")) {
      sc->ExpectFloat();
      DeathHeight = sc->Float;
    } else if (Type == OLDDEC_Breakable && sc->Check("BurnHeight")) {
      sc->ExpectFloat();
      BurnHeight = sc->Float;
    } else if (Type == OLDDEC_Breakable && sc->Check("SolidOnDeath")) {
      SolidOnDeath = true;
    } else if (Type == OLDDEC_Breakable && sc->Check("SolidOnBurn")) {
      SolidOnBurn = true;
    } else if ((Type == OLDDEC_Breakable || Type == OLDDEC_Projectile) && sc->Check("DeathSound")) {
      sc->ExpectString();
      SetClassFieldName(Class, "DeathSound", *sc->String);
    } else if (Type == OLDDEC_Breakable && sc->Check("BurnDeathSound")) {
      sc->ExpectString();
      SetClassFieldName(Class, "ActiveSound", *sc->String);
    }
    // projectile properties
    else if (Type == OLDDEC_Projectile && sc->Check("Speed")) {
      sc->ExpectFloat();
      SetClassFieldFloat(Class, "Speed", sc->Float*35.0);
    } else if (Type == OLDDEC_Projectile && sc->Check("Damage")) {
      sc->ExpectNumber();
      SetClassFieldFloat(Class, "MissileDamage", sc->Number);
    } else if (Type == OLDDEC_Projectile && sc->Check("DamageType")) {
      if (sc->Check("Normal")) {
        SetClassFieldName(Class, "DamageType", NAME_None);
      } else {
        sc->ExpectString();
        SetClassFieldName(Class, "DamageType", *sc->String);
      }
    } else if (Type == OLDDEC_Projectile && sc->Check("SpawnSound")) {
      sc->ExpectString();
      SetClassFieldName(Class, "SightSound", *sc->String);
    } else if (Type == OLDDEC_Projectile && sc->Check("ExplosionRadius")) {
      sc->ExpectNumber();
      SetClassFieldFloat(Class, "ExplosionRadius", sc->Number);
      Explosive = true;
    } else if (Type == OLDDEC_Projectile && sc->Check("ExplosionDamage")) {
      sc->ExpectNumber();
      SetClassFieldFloat(Class, "ExplosionDamage", sc->Number);
      Explosive = true;
    } else if (Type == OLDDEC_Projectile && sc->Check("DoNotHurtShooter")) {
      SetClassFieldBool(Class, "bExplosionDontHurtSelf", true);
    } else if (Type == OLDDEC_Projectile && sc->Check("DoomBounce")) {
      SetClassFieldByte(Class, "BounceType", BOUNCE_Doom);
    } else if (Type == OLDDEC_Projectile && sc->Check("HereticBounce")) {
      SetClassFieldByte(Class, "BounceType", BOUNCE_Heretic);
    } else if (Type == OLDDEC_Projectile && sc->Check("HexenBounce")) {
      SetClassFieldByte(Class, "BounceType", BOUNCE_Hexen);
    }
    // pickup properties
    else if (Type == OLDDEC_Pickup && sc->Check("PickupMessage")) {
      sc->ExpectString();
      SetClassFieldStr(Class, "PickupMessage", sc->String);
    } else if (Type == OLDDEC_Pickup && sc->Check("PickupSound")) {
      sc->ExpectString();
      SetClassFieldName(Class, "PickupSound", *sc->String);
    } else if (Type == OLDDEC_Pickup && sc->Check("Respawns")) {
      SetClassFieldBool(Class, "bRespawns", true);
    }
    // compatibility flags
    else if (sc->Check("LowGravity")) {
      SetClassFieldFloat(Class, "Gravity", 0.125);
    } else if (sc->Check("FireDamage")) {
      SetClassFieldName(Class, "DamageType", "Fire");
    }
    // flags
    else if (sc->Check("Solid")) SetClassFieldBool(Class, "bSolid", true);
    else if (sc->Check("NoSector")) SetClassFieldBool(Class, "bNoSector", true);
    else if (sc->Check("NoBlockmap")) SetClassFieldBool(Class, "bNoBlockmap", true);
    else if (sc->Check("SpawnCeiling")) SetClassFieldBool(Class, "bSpawnCeiling", true);
    else if (sc->Check("NoGravity")) SetClassFieldBool(Class, "bNoGravity", true);
    else if (sc->Check("Shadow")) SetClassFieldBool(Class, "bShadow", true);
    else if (sc->Check("NoBlood")) SetClassFieldBool(Class, "bNoBlood", true);
    else if (sc->Check("CountItem")) SetClassFieldBool(Class, "bCountItem", true);
    else if (sc->Check("WindThrust")) SetClassFieldBool(Class, "bWindThrust", true);
    else if (sc->Check("FloorClip")) SetClassFieldBool(Class, "bFloorClip", true);
    else if (sc->Check("SpawnFloat")) SetClassFieldBool(Class, "bSpawnFloat", true);
    else if (sc->Check("NoTeleport")) SetClassFieldBool(Class, "bNoTeleport", true);
    else if (sc->Check("Ripper")) SetClassFieldBool(Class, "bRip", true);
    else if (sc->Check("Pushable")) SetClassFieldBool(Class, "bPushable", true);
    else if (sc->Check("SlidesOnWalls")) SetClassFieldBool(Class, "bSlide", true);
    else if (sc->Check("CanPass")) SetClassFieldBool(Class, "bPassMobj", true);
    else if (sc->Check("CannotPush")) SetClassFieldBool(Class, "bCannotPush", true);
    else if (sc->Check("ThruGhost")) SetClassFieldBool(Class, "bThruGhost", true);
    else if (sc->Check("NoDamageThrust")) SetClassFieldBool(Class, "bNoDamageThrust", true);
    else if (sc->Check("Telestomp")) SetClassFieldBool(Class, "bTelestomp", true);
    else if (sc->Check("FloatBob")) SetClassFieldBool(Class, "bFloatBob", true);
    else if (sc->Check("ActivateImpact")) SetClassFieldBool(Class, "bActivateImpact", true);
    else if (sc->Check("CanPushWalls")) SetClassFieldBool(Class, "bActivatePushWall", true);
    else if (sc->Check("ActivateMCross")) SetClassFieldBool(Class, "bActivateMCross", true);
    else if (sc->Check("ActivatePCross")) SetClassFieldBool(Class, "bActivatePCross", true);
    else if (sc->Check("Reflective")) SetClassFieldBool(Class, "bReflective", true);
    else if (sc->Check("FloorHugger")) SetClassFieldBool(Class, "bIgnoreFloorStep", true);
    else if (sc->Check("CeilingHugger")) SetClassFieldBool(Class, "bIgnoreCeilingStep", true);
    else if (sc->Check("DontSplash")) SetClassFieldBool(Class, "bNoSplash", true);
    else {
      if (decorate_fail_on_unknown) {
        Sys_Error("Unknown property '%s'", *sc->String);
      } else {
        GCon->Logf("WARNING: Unknown property '%s'", *sc->String);
      }
      if (!sc->IsAtEol()) {
        sc->Crossed = false;
        while (!sc->AtEnd() && !sc->Crossed) sc->GetString();
      } else {
        sc->GetString();
      }
      continue;
    }
  }

  if (SpawnEnd == 0) sc->Error(va("%s has no Frames definition", *ClassName));
  if (Type == OLDDEC_Breakable && DeathEnd == 0) sc->Error(va("%s has no DeathFrames definition", *ClassName));
  if (GenericIceDeath && IceEnd != 0) sc->Error("IceDeathFrames and GenericIceDeath are mutually exclusive");

  if (DoomEdNum > 0) {
    mobjinfo_t &MI = VClass::GMobjInfos.Alloc();
    MI.Class = Class;
    MI.DoomEdNum = DoomEdNum;
    MI.GameFilter = GameFilter ? GameFilter : GAME_Any;
  }

  if (SpawnNum > 0) {
    mobjinfo_t &SI = VClass::GScriptIds.Alloc();
    SI.Class = Class;
    SI.DoomEdNum = SpawnNum;
    SI.GameFilter = GameFilter ? GameFilter : GAME_Any;
  }

  // set up linked list of states
  Class->States = States[0];
  for (int i = 0; i < States.Num()-1; ++i) States[i]->Next = States[i+1];

  // set up default sprite for all states
  for (int i = 0; i < States.Num(); ++i) States[i]->SpriteName = Sprite;

  // set death sprite if it's defined
  if (DeathSprite != NAME_None && DeathEnd != 0) {
    for (int i = DeathStart; i < DeathEnd; ++i) States[i]->SpriteName = DeathSprite;
  }

  // set up links of spawn states
  if (States.Num() == 1) {
    States[SpawnStart]->Time = -1.0;
  } else {
    for (int i = SpawnStart; i < SpawnEnd-1; ++i) States[i]->NextState = States[i+1];
    States[SpawnEnd-1]->NextState = States[SpawnStart];
  }
  Class->SetStateLabel("Spawn", States[SpawnStart]);

  // set up links of death states
  if (DeathEnd != 0) {
    for (int i = DeathStart; i < DeathEnd-1; ++i) States[i]->NextState = States[i + 1];
    if (!DiesAway && Type != OLDDEC_Projectile) States[DeathEnd-1]->Time = -1.0;
    if (Type == OLDDEC_Projectile) {
      if (Explosive) States[DeathStart]->Function = FuncA_ExplodeParms;
    } else {
      // first death state plays death sound, second makes it non-blocking unless it should stay solid
      States[DeathStart]->Function = FuncA_Scream;
      if (!SolidOnDeath) {
        if (DeathEnd-DeathStart > 1) {
          States[DeathStart+1]->Function = FuncA_NoBlocking;
        } else {
          States[DeathStart]->Function = FuncA_ScreamAndUnblock;
        }
      }
      if (!DeathHeight) DeathHeight = GetClassFieldFloat(Class, "Height");
      SetClassFieldFloat(Class, "DeathHeight", DeathHeight);
    }
    Class->SetStateLabel("Death", States[DeathStart]);
  }

  // set up links of burn death states
  if (BurnEnd != 0) {
    for (int i = BurnStart; i < BurnEnd-1; ++i) States[i]->NextState = States[i + 1];
    if (!BurnsAway) States[BurnEnd-1]->Time = -1.0;
    // First death state plays active sound, second makes it non-blocking unless it should stay solid
    States[BurnStart]->Function = FuncA_ActiveSound;
    if (!SolidOnBurn) {
      if (BurnEnd-BurnStart > 1) {
        States[BurnStart+1]->Function = FuncA_NoBlocking;
      } else {
        States[BurnStart]->Function = FuncA_ActiveAndUnblock;
      }
    }

    if (!BurnHeight) BurnHeight = GetClassFieldFloat(Class, "Height");
    SetClassFieldFloat(Class, "BurnHeight", BurnHeight);

    TArray<VName> Names;
    Names.Append("Death");
    Names.Append("Fire");
    Class->SetStateLabel(Names, States[BurnStart]);
  }

  // set up links of ice death states
  if (IceEnd != 0) {
    for (int i = IceStart; i < IceEnd-1; ++i) States[i]->NextState = States[i + 1];

    States[IceEnd-2]->Time = 5.0 / 35.0;
    States[IceEnd-2]->Function = FuncA_FreezeDeath;

    States[IceEnd-1]->NextState = States[IceEnd-1];
    States[IceEnd-1]->Time = 1.0 / 35.0;
    States[IceEnd-1]->Function = FuncA_FreezeDeathChunks;

    TArray<VName> Names;
    Names.Append("Death");
    Names.Append("Ice");
    Class->SetStateLabel(Names, States[IceStart]);
  } else if (GenericIceDeath) {
    VStateLabel *Lbl = Class->FindStateLabel("GenericIceDeath");
    TArray<VName> Names;
    Names.Append("Death");
    Names.Append("Ice");
    Class->SetStateLabel(Names, Lbl ? Lbl->State : nullptr);
  }
  unguard;
}


//==========================================================================
//
//  ParseDamageType
//
//==========================================================================
static void ParseDamageType (VScriptParser *sc) {
  GCon->Logf("WARNING: %s: 'DamageType' in decorate is not implemented yet!", *sc->GetLoc().toStringNoCol());
  sc->SkipBracketed();
}


//==========================================================================
//
//  ParseDecorate
//
//==========================================================================
static void ParseDecorate (VScriptParser *sc, TArray<VClassFixup> &ClassFixups) {
  guard(ParseDecorate);
  while (!sc->AtEnd()) {
    if (sc->Check("#include")) {
      sc->ExpectString();
      int Lump = W_CheckNumForFileName(sc->String);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && sc->String.Length() <= 8 && sc->String.IndexOf('/') < 0) {
        Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
      }
      if (Lump < 0) sc->Error(va("Lump %s not found", *sc->String));
      ParseDecorate(new VScriptParser(sc->String, W_CreateLumpReaderNum(Lump)), ClassFixups);
    } else if (sc->Check("const")) {
      ParseConst(sc);
    } else if (sc->Check("enum")) {
      ParseEnum(sc);
    } else if (sc->Check("class")) {
      ParseClass(sc);
    } else if (sc->Check("actor")) {
      ParseActor(sc, ClassFixups);
    } else if (sc->Check("breakable")) {
      ParseOldDecoration(sc, OLDDEC_Breakable);
    } else if (sc->Check("pickup")) {
      ParseOldDecoration(sc, OLDDEC_Pickup);
    } else if (sc->Check("projectile")) {
      ParseOldDecoration(sc, OLDDEC_Projectile);
    } else if (sc->Check("damagetype")) {
      ParseDamageType(sc);
    } else {
      ParseOldDecoration(sc, OLDDEC_Decoration);
    }
  }
  delete sc;
  sc = nullptr;
  unguard;
}


//==========================================================================
//
//  ReadLineSpecialInfos
//
//==========================================================================
void ReadLineSpecialInfos () {
  guard(ReadLineSpecialInfos);
  VStream *Strm = FL_OpenFileRead("line_specials.txt");
  check(Strm);
  VScriptParser *sc = new VScriptParser("line_specials.txt", Strm);
  while (!sc->AtEnd()) {
    VLineSpecInfo &I = LineSpecialInfos.Alloc();
    sc->ExpectNumber();
    I.Number = sc->Number;
    sc->ExpectString();
    I.Name = sc->String.ToLower();
  }
  delete sc;
  sc = nullptr;
  unguard;
}


//==========================================================================
//
//  ProcessDecorateScripts
//
//==========================================================================
void ProcessDecorateScripts () {
  guard(ProcessDecorateScripts);
  GCon->Logf(NAME_Init, "Parsing DECORATE definition files");
  for (int Lump = W_IterateFile(-1, "vavoom_decorate_defs.xml"); Lump != -1; Lump = W_IterateFile(Lump, "vavoom_decorate_defs.xml")) {
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    check(Strm);
    VXmlDocument *Doc = new VXmlDocument();
    Doc->Parse(*Strm, "vavoom_decorate_defs.xml");
    delete Strm;
    Strm = nullptr;
    ParseDecorateDef(*Doc);
    delete Doc;
    Doc = nullptr;
  }

  GCon->Logf(NAME_Init, "Processing DECORATE scripts");

  DecPkg = new VPackage(NAME_decorate);

  // find classes
  ActorClass = VClass::FindClass("Actor");
  FakeInventoryClass = VClass::FindClass("FakeInventory");
  InventoryClass = VClass::FindClass("Inventory");
  AmmoClass = VClass::FindClass("Ammo");
  BasicArmorPickupClass = VClass::FindClass("BasicArmorPickup");
  BasicArmorBonusClass = VClass::FindClass("BasicArmorBonus");
  HealthClass = VClass::FindClass("Health");
  PowerupGiverClass = VClass::FindClass("PowerupGiver");
  PuzzleItemClass = VClass::FindClass("PuzzleItem");
  WeaponClass = VClass::FindClass("Weapon");
  WeaponPieceClass = VClass::FindClass("WeaponPiece");
  PlayerPawnClass = VClass::FindClass("PlayerPawn");
  MorphProjectileClass = VClass::FindClass("MorphProjectile");

  // find methods used by old style decorations
  FuncA_Scream = ActorClass->FindMethodChecked("A_Scream");
  FuncA_NoBlocking = ActorClass->FindMethodChecked("A_NoBlocking");
  FuncA_ScreamAndUnblock = ActorClass->FindMethodChecked("A_ScreamAndUnblock");
  FuncA_ActiveSound = ActorClass->FindMethodChecked("A_ActiveSound");
  FuncA_ActiveAndUnblock = ActorClass->FindMethodChecked("A_ActiveAndUnblock");
  FuncA_ExplodeParms = ActorClass->FindMethodChecked("A_ExplodeParms");
  FuncA_FreezeDeath = ActorClass->FindMethodChecked("A_FreezeDeath");
  FuncA_FreezeDeathChunks = ActorClass->FindMethodChecked("A_FreezeDeathChunks");

  // parse scripts
  TArray<VClassFixup> ClassFixups;
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_decorate) {
      GCon->Logf("Parsing decorate script '%s'...", *W_FullLumpName(Lump));
      ParseDecorate(new VScriptParser(*W_LumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassFixups);
    }
  }

  // make sure all import classes were defined
  if (VMemberBase::GDecorateClassImports.Num()) {
    for (int i = 0; i < VMemberBase::GDecorateClassImports.Num(); ++i) {
      GCon->Logf("Undefined DECORATE class %s", VMemberBase::GDecorateClassImports[i]->GetName());
    }
    Sys_Error("Not all DECORATE class imports were defined");
  }

  GCon->Logf(NAME_Init, "Post-procesing");

  // set class properties
  for (int i = 0; i < ClassFixups.Num(); ++i) {
    VClassFixup &CF = ClassFixups[i];
    check(CF.ReqParent);
    if (CF.Name.ICmp("None") == 0) {
      *(VClass**)(CF.Class->Defaults+CF.Offset) = nullptr;
    } else {
      VClass *C = VClass::FindClassLowerCase(*CF.Name.ToLower());
      if (!C) {
        if (dbg_show_missing_class) GCon->Logf("WARNING: No such class %s", *CF.Name);
      } else if (!C->IsChildOf(CF.ReqParent)) {
        GCon->Logf("WARNING: Class %s is not a descendant of %s", *CF.Name, CF.ReqParent->GetName());
      } else {
        *(VClass**)(CF.Class->Defaults + CF.Offset) = C;
      }
    }
  }

  VField *DropItemListField = ActorClass->FindFieldChecked("DropItemList");
  for (int i = 0; i < DecPkg->ParsedClasses.Num(); ++i) {
    TArray<VDropItemInfo> &List = *(TArray<VDropItemInfo>*)DropItemListField->GetFieldPtr((VObject*)DecPkg->ParsedClasses[i]->Defaults);
    for (int j = 0; j < List.Num(); ++j) {
      VDropItemInfo &DI = List[j];
      if (DI.TypeName == NAME_None) continue;
      VClass *C = VClass::FindClassLowerCase(DI.TypeName);
           if (!C) { if (dbg_show_missing_class) GCon->Logf("WARNING: No such class %s", *DI.TypeName); }
      else if (!C->IsChildOf(ActorClass)) GCon->Logf("WARNING: Class %s is not an actor class", *DI.TypeName);
      else DI.Type = C;
    }
  }

  // emit code
  for (int i = 0; i < DecPkg->ParsedClasses.Num(); ++i) {
    if (GArgs.CheckParm("-debug_decorate")) GCon->Logf("Emiting Class %s", *DecPkg->ParsedClasses[i]->GetFullName());
    DecPkg->ParsedClasses[i]->DecorateEmit();
  }

  // compile and set up for execution
  for (int i = 0; i < DecPkg->ParsedClasses.Num(); ++i) {
    if (GArgs.CheckParm("-debug_decorate")) {
      GCon->Logf("Compiling Class %s", *DecPkg->ParsedClasses[i]->GetFullName());
    }
    DecPkg->ParsedClasses[i]->DecoratePostLoad();
  }

  if (vcErrorCount) BailOut();

  VClass::StaticReinitStatesLookup();

  TLocation::ClearSourceFiles();
  unguard;
}


//==========================================================================
//
//  ShutdownDecorate
//
//==========================================================================
void ShutdownDecorate () {
  guard(ShutdownDecorate);
  FlagList.Clear();
  LineSpecialInfos.Clear();
  unguard;
}


//==========================================================================
//
//  VEntity::SetDecorateFlag
//
//==========================================================================
void VEntity::SetDecorateFlag (const VStr &Flag, bool Value) {
  guard(VEntity::SetDecorateFlag);
  VName FlagName;
  VName ClassFilter(NAME_None);
  int DotPos = Flag.IndexOf('.');
  if (DotPos >= 0) {
    ClassFilter = *VStr(Flag, 0, DotPos).ToLower();
    FlagName = *VStr(Flag, DotPos+1, Flag.Length()-DotPos-1).ToLower();
  } else {
    FlagName = *Flag.ToLower();
  }
  for (int j = 0; j < FlagList.Num(); ++j) {
    VFlagList &ClassDef = FlagList[j];
    if (ClassFilter != NAME_None && ClassDef.Class->LowerCaseName != ClassFilter) continue;
    if (!IsA(ClassDef.Class)) continue;
    for (int i = ClassDef.FlagsHash[GetTypeHash(FlagName)&(FLAGS_HASH_SIZE-1)]; i != -1; i = ClassDef.Flags[i].HashNext) {
      const VFlagDef &F = ClassDef.Flags[i];
      if (FlagName == F.Name) {
        switch (F.Type) {
          case FLAG_Bool: F.Field->SetBool(this, Value); break;
          case FLAG_Unsupported: if (dbg_show_decorate_unsupported) GCon->Logf("Unsupported flag %s in %s", *Flag, GetClass()->GetName()); break;
          case FLAG_Byte: F.Field->SetByte(this, Value ? F.BTrue : F.BFalse); break;
          case FLAG_Float: F.Field->SetFloat(this, Value ? F.FTrue : F.FFalse); break;
          case FLAG_Name: F.Field->SetName(this, Value ? F.NTrue : F.NFalse); break;
          case FLAG_Class:
            F.Field->SetClass(this, Value ?
                F.NTrue != NAME_None ? VClass::FindClass(*F.NTrue) : nullptr :
                F.NFalse != NAME_None ? VClass::FindClass(*F.NFalse) : nullptr);
            break;
          case FLAG_NoClip:
            F.Field->SetBool(this, !Value);
            F.Field2->SetBool(this, !Value);
            break;
        }
        return;
      }
    }
  }
  GCon->Logf("Unknown flag '%s'", *Flag);
  unguard;
}


//==========================================================================
//
//  comatoze
//
//==========================================================================
static const char *comatoze (vuint32 n) {
  static char buf[128];
  int bpos = (int)sizeof(buf);
  buf[--bpos] = 0;
  int xcount = 0;
  do {
    if (xcount == 3) { buf[--bpos] = ','; xcount = 0; }
    buf[--bpos] = '0'+n%10;
    ++xcount;
  } while ((n /= 10) != 0);
  return &buf[bpos];
}


//==========================================================================
//
//  CompilerReportMemory
//
//==========================================================================
void CompilerReportMemory () {
  //GCon->Logf("Compiler allocated %u bytes.", VExpression::TotalMemoryUsed);
  GCon->Logf(NAME_Init, "Peak compiler memory usage: %s bytes.", comatoze(VExpression::PeakMemoryUsed));
  GCon->Logf(NAME_Init, "Released compiler memory  : %s bytes.", comatoze(VExpression::TotalMemoryFreed));
  if (VExpression::CurrMemoryUsed != 0) {
    GCon->Logf(NAME_Init, "Compiler leaks %s bytes (this is harmless).", comatoze(VExpression::CurrMemoryUsed));
    VExpression::ReportLeaks();
  }
}
