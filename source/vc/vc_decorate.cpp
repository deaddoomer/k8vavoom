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
  //WARNING! keep in sync with script code (LineSpecialGameInfo.vc)
  NUM_WEAPON_SLOTS = 10,
  MAX_WEAPONS_PER_SLOT = 8,
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
  PROP_PawnWeaponSlot,
  PROP_DefaultAlpha,
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


struct VWeaponSlotFixups {
  bool defined[NUM_WEAPON_SLOTS+1]; // [1..10]
  VName names[(NUM_WEAPON_SLOTS+1)*MAX_WEAPONS_PER_SLOT];

  VWeaponSlotFixups () {
    for (int f = 0; f <= NUM_WEAPON_SLOTS; ++f) defined[f] = false;
    for (int f = 0; f < (NUM_WEAPON_SLOTS+1)*MAX_WEAPONS_PER_SLOT; ++f) names[f] = NAME_None;
  }

  inline bool isValidSlot (int idx) const { return (idx >= 0 && idx <= NUM_WEAPON_SLOTS); }

  bool hasAnyDefinedSlot () const {
    for (int f = 0; f <= NUM_WEAPON_SLOTS; ++f) if (defined[f]) return true;
    return false;
  }

  inline bool isDefinedSlot (int idx) const { return (idx >= 0 && idx <= NUM_WEAPON_SLOTS ? defined[idx] : false); }

  VName getSlotName (int sidx, int nidx) const {
    if (sidx < 0 || sidx > NUM_WEAPON_SLOTS) return NAME_None;
    if (!defined[sidx]) return NAME_None;
    if (nidx < 0 || nidx >= MAX_WEAPONS_PER_SLOT) return NAME_None;
    return names[sidx*MAX_WEAPONS_PER_SLOT+nidx];
  }

  void clearSlot (int idx) {
    if (idx < 0 || idx > NUM_WEAPON_SLOTS) return;
    defined[idx] = true;
    for (int f = 0; f < MAX_WEAPONS_PER_SLOT; ++f) names[idx*MAX_WEAPONS_PER_SLOT+f] = NAME_None;
  }

  void addToSlot (int idx, VName aname) {
    if (idx < 0 || idx > NUM_WEAPON_SLOTS) return;
    defined[idx] = true;
    for (int f = 0; f < MAX_WEAPONS_PER_SLOT; ++f) {
      if (names[idx*MAX_WEAPONS_PER_SLOT+f] == NAME_None) {
        names[idx*MAX_WEAPONS_PER_SLOT+f] = aname;
        return;
      }
    }
  }
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

  virtual VStr toString () const override;

protected:
  VDecorateInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDecorateUserVar
//
//  this generates call to _get/_set uservar methods
//
//==========================================================================
class VDecorateUserVar : public VExpression {
public:
  VName fldname;
  VExpression *index; // array index, if not nullptr

  VDecorateUserVar (VName afldname, const TLocation &aloc);
  VDecorateUserVar (VName afldname, VExpression *aindex, const TLocation &aloc);
  virtual ~VDecorateUserVar () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  //virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec) override;
  virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) override;
  virtual void Emit (VEmitContext &ec) override;
  virtual bool IsDecorateUserVar () const override;
  virtual VStr toString () const override;

protected:
  VDecorateUserVar () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


// ////////////////////////////////////////////////////////////////////////// //
static VExpression *ParseExpressionPriority13 (VScriptParser *sc, VClass *Class);


// ////////////////////////////////////////////////////////////////////////// //
TArray<VLineSpecInfo> LineSpecialInfos;


// ////////////////////////////////////////////////////////////////////////// //
static VClass *decoClass = nullptr;

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

static TMapNC<VName, bool> IgnoredDecorateActions; // lowercased

static bool inCodeBlock = false;


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
        P.FMin = VStr::atof(*PN->GetAttribute("min"), 0); //FIXME
        P.FMax = VStr::atof(*PN->GetAttribute("max"), 1); //FIXME
      } else if (PN->Name == "prop_float_clamped_2") {
        VPropDef &P = Lst.NewProp(PROP_FloatClamped2, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        P.SetField2(Lst.Class, *PN->GetAttribute("property2"));
        P.FMin = VStr::atof(*PN->GetAttribute("min"), 0); //FIXME
        P.FMax = VStr::atof(*PN->GetAttribute("max"), 1); //FIXME
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
      } else if (PN->Name == "prop_default_alpha") {
        VPropDef &P = Lst.NewProp(PROP_DefaultAlpha, PN);
        P.SetField(Lst.Class, "Alpha");
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
        VPropDef &P = Lst.NewProp(PROP_StencilColour, PN);
        P.SetField(Lst.Class, "StencilColour");
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
      } else if (PN->Name == "prop_pawn_weapon_slot") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_PawnWeaponSlot, PN);
        //P.SetField(Lst.Class, *PN->GetAttribute("property"));
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
        F.FTrue = VStr::atof(*PN->GetAttribute("true_value"), 0); //FIXME
        F.FFalse = VStr::atof(*PN->GetAttribute("false_value"), 1); //FIXME
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
//  VDecorateSingleName::toString
//
//==========================================================================
VStr VDecorateSingleName::toString () const {
  return VStr(*Name);
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
        ParseError(Loc, "Property `%s` cannot be read", *Name);
        delete this;
        return nullptr;
      }
      VExpression *e = new VInvocation(nullptr, Prop->GetFunc, nullptr, false, false, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
  }

  CheckName = *Name.ToLower();

  if (ec.SelfClass) {
    VName fldn = ec.SelfClass->FindDecorateStateFieldTrans(CheckName);
    //FIXME: this is ALL WRONG! we should introduce new field accessor with run-time field checking
    if (fldn != NAME_None) {
      // checked field access
      VExpression *e = new VDecorateUserVar(VName(*Name), Loc);
      delete this;
      return e->Resolve(ec);
      /*
      VStr fns = VStr(*fldn);
      int dotpos = fns.IndexOf('.');
      if (dotpos > 0 && dotpos < fns.length()-1) {
          VStr n0 = fns.mid(0, dotpos);
          VStr n1 = fns.mid(dotpos+1, fns.length()-dotpos);
          //fprintf(stderr, "::: <%s> <%s>\n", *n0, *n1);
          VSingleName *sn0 = new VSingleName(VName(*n0), Loc);
          VDotField *fa = new VDotField(sn0, VName(*n1), Loc);
          delete this;
          return fa->Resolve(ec);
        } else {
          //fprintf(stderr, "[%s] -> [%s]\n", *CheckName, *fldn);
          VSingleName *sn = new VSingleName(fldn, Loc);
          delete this;
          return sn->Resolve(ec);
        }
      }
      */
    }
  }

  // look only for constants defined in DECORATE scripts
  VConstant *Const = ec.Package->FindConstant(CheckName);
  if (Const) {
    VExpression *e = new VConstantValue(Const, Loc);
    delete this;
    return e->Resolve(ec);
  }

  ParseError(Loc, "Illegal expression identifier `%s`", *Name);
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
//  VDecorateUserVar::VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::VDecorateUserVar (VName afldname, const TLocation &aloc)
  : VExpression(aloc)
  , fldname(afldname)
  , index(nullptr)
{
}


//==========================================================================
//
//  VDecorateUserVar::VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::VDecorateUserVar (VName afldname, VExpression *aindex, const TLocation &aloc)
  : VExpression(aloc)
  , fldname(afldname)
  , index(aindex)
{
}


//==========================================================================
//
//  VDecorateUserVar::~VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::~VDecorateUserVar () {
  delete index; index = nullptr;
  fldname = NAME_None; // just4fun
}


//==========================================================================
//
//  VDecorateUserVar::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateUserVar::SyntaxCopy () {
  auto res = new VDecorateUserVar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateUserVar::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateUserVar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateUserVar *)e;
  res->fldname = fldname;
  res->index = (index ? index->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDecorateUserVar::DoResolve
//
//==========================================================================
VExpression *VDecorateUserVar::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass) Sys_Error("VDecorateUserVar::DoResolve: internal compiler error");
  VName fldnamelo = (fldname == NAME_None ? fldname : VName(*fldname, VName::AddLower));
  VName fldn = ec.SelfClass->FindDecorateStateFieldTrans(fldnamelo);
  if (fldn == NAME_None) {
    ParseError(Loc, "field `%s` is not found in class `%s`", *fldname, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  // first try to find the corresponding non-array method
  if (!index) {
    VMethod *mt = ec.SelfClass->FindMethod(fldn);
    if (mt) {
      // use found method
      VExpression *e = new VInvocation(nullptr, mt, nullptr, false, false, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
  }
  // no method, use checked field access
  VField *fld = ec.SelfClass->FindField(fldn);
  if (!fld) {
    ParseError(Loc, "field `%s` => `%s` is not found in class `%s`", *fldname, *fldn, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  VFieldType ftype = fld->Type;
  if (ftype.Type == TYPE_Array) ftype = ftype.GetArrayInnerType();
  VName mtname = (ftype.Type == TYPE_Int ? "_get_user_var_int" : "_get_user_var_float");
  VMethod *mt = ec.SelfClass->FindMethod(mtname);
  if (!mt) {
    ParseError(Loc, "internal method `%s` not found in class `%s`", *mtname, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  VExpression *args[2];
  args[0] = new VNameLiteral(fldn, Loc);
  args[1] = index;
  VExpression *e = new VInvocation(nullptr, mt, nullptr, false, false, Loc, 2, args);
  index = nullptr;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDecorateUserVar::ResolveCompleteAssign
//
//  this will be called before actual assign resolving
//  return `nullptr` to indicate error, or consume `val` and set `resolved`
//  to `true` if resolved
//  if `nullptr` is returned, both `this` and `val` should be destroyed
//
//==========================================================================
VExpression *VDecorateUserVar::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  if (!ec.SelfClass) Sys_Error("VDecorateUserVar::DoResolve: internal compiler error");
  if (!val) { delete this; return nullptr; }
  resolved = true; // anyway
  VName fldnamelo = (fldname == NAME_None ? fldname : VName(*fldname, VName::AddLower));
  VName fldn = ec.SelfClass->FindDecorateStateFieldTrans(fldnamelo);
  if (fldn == NAME_None) {
    ParseError(Loc, "field `%s` is not found in class `%s`", *fldname, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VField *fld = ec.SelfClass->FindField(fldn);
  if (!fld) {
    ParseError(Loc, "field `%s` => `%s` is not found in class `%s`", *fldname, *fldn, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VFieldType ftype = fld->Type;
  if (ftype.Type == TYPE_Array) ftype = ftype.GetArrayInnerType();
  VName mtname = (ftype.Type == TYPE_Int ? "_set_user_var_int" : "_set_user_var_float");
  VMethod *mt = ec.SelfClass->FindMethod(mtname);
  if (!mt) {
    ParseError(Loc, "internal method `%s` not found in class `%s`", *mtname, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VExpression *args[3];
  args[0] = new VNameLiteral(fldn, Loc);
  args[1] = val;
  args[2] = index;
  VExpression *e = new VInvocation(nullptr, mt, nullptr, false, false, Loc, 3, args);
  index = nullptr;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDecorateUserVar::Emit
//
//==========================================================================
void VDecorateUserVar::Emit (VEmitContext &ec) {
  Sys_Error("VDecorateUserVar::Emit: the thing that should not be!");
}


//==========================================================================
//
//  VDecorateUserVar::toString
//
//==========================================================================
VStr VDecorateUserVar::toString () const {
  VStr res = VStr(*fldname);
  if (index) { res += "["; res += index->toString(); res += "]"; }
  return res;
}


//==========================================================================
//
//  VDecorateUserVar::IsDecorateUserVar
//
//==========================================================================
bool VDecorateUserVar::IsDecorateUserVar () const {
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
//  VDecorateInvocation::toString
//
//==========================================================================
VStr VDecorateInvocation::toString () const {
  VStr res = *Name;
  res += "(";
  for (int f = 0; f < NumArgs; ++f) {
    if (f != 0) res += ", ";
    if (Args[f]) res += Args[f]->toString(); else res += "default";
  }
  res += ")";
  return res;
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
  //if (VStr::ICmp(*Name, "CallACS") == 0) Name = VName("ACS_NamedExecuteWithResult"); // decorate hack
  if (ec.SelfClass) {
    //FIXME: sanitize this!
    // first try with decorate_ prefix, then without
    VMethod *M = ec.SelfClass->FindMethod/*NoCase*/(va("decorate_%s", *Name));
    if (!M) M = ec.SelfClass->FindMethod/*NoCase*/(Name);
    if (!M) {
      VStr loname = VStr(*Name).toLowerCase();
      M = ec.SelfClass->FindMethod/*NoCase*/(va("decorate_%s", *loname));
      if (!M) M = ec.SelfClass->FindMethod(VName(*loname));
      if (!M && ec.SelfClass) {
        // just in case
        VDecorateStateAction *Act = ec.SelfClass->FindDecorateStateAction(*loname);
        if (Act) M = Act->Method;
      }
      if (M) Name = VName(*loname);
    }
    if (M) {
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements (method '%s', class '%s')", *Name, *ec.SelfClass->GetFullName());
        delete this;
        return nullptr;
      }
      VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (ec.SelfClass) {
    ParseError(Loc, "Unknown decorate action `%s` in class `%s`", *Name, *ec.SelfClass->GetFullName());
  } else {
    ParseError(Loc, "Unknown decorate action `%s`", *Name);
  }
  //*(int *)0 = 0;
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
  //fprintf(stderr, "AddClassFixup0: Class=<%s>; FieldName=<%s>, ClassName=<%s>\n", (Class ? *Class->GetFullName() : "None"), *FieldName, *ClassName);
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
  //fprintf(stderr, "AddClassFixup1: Class=<%s>; FieldName=<%s>, ClassName=<%s>\n", (Class ? *Class->GetFullName() : "None"), *Field->GetFullName(), *ClassName);
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
/*
static void SkipBlock (VScriptParser *sc, int Level) {
  while (!sc->AtEnd() && Level > 0) {
         if (sc->Check("{")) ++Level;
    else if (sc->Check("}")) --Level;
    else sc->GetString();
  }
}
*/


//==========================================================================
//
//  CheckParseSetUserVarExpr
//
//==========================================================================
static VExpression *CheckParseSetUserVarExpr (VScriptParser *sc, VClass *Class, VStr FuncName) {
  if (FuncName.ICmp("A_SetUserVar") == 0 || FuncName.ICmp("A_SetUserVarFloat") == 0) {
    auto stloc = sc->GetLoc();
    sc->Expect("(");
    sc->ExpectString();
    VStr varName = sc->String;
    VStr uvname = sc->String.toLowerCase();
    if (!uvname.startsWith("user_")) sc->Error(va("%s: user variable name in DECORATE must start with `user_`", *sc->GetLoc().toStringNoCol()));
    sc->Expect(",");
    VExpression *val = ParseExpressionPriority13(sc, Class);
    sc->Expect(")");
    if (!val) sc->Error("invalid assignment");
    VExpression *dest = new VDecorateUserVar(VName(*varName), stloc);
    return new VAssignment(VAssignment::Assign, dest, val, stloc);
    /*
    VExpression *op1 = new VDecorateSingleName(*sc->String, sc->GetLoc());
    if (!op1) sc->Error("decorate parsing error");
    sc->Expect(",");
    VExpression *op2 = ParseExpressionPriority13(sc, Class);
    if (!op2) sc->Error("decorate parsing error");
    sc->Expect(")");
    // create assignment
    //GCon->Logf("SFU:%s: %s : %s", *sc->GetLoc().toStringNoCol(), *varName, *op2->toString());
    return new VAssignment(VAssignment::Assign, op1, op2, stloc);
    */
  } else if (FuncName.ICmp("A_SetUserArray") == 0 || FuncName.ICmp("A_SetUserArrayFloat") == 0) {
    auto stloc = sc->GetLoc();
    sc->Expect("(");
    sc->ExpectString();
    VStr varName = sc->String;
    VStr uvname = sc->String.toLowerCase();
    if (!uvname.startsWith("user_")) sc->Error(va("%s: user variable name in DECORATE must start with `user_`", *sc->GetLoc().toStringNoCol()));
    //VExpression *uvar = new VDecorateSingleName(*sc->String, sc->GetLoc());
    sc->Expect(",");
    // index
    VExpression *idx = ParseExpressionPriority13(sc, Class);
    if (!idx) sc->Error("decorate parsing error");
    sc->Expect(",");
    // value
    VExpression *val = ParseExpressionPriority13(sc, Class);
    if (!val) sc->Error("decorate parsing error");
    sc->Expect(")");
    VExpression *dest = new VDecorateUserVar(VName(*varName), idx, stloc);
    return new VAssignment(VAssignment::Assign, dest, val, stloc);
    /*
    VArrayElement *ael = new VArrayElement(uvar, idx, stloc);
    // create assignment
    //GCon->Logf("SFU:%s: %s : %s", *sc->GetLoc().toStringNoCol(), *varName, *op2->toString());
    return new VAssignment(VAssignment::Assign, ael, val, stloc);
    */
  }
  return nullptr;
}


//==========================================================================
//
//  CheckParseSetUserVarStmt
//
//==========================================================================
static VStatement *CheckParseSetUserVarStmt (VScriptParser *sc, VClass *Class, VStr FuncName) {
  VExpression *asse = CheckParseSetUserVarExpr(sc, Class, FuncName);
  if (!asse) return nullptr;
  return new VExpressionStatement(asse);
}


//==========================================================================
//
//  ParseFunCallWithName
//
//==========================================================================
static VMethod *ParseFunCallWithName (VScriptParser *sc, VStr FuncName, VClass *Class, int &NumArgs, VExpression **Args, bool gotParen) {
  // get function name and parse arguments
  VStr FuncNameLower = FuncName.ToLower();
  NumArgs = 0;
  int totalCount = 0;

  //fprintf(stderr, "***8:<%s> %s\n", *sc->String, *sc->GetLoc().toStringNoCol());
  if (!gotParen) gotParen = sc->Check("(");
  if (gotParen) {
    if (!sc->Check(")")) {
      do {
        ++totalCount;
        Args[NumArgs] = ParseExpressionPriority13(sc, Class);
        if (NumArgs == VMethod::MAX_PARAMS) {
          delete Args[NumArgs];
          Args[NumArgs] = nullptr;
          if (VStr::ICmp(*FuncName, "A_Jump") != 0 && VStr::ICmp(*FuncName, "randompick") != 0 &&
              VStr::ICmp(*FuncName, "decorate_randompick") != 0)
          {
            ParseError(sc->GetLoc(), "Too many arguments to `%s`", *FuncName);
          }
        } else {
          ++NumArgs;
        }
      } while (sc->Check(","));
      sc->Expect(")");
    }
  }
  //fprintf(stderr, "***9:<%s> %s\n", *sc->String, *sc->GetLoc().toStringNoCol());

  VMethod *Func = nullptr;

  // check ignores
  if (!IgnoredDecorateActions.find(VName(*FuncNameLower))) {
    // find the state action method: first check action specials, then state actions
    // hack: `ACS_ExecuteWithResult` has its own method, but it still should be in line specials
    if (FuncName.ICmp("ACS_ExecuteWithResult") != 0) {
      for (int i = 0; i < LineSpecialInfos.Num(); ++i) {
        if (LineSpecialInfos[i].Name == FuncNameLower) {
          Func = Class->FindMethodChecked("A_ExecActionSpecial");
          if (NumArgs > 5) {
            sc->Error(va("Too many arguments to translated action special `%s`", *FuncName));
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
    }

    if (!Func) {
      VDecorateStateAction *Act = Class->FindDecorateStateAction(*FuncNameLower);
      Func = (Act ? Act->Method : nullptr);
    }
  }

  //fprintf(stderr, "<%s>\n", *FuncNameLower);
  if (!Func) {
    //fprintf(stderr, "***8:<%s> %s\n", *FuncName, *sc->GetLoc().toStringNoCol());
  } else {
    if (Func && NumArgs > Func->NumParams &&
        (VStr::ICmp(*FuncName, "A_Jump") == 0 || VStr::ICmp(*FuncName, "randompick") == 0 ||
         VStr::ICmp(*FuncName, "decorate_randompick") == 0))
    {
      ParseWarning(sc->GetLoc(), "Too many arguments to `%s` (%d -- are you nuts?!)", *FuncName, totalCount);
      for (int f = Func->NumParams; f < NumArgs; ++f) { delete Args[f]; Args[f] = nullptr; }
      NumArgs = Func->NumParams;
    }
  }

  return Func;
}


//==========================================================================
//
//  ParseMethodCall
//
//==========================================================================
static VExpression *ParseMethodCall (VScriptParser *sc, VStr Name, TLocation Loc, bool parenEaten=true) {
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = 0;
  VMethod *Func = ParseFunCallWithName(sc, Name, decoClass, NumArgs, Args, parenEaten); // got paren
  /*
  if (Name.ICmp("random") == 0) {
    fprintf(stderr, "*** RANDOM; NumArgs=%d (%s); func=%p (%s, %s)\n", NumArgs, *sc->GetLoc().toStringNoCol(), Func, *Args[0]->toString(), *Args[1]->toString());
  }
  */
  return new VDecorateInvocation((Func ? Func->GetVName() : VName(*Name, VName::AddLower)), Loc, NumArgs, Args);
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
    VExpression *op = ParseExpressionPriority13(sc, decoClass);
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
          Args[0] = ParseExpressionPriority13(sc, decoClass);
          if (!Args[0]) ParseError(l, "`args` index expression expected");
          sc->Expect("]");
          return new VDecorateInvocation(VName(*Name), l, 1, Args);
        }
        sc->UnGet();
      }
    }
    // skip random generator ID
    if ((Name.ICmp("random") == 0 || Name.ICmp("random2") == 0 || Name.ICmp("frandom") == 0 ||
         Name.ICmp("randompick") == 0 || Name.ICmp("frandompick") == 0) && sc->Check("["))
    {
      sc->ExpectString();
      sc->Expect("]");
    }
    // special argument parsing
    /*
    if (Name.ICmp("GetCvar") == 0) {
      sc->Expect("(");
      auto vnloc = sc->GetLoc();
      sc->ExpectString();
      VStr vname = sc->String;
      sc->Expect(")");
      // create expression
      VExpression *Args[1];
      Args[0] = new VNameLiteral(VName(*vname), vnloc);
      return new VCastOrInvocation(VName("GetCvarF"), l, 1, Args);
    }
    */
    if (sc->Check("(")) return ParseMethodCall(sc, Name, l, true); // paren eaten
    if (sc->String.length() > 2 && sc->String[1] == '_' && (sc->String[0] == 'A' || sc->String[0] == 'a')) {
      return ParseMethodCall(sc, Name, l, false); // paren not eaten
    }
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
  //TLocation l = sc->GetLoc();
  if (!op) return nullptr;
  bool done = false;
  do {
    if (sc->Check("[")) {
      VExpression *ind = ParseExpressionPriority13(sc, decoClass);
      if (!ind) sc->Error("index expression error");
      sc->Expect("]");
      //op = new VArrayElement(op, ind, l);
      if (!op->IsDecorateSingleName()) sc->Error("cannot index non-array");
      VExpression *e = new VDecorateUserVar(*((VDecorateSingleName *)op)->Name, ind, op->Loc);
      delete op;
      op = e;
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
static VExpression *ParseExpressionPriority13 (VScriptParser *sc, VClass *Class) {
  guard(ParseExpressionPriority13);
  VClass *olddc = decoClass;
  decoClass = Class;
  VExpression *op = ParseExpressionPriority12(sc);
  if (!op) { decoClass = olddc; return nullptr; }
  TLocation l = sc->GetLoc();
  /*
  if (inCodeBlock && sc->Check("=")) {
    //return new VAssignment(VAssignment::Assign, op1, op2, stloc);
    abort();
  }
  */
  if (sc->Check("?")) {
    VExpression *op1 = ParseExpressionPriority13(sc, Class);
    sc->Expect(":");
    VExpression *op2 = ParseExpressionPriority13(sc, Class);
    op = new VConditional(op, op1, op2, l);
  }
  decoClass = olddc;
  return op;
  unguard;
}


//==========================================================================
//
//  ParseExpression
//
//==========================================================================
static VExpression *ParseExpression (VScriptParser *sc, VClass *Class) {
  guard(ParseExpression);
  return ParseExpressionPriority13(sc, Class);
  unguard;
}


//==========================================================================
//
//  ParseFunCallAsStmt
//
//==========================================================================
static VStatement *ParseFunCallAsStmt (VScriptParser *sc, VClass *Class, VState *State) {
  // get function name and parse arguments
  //auto actionLoc = sc->GetLoc();
  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = 0;

  sc->ExpectIdentifier();
  VStr FuncName = sc->String;
  auto stloc = sc->GetLoc();

  VStatement *suvst = CheckParseSetUserVarStmt(sc, Class, FuncName);
  if (suvst) return suvst;
  //fprintf(stderr, "***1:<%s>\n", *sc->String);

  // assign?
  if (inCodeBlock) {
    VExpression *dest = nullptr;
    if (sc->Check("[")) {
      dest = new VDecorateSingleName(FuncName, stloc);
      VExpression *ind = ParseExpressionPriority13(sc, Class);
      if (!ind) sc->Error("decorate parsing error");
      sc->Expect("]");
      dest = new VArrayElement(dest, ind, stloc);
      sc->Expect("=");
    } else if (sc->Check("=")) {
      dest = new VDecorateSingleName(FuncName, stloc);
      //GCon->Logf("ASS to '%s'...", *FuncName);
    }
    if (dest) {
      // we're supporting assign to array element or simple names
      // convert "single name" to uservar access
      if (dest->IsDecorateSingleName()) {
        VExpression *e = new VDecorateUserVar(*((VDecorateSingleName *)dest)->Name, dest->Loc);
        delete dest;
        dest = e;
      }
      if (!dest->IsDecorateUserVar()) sc->Error("cannot assign to non-field");
      VExpression *val = ParseExpression(sc, Class);
      if (!val) sc->Error("decorate parsing error");
      VExpression *ass = new VAssignment(VAssignment::Assign, dest, val, stloc);
      return new VExpressionStatement(new VDropResult(ass));
    }
  }

  VMethod *Func = ParseFunCallWithName(sc, FuncName, Class, NumArgs, Args, false); // no paren
  //fprintf(stderr, "***2:<%s>\n", *sc->String);

  VExpression *callExpr = nullptr;
  if (!Func) {
    //GCon->Logf("ERROR000: %s: Unknown state action `%s` in `%s` (replaced with NOP)", *actionLoc.toStringNoCol(), *FuncName, Class->GetName());
    //return nullptr;
    callExpr = new VDecorateInvocation(VName(*FuncName/*, VName::AddLower*/), stloc, NumArgs, Args);
  } else {
    VInvocation *Expr = new VInvocation(nullptr, Func, nullptr, false, false, stloc, NumArgs, Args);
    Expr->CallerState = State;
    callExpr = Expr;
  }
  return new VExpressionStatement(new VDropResult(callExpr));
}


//==========================================================================
//
//  ParseActionStatement
//
//==========================================================================
static VStatement *ParseActionStatement (VScriptParser *sc, VClass *Class, VState *State) {
  if (sc->Check("{")) {
    VCompound *stmt = new VCompound(sc->GetLoc());
    while (!sc->Check("}")) {
      if (sc->Check(";")) continue;
      VStatement *st;
      bool wantSemi = true;
      if (sc->Check("{")) {
        st = ParseActionStatement(sc, Class, State);
        wantSemi = false;
      } else {
        //fprintf(stderr, "***0:<%s>\n", *sc->String);
        st = ParseFunCallAsStmt(sc, Class, State);
      }
      if (st) stmt->Statements.append(st);
      if (wantSemi) sc->Expect(";");
    }
    return stmt;
  }

  if (sc->Check(";")) return nullptr;

  auto stloc = sc->GetLoc();

  // if
  if (sc->Check("if")) {
    sc->Expect("(");
    VExpression *cond = ParseExpression(sc, Class);
    sc->Expect(")");
    if (!cond) sc->Error("invalid `if` expression");
    VStatement *ts = ParseActionStatement(sc, Class, State);
    if (!ts) sc->Error("invalid `if` true branch");
    if (sc->Check("else")) {
      VStatement *fs = ParseActionStatement(sc, Class, State);
      if (fs) return new VIf(cond, ts, fs, stloc);
    }
    return new VIf(cond, ts, stloc);
  }

  // return
  if (sc->Check("return")) {
    if (sc->Check("state")) {
      // return state("name");
      // specials: `state()`, `state(0)`, `state("")`
      sc->Expect("(");
      if (sc->Check(")")) {
        sc->Expect(";");
        return new VReturn(nullptr, stloc);
      }
      VExpression *ste = ParseExpression(sc, Class);
      if (!ste) sc->Error("invalid `return`");
      sc->Expect(")");
      sc->Expect(";");
      // check for specials
      // replace `+number` with `number`
      for (;;) {
        if (ste->IsUnaryMath()) {
          VUnary *un = (VUnary *)ste;
          if (un->op) {
            if (un->Oper == VUnary::Plus && (un->op->IsIntConst() || un->op->IsFloatConst())) {
              VExpression *etmp = un->op;
              un->op = nullptr;
              delete ste;
              ste = etmp;
              continue;
            }
          }
        }
        break;
      }
      if (!ste) sc->Error("invalid `return`"); // just in case
      // `state(0)`?
      if ((ste->IsIntConst() && ste->GetIntConst() == 0) ||
          (ste->IsFloatConst() && ste->GetFloatConst() == 0))
      {
        delete ste;
        return new VReturn(nullptr, stloc);
      }
      // `state("")`?
      if (ste->IsStrConst()) {
        const char *lbl = ste->GetStrConst(DecPkg);
        while (*lbl && *(const vuint8 *)lbl <= 32) ++lbl;
        if (!lbl[0]) {
          delete ste;
          return new VReturn(nullptr, stloc);
        }
      }
      if (ste->IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)ste;
        if (e->Name.length() == 0) {
          delete ste;
          return new VReturn(nullptr, stloc);
        }
      }
      // call `decorate_A_RetDoJump`
      VExpression *Args[1];
      Args[0] = ste;
      VExpression *einv = new VDecorateInvocation(VName("decorate_A_RetDoJump"), stloc, 1, Args);
      VStatement *stinv = new VExpressionStatement(new VDropResult(einv));
      VCompound *cst = new VCompound(stloc);
      cst->Statements.append(stinv);
      cst->Statements.append(new VReturn(nullptr, stloc));
      return cst;
    } else if (sc->Check(";")) {
      // return;
      return new VReturn(nullptr, stloc);
    } else {
      // return expr;
      // just call `expr`, and do normal return
      VStatement *fs = ParseActionStatement(sc, Class, State);
      if (!fs) return new VReturn(nullptr, stloc);
      // create compound
      VCompound *cst = new VCompound(stloc);
      cst->Statements.append(fs);
      cst->Statements.append(new VReturn(nullptr, stloc));
      return cst;
    }
  }

  VStatement *res = ParseFunCallAsStmt(sc, Class, State);
  sc->Expect(";");
  if (!res) sc->Error("invalid action statement");
  return res;
}


//==========================================================================
//
//  ParseActionBlock
//
//  "{" checked
//
//==========================================================================
static void ParseActionBlock (VScriptParser *sc, VClass *Class, VState *State) {
  bool oldicb = inCodeBlock;
  inCodeBlock = true;
  VCompound *stmt = new VCompound(sc->GetLoc());
  while (!sc->Check("}")) {
    VStatement *st = ParseActionStatement(sc, Class, State);
    if (!st) continue;
    stmt->Statements.append(st);
  }
  inCodeBlock = oldicb;

  if (stmt->Statements.length()) {
    VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
    M->Flags = FUNC_Final;
    M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
    M->ReturnType = VFieldType(TYPE_Void);
    M->Statement = stmt;
    M->NumParams = 0;
    Class->AddMethod(M);
    M->Define();
    State->Function = M;
  } else {
    delete stmt;
    State->Function = nullptr;
  }
}


//==========================================================================
//
//  ParseActionCall
//
//  parse single decorate action
//
//==========================================================================
static void ParseActionCall (VScriptParser *sc, VClass *Class, VState *State) {
  // get function name and parse arguments
  //fprintf(stderr, "!!!***000: <%s> (%s)\n", *sc->String, *FuncName);
  //fprintf(stderr, "!!!***000: <%s>\n", *sc->String);
  sc->ExpectIdentifier();
  //fprintf(stderr, "!!!***001: <%s>\n", *sc->String);

  VExpression *Args[VMethod::MAX_PARAMS+1];
  int NumArgs = 0;
  auto actionLoc = sc->GetLoc();
  VStr FuncName = sc->String;
  VMethod *Func = nullptr;

  VStatement *suvst = CheckParseSetUserVarStmt(sc, Class, FuncName);
  if (suvst) {
    VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
    M->Flags = FUNC_Final;
    M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
    M->ReturnType = VFieldType(TYPE_Void);
    M->Statement = suvst;
    M->NumParams = 0;
    //M->ParamsSize = 1;
    Class->AddMethod(M);
    M->Define();
    Func = M;
  } else {
    Func = ParseFunCallWithName(sc, FuncName, Class, NumArgs, Args, false); // no paren
    //fprintf(stderr, "<%s>\n", *FuncNameLower);
    if (!Func) {
      GCon->Logf(NAME_Warning, "%s: Unknown state action `%s` in `%s` (replaced with NOP)", *actionLoc.toStringNoCol(), *FuncName, Class->GetName());
      // if function is not found, it means something is wrong
      // in that case we need to free argument expressions
      for (int i = 0; i < NumArgs; ++i) {
        if (Args[i]) {
          delete Args[i];
          Args[i] = nullptr;
        }
      }
    } else if (Func->NumParams || NumArgs /*|| FuncName.ICmp("a_explode") == 0*/) {
      VInvocation *Expr = new VInvocation(nullptr, Func, nullptr, false, false, sc->GetLoc(), NumArgs, Args);
      Expr->CallerState = State;
      VExpressionStatement *Stmt = new VExpressionStatement(new VDropResult(Expr));
      VMethod *M = new VMethod(NAME_None, Class, sc->GetLoc());
      M->Flags = FUNC_Final;
      M->ReturnTypeExpr = new VTypeExprSimple(TYPE_Void, sc->GetLoc());
      M->ReturnType = VFieldType(TYPE_Void);
      M->Statement = Stmt;
      M->NumParams = 0;
      Class->AddMethod(M);
      M->Define();
      Func = M;
    }
  }

  State->Function = Func;
  if (sc->Check(";")) {}
}


//==========================================================================
//
//  ParseConst
//
//==========================================================================
static void ParseConst (VScriptParser *sc) {
  guard(ParseConst);

  bool isInt = false;

  sc->SetCMode(true);
       if (sc->Check("int")) isInt = true;
  else if (sc->Check("float")) isInt = false;
  else sc->Error(va("%s: expected 'int' or 'float'", *sc->GetLoc().toStringNoCol()));
  //sc->Expect("int");
  sc->ExpectString();
  TLocation Loc = sc->GetLoc();
  VStr Name = sc->String.ToLower();
  sc->Expect("=");

  VExpression *Expr = ParseExpression(sc, nullptr);
  if (!Expr) {
    sc->Error("Constant value expected");
  } else {
    VEmitContext ec(DecPkg);
    Expr = Expr->Resolve(ec);
    if (isInt) {
      if (Expr && !Expr->IsIntConst()) sc->Error(va("%s: expected integer literal", *sc->GetLoc().toStringNoCol()));
      if (Expr) {
        int Val = Expr->GetIntConst();
        delete Expr;
        Expr = nullptr;
        VConstant *C = new VConstant(*Name, DecPkg, Loc);
        C->Type = TYPE_Int;
        C->Value = Val;
      }
    } else {
      if (Expr && !Expr->IsFloatConst() && !Expr->IsIntConst()) sc->Error(va("%s: expected float literal", *sc->GetLoc().toStringNoCol()));
      if (Expr) {
        float Val = (Expr->IsFloatConst() ? Expr->GetFloatConst() : (float)Expr->GetIntConst());
        delete Expr;
        Expr = nullptr;
        VConstant *C = new VConstant(*Name, DecPkg, Loc);
        C->Type = TYPE_Float;
        C->Value = Val;
      }
    }
  }
  sc->Expect(";");
  sc->SetCMode(false);

  unguard;
}


//==========================================================================
//
//  ParseActionDef
//
//==========================================================================
static void ParseActionDef (VScriptParser *sc, VClass *Class) {
  guard(ParseActionDef);
  // parse definition
  sc->Expect("native");
  // find the method: first try with decorate_ prefix, then without
  sc->ExpectIdentifier();
  VMethod *M = Class->FindMethod(va("decorate_%s", *sc->String));
  if (!M) M = Class->FindMethod(*sc->String);
  if (!M) sc->Error(va("Method `%s` not found in class `%s`", *sc->String, Class->GetName()));
  if (M && M->ReturnType.Type != TYPE_Void) {
    //k8: engine is capable of calling non-void methods, why
    //sc->Error(va("State action %s doesn't return void", *sc->String));
  }
  //GCon->Logf("***: <%s> -> <%s>", *sc->String, *sc->String.ToLower());
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
//  ParseActionAlias
//
//==========================================================================
static void ParseActionAlias (VScriptParser *sc, VClass *Class) {
  guard(ParseActionAlias);
  // parse alias
  sc->ExpectIdentifier();
  VStr newname = sc->String;
  sc->Expect("=");
  sc->ExpectIdentifier();
  VStr oldname = sc->String;
  VMethod *M = Class->FindMethod(va("decorate_%s", *oldname));
  if (!M) M = Class->FindMethod(*oldname);
  if (!M) sc->Error(va("Method `%s` not found in class `%s`", *oldname, Class->GetName()));
  //GCon->Logf("***ALIAS: <%s> -> <%s>", *newname, *oldname);
  VDecorateStateAction &A = Class->DecorateStateActions.Alloc();
  A.Name = *newname.ToLower();
  A.Method = M;
  sc->Expect(";");
  unguard;
}


//==========================================================================
//
//  ParseFieldAlias
//
//==========================================================================
static void ParseFieldAlias (VScriptParser *sc, VClass *Class) {
  guard(ParseActionAlias);
  // parse alias
  sc->ExpectIdentifier();
  VStr newname = sc->String;
  sc->Expect("=");
  sc->ExpectIdentifier();
  VStr oldname = sc->String;
  if (sc->Check(".")) {
    oldname += ".";
    sc->ExpectIdentifier();
    oldname += sc->String;
  }
  Class->DecorateStateFieldTrans.put(VName(*newname.toLowerCase()), VName(*oldname));
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
         if (sc->Check("action")) ParseActionDef(sc, Class);
    else if (sc->Check("alias")) ParseActionAlias(sc, Class);
    else if (sc->Check("field")) ParseFieldAlias(sc, Class);
    else sc->Error(va("Unknown class property '%s'", *sc->String));
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

  sc->SetCMode(true);
  //GCon->Logf("Enum");
  if (!sc->Check("{")) {
    sc->ExpectIdentifier();
    sc->Expect("{");
  }

  int currValue = 0;
  while (!sc->Check("}")) {
    sc->ExpectIdentifier();
    TLocation Loc = sc->GetLoc();
    VStr Name = sc->String.ToLower();
    VExpression *eval;
    if (sc->Check("=")) {
      eval = ParseExpression(sc, nullptr);
      if (eval) {
        VEmitContext ec(DecPkg);
        eval = eval->Resolve(ec);
        if (eval && !eval->IsIntConst()) sc->Error(va("%s: expected integer literal", *sc->GetLoc().toStringNoCol()));
        if (eval) {
          currValue = eval->GetIntConst();
          delete eval;
        }
      }
    }
    // create constant
    VConstant *C = new VConstant(*Name, DecPkg, Loc);
    C->Type = TYPE_Int;
    C->Value = currValue;
    ++currValue;
    if (!sc->Check(",")) {
      sc->Expect("}");
      break;
    }
  }
  sc->Check(";");
  sc->SetCMode(false);
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
  GCon->Logf(NAME_Warning, "%s: Unknown flag \"%s\"", *floc.toStringNoCol(), *FlagName);
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
  VState *PrevState = nullptr; // previous state in current execution chain
  VState *LastState = nullptr; // last defined state (`nullptr` right after new label)
  VState *LoopStart = nullptr; // state with last defined label (used to resolve `loop`)
  int NewLabelsStart = Class->StateLabelDefs.Num(); // first defined, but not assigned label index
  //bool inSpawnLabel = false;

  sc->Expect("{");
  // disable escape sequences in states
  sc->SetEscape(false);
  while (!sc->Check("}")) {
    TLocation TmpLoc = sc->GetLoc();
    VStr TmpName = ParseStateString(sc);

    // goto command
    if (TmpName.ICmp("Goto") == 0) {
      VName GotoLabel = *ParseStateString(sc);
      int GotoOffset = 0;
      if (sc->Check("+")) {
        sc->ExpectNumber();
        GotoOffset = sc->Number;
      }
      if (!LastState && NewLabelsStart == Class->StateLabelDefs.Num()) sc->Error("Goto before first state");
      // if we have no defined states for latest labels, create dummy state to attach gotos to it
      // simple redirection won't work, because this label can be used as `A_JumpXXX()` destination, for example
      // sigh... k8
      if (!LastState) {
        // yet if we are in spawn label, demand at least one defined state
        //if (inSpawnLabel) sc->Error("you cannot do immediate jump in spawn state");
        // ah, screw it, just define TNT1
        VState *dummyState = new VState(va("S_%d", States.Num()), Class, TmpLoc);
        States.Append(dummyState);
        dummyState->SpriteName = "tnt1";
        dummyState->Frame = 0|VState::FF_SKIPOFFS|VState::FF_SKIPMODEL;
        dummyState->Time = 0;
        // link previous state
        if (PrevState) PrevState->NextState = dummyState;
        // assign state to the labels
        for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) {
          Class->StateLabelDefs[i].State = dummyState;
          LoopStart = dummyState; // this will replace loop start only if we have any labels
        }
        NewLabelsStart = Class->StateLabelDefs.Num(); // no current label
        PrevState = dummyState;
        LastState = dummyState;
        //inSpawnLabel = false; // no need to add dummy state for "nodelay" anymore
      }
      LastState->GotoLabel = GotoLabel;
      LastState->GotoOffset = GotoOffset;
      check(NewLabelsStart == Class->StateLabelDefs.Num());
      /*k8: this doesn't work, see above
      for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) {
        Class->StateLabelDefs[i].GotoLabel = GotoLabel;
        Class->StateLabelDefs[i].GotoOffset = GotoOffset;
      }
      NewLabelsStart = Class->StateLabelDefs.Num(); // no current label
      */
      PrevState = nullptr; // new execution chain
      if (!sc->Crossed && sc->Check(";")) {}
      continue;
    }

    // stop command
    if (TmpName.ICmp("Stop") == 0) {
      if (!LastState && NewLabelsStart == Class->StateLabelDefs.Num()) sc->Error("Stop before first state");
      // see above for the reason to introduce this dummy state
      if (!LastState) {
        VState *dummyState = new VState(va("S_%d", States.Num()), Class, TmpLoc);
        States.Append(dummyState);
        dummyState->SpriteName = "tnt1";
        dummyState->Frame = 0|VState::FF_SKIPOFFS|VState::FF_SKIPMODEL;
        dummyState->Time = 0;
        // link previous state
        if (PrevState) PrevState->NextState = dummyState;
        // assign state to the labels
        for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) {
          Class->StateLabelDefs[i].State = dummyState;
          LoopStart = dummyState; // this will replace loop start only if we have any labels
        }
        NewLabelsStart = Class->StateLabelDefs.Num(); // no current label
        PrevState = dummyState;
        LastState = dummyState;
        //inSpawnLabel = false; // no need to add dummy state for "nodelay" anymore
      }
      LastState->NextState = nullptr;
      /*
      // if we have no defined states for latest labels, simply redirect labels to nowhere
      for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) Class->StateLabelDefs[i].State = nullptr;
      NewLabelsStart = Class->StateLabelDefs.Num(); // no current label
      */
      check(NewLabelsStart == Class->StateLabelDefs.Num());
      PrevState = nullptr; // new execution chain
      if (!sc->Crossed && sc->Check(";")) {}
      continue;
    }

    // wait command
    if (TmpName.ICmp("Wait") == 0 || TmpName.ICmp("Fail") == 0) {
      if (!LastState) sc->Error(va("%s before first state", *TmpName));
      LastState->NextState = LastState;
      PrevState = nullptr; // new execution chain
      if (!sc->Crossed && sc->Check(";")) {}
      continue;
    }

    // loop command
    if (TmpName.ICmp("Loop") == 0) {
      if (!LastState) sc->Error("Loop before first state");
      LastState->NextState = LoopStart;
      PrevState = nullptr; // new execution chain
      if (!sc->Crossed && sc->Check(";")) {}
      continue;
    }

    // check for label
    if (sc->Check(":")) {
      LastState = nullptr;
      // allocate new label (it will be resolved later)
      VStateLabelDef &Lbl = Class->StateLabelDefs.Alloc();
      Lbl.Loc = TmpLoc;
      Lbl.Name = TmpName;
      //if (TmpName.ICmp("Spawn") == 0) inSpawnLabel = true;
      if (!sc->Crossed && sc->Check(";")) {}
      continue;
    }

    // create new state
    VState *State = new VState(va("S_%d", States.Num()), Class, TmpLoc);
    States.Append(State);

    // sprite name
    bool totalKeepSprite = false; // remember "----" for other frames of this state
    bool keepSpriteBase = false; // remember "----" or "####" for other frames of this state

    if (TmpName.Length() != 4) sc->Error(va("Invalid sprite name '%s'", *TmpName));
    if (TmpName == "####" || TmpName == "----") {
      State->SpriteName = NAME_None; // don't change
      keepSpriteBase = true;
      if (TmpName == "----") totalKeepSprite = true;
    } else {
      State->SpriteName = *TmpName.toLowerCase();
    }

    // sprite frame
    sc->ExpectString();
    if (sc->String.length() == 0) sc->Error("Missing sprite frames");
    VStr FramesString = sc->String;

    // check first frame
    char FChar = VStr::ToUpper(sc->String[0]);
    if (totalKeepSprite || FChar == '#') {
      // frame letter is irrelevant
      State->Frame = VState::FF_DONTCHANGE;
    } else if (FChar < 'A' || FChar > ']') {
      if (FChar < 33 || FChar > 127) {
        sc->Error(va("Frames must be A-Z, [, \\ or ], got <0x%02x>", (vuint32)(FChar&0xff)));
      } else {
        sc->Error(va("Frames must be A-Z, [, \\ or ], got <%c>", FChar));
      }
    } else {
      State->Frame = FChar-'A';
    }
    if (keepSpriteBase) State->Frame |= VState::FF_KEEPSPRITE;

    sc->ResetCrossed();
    // tics
    // `random(a, b)`?
    if (sc->Check("random")) {
      sc->Expect("(");
      sc->ExpectNumberWithSign();
      State->Arg1 = sc->Number;
      sc->Expect(",");
      sc->ExpectNumberWithSign();
      State->Arg2 = sc->Number;
      sc->Expect(")");
      State->Time = float(State->Arg1)/35.0f;
      State->TicType = VState::TCK_Random;
    } else {
      // number
      sc->ExpectNumberWithSign();
      if (sc->Number < 0) {
        State->Time = sc->Number;
      } else {
        State->Time = float(sc->Number)/35.0f;
      }
    }
    //fprintf(stderr, "**0:%s: <%s> (%d)\n", *sc->GetLoc().toStringNoCol(), *sc->String, (sc->Crossed ? 1 : 0));

    // parse state action
    bool wasAction = false;
    for (;;) {
      if (!sc->GetString()) break;
      if (sc->Crossed) { sc->UnGet(); break; }
      //fprintf(stderr, "**1:%s: <%s>\n", *sc->GetLoc().toStringNoCol(), *sc->String);
      sc->UnGet();

      // simulate "nodelay" by inserting one dummy state if necessary
      if (sc->Check("NoDelay")) {
        // "nodelay" has sense only for "spawn" state
        // k8: play safe here: add dummy state for any label, just in case
        if (!LastState /*&& inSpawnLabel*/) {
          // there were no states after the label, insert dummy one
          VState *dupState = new VState(va("S_%d", States.Num()), Class, TmpLoc);
          States.Append(dupState);
          // copy real state data to duplicate one (it will be used as new "real" state)
          dupState->SpriteName = State->SpriteName;
          dupState->Frame = State->Frame;
          dupState->Time = State->Time;
          dupState->TicType = State->TicType;
          dupState->Arg1 = State->Arg1;
          dupState->Arg2 = State->Arg2;
          dupState->Misc1 = State->Misc1;
          dupState->Misc2 = State->Misc2;
          dupState->LightName = State->LightName;
          // dummy out "real" state (we copied all necessary data to duplicate one here)
          State->Frame = (State->Frame&(VState::FF_FRAMEMASK|VState::FF_DONTCHANGE|VState::FF_KEEPSPRITE))|VState::FF_SKIPOFFS|VState::FF_SKIPMODEL;
          State->Time = 0;
          State->TicType = VState::TCK_Normal;
          State->Arg1 = 0;
          State->Arg2 = 0;
          State->LightName = VStr();
          // link previous state
          if (PrevState) PrevState->NextState = State;
          // assign state to the labels
          for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) {
            Class->StateLabelDefs[i].State = State;
            LoopStart = State;
          }
          NewLabelsStart = Class->StateLabelDefs.Num(); // no current label
          PrevState = State;
          LastState = State;
          // and use duplicate state as a new state
          State = dupState;
          //inSpawnLabel = false; // no need to add dummy state for "nodelay" anymore
          continue;
        }
        continue;
      }

      // check various flags
      if (sc->Check("Bright")) { State->Frame |= VState::FF_FULLBRIGHT; continue; }
      if (sc->Check("CanRaise")) { State->Frame |= VState::FF_CANRAISE; continue; }
      if (sc->Check("MdlSkip")) { State->Frame |= VState::FF_SKIPMODEL; continue; }
      if (sc->Check("Fast")) { State->Frame |= VState::FF_FAST; continue; }
      if (sc->Check("Slow")) { State->Frame |= VState::FF_SLOW; continue; }

      // process `light() parameter
      if (sc->Check("Light")) {
        sc->Expect("(");
        sc->ExpectString();
        State->LightName = sc->String;
        sc->Expect(")");
        continue;
      }

      // process `offset` parameter
      if (sc->Check("Offset")) {
        sc->Expect("(");
        sc->ExpectNumberWithSign();
        State->Misc1 = sc->Number;
        sc->Expect(",");
        sc->ExpectNumberWithSign();
        State->Misc2 = sc->Number;
        sc->Expect(")");
        continue;
      }

      if (sc->Check("{")) {
        ParseActionBlock(sc, Class, State);
      } else {
        ParseActionCall(sc, Class, State);
      }

      wasAction = true;
      break;
    }

    if (sc->Check("{")) {
      if (wasAction) {
        sc->Error(va("%s: duplicate complex state actions in DECORATE aren't supported", *sc->GetLoc().toStringNoCol()));
        return false;
      }
      ParseActionBlock(sc, Class, State);
      wasAction = true;
    } else {
      if (!sc->Crossed && sc->Check(";")) {}
    }
    sc->ResetCrossed();

    // link previous state
    if (PrevState) PrevState->NextState = State;

    // assign state to the labels
    for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); ++i) {
      Class->StateLabelDefs[i].State = State;
      LoopStart = State; // this will replace loop start only if we have any labels
    }
    NewLabelsStart = Class->StateLabelDefs.Num(); // no current label
    PrevState = State;
    LastState = State;
    //inSpawnLabel = false; // no need to add dummy state for "nodelay" anymore

    for (int i = 1; i < FramesString.Length(); ++i) {
      vint32 frm = (State->Frame&~(VState::FF_FRAMEMASK|VState::FF_DONTCHANGE));

      char FSChar = VStr::ToUpper(FramesString[i]);
      if (totalKeepSprite || FSChar == '#') {
        // frame letter is irrelevant
        frm = VState::FF_DONTCHANGE;
      } else if (FSChar < 'A' || FSChar > ']') {
        if (FSChar < 33 || FSChar > 127) {
          sc->Error(va("Frames must be A-Z, [, \\ or ], got <0x%02x>", (vuint32)(FSChar&0xff)));
        } else {
          sc->Error(va("Frames must be A-Z, [, \\ or ], got <%c>", FSChar));
        }
      } else {
        frm |= FSChar-'A';
      }
      if (keepSpriteBase) frm |= VState::FF_KEEPSPRITE;

      // create a new state
      VState *s2 = new VState(va("S_%d", States.Num()), Class, sc->GetLoc());
      States.Append(s2);
      // add temporary labels
      s2->SpriteName = State->SpriteName;
      s2->Frame = frm;
      s2->Time = State->Time;
      s2->TicType = State->TicType;
      s2->Arg1 = State->Arg1;
      s2->Arg2 = State->Arg2;
      s2->Misc1 = State->Misc1;
      s2->Misc2 = State->Misc2;
      s2->Function = State->Function;
      s2->LightName = State->LightName;

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
//  ScanActorDefForUserVars
//
//==========================================================================
static void ScanActorDefForUserVars (VScriptParser *sc, TArray<VDecorateUserVarDef> &uvars) {
  if (sc->Check("replaces")) sc->ExpectString();

  // time to switch to the C mode
  sc->SetCMode(true);
  sc->SetEscape(false);

  sc->CheckNumber();
  sc->Expect("{");
  sc->ResetCrossed();

  while (!sc->Check("}")) {
    if (!sc->Check("var")) {
      // not a var, skip whole line
      //sc->SkipLine();
      //while (!sc->AtEnd() && !sc->Crossed) sc->GetString();
      //sc->UnGet();
      //sc->ResetCrossed();
      //GCon->Logf("<%s>", *sc->String);
      sc->GetString();
      continue;
    }

    if (sc->Check("{")) {
      sc->SkipBracketed(true); // bracket eaten
      continue;
    }

    sc->ExpectIdentifier();

    VFieldType vtype;
         if (sc->String.ICmp("int") == 0) vtype = VFieldType(TYPE_Int);
    else if (sc->String.ICmp("bool") == 0) vtype = VFieldType(TYPE_Int);
    else if (sc->String.ICmp("float") == 0) vtype = VFieldType(TYPE_Float);
    else sc->Error(va("%s: user variables in DECORATE must be `int`", *sc->GetLoc().toStringNoCol()));

    for (;;) {
      auto fnloc = sc->GetLoc(); // for error messages
      sc->ExpectIdentifier();
      VName fldname = VName(*sc->String, VName::AddLower);
      VStr uvname = VStr(*fldname);
      if (!uvname.startsWith("user_")) sc->Error(va("%s: user variable name '%s' in DECORATE must start with `user_`", *sc->String, *sc->GetLoc().toStringNoCol()));
      for (int f = 0; f < uvars.length(); ++f) {
        if (fldname == uvars[f].name) sc->Error(va("%s: duplicate DECORATE user variable `%s`", *sc->GetLoc().toStringNoCol(), *sc->String));
      }
      uvname = sc->String;
      VFieldType realtype = vtype;
      if (sc->Check("[")) {
        sc->ExpectNumber();
        if (sc->Number < 0 || sc->Number > 1024) sc->Error(va("%s: duplicate DECORATE user array `%s` has invalid size %d", *sc->GetLoc().toStringNoCol(), *uvname, sc->Number));
        sc->Expect("]");
        realtype = realtype.MakeArrayType(sc->Number, fnloc);
      }
      //fprintf(stderr, "VTYPE=<%s>; realtype=<%s>\n", *vtype.GetName(), *realtype.GetName());
      VDecorateUserVarDef &vd = uvars.alloc();
      vd.name = fldname;
      vd.loc = fnloc;
      vd.type = realtype;
      if (sc->Check(",")) continue;
      break;
    }
    sc->Expect(";");
  }

  //if (uvars.length()) for (int f = 0; f < uvars.length(); ++f) GCon->Logf("DC: <%s>", *uvars[f]);
}


//==========================================================================
//
//  ParseActor
//
//==========================================================================
static void ParseActor (VScriptParser *sc, TArray<VClassFixup> &ClassFixups, VWeaponSlotFixups &newWSlots) {
  guard(ParseActor);
  // parse actor name
  // in order to allow dots in actor names, this is done in non-C mode,
  // so we have to do a little bit more complex parsing
  sc->ExpectString();
  auto cstloc = sc->GetLoc();
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
    GCon->Logf(NAME_Warning, "%s: Redeclared class `%s` (old at %s)", *cstloc.toStringNoCol(), *NameStr, *DupCheck->Loc.toStringNoCol());
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

  //!HACK ZONE!
  // here i will clone `sc`, and will scan actor definition for any uservars.
  // this is 'cause `CreateDerivedClass` wants to finalize object fields, and
  // we need it to do that, so we can change defaults.
  // of course, i can collect changed defaults, and put 'em into default object
  // later, but meh... i want something easier as a starting point.

  TArray<VDecorateUserVarDef> uvars;
  {
    //GCon->Logf("*: '%s'", *NameStr);
    auto sc2 = sc->clone();
    ScanActorDefForUserVars(sc2, uvars);
    delete sc2;
  }

  VClass *Class = ParentClass->CreateDerivedClass(*NameStr, DecPkg, uvars, sc->GetLoc());
  uvars.clear(); // we don't need it anymore
  DecPkg->ParsedClasses.Append(Class);

  if (Class) {
    // copy class fixups of the parent class
    for (int i = 0; i < ClassFixups.Num(); ++i) {
      VClassFixup *CF = &ClassFixups[i];
      if (CF->Class == ParentClass) {
        VClassFixup &NewCF = ClassFixups.Alloc();
        CF = &ClassFixups[i]; // array can be resized, so update cache
        NewCF.Offset = CF->Offset;
        NewCF.Name = CF->Name;
        NewCF.ReqParent = CF->ReqParent;
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
  TArray<VState *> States;
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
      //while (sc->Check(";")) {}
      continue;
    }

    if (sc->Check("-")) {
      if (!ParseFlag(sc, Class, false, ClassFixups)) return;
      //while (sc->Check(";")) {}
      continue;
    }

    if (sc->Check("action")) {
      ParseActionDef(sc, Class);
      continue;
    }

    if (sc->Check("alias")) {
      ParseActionAlias(sc, Class);
      continue;
    }

    // get full name of the property
    auto prloc = sc->GetLoc(); // for error messages
    sc->ExpectIdentifier();

    // skip uservars (they are already scanned)
    if (sc->String.ICmp("var") == 0) {
      while (!sc->Check(";")) {
        if (!sc->GetString()) break;
      }
      continue;
    }

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
            P.Field->SetFloat(DefObj, (sc->Number > 0 ? sc->Number/35.0 : -sc->Number));
            break;
          case PROP_TicsSecs:
            sc->ExpectNumberWithSign();
            P.Field->SetFloat(DefObj, sc->Number >= 0 ? sc->Number / 35.0 : sc->Number);
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
            else if (sc->Check("Chex")) GameFilter |= GAME_Chex;
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
              VExpression *Expr = ParseExpression(sc, Class);
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
              else if (sc->Check("Stencil")) RenderStyle = STYLE_Stencil;
              else if (sc->Check("AddStencil")) RenderStyle = STYLE_AddStencil;
              else if (sc->Check("Subtract")) { RenderStyle = STYLE_Add; if (dbg_show_decorate_unsupported) GCon->Log(va("%s: Render style 'Subtract' in '%s' is not yet supported", *prloc.toStringNoCol(), Class->GetName())); } //FIXME
              else if (sc->Check("Shaded")) { RenderStyle = STYLE_Translucent; if (dbg_show_decorate_unsupported) GCon->Log(va("%s: Render style 'Shaded' in '%s' is not yet supported", *prloc.toStringNoCol(), Class->GetName())); } //FIXME
              else if (sc->Check("AddShaded")) { RenderStyle = STYLE_Add; if (dbg_show_decorate_unsupported) GCon->Log(va("%s: Render style 'AddShaded' in '%s' is not yet supported", *prloc.toStringNoCol(), Class->GetName())); } //FIXME
              else if (sc->Check("Shadow")) { RenderStyle = STYLE_Fuzzy; if (dbg_show_decorate_unsupported) GCon->Log(va("%s: Render style 'Shadow' in '%s' is not yet supported", *prloc.toStringNoCol(), Class->GetName())); } //FIXME
              else sc->Error("Bad render style");
              P.Field->SetByte(DefObj, RenderStyle);
            }
            break;
          case PROP_DefaultAlpha: // heretic should have 0.6, but meh...
            P.Field->SetFloat(DefObj, 0.6f);
            break;
          case PROP_Translation:
            P.Field->SetInt(DefObj, R_ParseDecorateTranslation(sc, (GameFilter&GAME_Strife ? 7 : 3)));
            break;
          case PROP_BloodColour:
            {
              vuint32 Col;
              //GCon->Logf("********** <%s>", *sc->String);
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
                //GCon->Logf("********** <%s>", *sc->String);
                sc->ExpectString();
                if (sc->String.length()) {
                  Col = M_ParseColour(sc->String);
                } else {
                  Col = 0xff000000;
                }
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
            }
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
          case PROP_PawnWeaponSlot: // Player.WeaponSlot 1, XFist, XChainsaw
            {
              // get slot number
              sc->ExpectNumber();
              int sidx = sc->Number;
              if (!newWSlots.isValidSlot(sidx)) GCon->Logf(NAME_Warning, "%s: invalid weapon slot number %d", *sc->GetLoc().toStringNoCol(), sidx);
              newWSlots.clearSlot(sidx);
              while (sc->Check(",")) {
                sc->ExpectString();
                newWSlots.addToSlot(sidx, VName(*sc->String));
              }
            }
            break;
          case PROP_SkipLineUnsupported:
            {
              if (dbg_show_decorate_unsupported) GCon->Logf(NAME_Warning, "%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
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
    //while (sc->Check(";")) {}
    if (FoundProp) continue;

    if (decorate_fail_on_unknown) {
      sc->Error(va("Unknown property \"%s\"", *Prop));
    } else {
      GCon->Logf(NAME_Warning, "%s: Unknown property \"%s\"", *prloc.toStringNoCol(), *Prop);
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
    /*mobjinfo_t *nfo =*/ VClass::AllocMObjId(DoomEdNum, (GameFilter ? GameFilter : GAME_Any), Class);
    //if (nfo) nfo->Class = Class;
    //GCon->Logf("DECORATE: DoomEdNum #%d assigned to '%s'", DoomEdNum, *Class->GetFullName());
    //VMemberBase::StaticDumpMObjInfo();
  }

  if (SpawnNum > 0) {
    /*mobjinfo_t *nfo =*/ VClass::AllocScriptId(SpawnNum, (GameFilter ? GameFilter : GAME_Any), Class);
    //if (nfo) nfo->Class = Class;
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
  TArray<VDecorateUserVarDef> uvars;
  VClass *Class = Type == OLDDEC_Pickup ?
    FakeInventoryClass->CreateDerivedClass(ClassName, DecPkg, uvars, sc->GetLoc()) :
    ActorClass->CreateDerivedClass(ClassName, DecPkg, uvars, sc->GetLoc());
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
    else if (sc->Check("Chex")) GameFilter |= GAME_Chex;
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
        GCon->Logf(NAME_Warning, "Unknown property '%s'", *sc->String);
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
    /*mobjinfo_t *nfo =*/ VClass::AllocMObjId(DoomEdNum, (GameFilter ? GameFilter : GAME_Any), Class);
    //if (nfo) nfo->Class = Class;
  }

  if (SpawnNum > 0) {
    /*mobjinfo_t *nfo =*/ VClass::AllocScriptId(SpawnNum, (GameFilter ? GameFilter : GAME_Any), Class);
    //if (nfo) nfo->Class = Class;
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
  GCon->Logf(NAME_Warning, "%s: 'DamageType' in decorate is not implemented yet!", *sc->GetLoc().toStringNoCol());
  sc->SkipBracketed();
}


//==========================================================================
//
//  ParseDecorate
//
//==========================================================================
static void ParseDecorate (VScriptParser *sc, TArray<VClassFixup> &ClassFixups, VWeaponSlotFixups &newWSlots) {
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
      ParseDecorate(new VScriptParser(/*sc->String*/W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassFixups, newWSlots);
    } else if (sc->Check("const")) {
      ParseConst(sc);
    } else if (sc->Check("enum")) {
      ParseEnum(sc);
    } else if (sc->Check("class")) {
      ParseClass(sc);
    } else if (sc->Check("actor")) {
      ParseActor(sc, ClassFixups, newWSlots);
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


/*
static void dumpFieldDefs (VClass *cls) {
  if (!cls || !cls->Fields) return;
  GCon->Logf("=== CLASS:%s ===", cls->GetName());
  for (VField *fi = cls->Fields; fi; fi = fi->Next) {
    if (fi->Type.Type == TYPE_Int) {
      GCon->Logf("  %s: %d v=%d", fi->GetName(), fi->Ofs, *(vint32 *)(cls->Defaults+fi->Ofs));
    } else if (fi->Type.Type == TYPE_Float) {
      GCon->Logf("  %s: %d v=%f", fi->GetName(), fi->Ofs, *(float *)(cls->Defaults+fi->Ofs));
    } else {
      GCon->Logf("  %s: %d", fi->GetName(), fi->Ofs);
    }
  }
}
*/


//==========================================================================
//
//  ProcessDecorateScripts
//
//==========================================================================
void ProcessDecorateScripts () {
  guard(ProcessDecorateScripts);

  for (int Lump = W_IterateFile(-1, "decorate_ignore.txt"); Lump != -1; Lump = W_IterateFile(Lump, "decorate_ignore.txt")) {
    GCon->Logf(NAME_Init, "Parsing DECORATE ignore file '%s'...", *W_FullLumpName(Lump));
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    check(Strm);
    VScriptParser *sc = new VScriptParser(W_FullLumpName(Lump), Strm);
    while (sc->GetString()) {
      if (sc->String.length() == 0) continue;
      VName nl = VName(*sc->String.toLowerCase());
      IgnoredDecorateActions.put(nl, true);
    }
    delete sc;
    delete Strm;
  }

  int fp = GArgs.CheckParm("-decignore");
  if (fp) {
    while (++fp != GArgs.Count()) {
      if (GArgs[fp][0] == '-' || GArgs[fp][0] == '+') break;
      VStr fname = VStr(GArgs[fp]);
      if (Sys_FileExists(fname)) {
        VStream *Strm = FL_OpenSysFileRead(fname);
        GCon->Logf(NAME_Init, "Parsing DECORATE ignore file '%s'...", *fname);
        check(Strm);
        VScriptParser *sc = new VScriptParser(fname, Strm);
        while (sc->GetString()) {
          if (sc->String.length() == 0) continue;
          VName nl = VName(*sc->String.toLowerCase());
          IgnoredDecorateActions.put(nl, true);
        }
        delete sc;
        delete Strm;
      }
    }
  }

  fp = GArgs.CheckParm("-decignoreaction");
  if (fp) {
    while (++fp != GArgs.Count()) {
      if (GArgs[fp][0] == '-' || GArgs[fp][0] == '+') break;
      VStr actname = VStr(GArgs[fp]);
      if (actname.length()) {
        actname = actname.toLowerCase();
        IgnoredDecorateActions.put(VName(*actname), true);
      }
    }
  }

  GCon->Logf(NAME_Init, "Parsing DECORATE definition files");
  for (int Lump = W_IterateFile(-1, "vavoom_decorate_defs.xml"); Lump != -1; Lump = W_IterateFile(Lump, "vavoom_decorate_defs.xml")) {
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    check(Strm);
    VXmlDocument *Doc = new VXmlDocument();
    Doc->Parse(*Strm, "vavoom_decorate_defs.xml");
    delete Strm;
    ParseDecorateDef(*Doc);
    delete Doc;
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
  VWeaponSlotFixups newWSlots;
  for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0; Lump = W_IterateNS(Lump, WADNS_Global)) {
    if (W_LumpName(Lump) == NAME_decorate) {
      GCon->Logf(NAME_Init, "Parsing decorate script '%s'...", *W_FullLumpName(Lump));
      ParseDecorate(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassFixups, newWSlots);
    }
  }
  //VMemberBase::StaticDumpMObjInfo();

  // make sure all import classes were defined
  if (VMemberBase::GDecorateClassImports.Num()) {
    for (int i = 0; i < VMemberBase::GDecorateClassImports.Num(); ++i) {
      GCon->Logf("Undefined DECORATE class `%s`", VMemberBase::GDecorateClassImports[i]->GetName());
    }
    Sys_Error("Not all DECORATE class imports were defined");
  }

  GCon->Logf(NAME_Init, "Post-procesing");
  //VMemberBase::StaticDumpMObjInfo();

  /*k8: not yet
  for (int i = 0; i < DecPkg->ParsedClasses.Num(); ++i) {
    if (GArgs.CheckParm("-debug_decorate")) GCon->Logf("Defining Class %s", *DecPkg->ParsedClasses[i]->GetFullName());
    if (!DecPkg->ParsedClasses[i]->DecorateDefine()) Sys_Error("DECORATE ERROR: cannot define class '%s'", *DecPkg->ParsedClasses[i]->GetFullName());
  }
  */

  // set class properties
  for (int i = 0; i < ClassFixups.Num(); ++i) {
    VClassFixup &CF = ClassFixups[i];
    if (!CF.ReqParent) Sys_Error("Invalid decorate class fixup (no parent); class is '%s', offset is %d, name is '%s'", (CF.Class ? *CF.Class->GetFullName() : "None"), CF.Offset, *CF.Name);
    check(CF.ReqParent);
    //GCon->Logf("*** FIXUP (class='%s'; name='%s'; ofs=%d)", CF.Class->GetName(), *CF.Name, CF.Offset);
    if (CF.Name.ICmp("None") == 0) {
      *(VClass **)(CF.Class->Defaults+CF.Offset) = nullptr;
    } else {
      VClass *C = VClass::FindClassLowerCase(*CF.Name.ToLower());
      if (!C) {
        if (dbg_show_missing_class) GCon->Logf(NAME_Warning, "No such class `%s`", *CF.Name);
      } else if (!C->IsChildOf(CF.ReqParent)) {
        GCon->Logf(NAME_Warning, "Class `%s` is not a descendant of `%s`", *CF.Name, CF.ReqParent->GetName());
      } else {
        *(VClass**)(CF.Class->Defaults+CF.Offset) = C;
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
           if (!C) { if (dbg_show_missing_class) GCon->Logf(NAME_Warning, "No such class `%s`", *DI.TypeName); }
      else if (!C->IsChildOf(ActorClass)) GCon->Logf(NAME_Warning, "Class `%s` is not an actor class", *DI.TypeName);
      else DI.Type = C;
    }
  }

  // fix weapon slots
  if (newWSlots.hasAnyDefinedSlot()) {
    //VName getSlotName (int sidx, int nidx) const
    VClass *gi = VClass::FindClass("MainGameInfo");
    VClass *wpnbase = VClass::FindClass("Weapon");
    if (!gi || !wpnbase) {
      if (!gi) GCon->Logf(NAME_Warning, "`MainGameInfo` class not found, cannot set weapon slots");
      if (!wpnbase) GCon->Logf(NAME_Warning, "`Weapon` class not found, cannot set weapon slots");
    } else {
      //WARNING: keep this in sync with script code!
      gi = gi->GetReplacement();
      VField *fldFlags = gi->FindFieldChecked(VName("WeaponSlotDefined"));
      VField *fldList = gi->FindFieldChecked(VName("WeaponSlotClasses"));
      if (!fldFlags || !fldList) {
        GCon->Logf(NAME_Warning, "some fields not found, cannot set weapon slots");
      } else {
        vint32 *wsflag = (vint32 *)fldFlags->GetFieldPtr((VObject *)gi->Defaults);
        VClass **wslist = (VClass **)fldList->GetFieldPtr((VObject *)gi->Defaults);
        for (int sidx = 0; sidx <= NUM_WEAPON_SLOTS; ++sidx) {
          wsflag[sidx] = 0;
          if (!newWSlots.isDefinedSlot(sidx)) continue;
          wsflag[sidx] = 1; // redefined
          VClass **sarr = wslist+MAX_WEAPONS_PER_SLOT*sidx;
          for (int widx = 0; widx < MAX_WEAPONS_PER_SLOT; ++widx) {
            sarr[widx] = nullptr;
            VName cn = newWSlots.getSlotName(sidx, widx);
            if (cn == NAME_None) continue;
            VStr lcn = VStr(*cn).toLowerCase();
            VClass *wc = VClass::FindClassLowerCase(*lcn);
            if (!wc) { GCon->Logf(NAME_Warning, "unknown weapon class '%s'", *cn); continue; }
            if (!wc->IsChildOf(wpnbase)) { GCon->Logf(NAME_Warning, "class '%s' is not a weapon", *cn); continue; }
            if (GArgs.CheckParm("-debug-decorate-weapon-slots")) GCon->Logf(NAME_Init, "DECORATE: slot #%d, weapon #%d set to '%s'", sidx, widx, *wc->GetFullName());
            sarr[widx] = wc;
          }
        }
      }
    }
  }

  // emit code
  for (int i = 0; i < DecPkg->ParsedClasses.Num(); ++i) {
    if (GArgs.CheckParm("-debug_decorate")) GCon->Logf("Emiting Class %s", *DecPkg->ParsedClasses[i]->GetFullName());
    //dumpFieldDefs(DecPkg->ParsedClasses[i]);
    DecPkg->ParsedClasses[i]->DecorateEmit();
  }

  // compile and set up for execution
  for (int i = 0; i < DecPkg->ParsedClasses.Num(); ++i) {
    if (GArgs.CheckParm("-debug_decorate")) GCon->Logf("Compiling Class %s", *DecPkg->ParsedClasses[i]->GetFullName());
    //dumpFieldDefs(DecPkg->ParsedClasses[i]);
    DecPkg->ParsedClasses[i]->DecoratePostLoad();
    //dumpFieldDefs(DecPkg->ParsedClasses[i]);
  }

  if (vcErrorCount) BailOut();
  //VMemberBase::StaticDumpMObjInfo();

  VClass::StaticReinitStatesLookup();

  //!TLocation::ClearSourceFiles();
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
bool VEntity::SetDecorateFlag (const VStr &Flag, bool Value) {
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

  //if (VStr::ICmp(*FlagName, "noblockmap") == 0) return false; // cannot change it
  //GCon->Logf("SETFLAG '%s'(%s) on '%s'", *FlagName, *Flag, *GetClass()->GetFullName());
  //return false;

  for (int j = 0; j < FlagList.Num(); ++j) {
    VFlagList &ClassDef = FlagList[j];
    if (ClassFilter != NAME_None && ClassDef.Class->LowerCaseName != ClassFilter) continue;
    if (!IsA(ClassDef.Class)) continue;
    for (int i = ClassDef.FlagsHash[GetTypeHash(FlagName)&(FLAGS_HASH_SIZE-1)]; i != -1; i = ClassDef.Flags[i].HashNext) {
      const VFlagDef &F = ClassDef.Flags[i];
      if (FlagName == F.Name) {
        //GCon->Logf("SETFLAG '%s'(%s) on '%s'", *FlagName, *Flag, *GetClass()->GetFullName());
        UnlinkFromWorld(); // some flags can affect word linking, so unlink here...
        bool didset = true;
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
          default: didset = false;
        }
        LinkToWorld(); // ...and link back again
        return didset;
      }
    }
  }
  GCon->Logf("Unknown flag '%s'", *Flag);
  return false;
  unguard;
}


//==========================================================================
//
//  VEntity::GetDecorateFlag
//
//==========================================================================
bool VEntity::GetDecorateFlag (const VStr &Flag) {
  guard(VEntity::GetDecorateFlag);
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
          case FLAG_Bool: return F.Field->GetBool(this);
          case FLAG_Unsupported: if (dbg_show_decorate_unsupported) GCon->Logf("Unsupported flag %s in %s", *Flag, GetClass()->GetName()); return false;
          case FLAG_Byte: return !!F.Field->GetByte(this);
          case FLAG_Float: return (F.Field->GetFloat(this) != 0.0f);
          case FLAG_Name: return (F.Field->GetNameValue(this) != NAME_None);
          case FLAG_Class: return !!F.Field->GetClassValue(this);
          case FLAG_NoClip: return (!F.Field->GetBool(this) && !F.Field2->GetBool(this)); //FIXME??
        }
        return false;
      }
    }
  }
  GCon->Logf("Unknown flag '%s'", *Flag);
  return false;
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
  if (GArgs.CheckParm("-compiler") != 0 || GArgs.CheckParm("-c") != 0) {
    //GCon->Logf("Compiler allocated %u bytes.", VExpression::TotalMemoryUsed);
    GCon->Logf(NAME_Init, "Peak compiler memory usage: %s bytes.", comatoze(VExpression::PeakMemoryUsed));
    GCon->Logf(NAME_Init, "Released compiler memory  : %s bytes.", comatoze(VExpression::TotalMemoryFreed));
    if (VExpression::CurrMemoryUsed != 0) {
      GCon->Logf(NAME_Init, "Compiler leaks %s bytes (this is harmless).", comatoze(VExpression::CurrMemoryUsed));
      VExpression::ReportLeaks();
    }
  }
}
