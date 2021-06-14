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
#include "../../libs/vavoomc/vc_local.h"
#include "../gamedefs.h"
#include "../render/r_local.h" // for SPR_xxx
#include "../psim/p_entity.h"
#include "vc_decorate.h"


// WARNING! dehacked parser requires this!
#define VC_DECORATE_ACTION_BELONGS_TO_STATE

static VCvarB dbg_show_decorate_unsupported("dbg_show_decorate_unsupported", false, "Show unsupported decorate props/flags?", CVAR_PreInit|CVAR_Archive);
static VCvarB dbg_debug_weapon_slots("dbg_debug_weapon_slots", false, "Debug weapon slots?", CVAR_PreInit);
static VCvarB dbg_dump_flag_changes("dbg_dump_flag_changes", false, "Dump all flag changes?", CVAR_PreInit);
VCvarB dbg_show_missing_classes("dbg_show_missing_classes", false, "Show missing classes?", CVAR_PreInit|CVAR_Archive);
VCvarB decorate_fail_on_unknown("decorate_fail_on_unknown", false, "Fail on unknown decorate properties?", CVAR_PreInit|CVAR_Archive);

static int dcTotalSourceSize = 0;

static int disableBloodReplaces = 0;
static int bloodOverrideAllowed = 0;
static int enableKnownBlood = 0;
bool decorateSkipBDWClasses = false;

// you cannot override/replace those actors
static bool GlobalDisableOverride = false;
static TMap<VStrCI, bool> NoOverrideNames;

// globals
bool decoIgnorePlayerSpeed = false;
TMapNC<VName, bool> BlockedSpawnSet;
TMapNC<VName, VClass *> ForceReplacements;


//WARNING! keep this in sync with "Inventory.vc"
enum {
  INVERSECOLOR = 0x00123456,
  GOLDCOLOR    = 0x00123457,
  REDCOLOR     = 0x00123458,
  GREENCOLOR   = 0x00123459,
  MONOCOLOR    = 0x0012345A,
  BEREDCOLOR   = 0x0012345B,
  BLUECOLOR    = 0x0012345C,
  INVERSXCOLOR = 0x0012345D,
};

/*
static inline VVA_OKUNUSED vuint32 GetTypeHashCI (const VStr &s) { return fnvHashStrCI(*s); }
static inline VVA_OKUNUSED vuint32 GetTypeHashCI (const char *s) { return fnvHashStrCI(s); }
static inline VVA_OKUNUSED vuint32 GetTypeHashCI (VName s) { return fnvHashStrCI(*s); }
*/

// GameInfo is not spawned yet, so keep them here
TArray<VDamageFactor> CustomDamageFactors;


enum {
  //PROPS_HASH_SIZE = 256,
  //FLAGS_HASH_SIZE = 256,
  //WARNING! keep in sync with script code (LineSpecialGameInfo.vc)
  NUM_WEAPON_SLOTS = 10,
};

enum {
  THINGSPEC_Default = 0,
  THINGSPEC_ThingActs = 1,
  THINGSPEC_ThingTargets = 2,
  THINGSPEC_TriggerTargets = 4,
  THINGSPEC_MonsterTrigger = 8,
  THINGSPEC_MissileTrigger = 16,
  THINGSPEC_ClearSpecial = 32,
  THINGSPEC_NoDeathSpecial = 64,
  THINGSPEC_TriggerActs = 128,
  THINGSPEC_Activate = 1<<8, // the thing is activated when triggered
  THINGSPEC_Deactivate = 1<<9, // the thing is deactivated when triggered
  THINGSPEC_Switch = 1<<10, // the thing is alternatively activated and deactivated when triggered
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
  PROP_IntBobPhase,
  PROP_BitIndex,
  PROP_Float,
  PROP_FloatUnsupported,
  PROP_Speed,
  PROP_SpeedMult,
  PROP_Tics,
  PROP_TicsSecs,
  PROP_Percent,
  PROP_FloatClamped,
  PROP_FloatClamped2,
  PROP_FloatOpt2, //WARNING! used exclusively for movement speed
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
  PROP_BloodColor,
  PROP_BloodTranslation,
  PROP_BloodType,
  PROP_StencilColor,
  PROP_Monster,
  PROP_Projectile,
  PROP_BounceType,
  PROP_ClearFlags,
  PROP_DropItem,
  PROP_States,
  PROP_SkipSuper,
  PROP_Args,
  PROP_LowMessage,
  PROP_PowerupColor,
  PROP_ColorRange,
  PROP_DamageScreenColor,
  PROP_HexenArmor,
  PROP_StartItem,
  PROP_MorphStyle,
  PROP_SkipLineUnsupported,
  PROP_PawnWeaponSlot,
  PROP_DefaultAlpha,
  PROP_Activation,
  PROP_PoisonDamage,
};

enum {
  FLAG_Bool,
  FLAG_BoolInverted,
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
  VStr PowerPrefix; // for powerup type
};


// ////////////////////////////////////////////////////////////////////////// //
static void ParseConst (VScriptParser *sc, VMemberBase *parent, bool changeMode=true);
static void ParseEnum (VScriptParser *sc, VMemberBase *parent, bool changeMode=true);

#ifdef VAVOOM_K8_DEVELOPER
# define VC_DECO_DEF_WARNS  0
#else
# define VC_DECO_DEF_WARNS  1
#endif
static int cli_DecorateDebug = 0;
static int cli_DecorateMoronTolerant = 0;
static int cli_DecorateOldReplacement = 0;
static int cli_DecorateDumpReplaces = 0;
static int cli_DecorateLaxParents = 0;
static int cli_DecorateNonActorReplace = 0;
static int cli_DecorateWarnPowerupRename = 0;
static int cli_DecorateAllowUnsafe = 0;
static int cli_CompilerReport = 0;
static int cli_ShowClassRTRouting = VC_DECO_DEF_WARNS;
static int cli_ShowDropItemMissingClasses = VC_DECO_DEF_WARNS;
static int cli_ShowRemoveStateWarning = VC_DECO_DEF_WARNS;
static TArray<VStr> cli_DecorateIgnoreFiles;
static TArray<VStr> cli_DecorateIgnoreActions;
static TMap<VStrCI, bool> IgnoredDecorateActions;


/*static*/ bool cliRegister_decorate_args =
  VParsedArgs::RegisterFlagSet("-debug-decorate", "!show various debug messages from decorate parser", &cli_DecorateDebug) &&
  VParsedArgs::RegisterFlagSet("-vc-decorate-moron-tolerant", "!decorate parser tolerancy", &cli_DecorateMoronTolerant) &&
  VParsedArgs::RegisterFlagSet("-vc-decorate-old-replacement", "!decorate parser tolerancy", &cli_DecorateOldReplacement) &&
  VParsedArgs::RegisterFlagSet("-vc-decorate-dump-replaces", "!decorate parser tolerancy", &cli_DecorateDumpReplaces) &&
  VParsedArgs::RegisterFlagSet("-vc-decorate-lax-parents", "!decorate parser tolerancy", &cli_DecorateLaxParents) &&
  VParsedArgs::RegisterFlagSet("-vc-decorate-nonactor-replace", "!decorate parser tolerancy", &cli_DecorateNonActorReplace) &&

  VParsedArgs::RegisterFlagSet("-vc-decorate-rt-warnings", "show \"class rt-routed\" decorate warnings", &cli_ShowClassRTRouting) &&
  VParsedArgs::RegisterFlagReset("-vc-no-decorate-rt-warnings", "hide \"class rt-routed\" decorate warnings", &cli_ShowClassRTRouting) &&

  VParsedArgs::RegisterFlagSet("-vc-decorate-dropitem-warnings", "show \"bad DropItem class\" decorate warnings", &cli_ShowDropItemMissingClasses) &&
  VParsedArgs::RegisterFlagReset("-vc-no-decorate-dropitem-warnings", "hide \"bad DropItem class\" decorate warnings", &cli_ShowDropItemMissingClasses) &&

  VParsedArgs::RegisterFlagSet("-vc-decorate-removestate-warnings", "show \"use RemoveState\" decorate warnings", &cli_ShowRemoveStateWarning) &&
  VParsedArgs::RegisterFlagReset("-vc-no-decorate-removestate-warnings", "hide \"use RemoveState\" decorate warnings", &cli_ShowRemoveStateWarning) &&

  VParsedArgs::RegisterFlagSet("-disable-blood-replaces", "do not replace blood actors with mod-provided blood", &disableBloodReplaces) &&
  VParsedArgs::RegisterAlias("-disable-blood-replacement", "-disable-blood-replaces") &&
  VParsedArgs::RegisterAlias("-no-blood-replaces", "-disable-blood-replaces") &&
  VParsedArgs::RegisterAlias("-no-blood-replacement", "-disable-blood-replaces") &&

  VParsedArgs::RegisterFlagSet("-enable-known-blood", "enable some well-known blood replacements (NashGore, for example)", &enableKnownBlood) &&

  VParsedArgs::RegisterFlagSet("-decorate-allow-unsafe", "allow loops in decorate anonymous functions", &cli_DecorateAllowUnsafe) &&
  VParsedArgs::RegisterAlias("--decorate-allow-unsafe", "-decorate-allow-unsafe") &&

  VParsedArgs::RegisterFlagSet("-Wdecorate-powerup-rename", "!warn about powerup renames (internal k8vavoom feature)", &cli_DecorateWarnPowerupRename) &&

  VParsedArgs::RegisterFlagSet("-compiler", "report some compiler info", &cli_CompilerReport) &&

  VParsedArgs::RegisterCallback("-vc-decorate-ignore-file", "!", [] (VArgs &args, int idx) -> int {
    ++idx;
    if (!VParsedArgs::IsArgBreaker(args, idx)) {
      VStr mn = args[idx];
      if (!mn.isEmpty() && Sys_FileExists(mn)) {
        cli_DecorateIgnoreFiles.append(mn);
      }
    }
    return idx;
  }) &&

  VParsedArgs::RegisterCallback("-vc-decorate-ignore-actions", "!", [] (VArgs &args, int idx) -> int {
    for (++idx; !VParsedArgs::IsArgBreaker(args, idx); ++idx) {
      VStr mn = args[idx];
      if (!mn.isEmpty()) {
        IgnoredDecorateActions.put(mn, true);
      }
    }
    return idx;
  });


static inline bool getDecorateDebug () { return !!cli_DecorateDebug; }
static inline bool getIgnoreMoronicStateCommands () { return !!cli_DecorateMoronTolerant; }


// ////////////////////////////////////////////////////////////////////////// //
// this is workaround for mo...dders overriding the same class several times in
// the same mod (yes, smoothdoom, i am talking about you).
// we will cut off old override if we'll find a new one
static TMapNC<VClass *, bool> currFileRepls; // set; key is old class


//==========================================================================
//
//  IsNoOverrideName
//
//==========================================================================
static inline bool IsNoOverrideName (VStr name) noexcept {
  return NoOverrideNames.has(name);
}


//==========================================================================
//
//  AddNoOverrideName
//
//==========================================================================
static void AddNoOverrideName (VStr name) noexcept {
  if (!name.isEmpty()) NoOverrideNames.put(name, true);
}


//==========================================================================
//
//  ClearReplacementBase
//
//==========================================================================
static void ClearReplacementBase () {
  currFileRepls.clear();
}


//==========================================================================
//
//  ResetReplacementBase
//
//==========================================================================
static void ResetReplacementBase () {
  currFileRepls.reset();
}


//==========================================================================
//
//  SC_ParseGameDef
//
//==========================================================================
vuint32 SC_ParseGameDef (VScriptParser *sc, bool allowAny) {
  vassert(sc);
  vuint32 GameFilter = 0;
       if (sc->Check("Doom")) GameFilter |= GAME_Doom;
  else if (sc->Check("Heretic")) GameFilter |= GAME_Heretic;
  else if (sc->Check("Hexen")) GameFilter |= GAME_Hexen;
  else if (sc->Check("Strife")) GameFilter |= GAME_Strife;
  else if (sc->Check("Raven")) GameFilter |= GAME_Raven;
  else if (allowAny && sc->Check("Any")) GameFilter |= GAME_Any;
  else if (sc->Check("Chex")) GameFilter |= GAME_Chex;
  return GameFilter;
}


// ////////////////////////////////////////////////////////////////////////// //
#include "vc_bloodinfo.cpp"
#include "vc_classignore.cpp"


//==========================================================================
//
//  DoClassReplacement
//
//==========================================================================
static void DoClassReplacement (VClass *oldcls, VClass *newcls) {
  if (!oldcls) return;

  // check for knows blood classes if gore mod is active
  if (cli_GoreMod && !enableKnownBlood) {
    bool blockSpawn = false;
    VStr kbc = DetectKnownBloodClass(newcls, &blockSpawn, IsAnyBloodClass(oldcls));
    if (!kbc.isEmpty()) {
      GLog.Logf(NAME_Warning, "%s: %s known blood replacement class '%s' (%s)", *newcls->Loc.toStringNoCol(), (blockSpawn ? "blocked" : "skipped"), newcls->GetName(), *kbc);
      if (blockSpawn) BlockedSpawnSet.put(newcls->Name, true);
      return;
    }
  }

  // also, block spawning of all replacements of the blocked class
  if (BlockedSpawnSet.has(oldcls->Name) && !BlockedSpawnSet.has(newcls->Name)) {
    GLog.Logf(NAME_Warning, "%s: blocked known blood replacement class '%s' (%s)", *newcls->Loc.toStringNoCol(), newcls->GetName(), oldcls->GetName());
    BlockedSpawnSet.put(newcls->Name, true);
  }

  const bool doOldRepl = (cli_DecorateOldReplacement > 0);
  const bool dumpReplaces = (cli_DecorateDumpReplaces > 0);

  if (doOldRepl) {
    if (disableBloodReplaces && !bloodOverrideAllowed && IsAnyBloodClass(oldcls)) {
      GLog.Logf(NAME_Warning, "%s: skipped blood replacement class '%s'", *newcls->Loc.toStringNoCol(), newcls->GetName());
      return;
    }
    // old k8vavoom method
    oldcls->Replacement = newcls;
    newcls->Replacee = oldcls;
  } else {
    if (disableBloodReplaces && !bloodOverrideAllowed && IsAnyBloodClass(oldcls)) {
      GLog.Logf(NAME_Warning, "%s: skipped blood replacement class '%s'", *newcls->Loc.toStringNoCol(), newcls->GetName());
      return;
    }
    VClass *repl = oldcls->GetReplacement();
    if (disableBloodReplaces && !bloodOverrideAllowed && IsAnyBloodClass(repl)) {
      GLog.Logf(NAME_Warning, "%s: skipped blood replacement class '%s'", *newcls->Loc.toStringNoCol(), newcls->GetName());
      return;
    }
    vassert(repl);
    auto orpp = currFileRepls.find(oldcls);
    if (orpp) {
      //GLog.Logf("*** duplicate replacement of '%s' with '%s' (current is '%s')", *oldcls->GetFullName(), *newcls->GetFullName(), *repl->GetFullName());
      GLog.Logf(NAME_Warning, "DECORATE error: already replaced class '%s' replaced again with '%s' (current replacement is '%s')", oldcls->GetName(), newcls->GetName(), repl->GetName());
      GLog.Logf(NAME_Warning, "  current replacement is at %s", *repl->Loc.toStringNoCol());
      GLog.Logf(NAME_Warning, "  new replacement is at %s", *newcls->Loc.toStringNoCol());
      GLog.Log(NAME_Warning, "  PLEASE, FIX THE CODE!");
      repl = oldcls;
    } else {
      // first occurence
      //VClass *repl = oldcls->GetReplacement();
      //GLog.Logf("*** first replacement of '%s' with '%s' (wanted '%s')", *repl->GetFullName(), *newcls->GetFullName(), *oldcls->GetFullName());
      currFileRepls.put(oldcls, true);
    }
    if (repl != newcls) {
      if (dumpReplaces) GCon->Logf(NAME_Init, "DECORATE: class `%s` replaced with class `%s`", repl->GetName(), newcls->GetName());
      repl->Replacement = newcls;
      newcls->Replacee = repl;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
struct VWeaponSlotFixups {
private:
  VStr plrClassName;
  TArray<VStr> slots[NUM_WEAPON_SLOTS]; // 0..9

private:
  // slot 0 is actually slot 10
  int normSlotNumber (int idx) const { return (idx == 0 ? NUM_WEAPON_SLOTS-1 : idx-1); }

public:
  VWeaponSlotFixups () : plrClassName(VStr::EmptyString) { for (int f = 0; f < NUM_WEAPON_SLOTS; ++f) slots[f].clear(); }

  inline void clearAllSlots () { for (int f = 0; f < NUM_WEAPON_SLOTS; ++f) slots[f].clear(); }

  inline VStr getPlayerClassName () const { return plrClassName; }
  inline void setPlayerClassName (VStr aname) { plrClassName = aname; }

  inline static bool isValidSlot (int idx) { return (idx >= 0 && idx <= NUM_WEAPON_SLOTS); }

  inline int getSlotWeaponCount (int idx) const { return (isValidSlot(idx) ? slots[normSlotNumber(idx)].length() : 0); }

  inline const TArray<VStr> &getSlotWeaponList (int idx) const {
    vassert(idx >= 0 && idx < NUM_WEAPON_SLOTS);
    return slots[/*normSlotNumber*/(idx)];
  }

  inline void clearSlot (int idx) { if (isValidSlot(idx)) slots[normSlotNumber(idx)].clear(); }

  void addToSlot (int idx, VStr aname) {
    if (!isValidSlot(idx)) return;
    if (aname.isEmpty() || aname.strEquCI("none")) return;
    idx = normSlotNumber(idx);
    // remove previous instances of aname
    TArray<VStr> &list = slots[idx];
    int pos = 0;
    while (pos < list.length()) {
      if (list[pos].strEquCI(aname)) {
        list.removeAt(pos);
      } else {
        ++pos;
      }
    }
    list.append(aname);
  }
};


static VWeaponSlotFixups &allocWeaponSlotsFor (TArray<VWeaponSlotFixups> &list, VClass *klass) {
  vassert(klass);
  for (auto &&el : list) {
    if (el.getPlayerClassName().strEquCI(klass->GetName())) return el;
  }
  VWeaponSlotFixups &fxp = list.alloc();
  fxp.setPlayerClassName(klass->GetName());
  fxp.clearAllSlots();
  return fxp;
}


// ////////////////////////////////////////////////////////////////////////// //
struct VPropDef {
  vuint8 Type;
  int HashNext;
  //VName Name;
  VStr Name;
  VField *Field;
  VField *Field2;
  VName PropName;
  union {
    int IConst;
    float FMin;
  };
  float FMax;
  VStr CPrefix;

  bool ShowWarning;

  inline void SetField (VClass *Class, const char *FieldName) {
    Field = Class->FindFieldChecked(FieldName);
  }

  inline void SetField2 (VClass *Class, const char *FieldName) {
    Field2 = Class->FindFieldChecked(FieldName);
  }
};


struct VFlagDef {
  vuint8 Type;
  int HashNext;
  //VName Name;
  VStr Name;
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

  bool ShowWarning;
  bool RelinkToWorld; // for decorate actions

  inline void SetField (VClass *Class, const char *FieldName) {
    Field = Class->FindFieldChecked(FieldName);
  }

  inline void SetField2 (VClass *Class, const char *FieldName) {
    Field2 = Class->FindFieldChecked(FieldName);
  }
};


struct VFlagList {
  VClass *Class;

  TArray<VPropDef> Props;
  TMap<VStrCI, int> PropsHash;

  TArray<VFlagDef> Flags;
  TMap<VStrCI, int> FlagsHash;

  VPropDef *FindProp (VStr name) {
    auto pi = PropsHash.find(name);
    if (!pi) {
      const int dotidx = name.indexOf('.');
      if (dotidx >= 0) {
        VStr cn = name.left(dotidx);
        name.chopLeft(dotidx+1);
        if (name.length() == 0) return nullptr;
        if (cn.strEquCI("Player")) cn = "PlayerPawn";
        if (!cn.strEquCI(Class->GetName())) return nullptr;
        pi = PropsHash.find(name);
      }
    }
    return (pi ? &Props[*pi] : nullptr);
  }

  // name must be short (i.e. without qualification part)
  VFlagDef *FindFlag (VStr name) {
    auto pi = FlagsHash.find(name);
    return (pi ? &Flags[*pi] : nullptr);
  }

  VPropDef &NewProp (vuint8 Type, VXmlNode *PN, bool checkUnsupported=false) {
    VPropDef &P = Props.Alloc();
    P.Type = Type;
    //P.Name = VName(*PN->GetAttribute("name"), VName::AddLower);
    P.Name = PN->GetAttribute("name");
    if (checkUnsupported && PN->HasAttribute("warning")) {
      VStr bs = PN->GetAttribute("warning");
      if (bs.strEquCI("true") || bs.strEquCI("tan")) P.ShowWarning = true;
    }
    PropsHash.put(VStr(P.Name), Props.length()-1);
    return P;
  }

  VFlagDef &NewFlag (vuint8 Type, VXmlNode *PN, bool checkUnsupported=false) {
    VFlagDef &F = Flags.Alloc();
    F.Type = Type;
    //F.Name = VName(*PN->GetAttribute("name"), VName::AddLower);
    F.Name = PN->GetAttribute("name");
    const int dotidx = F.Name.indexOf('.');
    if (dotidx >= 0) {
      VStr cn = F.Name.left(dotidx);
      F.Name.chopLeft(dotidx+1);
      if (F.Name.length() == 0) Sys_Error("invalid flag name: '%s'", *PN->GetAttribute("name"));
      if (cn.strEquCI("Player")) cn = "PlayerPawn";
      if (!cn.strEquCI(Class->GetName())) Sys_Error("flag '%s' is of class '%s', but prefix is '%s'", *PN->GetAttribute("name"), Class->GetName(), *cn);
      //GCon->Logf(NAME_Debug, "=== flag '%s' trimmed to '%s' (prefix is '%s')", *PN->GetAttribute("name"), *F.Name, *cn);
    }
    if (checkUnsupported && PN->HasAttribute("warning")) {
      VStr bs = PN->GetAttribute("warning");
      if (bs.strEquCI("true") || bs.strEquCI("tan")) F.ShowWarning = true;
    }
    if (PN->HasAttribute("relinktoworld")) {
      VStr bs = PN->GetAttribute("relinktoworld");
      if (bs.strEquCI("true") || bs.strEquCI("tan")) {
        F.RelinkToWorld = true;
        //GCon->Logf(NAME_Debug, "RELINK FLAG '%s'", *F.Name);
      }
    }
    FlagsHash.put(VStr(F.Name), Flags.length()-1);
    return F;
  }
};


// ////////////////////////////////////////////////////////////////////////// //
TArray<VLineSpecInfo> LineSpecialInfos;


// ////////////////////////////////////////////////////////////////////////// //
static VPackage *DecPkg;

static VClass *EntityClass;
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
static VClass *PowerSpeedClass;

static VMethod *FuncA_Scream;
static VMethod *FuncA_NoBlocking;
static VMethod *FuncA_ScreamAndUnblock;
static VMethod *FuncA_ActiveSound;
static VMethod *FuncA_ActiveAndUnblock;
static VMethod *FuncA_ExplodeParms;
static VMethod *FuncA_FreezeDeath;
static VMethod *FuncA_FreezeDeathChunks;

static TArray<VFlagList> FlagList;

static bool inCodeBlock = false;

static int mainDecorateLump = -1;
static bool thisIsBasePak = false;


// ////////////////////////////////////////////////////////////////////////// //
struct PropLimitSub {
  VClass *baseClass;
  bool isInt;
  int amount;
  VStr cvar;
};

static TArray<PropLimitSub> limitSubs;
// list of all classes that need to be limited in some way
TArray<VClass *> NumberLimitedClasses;


//==========================================================================
//
//  FindPropLimitSub
//
//==========================================================================
static int FindPropLimitSub (VClass *aclass) {
  for (int f = 0; f < limitSubs.length(); ++f) {
    if (limitSubs[f].baseClass == aclass) return f;
  }
  return -1;
}


//==========================================================================
//
//  NewPropLimitSubInt
//
//==========================================================================
static void NewPropLimitSubInt (VClass *abaseClass, int amount) {
  vassert(abaseClass);
  if (amount > 0) {
    int idx = FindPropLimitSub(abaseClass);
    if (idx >= 0) {
      limitSubs[idx].isInt = true;
      limitSubs[idx].amount = amount;
      limitSubs[idx].cvar.clear();
    } else {
      PropLimitSub &ls = limitSubs.alloc();
      ls.baseClass = abaseClass;
      ls.isInt = true;
      ls.amount = amount;
      ls.cvar.clear();
    }
  }
}


//==========================================================================
//
//  NewPropLimitSubCvar
//
//==========================================================================
static void NewPropLimitSubCvar (VClass *abaseClass, VStr cvar) {
  vassert(abaseClass);
  if (!cvar.isEmpty()) {
    int idx = FindPropLimitSub(abaseClass);
    if (idx >= 0) {
      limitSubs[idx].isInt = false;
      limitSubs[idx].amount = 0;
      limitSubs[idx].cvar = cvar;
    } else {
      PropLimitSub &ls = limitSubs.alloc();
      ls.baseClass = abaseClass;
      ls.isInt = false;
      ls.amount = 0;
      ls.cvar = cvar;
    }
  }
}


// ////////////////////////////////////////////////////////////////////////// //
#include "vc_decorate_ast.cpp"


//==========================================================================
//
//  DefParseIntConst
//
//==========================================================================
int DefParseIntConst (const VXmlNode *node, const char *attrname) {
  VStr s = node->GetAttribute(attrname);
  // sprite orientations
  if (s.strEquCI("SPR_VP_PARALLEL_UPRIGHT")) return SPR_VP_PARALLEL_UPRIGHT;
  if (s.strEquCI("SPR_FACING_UPRIGHT")) return SPR_FACING_UPRIGHT;
  if (s.strEquCI("SPR_VP_PARALLEL")) return SPR_VP_PARALLEL;
  if (s.strEquCI("SPR_ORIENTED")) return SPR_ORIENTED;
  if (s.strEquCI("SPR_VP_PARALLEL_ORIENTED")) return SPR_VP_PARALLEL_ORIENTED;
  if (s.strEquCI("SPR_VP_PARALLEL_UPRIGHT_ORIENTED")) return SPR_VP_PARALLEL_UPRIGHT_ORIENTED;
  if (s.strEquCI("SPR_ORIENTED_OFS")) return SPR_ORIENTED_OFS;
  if (s.strEquCI("SPR_FLAT")) return SPR_FLAT;
  if (s.strEquCI("SPR_WALL")) return SPR_WALL;
  int res = 0;
  if (!s.convertInt(&res)) Sys_Error("Invalid integer \"%s\" in node '%s' attribute '%s' at %s", *s, *node->Name, attrname, *node->GetAttributeLoc(attrname).toStringNoCol());
  return res;
};


//==========================================================================
//
//  DefParseFloatConst
//
//==========================================================================
float DefParseFloatConst (const VXmlNode *node, const char *attrname) {
  VStr s = node->GetAttribute(attrname);
  float res = 0.0f;
  if (!s.convertFloat(&res)) Sys_Error("Invalid float \"%s\" in node '%s' attribute '%s' at %s", *s, *node->Name, attrname, *node->GetAttributeLoc(attrname).toStringNoCol());
  return res;
}


//==========================================================================
//
//  ParseDecorateDef
//
//==========================================================================
static void ParseDecorateDef (VXmlDocument &Doc) {
  for (VXmlNode *N = Doc.Root.FindChild("class"); N; N = N->FindNext()) {
    VStr ClassName = N->GetAttribute("name");
    VFlagList &Lst = FlagList.Alloc();
    Lst.Class = VClass::FindClass(*ClassName);
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
        P.IConst = DefParseIntConst(PN, "value");
      } else if (PN->Name == "prop_int_bob_phase") {
        VPropDef &P = Lst.NewProp(PROP_IntBobPhase, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_int_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_IntUnsupported, PN, true);
      } else if (PN->Name == "prop_float_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_FloatUnsupported, PN, true);
      } else if (PN->Name == "prop_int_id_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_IntIdUnsupported, PN, true);
      } else if (PN->Name == "prop_skip_line_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_SkipLineUnsupported, PN, true);
      } else if (PN->Name == "prop_string_unsupported") {
        /*VPropDef &P =*/(void)Lst.NewProp(PROP_StrUnsupported, PN, true);
      } else if (PN->Name == "flag_unsupported") {
        /*VFlagDef &F =*/(void)Lst.NewFlag(FLAG_Unsupported, PN, true);
      } else if (PN->Name == "prop_bit_index") {
        VPropDef &P = Lst.NewProp(PROP_BitIndex, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_float") {
        VPropDef &P = Lst.NewProp(PROP_Float, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_speed") {
        VPropDef &P = Lst.NewProp(PROP_Speed, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_speed_mult") {
        VPropDef &P = Lst.NewProp(PROP_SpeedMult, PN);
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
        P.FMin = DefParseFloatConst(PN, "min");
        P.FMax = DefParseFloatConst(PN, "max");
      } else if (PN->Name == "prop_float_clamped_2") {
        VPropDef &P = Lst.NewProp(PROP_FloatClamped2, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
        P.SetField2(Lst.Class, *PN->GetAttribute("property2"));
        P.FMin = DefParseFloatConst(PN, "min");
        P.FMax = DefParseFloatConst(PN, "max");
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
      } else if (PN->Name == "prop_blood_color") {
        VPropDef &P = Lst.NewProp(PROP_BloodColor, PN);
        P.SetField(Lst.Class, "BloodColor");
        P.SetField2(Lst.Class, "BloodTranslation");
      } else if (PN->Name == "prop_blood_translation") {
        VPropDef &P = Lst.NewProp(PROP_BloodTranslation, PN);
        P.SetField(Lst.Class, "BloodColor");
        P.SetField2(Lst.Class, "BloodTranslation");
      } else if (PN->Name == "prop_blood_type") {
        VPropDef &P = Lst.NewProp(PROP_BloodType, PN);
        P.SetField(Lst.Class, "BloodType");
        P.SetField2(Lst.Class, "BloodSplatterType");
      } else if (PN->Name == "prop_stencil_color") {
        VPropDef &P = Lst.NewProp(PROP_StencilColor, PN);
        P.SetField(Lst.Class, "StencilColor");
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
      } else if (PN->Name == "prop_powerup_color") {
        VPropDef &P = Lst.NewProp(PROP_PowerupColor, PN);
        P.SetField(Lst.Class, "BlendColor");
      } else if (PN->Name == "prop_color_range") {
        VPropDef &P = Lst.NewProp(PROP_ColorRange, PN);
        P.SetField(Lst.Class, "TranslStart");
        P.SetField2(Lst.Class, "TranslEnd");
      } else if (PN->Name == "prop_damage_screen_color") {
        VPropDef &P = Lst.NewProp(PROP_DamageScreenColor, PN);
        P.SetField(Lst.Class, "DamageScreenColor");
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
      } else if (PN->Name == "prop_activation") {
        VPropDef &P = Lst.NewProp(PROP_Activation, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "prop_poison_damage") {
        VPropDef &P = Lst.NewProp(PROP_PoisonDamage, PN);
        P.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "flag") {
        VFlagDef &F = Lst.NewFlag(FLAG_Bool, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "flag_inverted") {
        VFlagDef &F = Lst.NewFlag(FLAG_BoolInverted, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
      } else if (PN->Name == "flag_byte") {
        VFlagDef &F = Lst.NewFlag(FLAG_Byte, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
        F.BTrue = DefParseIntConst(PN, "true_value");
        F.BFalse = DefParseIntConst(PN, "false_value");
      } else if (PN->Name == "flag_float") {
        VFlagDef &F = Lst.NewFlag(FLAG_Float, PN);
        F.SetField(Lst.Class, *PN->GetAttribute("property"));
        F.FTrue = DefParseFloatConst(PN, "true_value");
        F.FFalse = DefParseFloatConst(PN, "false_value");
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
      } else if (PN->Name == "limitsub_cvar") {
        //NewPropLimitSubCvar(*PN->GetAttribute("")
      } else if (PN->Name == "limitsub_int") {
      } else {
        Sys_Error("Unknown XML node %s", *PN->Name);
      }
    }
  }
}


//==========================================================================
//
//  GetClassDropItems
//
//==========================================================================
static TArray<VDropItemInfo> &GetClassDropItems (VClass *Class) {
  VField *F = Class->FindFieldChecked("DropItemList");
  return *(TArray<VDropItemInfo>*)F->GetFieldPtr((VObject *)Class->Defaults);
}


//==========================================================================
//
//  GetClassDamageFactors
//
//==========================================================================
static TArray<VDamageFactor> &GetClassDamageFactors (VClass *Class) {
  VField *F = Class->FindFieldChecked("DamageFactors");
  return *(TArray<VDamageFactor>*)F->GetFieldPtr((VObject *)Class->Defaults);
}


//==========================================================================
//
//  GetClassPainChances
//
//==========================================================================
static TArray<VPainChanceInfo> &GetClassPainChances (VClass *Class) {
  VField *F = Class->FindFieldChecked("PainChances");
  return *(TArray<VPainChanceInfo>*)F->GetFieldPtr((VObject *)Class->Defaults);
}


//==========================================================================
//
//  GetClassDamageColors
//
//==========================================================================
static TArray<VDamageColorType> &GetClassDamageColors (VClass *Class) {
  VField *F = Class->FindFieldChecked("DamageColors");
  return *(TArray<VDamageColorType>*)F->GetFieldPtr((VObject *)Class->Defaults);
}


//==========================================================================
//
//  AddClassFixup
//
//==========================================================================
static void AddClassFixup (VClass *Class, VName FieldName, VStr ClassName, TArray<VClassFixup> &ClassFixups, VStr powerpfx=VStr()) {
  VField *F = Class->FindFieldChecked(FieldName);
  //fprintf(stderr, "AddClassFixup0: Class=<%s>; FieldName=<%s>, ClassName=<%s>\n", (Class ? *Class->GetFullName() : "None"), *FieldName, *ClassName);
  VClassFixup &CF = ClassFixups.Alloc();
  CF.Offset = F->Ofs;
  CF.Name = ClassName;
  CF.ReqParent = F->Type.Class;
  CF.Class = Class;
  CF.PowerPrefix = powerpfx;
}


//==========================================================================
//
//  AddClassFixup
//
//==========================================================================
static void AddClassFixup (VClass *Class, VField *Field, VStr ClassName, TArray<VClassFixup> &ClassFixups, VStr powerpfx=VStr()) {
  //fprintf(stderr, "AddClassFixup1: Class=<%s>; FieldName=<%s>, ClassName=<%s>\n", (Class ? *Class->GetFullName() : "None"), *Field->GetFullName(), *ClassName);
  VClassFixup &CF = ClassFixups.Alloc();
  CF.Offset = Field->Ofs;
  CF.Name = ClassName;
  CF.ReqParent = Field->Type.Class;
  CF.Class = Class;
  CF.PowerPrefix = powerpfx;
}


// ////////////////////////////////////////////////////////////////////////// //
#include "vc_decorate_expr.cpp"


//==========================================================================
//
//  ParseConst
//
//==========================================================================
static void ParseConst (VScriptParser *sc, VMemberBase *parent, bool changeMode) {
  if (changeMode) sc->SetCMode(true);

  bool isInt = false;
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
    VEmitContext ec(parent);
    if (parent->MemberType == MEMBER_Class) ec.SelfClass = (VClass *)parent;
    Expr = Expr->Resolve(ec);
    if (Expr) {
      if (isInt) {
        int Val;
        if (Expr->IsFloatConst()) {
               if (Expr->GetFloatConst() < -0x3fffffff) Val = -0x3fffffff;
          else if (Expr->GetFloatConst() > 0x3fffffff) Val = 0x3fffffff;
          else Val = (int)Expr->GetFloatConst();
          if (!vcWarningsSilenced) GCon->Logf(NAME_Warning, "%s: some mo...dder cannot put a proper float type to a constant `%s`; %g truncated to %d", *sc->GetLoc().toStringNoCol(), *Name, Expr->GetFloatConst(), Val);
        } else {
          if (!Expr->IsIntConst()) sc->Error(va("%s: expected integer literal", *sc->GetLoc().toStringNoCol()));
          Val = Expr->GetIntConst();
        }
        //GCon->Logf("*** INT CONST '%s' is %d", *Name, Val);
        delete Expr;
        Expr = nullptr;
        VConstant *C = new VConstant(*Name, parent, Loc);
        C->Type = TYPE_Int;
        C->Value = Val;
      } else {
        if (!Expr->IsFloatConst() && !Expr->IsIntConst()) sc->Error(va("%s: expected float literal", *sc->GetLoc().toStringNoCol()));
        float Val = (Expr->IsFloatConst() ? Expr->GetFloatConst() : (float)Expr->GetIntConst());
        //GCon->Logf("*** FLOAT CONST '%s' is %g", *Name, Val);
        delete Expr;
        Expr = nullptr;
        VConstant *C = new VConstant(*Name, parent, Loc);
        C->Type = TYPE_Float;
        C->FloatValue = Val;
      }
    }
  }
  sc->Expect(";");

  if (changeMode) sc->SetCMode(false);
}


//==========================================================================
//
//  ParseEnum
//
//==========================================================================
static void ParseEnum (VScriptParser *sc, VMemberBase *parent, bool changeMode) {
  if (changeMode) sc->SetCMode(true);

  //GLog.Logf("Enum");
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
        VEmitContext ec(parent);
        eval = eval->Resolve(ec);
        if (eval && !eval->IsIntConst()) sc->Error(va("%s: expected integer literal", *sc->GetLoc().toStringNoCol()));
        if (eval) {
          currValue = eval->GetIntConst();
          delete eval;
        }
      }
    }
    // create constant
    VConstant *C = new VConstant(*Name, parent, Loc);
    C->Type = TYPE_Int;
    C->Value = currValue;
    ++currValue;
    if (!sc->Check(",")) {
      sc->Expect("}");
      break;
    }
  }
  sc->Check(";");

  if (changeMode) sc->SetCMode(false);
}




//==========================================================================
//
//  RegisterDecorateMethods
//
//==========================================================================
static void RegisterDecorateMethods () {
#if 0
  VClass::ForEachClass([](VClass *cls) -> FERes {
    /*
    for (auto it = cls->MethodMap.first(); it; ++it) {
      VName mtname = it.getKey();
      if (mtname == NAME_None) continue;
      VMethod *mt = it.getValue();
      if (!mt) continue; // just in case
      if ((mt->Flags&FUNC_Decorate) == 0) continue;
      VStr mts = VStr(mtname);
      // only register "decorate_" counterpart if we have no decorate method without that prefix
      // also, remove prefix in this case
      if (mts.startsWith("decorate_")) {
        VStr shortname = mts.mid(9, mts.length());
        VName xn = VName(*shortname, VName::Find);
        if (xn != NAME_None) {
          auto mtx = cls->MethodMap.find(xn);
          if (mtx) {
            if (((*mtx)->Flags&FUNC_Decorate) == 0) {
              GLog.Logf(NAME_Warning, "%s: decorate method `%s` has non-decorate VC counterpart at `%s`", *mt->Loc.toString(), *mt->GetFullName(), *(*mtx)->Loc.toString());
            } else {
              continue;
            }
          }
        }
        mts = shortname;
      }
      VDecorateStateAction &A = cls->DecorateStateActions.Alloc();
      A.Name = VName(*mts, VName::AddLower);
      A.Method = mt;
      //GLog.Logf(NAME_Debug, "%s: found decorate method `%s` (%s)", cls->GetName(), mt->GetName(), *A.Name);
    }
    */
    /*
    for (auto &&mt : cls->Methods) {
      if (!mt || mt->Name == NAME_None) continue;
      if ((mt->Flags&FUNC_Decorate) == 0) continue;
      //if (!actname.strEquCI(*mt->name)) continue;
      return mt;
    }
    */
    return FERes::FOREACH_NEXT;
  });
#endif
}


//==========================================================================
//
//  ParseActionDef
//
//==========================================================================
static void ParseActionDef (VScriptParser *sc, VClass *Class) {
  // parse definition
  sc->Expect("native");
  // find the method: first try with decorate_ prefix, then without
  sc->ExpectIdentifier();
  if (!Class->FindDecorateStateAction(sc->String)) {
    sc->Error(va("Decorate method `%s` not found in class `%s`", *sc->String, Class->GetName()));
  }
  /*
  VMethod *M = Class->FindMethod(va("decorate_%s", *sc->String));
  if (!M) M = Class->FindMethod(*sc->String);
  if (!M) sc->Error(va("Method `%s` not found in class `%s`", *sc->String, Class->GetName()));
  VDecorateStateAction &A = Class->DecorateStateActions.Alloc();
  A.Name = *sc->String.ToLower();
  A.Method = M;
  */
  // skip arguments, right now I don't care bout them
  sc->Expect("(");
  while (!sc->Check(")")) sc->ExpectString();
  sc->Expect(";");
}


//==========================================================================
//
//  ParseActionAlias
//
//==========================================================================
/*
static void ParseActionAlias (VScriptParser *sc, VClass *Class) {
  // parse alias
  sc->ExpectIdentifier();
  VStr newname = sc->String;
  sc->Expect("=");
  sc->ExpectIdentifier();
  VStr oldname = sc->String;
  VMethod *M = Class->FindMethod(va("decorate_%s", *oldname));
  if (!M) M = Class->FindMethod(*oldname);
  if (!M) sc->Error(va("Method `%s` not found in class `%s`", *oldname, Class->GetName()));
  //GLog.Logf("***ALIAS: <%s> -> <%s>", *newname, *oldname);
  VDecorateStateAction &A = Class->DecorateStateActions.Alloc();
  A.Name = *newname.ToLower();
  A.Method = M;
  sc->Expect(";");
}
*/


/*
//==========================================================================
//
//  ParseFieldAlias
//
//==========================================================================
static void ParseFieldAlias (VScriptParser *sc, VClass *Class) {
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
}
*/


//==========================================================================
//
//  ParseClass
//
//==========================================================================
static void ParseClass (VScriptParser *sc) {
  sc->SetCMode(true);
  // get class name and find the class
  sc->ExpectString();
  VClass *Class = VClass::FindClass(*sc->String);
  if (!Class) sc->Error("Class not found");
  // I don't care about parent class name because in k8vavoom it can be different
  sc->Expect("extends");
  sc->ExpectString();
  sc->Expect("native");
  sc->Expect("{");
  while (!sc->Check("}")) {
         if (sc->Check("action")) ParseActionDef(sc, Class);
    //else if (sc->Check("alias")) ParseActionAlias(sc, Class);
    //else if (sc->Check("field")) ParseFieldAlias(sc, Class);
    else sc->Error(va("Unknown class property '%s'", *sc->String));
  }
  sc->SetCMode(false);
}


//==========================================================================
//
//  BuildFlagName
//
//==========================================================================
static VStr BuildFlagName (VStr ClassFilter, VStr Flag) {
  if (ClassFilter.isEmpty()) return Flag;
  return ClassFilter+"."+Flag;
}


//==========================================================================
//
//  ParseFlag
//
//==========================================================================
static bool ParseFlag (VScriptParser *sc, VClass *Class, bool Value, TArray<VClassFixup> &ClassFixups) {
  auto floc = sc->GetLoc(); // for warnings
  // get full name of the flag
  sc->ExpectIdentifier();
  VStr ClassFilter;
  VStr Flag = sc->String;
  if (sc->Check(".")) {
    ClassFilter = Flag;
    sc->ExpectIdentifier();
    Flag = sc->String;
  }

  //sorry!
  //if (ClassFilter.strEquCI("POWERSPEED")) return true; //k8: why?
  if (ClassFilter.strEquCI("Player")) ClassFilter = "PlayerPawn";

  //GCon->Logf(NAME_Debug, "*** ParseFlag: Class=%s; <%s> (%s) (cf=%s)", Class->GetName(), *sc->String, *Flag, *ClassFilter);
  for (auto &&ClassDef : FlagList) {
    //VFlagList &ClassDef = FlagList[j];
    //GCon->Logf(NAME_Debug, "   +++ Class=%s; Flag=%s; ClassDef=%s; childof=%d", Class->GetName(), *Flag, ClassDef.Class->GetName(), (int)Class->IsChildOf(ClassDef.Class));
    if (!Class->IsChildOf(ClassDef.Class)) continue;
    if (!ClassFilter.isEmpty() && !ClassFilter.strEquCI(*ClassDef.Class->Name)) continue;
    VFlagDef *fdef = ClassDef.FindFlag(Flag);
    if (!fdef) continue;
    VObject *DefObj = (VObject *)Class->Defaults;
    switch (fdef->Type) {
      case FLAG_Bool: fdef->Field->SetBool(DefObj, Value); break;
      case FLAG_BoolInverted: fdef->Field->SetBool(DefObj, !Value); break;
      case FLAG_Unsupported: if (!vcWarningsSilenced && (fdef->ShowWarning || dbg_show_decorate_unsupported)) GLog.Logf(NAME_Warning, "%s: Unsupported flag %s in %s", *floc.toStringNoCol(), *BuildFlagName(ClassFilter, Flag), Class->GetName()); break;
      case FLAG_Byte: fdef->Field->SetByte(DefObj, Value ? fdef->BTrue : fdef->BFalse); break;
      case FLAG_Float: fdef->Field->SetFloat(DefObj, Value ? fdef->FTrue : fdef->FFalse); break;
      case FLAG_Name: fdef->Field->SetNameValue(DefObj, Value ? fdef->NTrue : fdef->NFalse); break;
      case FLAG_Class: AddClassFixup(Class, fdef->Field, (Value ? *fdef->NTrue : *fdef->NFalse), ClassFixups); break;
      case FLAG_NoClip: fdef->Field->SetBool(DefObj, !Value); fdef->Field2->SetBool(DefObj, !Value); break;
    }
    //if (fdef->Type == FLAG_Bool) GCon->Logf(NAME_Debug, "ParseFlag: %s: {%s}=%d", Class->GetName(), *fdef->Name, (int)fdef->Field->GetBool(DefObj));
    return true;
  }

  if (decorate_fail_on_unknown) {
    sc->Error(va("Unknown flag \"%s\"", *BuildFlagName(ClassFilter, Flag)));
    return false;
  }

  if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: Unknown flag \"%s\"", *floc.toStringNoCol(), *BuildFlagName(ClassFilter, Flag));
  return true;
}


//==========================================================================
//
//  ParseStateString
//
//==========================================================================
static VStr ParseStateString (VScriptParser *sc) {
  VStr StateStr;

  if (!sc->CheckQuotedString()) sc->ExpectIdentifier();
  StateStr = sc->String;

  if (sc->Check("::")) {
    sc->ExpectIdentifier();
    StateStr += "::";
    StateStr += sc->String;
  }

  while (sc->Check(".")) {
    sc->ExpectIdentifier();
    StateStr += ".";
    StateStr += sc->String;
  }

  return StateStr;
}


//==========================================================================
//
//  RemapOldLabel
//
//==========================================================================
static VStr RemapOldLabel (VStr name) {
  if (name.strEquCI("XDeath")) return VStr("Death.Extreme");
  if (name.strEquCI("Burn")) return VStr("Death.Fire");
  if (name.strEquCI("Ice")) return VStr("Death.Ice");
  if (name.strEquCI("Disintegrate")) return VStr("Death.Disintegrate");
  return name;
}


//==========================================================================
//
//  CanBeSpriteNameWorker
//
//  called after the base sprite or command read
//
//==========================================================================
static bool CanBeSpriteNameWorker (VScriptParser *sc, bool asGoto) {
  // get argument
  if (!sc->GetString()) return false;
  //GCon->Logf(NAME_Debug, "asGoto=%d: <%s>", (int)asGoto, *sc->String.quote(true));
  if (sc->String == "}" || sc->String.startsWith(":") || sc->String.startsWith(".")) return false;
  if (sc->IsAtEol()) return false; // no frames -- not a sprite line
  if (!asGoto) {
    // check for valid frame names
    for (int f = 0; f < sc->String.length(); ++f) {
      char FChar = VStr::ToUpper(sc->String[f]);
      if (FChar == '#') continue;
      if (FChar < 'A' || FChar > '^') return false;
    }
    // there should be a duration
    if (!sc->GetString()) return false;
    return !(sc->String == "}" || sc->String.startsWith(":") || sc->String.startsWith("."));
  } else {
    if (!sc->GetString()) return false;
    return !(sc->String == "}" || sc->String.startsWith(":") || sc->String.startsWith(".") || sc->String.startsWith("+"));
  }
}


//==========================================================================
//
//  CanBeSpriteName
//
//  called after the base sprite or command read
//
//==========================================================================
static bool CanBeSpriteName (VScriptParser *sc, bool asGoto=false) {
  if (sc->IsAtEol()) return false; // no way
  auto sp = sc->SavePos();
  const bool res = CanBeSpriteNameWorker(sc, asGoto);
  sc->RestorePos(sp);
  return res;
}


static TArray<VScriptParser *> ParseStatesStack;


//==========================================================================
//
//  ClearStatesStack
//
//==========================================================================
/*
static void ClearStatesStack () {
  for (auto &&sc : ParseStatesStack) delete sc;
  ParseStatesStack.resetNoDtor();
}
*/


//==========================================================================
//
//  ParseStates
//
//==========================================================================
static bool ParseStates (VScriptParser *sc, VClass *Class, TArray<VState*> &States) {
  VState *PrevState = nullptr; // previous state in current execution chain
  VState *LastState = nullptr; // last defined state (`nullptr` right after new label)
  VState *LoopStart = nullptr; // state with last defined label (used to resolve `loop`)
  int NewLabelsStart = Class->StateLabelDefs.length(); // first defined, but not assigned label index

  VStr LastDefinedLabel; // to workaround another ZDoom idiocity

  //FIXME: this may leak!
  ParseStatesStack.resetNoDtor();

  // goto that follows goto should be ignored
  bool lastWasGoto = false;

  sc->Expect("{");
  // disable escape sequences in states
  sc->SetEscape(false);
  while (!sc->Check("}")) {
    if (sc->Check("stateinclude")) {
      if (ParseStatesStack.length() > 32) sc->Error("too many state includes");
      sc->ExpectString();
      int Lump = /*W_CheckNumForFileName*/W_CheckNumForFileNameInSameFile(mainDecorateLump, sc->String);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && sc->String.Length() <= 8 && sc->String.IndexOf('/') < 0) {
        if (mainDecorateLump < 0) {
          Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
        } else {
          Lump = W_CheckNumForNameInFile(VName(*sc->String, VName::AddLower8), W_LumpFile(mainDecorateLump));
        }
      }
      if (Lump < 0) sc->Error(va("Lump %s not found", *sc->String));
      //ParseDecorate(new VScriptParser(/*sc->String*/W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassFixups, newWSlots);
      //GCon->Logf(NAME_Debug, "*** state include: %s", *W_FullLumpName(Lump));
      VScriptParser *nsp = new VScriptParser(/*sc->String*/W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump));
      dcTotalSourceSize += nsp->GetScriptSize();
      ParseStatesStack.append(sc);
      sc = nsp;
      sc->SetCMode(true);
      sc->SetEscape(false);
      continue;
    }

    if (!sc->GetString()) {
      if (ParseStatesStack.length() == 0) sc->Error("unexpected decorate eof");
      delete sc;
      sc = ParseStatesStack[ParseStatesStack.length()-1];
      ParseStatesStack[ParseStatesStack.length()-1] = nullptr;
      ParseStatesStack.setLength(ParseStatesStack.length()-1);
      //GCon->Logf(NAME_Debug, "*** restored %s", *sc->GetScriptName());
      continue;
    }
    sc->UnGet();

    TLocation TmpLoc = sc->GetLoc();
    VStr TmpName = ParseStateString(sc);

    // check for label
    if (sc->Check(":")) {
      LastState = nullptr;
      // allocate new label (it will be resolved later)
      VStateLabelDef &Lbl = Class->StateLabelDefs.Alloc();
      Lbl.Loc = TmpLoc;
      Lbl.Name = TmpName;
      if (!sc->Crossed && sc->Check(";")) {}
      LastDefinedLabel = TmpName;
      continue;
    }

    // goto command
    if (TmpName.ICmp("Goto") == 0) {
      if (sc->IsAtEol()) sc->Error(va("`%s` without argument!", *TmpName));
      // check as "goto"
      if (!CanBeSpriteName(sc, true)) {
        VName GotoLabel = *ParseStateString(sc);
        int GotoOffset = 0;
        if (sc->Check("+")) {
          sc->ExpectNumber();
          GotoOffset = sc->Number;
        }
        if (lastWasGoto && LastState) {
          GLog.Logf(NAME_Error, "%s: `Goto` follows `Goto`/`Stop`/`Loop`, ignored.", *TmpLoc.toStringNoCol());
          if (!sc->Crossed && sc->Check(";")) {}
          continue;
        }
        lastWasGoto = true;
        // some degenerative mod authors do this
        if (LastState && GotoOffset == 0 && VStr::strEquCI(*GotoLabel, "Fail")) {
          if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: fixed `Goto Fail`, mod author is a mo...dder.", *TmpLoc.toStringNoCol());
          vassert(LastState);
          LastState->NextState = LastState;
          PrevState = nullptr; // new execution chain
        } else {
          if (!LastState && NewLabelsStart == Class->StateLabelDefs.length()) {
            if (!getIgnoreMoronicStateCommands()) sc->Error("'Goto' before first state frame");
            sc->Message("'Goto' before first state frame");
            NewLabelsStart = Class->StateLabelDefs.length();
          } else {
            // if we have no defined states for latest labels, create dummy state to attach gotos to it
            // simple redirection won't work, because this label can be used as `A_JumpXXX()` destination, for example
            // sigh... k8
            if (!LastState) {
              // yet if we are in spawn label, demand at least one defined state
              // ah, screw it, just define TNT1
              VState *dummyState = new VState(va("S_%d", States.length()), Class, TmpLoc);
              States.Append(dummyState);
              dummyState->SpriteName = "tnt1";
              dummyState->Frame = 0|VState::FF_SKIPOFFS|VState::FF_SKIPMODEL|VState::FF_DONTCHANGE|VState::FF_KEEPSPRITE;
              dummyState->Time = 0;
              // link previous state
              if (PrevState) PrevState->NextState = dummyState;
              // assign state to the labels
              for (int i = NewLabelsStart; i < Class->StateLabelDefs.length(); ++i) {
                Class->StateLabelDefs[i].State = dummyState;
                LoopStart = dummyState; // this will replace loop start only if we have any labels
              }
              NewLabelsStart = Class->StateLabelDefs.length(); // no current label
              PrevState = dummyState;
              LastState = dummyState;
            }
            LastState->GotoLabel = GotoLabel;
            LastState->GotoOffset = GotoOffset;
            vassert(NewLabelsStart == Class->StateLabelDefs.length());
            /*k8: this doesn't work, see above
            for (int i = NewLabelsStart; i < Class->StateLabelDefs.length(); ++i) {
              Class->StateLabelDefs[i].GotoLabel = GotoLabel;
              Class->StateLabelDefs[i].GotoOffset = GotoOffset;
            }
            NewLabelsStart = Class->StateLabelDefs.length(); // no current label
            */
          }
          PrevState = nullptr; // new execution chain
        }
        if (!sc->Crossed && sc->Check(";")) {}
        continue;
      } else {
        sc->Message(va("`%s` sprite name is not recommended!", *TmpName));
      }
    }

    // stop command
    if (TmpName.ICmp("Stop") == 0) {
      if (CanBeSpriteName(sc)) {
        sc->Message(va("`%s` sprite name is not recommended!", *TmpName));
      } else {
        if (lastWasGoto && LastState) {
          GLog.Logf(NAME_Error, "%s: `Stop` follows `Goto`/`Stop`/`Loop`, ignored.", *TmpLoc.toStringNoCol());
          if (!sc->Crossed && sc->Check(";")) {}
          continue;
        }
        lastWasGoto = true;
        if (!LastState && NewLabelsStart == Class->StateLabelDefs.length()) {
          if (!getIgnoreMoronicStateCommands()) sc->Error("'Stop' before first state frame");
          sc->Message("'Stop' before first state frame");
        } else {
          // see above for the reason to introduce this dummy state
          // nope, zdoom wiki says that this should make state "invisible"
          //FIXME: for now, this is not working right (it seems; need to be checked!)
          if (!LastState) {
            if (!vcWarningsSilenced && cli_ShowRemoveStateWarning > 0) {
              GLog.Logf(NAME_Warning, "%s: Empty state detected; this may not work as you expect!", *TmpLoc.toStringNoCol());
              GLog.Logf(NAME_Warning, "%s:   this will remove a state, not an actor!", *TmpLoc.toStringNoCol());
              GLog.Logf(NAME_Warning, "%s:   you can use k8vavoom-specific `RemoveState` command to get rid of this warning.", *TmpLoc.toStringNoCol());
            }
          }
          if (LastState) LastState->NextState = nullptr;

          // if we have no defined states for latest labels, simply redirect labels to nowhere
          // this will make `FindStateLabel()` to return `nullptr`, effectively removing the state
          for (int i = NewLabelsStart; i < Class->StateLabelDefs.length(); ++i) {
            if (!vcWarningsSilenced && cli_ShowRemoveStateWarning > 0) GLog.Logf(NAME_Warning, "%s: removed state '%s'!", *TmpLoc.toStringNoCol(), *Class->StateLabelDefs[i].Name);
            Class->StateLabelDefs[i].State = nullptr;
          }
        }
        NewLabelsStart = Class->StateLabelDefs.length(); // no current label

        vassert(NewLabelsStart == Class->StateLabelDefs.length());
        PrevState = nullptr; // new execution chain
        if (!sc->Crossed && sc->Check(";")) {}
        continue;
      }
    }

    // special "removestate" command (k8vavoom specific)
    // this removes the inherited state
    if (TmpName.ICmp("RemoveState") == 0) {
      if (LastState) sc->Error("cannot remove non-empty state");
      for (int i = NewLabelsStart; i < Class->StateLabelDefs.length(); ++i) {
        //GLog.Logf(NAME_Init, "%s: removed state '%s'", *TmpLoc.toStringNoCol(), *Class->StateLabelDefs[i].Name);
        Class->StateLabelDefs[i].State = nullptr;
      }
      NewLabelsStart = Class->StateLabelDefs.length(); // no current label

      vassert(NewLabelsStart == Class->StateLabelDefs.length());
      PrevState = nullptr; // new execution chain
      if (!sc->Crossed && sc->Check(";")) {}
      continue;
    }

    // wait command
    if (TmpName.ICmp("Wait") == 0 || TmpName.ICmp("Fail") == 0) {
      if (CanBeSpriteName(sc)) {
        sc->Message(va("`%s` sprite name is not recommended!", *TmpName));
      } else {
        if (lastWasGoto && LastState) {
          GLog.Logf(NAME_Error, "%s: `Wait`/`Fail` follows `Goto`/`Stop`/`Loop`, ignored.", *TmpLoc.toStringNoCol());
          if (!sc->Crossed && sc->Check(";")) {}
          continue;
        }
        lastWasGoto = true;
        if (!LastState) {
          if (!getIgnoreMoronicStateCommands()) sc->Error(va("'%s' before first state frame", *TmpName));
          sc->Message(va("'%s' before first state frame", *TmpName));
        } else {
          LastState->NextState = LastState;
        }
        PrevState = nullptr; // new execution chain
        if (!sc->Crossed && sc->Check(";")) {}
        continue;
      }
    }

    // loop command
    if (TmpName.ICmp("Loop") == 0) {
      if (CanBeSpriteName(sc)) {
        sc->Message(va("`%s` sprite name is not recommended!", *TmpName));
      } else {
        if (lastWasGoto && LastState) {
          GLog.Logf(NAME_Error, "%s: `Loop` follows `Goto`/`Stop`/`Loop`, ignored.", *TmpLoc.toStringNoCol());
          if (!sc->Crossed && sc->Check(";")) {}
          continue;
        }
        lastWasGoto = true;
        if (!LastState) {
          if (!getIgnoreMoronicStateCommands()) sc->Error("'Loop' before first state frame");
          sc->Message("'Loop' before first state frame");
        } else {
          LastState->NextState = LoopStart;
        }
        PrevState = nullptr; // new execution chain
        if (!sc->Crossed && sc->Check(";")) {}
        continue;
      }
    }

    // this is definitely not a goto
    lastWasGoto = false;

    // create new state
    VState *State = new VState(va("S_%d", States.length()), Class, TmpLoc);
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
    } else if (FChar < 'A' || FChar > '^') {
      if (FChar < 33 || FChar > 127) {
        sc->Error(va("Frames must be A-Z, [, \\ or ], got <0x%02x>", (vuint32)(FChar&0xff)));
      } else {
        sc->Error(va("Frames must be A-Z, [, \\ or ], got <%c>", FChar));
      }
    } else {
      State->Frame = FChar-'A';
    }
    if (keepSpriteBase) State->Frame |= VState::FF_KEEPSPRITE;

    // tics
    // `random(a, b)`?
    if (sc->Check("random")) {
      sc->Expect("(");
      sc->ExpectNumberWithSign();
      // this is what GZDoom does
      if (sc->Number < -1 || sc->Number > 32767) {
        if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: random state min duration out of rande (%d)!", *TmpLoc.toStringNoCol(), sc->Number);
        sc->Number = clampval(sc->Number, -1, 32767);
      }
      State->Arg1 = sc->Number;
      sc->Expect(",");
      sc->ExpectNumberWithSign();
      // this is what GZDoom does
      if (sc->Number < -1 || sc->Number > 32767) {
        if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: random state min duration out of rande (%d)!", *TmpLoc.toStringNoCol(), sc->Number);
        sc->Number = clampval(sc->Number, -1, 32767);
      }
      State->Arg2 = sc->Number;
      // this is what GZDoom does
      if (State->Arg1 > State->Arg2) {
        if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: random state min/max duration is in invalid order!", *TmpLoc.toStringNoCol());
        const int tmp = State->Arg1;
        State->Arg1 = State->Arg2;
        State->Arg2 = tmp;
      }
      sc->Expect(")");
      State->Time = float(State->Arg1)/35.0f;
      State->TicType = VState::TCK_Random;
    } else {
      // number
      sc->ExpectNumberWithSign();
      if (sc->Number < 0) {
        //State->Time = sc->Number;
        if (sc->Number != -1) {
          if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: negative state duration %d (only -1 is allowed)!", *TmpLoc.toStringNoCol(), sc->Number);
        }
        State->Time = -1;
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
        if (!LastState) {
          // there were no states after the label, insert dummy one
          VState *dupState = new VState(va("S_%d", States.length()), Class, TmpLoc);
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
          for (int i = NewLabelsStart; i < Class->StateLabelDefs.length(); ++i) {
            Class->StateLabelDefs[i].State = State;
            LoopStart = State;
          }
          NewLabelsStart = Class->StateLabelDefs.length(); // no current label
          PrevState = State;
          LastState = State;
          // and use duplicate state as a new state
          State = dupState;
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
      if (sc->Check("Debug")) { State->Frame |= VState::FF_SKIPOFFS; continue; } //k8: this state will be ignored in offset calculations

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

    // link previous state
    if (PrevState) PrevState->NextState = State;

    // assign state to the labels
    for (int i = NewLabelsStart; i < Class->StateLabelDefs.length(); ++i) {
      Class->StateLabelDefs[i].State = State;
      LoopStart = State; // this will replace loop start only if we have any labels
    }
    NewLabelsStart = Class->StateLabelDefs.length(); // no current label
    PrevState = State;
    LastState = State;

    vassert(!State->funcIsCopy);
    for (int i = 1; i < FramesString.Length(); ++i) {
      vint32 frm = (State->Frame&~(VState::FF_FRAMEMASK|VState::FF_DONTCHANGE|VState::FF_SKIPOFFS));

      char FSChar = VStr::ToUpper(FramesString[i]);
      if (totalKeepSprite || FSChar == '#') {
        // frame letter is irrelevant
        frm = VState::FF_DONTCHANGE;
      } else if (FSChar < 'A' || FSChar > '^') {
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
      VState *s2 = new VState(va("S_%d", States.length()), Class, sc->GetLoc());
      States.Append(s2);
      s2->SpriteName = State->SpriteName;
      s2->Frame = frm;
      s2->Time = State->Time;
      s2->TicType = State->TicType;
      s2->Arg1 = State->Arg1;
      s2->Arg2 = State->Arg2;
      s2->Misc1 = State->Misc1;
      s2->Misc2 = State->Misc2;
      // no need to perform syntax copy here
      s2->Function = State->Function;
      s2->FunctionName = State->FunctionName;
      s2->funcIsCopy = true;
      s2->LightName = State->LightName;

      // link previous state
      PrevState->NextState = s2;
      PrevState = s2;
      LastState = s2;
    }
  }
  // re-enable escape sequences
  sc->SetEscape(true);

  // idiotic ZDoom allows end labeled code with nothing, and automatically routes
  // its last state to the next label. sigh.
  if (LastState && !LastState->NextState && PrevState && !LastDefinedLabel.isEmpty()) {
    LastDefinedLabel = RemapOldLabel(LastDefinedLabel);
    // find this label in the parent class, and route the state to the next label there
    bool found = false;
    for (VClass *pc = Class->GetSuperClass(); pc; pc = pc->GetSuperClass()) {
      for (auto &&lbl : pc->StateLabelDefs.itemsIdx()) {
        //GCon->Logf(NAME_Debug, "  %s: %s (%s)", pc->GetName(), *lbl.value().Name, (lbl.value().State ? *lbl.value().State->Loc.toStringNoCol() : "<none>"));
        if (RemapOldLabel(lbl.value().Name).strEquCI(LastDefinedLabel)) {
          found = true;
          if (lbl.index()+1 >= pc->StateLabelDefs.length()) {
            //GCon->Log(NAME_Error, "*** cannot fix this shit, sorry.");
          } else {
            GCon->Logf(NAME_Error, "%s: found ZDoom incomplete state at label %s; workarounded.", *LastState->Loc.toStringNoCol(), *LastDefinedLabel);
            VStateLabelDef &lnext = pc->StateLabelDefs[lbl.index()+1];
            if (lnext.State) {
              GCon->Logf(NAME_Debug, "  %s: %s (%s)", pc->GetName(), *lnext.Name, (lnext.State ? *lnext.State->Loc.toStringNoCol() : "<none>"));
              LastState->NextState = lnext.State;
            } else {
              GCon->Logf(NAME_Error, "*** %s: %s -- dead state", pc->GetName(), *lnext.Name);
            }
          }
          break;
        }
      }
      if (found) break;
    }
  }

  if (ParseStatesStack.length() != 0) sc->Error("unexpected states completion in state include");
  return true;
}


//==========================================================================
//
//  ParseParentState
//
//  This is for compatibility with old WADs.
//
//==========================================================================
static void ParseParentState (VScriptParser *sc, VClass *Class, const char *LblName) {
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

  sc->CheckNumberWithSign();
  while (sc->Check(";")) {}
  sc->Expect("{");

  while (!sc->Check("}")) {
    if (sc->QuotedString) {
      // skip this crap
      sc->GetString();
      continue;
    }

    if (!sc->Check("var")) {
      // not a var, skip until ';'
      // nope, just skip a keyword
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
      VStr uvname = VStr(fldname);
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

  //if (uvars.length()) for (int f = 0; f < uvars.length(); ++f) GLog.Logf("DC: <%s>", *uvars[f]);
}


//==========================================================================
//
//  isBDWActorClassName
//
//==========================================================================
static bool isBDWActorClassName (VStr cname) {
  return
    cname.startsWithCI("BDW_") ||
    cname.startsWithCI("K8BDW_");
}


//==========================================================================
//
//  ParseActor
//
//==========================================================================
static void ParseActor (VScriptParser *sc, TArray<VClassFixup> &ClassFixups, TArray<VWeaponSlotFixups> &newWSlots) {
  // actor options
  bool optionalActor = false;
  auto cstloc = sc->GetLoc();

  bool bloodTranslationSet = false;
  vuint32 bloodColor = 0;

  // parse actor name
  // in order to allow dots in actor names, this is done in non-C mode,
  // so we have to do a little bit more complex parsing
  for (;;) {
    sc->ExpectString();
    if (sc->String.strEquCI("(optional)")) {
      optionalActor = true;
      continue;
    }
    break;
  }

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

  if (getDecorateDebug()) sc->DebugMessage(va("Parsing class `%s`", *NameStr));

  VClass *DupCheck = VClass::FindClassNoCase(*NameStr);
  if (DupCheck != nullptr && DupCheck->MemberType == MEMBER_Class) {
    GLog.Logf(NAME_Warning, "%s: Redeclared class `%s` (old at %s)", *cstloc.toStringNoCol(), *NameStr, *DupCheck->Loc.toStringNoCol());
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

  if (decorateSkipBDWClasses && isBDWActorClassName(NameStr)) {
    sc->Message(va("skipping BDW class `%s`...", *NameStr));
    sc->SkipBracketed(false); // bracket is not eaten
    return;
  }

  if (IsNoOverrideName(NameStr)) {
    sc->Message(va("skipping class `%s` due to override protection", *NameStr));
    sc->SkipBracketed(false); // bracket is not eaten
    return;
    //NameStr += "_k8NoOverride";
  }
  if (GlobalDisableOverride) AddNoOverrideName(NameStr);

  VClass *ParentClass = ActorClass;
  if (ParentStr.IsNotEmpty()) {
    ParentClass = VClass::FindClassNoCase(*ParentStr);
    if (ParentClass == nullptr || ParentClass->MemberType != MEMBER_Class) {
      if (optionalActor) {
        sc->DebugMessage(va("Skipping optional actor `%s`", *NameStr));
        ParentClass = nullptr; // just in case
      } else if (CheckParentErrorHacks(sc, NameStr, ParentStr)) {
        ParentClass = nullptr; // ignore it
      } else if (cli_DecorateLaxParents) {
        sc->Message(va("Parent class `%s` not found for actor `%s`, ignoring actor", *ParentStr, *NameStr));
        ParentClass = nullptr; // just in case
      } else {
        sc->Error(va("Parent class `%s` not found", *ParentStr));
      }
    }
    if (ParentClass != nullptr && !ParentClass->IsChildOf(ActorClass)) {
      if (cli_DecorateLaxParents) {
        sc->Message(va("Parent class `%s` is not an actor class", *ParentStr));
        ParentClass = nullptr; // just in case
      } else {
        sc->Error(va("Parent class `%s` is not an actor class", *ParentStr));
      }
    }
    // skip this actor if it has invalid parent
    if (ParentClass == nullptr) {
      sc->SkipBracketed(false); // bracket is not eaten
      return;
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
    //GLog.Logf("*: '%s'", *NameStr);
    auto sc2 = sc->clone();
    ScanActorDefForUserVars(sc2, uvars);
    delete sc2;
  }

  if (getDecorateDebug()) sc->DebugMessage(va("Creating derived class `%s` from `%s`", *NameStr, ParentClass->GetName()));
  VClass *Class = ParentClass->CreateDerivedClass(*NameStr, DecPkg, uvars, sc->GetLoc());
  uvars.clear(); // we don't need it anymore
  DecPkg->ParsedClasses.Append(Class);

  if (Class) {
    // copy class fixups of the parent class
    for (int i = 0; i < ClassFixups.length(); ++i) {
      VClassFixup *CF = &ClassFixups[i];
      if (CF->Class == ParentClass) {
        VClassFixup &NewCF = ClassFixups.Alloc();
        CF = &ClassFixups[i]; // array can be resized, so update cache
        NewCF.Offset = CF->Offset;
        NewCF.Name = CF->Name;
        NewCF.ReqParent = CF->ReqParent;
        NewCF.Class = Class;
        NewCF.PowerPrefix = VStr();
      }
    }
  }


  VClass *ReplaceeClass = nullptr;
  if (sc->Check("replaces")) {
    sc->ExpectString();
    ReplaceeClass = VClass::FindClassNoCase(*sc->String);
    if (ReplaceeClass && IsNoOverrideName(sc->String)) {
      sc->Message(va("skipping class `%s` replacement (%s) due to override protection", *NameStr, *sc->String));
      ReplaceeClass = nullptr;
    } else {
      if (ReplaceeClass == nullptr || ReplaceeClass->MemberType != MEMBER_Class) {
        // D4V (and other mods) hacks
        bool ignoreReplaceError = (optionalActor || CheckReplaceErrorHacks(sc, NameStr, ParentStr, sc->String));
        if (cli_DecorateLaxParents || ignoreReplaceError) {
          ReplaceeClass = nullptr; // just in case
          if (!ignoreReplaceError) sc->Message(va("Replaced class `%s` not found for actor `%s` : `%s`", *sc->String, *NameStr, *ParentStr));
        } else {
          sc->Error(va("Replaced class `%s` not found for actor `%s` : `%s`", *sc->String, *NameStr, *ParentStr));
        }
      }
      if (ReplaceeClass != nullptr && !ReplaceeClass->IsChildOf(ActorClass)) {
        if (cli_DecorateNonActorReplace) {
          ReplaceeClass = nullptr; // just in case
          sc->Message(va("Replaced class `%s` is not an actor class", *sc->String));
        } else {
          sc->Error(va("Replaced class `%s` is not an actor class", *sc->String));
        }
      }
      /*
      if (ReplaceeClass != nullptr && !ReplaceeClass->IsChildOf(ParentClass)) {
        ReplaceeClass = nullptr; // just in case
        sc->Message(va("Replaced class `%s` is not a child of `%s`", *sc->String, ParentClass->GetName()));
      }
      */
      // skip this actor if it has invalid replacee
      if (ReplaceeClass == nullptr) {
        sc->SkipBracketed(false); // bracket is not eaten
        return;
      }
    }
  }

  // time to switch to the C mode
  sc->SetCMode(true);

  int GameFilter = 0;
  int DoomEdNum = -1;
  int SpawnNum = -1;
  TArray<VState *> States;
  bool DropItemsDefined = false;
  VObject *DefObj = (VObject *)Class->Defaults;

  if (sc->CheckNumberWithSign()) {
    if (sc->Number < -1 || sc->Number > 32767) sc->Error("DoomEdNum is out of range [-1, 32767]");
    DoomEdNum = sc->Number;
  }

  while (sc->Check(";")) {}
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

    /*
    if (sc->Check("alias")) {
      ParseActionAlias(sc, Class);
      continue;
    }
    */

    if (sc->Check("const")) {
      ParseConst(sc, Class, false); // don't touch scanner mode
      continue;
    }

    if (sc->Check("enum")) {
      ParseEnum(sc, Class, false); // don't touch scanner mode
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
    //VName PropName = *Prop.ToLower();
    bool FoundProp = false;
    for (int j = 0; j < FlagList.length() && !FoundProp; ++j) {
      VFlagList &ClassDef = FlagList[j];
      if (!Class->IsChildOf(ClassDef.Class)) continue;
      VPropDef *pdef = ClassDef.FindProp(Prop);
      if (!pdef) continue;
      switch (pdef->Type) {
        case PROP_Int:
          sc->ExpectNumberWithSign();
          pdef->Field->SetInt(DefObj, sc->Number);
          break;
        case PROP_IntTrunced:
          sc->ExpectFloatWithSign();
          pdef->Field->SetInt(DefObj, (int)sc->Float);
          break;
        case PROP_IntConst:
          pdef->Field->SetInt(DefObj, pdef->IConst);
          break;
        case PROP_IntBobPhase:
          sc->ExpectNumberWithSign();
          if (sc->Number < 0) {
            pdef->Field->SetFloat(DefObj, -1.0f);
          } else {
            pdef->Field->SetFloat(DefObj, (sc->Number&63)/35.0f);
          }
          break;
        case PROP_IntUnsupported:
          //FIXME
          sc->CheckNumberWithSign();
          if (!vcWarningsSilenced && (pdef->ShowWarning || dbg_show_decorate_unsupported)) GLog.Logf(NAME_Warning, "%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
          break;
        case PROP_IntIdUnsupported:
          //FIXME
          {
            //bool oldcm = sc->IsCMode();
            //sc->SetCMode(true);
            sc->CheckNumberWithSign();
            sc->Expect(",");
            sc->ExpectIdentifier();
            if (sc->Check(",")) sc->ExpectIdentifier();
            //sc->SetCMode(oldcm);
            if (!vcWarningsSilenced && (pdef->ShowWarning || dbg_show_decorate_unsupported)) GLog.Logf(NAME_Warning, "%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
          }
          break;
        case PROP_BitIndex:
          sc->ExpectNumber();
          pdef->Field->SetInt(DefObj, 1<<(sc->Number-1));
          break;
        case PROP_Float:
          sc->ExpectFloatWithSign();
          pdef->Field->SetFloat(DefObj, sc->Float);
          break;
        case PROP_FloatUnsupported:
          //FIXME
          sc->ExpectFloatWithSign();
          if (!vcWarningsSilenced && (pdef->ShowWarning || dbg_show_decorate_unsupported)) GLog.Logf(NAME_Warning, "%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
          break;
        case PROP_Speed:
          sc->ExpectFloatWithSign();
          // check for speed powerup
          if (PowerSpeedClass && Class->IsChildOf(PowerSpeedClass)) {
            pdef->Field->SetFloat(DefObj, sc->Float);
          } else {
            pdef->Field->SetFloat(DefObj, sc->Float*35.0f);
          }
          break;
        case PROP_SpeedMult:
          sc->ExpectFloatWithSign();
          pdef->Field->SetFloat(DefObj, sc->Float);
          break;
        case PROP_Tics:
          sc->ExpectNumberWithSign();
          pdef->Field->SetFloat(DefObj, (sc->Number > 0 ? sc->Number/35.0f : -sc->Number));
          break;
        case PROP_TicsSecs:
          sc->ExpectNumberWithSign();
          pdef->Field->SetFloat(DefObj, sc->Number >= 0 ? sc->Number/35.0f : sc->Number);
          break;
        case PROP_Percent:
          sc->ExpectFloat();
          pdef->Field->SetFloat(DefObj, midval(0.0f, (float)sc->Float, 100.0f)/100.0f);
          break;
        case PROP_FloatClamped:
          sc->ExpectFloatWithSign();
          pdef->Field->SetFloat(DefObj, midval(pdef->FMin, (float)sc->Float, pdef->FMax));
          break;
        case PROP_FloatClamped2:
          sc->ExpectFloatWithSign();
          pdef->Field->SetFloat(DefObj, midval(pdef->FMin, (float)sc->Float, pdef->FMax));
          pdef->Field2->SetFloat(DefObj, midval(pdef->FMin, (float)sc->Float, pdef->FMax));
          break;
        case PROP_FloatOpt2:
          sc->ExpectFloat();
          if (decoIgnorePlayerSpeed && (pdef->Field->Name == "ForwardMove1" || pdef->Field->Name == "SideMove1")) {
            GCon->Log(NAME_Warning, "ignored playerpawn movement speed property by user request");
            if (sc->Check(",")) sc->ExpectFloat(); else (void)sc->CheckFloat();
          } else {
            pdef->Field->SetFloat(DefObj, sc->Float);
            pdef->Field2->SetFloat(DefObj, sc->Float);
            if (sc->Check(",")) {
              sc->ExpectFloat();
              pdef->Field2->SetFloat(DefObj, sc->Float);
            } else if (sc->CheckFloat()) {
              pdef->Field2->SetFloat(DefObj, sc->Float);
            }
          }
          break;
        case PROP_Name:
          sc->ExpectString();
          pdef->Field->SetNameValue(DefObj, *sc->String);
          break;
        case PROP_NameLower:
          sc->ExpectString();
          pdef->Field->SetNameValue(DefObj, VName(*sc->String, VName::AddLower));
          break;
        case PROP_Str:
          sc->ExpectString();
          pdef->Field->SetStr(DefObj, sc->String);
          break;
        case PROP_StrUnsupported:
          //FIXME
          sc->ExpectString();
          if (!vcWarningsSilenced && (pdef->ShowWarning || dbg_show_decorate_unsupported)) GLog.Logf(NAME_Warning, "%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
          break;
        case PROP_Class:
          sc->ExpectString();
          AddClassFixup(Class, pdef->Field, pdef->CPrefix+sc->String, ClassFixups);
          break;
        case PROP_PoisonDamage:
          sc->ExpectNumberWithSign();
          pdef->Field->SetInt(DefObj, sc->Number);
          if (sc->Check(",")) {
            if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: Additional arguments to property '%s' in '%s' are not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
            for (;;) {
              sc->ExpectNumberWithSign();
              if (!sc->Check(",")) break;
            }
          }
          break;
        case PROP_Power_Class:
          // This is a very inconvenient shit!
          // but ZDoom had to prepend "power" to the name...
          sc->ExpectString();
          /*
          if (!sc->String.toLowerCase().StartsWith("power")) {
            GLog.Logf("*** POWERUP TYPE(0): <%s>", *sc->String);
          } else {
            GLog.Logf("*** POWERUP TYPE(1): <%s>", *(pdef->CPrefix+sc->String));
          }
          AddClassFixup(Class, pdef->Field, sc->String.StartsWith("Power") || sc->String.StartsWith("power") ?
              sc->String : pdef->CPrefix+sc->String, ClassFixups);
          */
          AddClassFixup(Class, pdef->Field, sc->String, ClassFixups, pdef->CPrefix);
          break;
        case PROP_BoolConst:
          pdef->Field->SetBool(DefObj, pdef->IConst);
          break;
        case PROP_State:
          ParseParentState(sc, Class, *pdef->PropName);
          break;
        case PROP_Game:
          {
            const vuint32 ngf = SC_ParseGameDef(sc);
            if (!ngf) sc->Error("Unknown game filter");
            GameFilter |= ngf;
          }
          break;
        case PROP_SpawnId:
          if (sc->CheckNumber()) {
            SpawnNum = sc->Number;
          } else {
            sc->ExpectString();
            if (sc->String.length() != 0) sc->Error("'spawnid' should be a number!");
            if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: 'spawnid' should be a number, not an empty string! FIX YOUR BROKEN CODE!", *sc->GetLoc().toStringNoCol());
            SpawnNum = 0;
          }
          break;
        case PROP_ConversationId:
          sc->ExpectNumber();
          pdef->Field->SetInt(DefObj, sc->Number);
          if (sc->Check(",")) {
            sc->ExpectNumberWithSign();
            sc->Expect(",");
            sc->ExpectNumberWithSign();
          }
          break;
        case PROP_PainChance:
          if (sc->CheckNumber()) {
            pdef->Field->SetFloat(DefObj, float(sc->Number)/256.0f);
          } else {
            sc->ExpectString();
            VName DamageType = (sc->String.ICmp("Normal") == 0 ? /*NAME_None*/VName("None") : VName(*sc->String));
            sc->Expect(",");
            sc->ExpectNumber();

            // check pain chances array for replacements
            TArray<VPainChanceInfo> &PainChances = GetClassPainChances(Class);
            VPainChanceInfo *PC = nullptr;
            for (int i = 0; i < PainChances.length(); ++i) {
              if (PainChances[i].DamageType == DamageType) {
                PC = &PainChances[i];
                break;
              }
            }
            if (!PC) {
              PC = &PainChances.Alloc();
              PC->DamageType = DamageType;
            }
            PC->Chance = float(sc->Number)/256.0f;
          }
          break;
        case PROP_DamageFactor:
          {
            auto loc = sc->GetLoc();

            VName DamageType = /*NAME_None*/VName("None");
            // Check if we only have a number instead of a string, since
            // there are some custom WAD files that don't specify a DamageType,
            // but specify a DamageFactor
            if (!sc->CheckFloatWithSign()) {
              sc->ExpectString();
              DamageType = (sc->String.ICmp("Normal") == 0 ? /*NAME_None*/VName("None") : VName(*sc->String));
              sc->Expect(",");
              sc->ExpectFloatWithSign();
            }

            // check damage factors array for replacements
            TArray<VDamageFactor> &DamageFactors = GetClassDamageFactors(Class);
            VDamageFactor *DF = nullptr, *ncDF = nullptr;
            for (int i = 0; i < DamageFactors.length(); ++i) {
              if (DamageFactors[i].DamageType == DamageType) {
                DF = &DamageFactors[i];
                break;
              }
              if (VStr::ICmp(*DamageFactors[i].DamageType, *DamageType) == 0) {
                ParseWarning(loc, "DamageFactor case mismatch: looking for '%s', got '%s'", *DamageType, *DamageFactors[i].DamageType);
                if (!ncDF) ncDF = &DamageFactors[i];
              }
            }

            if (!DF && ncDF) DF = ncDF;
            if (!DF) {
              DF = &DamageFactors.Alloc();
              DF->DamageType = DamageType;
            }
            DF->Factor = sc->Float;
            /*
            GCon->Logf(NAME_Warning, "*** class '%s': damage factor '%s', value is %g", Class->GetName(), *DamageType, sc->Float);
            for (i = 0; i < DamageFactors.length(); ++i) GCon->Logf(NAME_Debug, "Actor '%s': damage factor #%d: name=<%s>; factor=%g", Class->GetName(), i, *DamageFactors[i].DamageType, DamageFactors[i].Factor);
            */
          }
          break;
        case PROP_MissileDamage:
          if (sc->Check("(")) {
            VExpression *Expr = ParseExpression(sc, Class);
            if (!Expr) {
              ParseError(sc->GetLoc(), "Damage expression expected");
            } else {
              Expr = new VScalarToInt(Expr); // not resolved
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
            pdef->Field->SetInt(DefObj, sc->Number);
          }
          break;
        case PROP_VSpeed:
          {
            sc->ExpectFloatWithSign();
            TVec Val = pdef->Field->GetVec(DefObj);
            Val.z = sc->Float*35.0f;
            pdef->Field->SetVec(DefObj, Val);
          }
          break;
        case PROP_RenderStyle:
          {
            int RenderStyle = STYLE_Normal;
                 if (sc->Check("None")) RenderStyle = STYLE_None;
            else if (sc->Check("Normal")) RenderStyle = STYLE_Normal;
            else if (sc->Check("Fuzzy")) RenderStyle = STYLE_Fuzzy;
            else if (sc->Check("SoulTrans")) RenderStyle = STYLE_SoulTrans;
            else if (sc->Check("OptFuzzy")) RenderStyle = STYLE_OptFuzzy;
            else if (sc->Check("Translucent")) RenderStyle = STYLE_Translucent;
            else if (sc->Check("Add")) RenderStyle = STYLE_Add;
            else if (sc->Check("Stencil")) RenderStyle = STYLE_Stencil;
            else if (sc->Check("AddStencil")) RenderStyle = STYLE_AddStencil;
            else if (sc->Check("Subtract")) RenderStyle = STYLE_Subtract;
            else if (sc->Check("Shaded")) RenderStyle = STYLE_Shaded;
            else if (sc->Check("AddShaded")) RenderStyle = STYLE_AddShaded;
            else if (sc->Check("Shadow")) RenderStyle = STYLE_Shadow;
            // special k8vavoom style
            else if (sc->Check("Dark")) RenderStyle = STYLE_Dark;
            else {
              GCon->Logf(NAME_Error, "%s: invalid render style '%s' in '%s'", *prloc.toStringNoCol(), *sc->String, Class->GetName());
              //sc->Error("Bad render style");
            }
            pdef->Field->SetByte(DefObj, RenderStyle);
          }
          break;
        case PROP_DefaultAlpha: // heretic should have 0.6, but meh...
          pdef->Field->SetFloat(DefObj, 0.6f);
          break;
        case PROP_Translation:
          pdef->Field->SetInt(DefObj, R_ParseDecorateTranslation(sc, (GameFilter&GAME_Strife ? 7 : 3)));
          break;
        case PROP_BloodColor:
          bloodColor = sc->ExpectColor();
          pdef->Field->SetInt(DefObj, bloodColor);
          if (!bloodTranslationSet) pdef->Field2->SetInt(DefObj, R_GetBloodTranslation(bloodColor, true));
          break;
        case PROP_BloodTranslation:
          bloodTranslationSet = true;
          //k8: don't clear color, so we could be able to set it separately
          //pdef->Field->SetInt(DefObj, 0); // clear color
          pdef->Field2->SetInt(DefObj, R_ParseDecorateTranslation(sc, (GameFilter&GAME_Strife ? 7 : 3))); // set translation
          break;
        case PROP_BloodType:
          sc->ExpectString();
          AddClassFixup(Class, pdef->Field, sc->String, ClassFixups);
          if (sc->Check(",")) sc->ExpectString();
          AddClassFixup(Class, pdef->Field2, sc->String, ClassFixups);
          if (sc->Check(",")) sc->ExpectString();
          AddClassFixup(Class, "AxeBloodType", sc->String, ClassFixups);
          break;
        case PROP_StencilColor:
          {
            vuint32 Col = sc->ExpectColor();
            pdef->Field->SetInt(DefObj, Col);
          }
          break;
        case PROP_Monster:
          Class->SetFieldBool("bShootable", true);
          Class->SetFieldBool("bCountKill", true);
          Class->SetFieldBool("bSolid", true);
          Class->SetFieldBool("bActivatePushWall", true);
          Class->SetFieldBool("bActivateMCross", true);
          Class->SetFieldBool("bPassMobj", true);
          Class->SetFieldBool("bMonster", true);
          Class->SetFieldBool("bCanUseWalls", true);
          break;
        case PROP_Projectile:
          Class->SetFieldBool("bNoBlockmap", true);
          Class->SetFieldBool("bNoGravity", true);
          Class->SetFieldBool("bDropOff", true);
          Class->SetFieldBool("bMissile", true);
          Class->SetFieldBool("bActivateImpact", true);
          Class->SetFieldBool("bActivatePCross", true);
          Class->SetFieldBool("bNoTeleport", true);
          if (GGameInfo->Flags&VGameInfo::GIF_DefaultBloodSplatter) Class->SetFieldBool("bBloodSplatter", true);
          break;
        case PROP_BounceType:
          if (sc->Check("None")) {
            Class->SetFieldByte("BounceType", BOUNCE_None);
          } else if (sc->Check("Doom")) {
            Class->SetFieldByte("BounceType", BOUNCE_Doom);
            Class->SetFieldBool("bBounceWalls", true);
            Class->SetFieldBool("bBounceFloors", true);
            Class->SetFieldBool("bBounceCeilings", true);
            Class->SetFieldBool("bBounceOnActors", true);
            Class->SetFieldBool("bBounceAutoOff", true);
          } else if (sc->Check("Heretic")) {
            Class->SetFieldByte("BounceType", BOUNCE_Heretic);
            Class->SetFieldBool("bBounceFloors", true);
            Class->SetFieldBool("bBounceCeilings", true);
          } else if (sc->Check("Hexen")) {
            Class->SetFieldByte("BounceType", BOUNCE_Hexen);
            Class->SetFieldBool("bBounceWalls", true);
            Class->SetFieldBool("bBounceFloors", true);
            Class->SetFieldBool("bBounceCeilings", true);
            Class->SetFieldBool("bBounceOnActors", true);
          } else if (sc->Check("DoomCompat")) {
            Class->SetFieldByte("BounceType", BOUNCE_Doom);
          } else if (sc->Check("HereticCompat")) {
            Class->SetFieldByte("BounceType", BOUNCE_Heretic);
          } else if (sc->Check("HexenCompat")) {
            Class->SetFieldByte("BounceType", BOUNCE_Hexen);
          } else if (sc->Check("Grenade")) {
            // bounces on walls and flats like ZDoom bounce
            Class->SetFieldByte("BounceType", BOUNCE_Doom);
            Class->SetFieldBool("bBounceOnActors", false);
          } else if (sc->Check("Classic")) {
            // bounces on flats only, but does not die when bouncing
            Class->SetFieldByte("BounceType", BOUNCE_Heretic);
            Class->SetFieldBool("bMBFBounce", true);
          }
          break;
        case PROP_ClearFlags:
          for (int jj = 0; jj < FlagList.length(); ++jj) {
            if (FlagList[jj].Class != ActorClass) continue;
            for (int i = 0; i < FlagList[jj].Flags.length(); ++i) {
              VFlagDef &F = FlagList[jj].Flags[i];
              switch (F.Type) {
                case FLAG_Bool: F.Field->SetBool(DefObj, false); break;
              }
            }
          }
          Class->SetFieldByte("BounceType", BOUNCE_None);
          Class->SetFieldBool("bColideWithThings", true);
          Class->SetFieldBool("bColideWithWorld", true);
          Class->SetFieldBool("bPickUp", false);
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
            DI.Chance = 1.0f;
            bool HaveChance = false;
            if (sc->Check(",")) {
              sc->ExpectNumber();
              HaveChance = true;
            } else {
              HaveChance = sc->CheckNumber();
            }
            if (HaveChance) {
              DI.Chance = float(sc->Number)/255.0f;
              if (sc->Check(",")) {
                sc->ExpectNumberWithSign();
                DI.Amount = max2(0, sc->Number);
              } else if (sc->CheckNumberWithSign()) {
                DI.Amount = max2(0, sc->Number);
              }
            }
            if (DI.TypeName == "none" || DI.TypeName == NAME_None) {
              GetClassDropItems(Class).Clear();
            } else {
              GetClassDropItems(Class).Insert(0, DI);
            }
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
            ActorClass->DeepCopyObject(Class->Defaults, ActorClass->Defaults);
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
          for (int i = 0; i < 5; ++i) {
            sc->ExpectNumber();
            pdef->Field->SetInt(DefObj, sc->Number, i);
            if (i < 4 && !sc->Check(",")) break;
          }
          pdef->Field2->SetBool(DefObj, true);
          break;
        case PROP_LowMessage:
          sc->ExpectNumber();
          pdef->Field->SetInt(DefObj, sc->Number);
          sc->Expect(",");
          sc->ExpectString();
          pdef->Field2->SetStr(DefObj, sc->String);
          break;
        case PROP_PowerupColor:
          {
            bool fuckfloat = true;
                 if (sc->Check("InverseMap")) pdef->Field->SetInt(DefObj, INVERSECOLOR);
            else if (sc->Check("GoldMap")) pdef->Field->SetInt(DefObj, GOLDCOLOR);
            else if (sc->Check("RedMap")) pdef->Field->SetInt(DefObj, REDCOLOR);
            else if (sc->Check("BerserkRedMap")) pdef->Field->SetInt(DefObj, BEREDCOLOR);
            else if (sc->Check("GreenMap")) pdef->Field->SetInt(DefObj, GREENCOLOR);
            else if (sc->Check("MonoMap")) pdef->Field->SetInt(DefObj, MONOCOLOR);
            else if (sc->Check("MonochromeMap")) pdef->Field->SetInt(DefObj, MONOCOLOR);
            else if (sc->Check("BlueMap")) pdef->Field->SetInt(DefObj, BLUECOLOR);
            else {
              fuckfloat = false;
              vuint32 Col = sc->ExpectColor();
              int r = (Col>>16)&255;
              int g = (Col>>8)&255;
              int b = Col&255;
              int a = 88; // default alpha, around 0.(3)
              sc->Check(",");
              // alpha may be missing
              if (!sc->Crossed) {
                sc->ExpectFloat();
                     if (sc->Float <= 0) a = 1;
                else if (sc->Float >= 1) a = 255;
                else a = clampToByte((int)(sc->Float*255));
                if (a > 250) a = 250;
                if (a < 1) a = 1;
              }
              pdef->Field->SetInt(DefObj, (r<<16)|(g<<8)|b|(a<<24));
            }
            if (fuckfloat) {
              sc->Check(",");
              if (!sc->Crossed) sc->ExpectFloat();
            }
          }
          break;
        case PROP_ColorRange:
          sc->ExpectNumber();
          pdef->Field->SetInt(DefObj, sc->Number);
          sc->Check(",");
          sc->ExpectNumber();
          pdef->Field2->SetInt(DefObj, sc->Number);
          break;
        case PROP_DamageScreenColor:
          //FIXME: Player.DamageScreenColor color[, intensity[, damagetype]]
          {
            vuint32 Col = sc->ExpectColor();
            float Intensity = 1.0f;
            VStr dmgType;
            // intensity
            if (sc->Check(",")) {
              sc->ExpectFloat();
              Intensity = clampval(sc->Float, 0.0f, 1.0f);
              // damage type
              if (sc->Check(",")) {
                sc->ExpectString();
                if (sc->String.length() && !sc->String.strEquCI("None")) dmgType = sc->String;
              }
            }
            // set damage
            if (dmgType.length()) {
              // custom damage type
              TArray<VDamageColorType> &dclist = GetClassDamageColors(Class);
              int fidx = -1;
              for (int f = 0; f < dclist.length(); ++f) if (dmgType.strEquCI(*dclist[f].Type)) { fidx = f; break; }
              if (fidx < 0) {
                fidx = dclist.length();
                dclist.alloc();
                vassert(fidx == dclist.length()-1);
                dclist[fidx].Type = VName(*dmgType);
              }
              dclist[fidx].Color = Col;
              dclist[fidx].Intensity = Intensity;
            } else {
              // default
              pdef->Field->SetInt(DefObj, Col);
            }
          }
          break;
        case PROP_HexenArmor:
          sc->ExpectFloat();
          pdef->Field->SetFloat(DefObj, sc->Float, 0);
          sc->Expect(",");
          sc->ExpectFloat();
          pdef->Field->SetFloat(DefObj, sc->Float, 1);
          sc->Expect(",");
          sc->ExpectFloat();
          pdef->Field->SetFloat(DefObj, sc->Float, 2);
          sc->Expect(",");
          sc->ExpectFloat();
          pdef->Field->SetFloat(DefObj, sc->Float, 3);
          sc->Expect(",");
          sc->ExpectFloat();
          pdef->Field->SetFloat(DefObj, sc->Float, 4);
          break;
        case PROP_StartItem:
          {
            TArray<VDropItemInfo> &DropItems = *(TArray<VDropItemInfo>*)pdef->Field->GetFieldPtr(DefObj);
            if (!DropItemsDefined) {
              DropItems.Clear();
              DropItemsDefined = true;
            }
            sc->ExpectString();
            VDropItemInfo DI;
            DI.TypeName = *sc->String.ToLower();
            DI.Type = nullptr;
            DI.Amount = 0;
            DI.Chance = 1.0f;
            if (sc->Check(",")) {
              sc->ExpectNumber();
              DI.Amount = sc->Number;
              if (DI.Amount == 0) DI.Amount = -666; //k8:hack!
            } else if (sc->CheckNumber()) {
              DI.Amount = sc->Number;
              if (DI.Amount == 0) DI.Amount = -666; //k8:hack!
            }
            if (DI.TypeName == "none" || DI.TypeName == NAME_None) {
              DropItems.Clear();
            } else {
              DropItems.Insert(0, DI);
            }
          }
          break;
        case PROP_MorphStyle:
          if (sc->CheckNumber()) {
            pdef->Field->SetInt(DefObj, sc->Number);
          } else {
            // WANING! keep in sync with "EntityEx.Morph.vc"!
            enum {
              MORPH_ADDSTAMINA          = 0x00000001, // player has a "power" instead of a "curse" (add stamina instead of limiting to health)
              MORPH_FULLHEALTH          = 0x00000002, // player uses new health semantics (!POWER => MaxHealth of animal, POWER => Normal health behaviour)
              MORPH_UNDOBYTOMEOFPOWER   = 0x00000004, // player unmorphs upon activating a Tome of Power
              MORPH_UNDOBYCHAOSDEVICE   = 0x00000008, // player unmorphs upon activating a Chaos Device
              MORPH_FAILNOTELEFRAG      = 0x00000010, // player stays morphed if unmorph by Tome of Power fails
              MORPH_FAILNOLAUGH         = 0x00000020, // player doesn't laugh if unmorph by Chaos Device fails
              MORPH_WHENINVULNERABLE    = 0x00000040, // player can morph (or scripted unmorph) when invulnerable but ONLY if doing it to themselves
              MORPH_LOSEACTUALWEAPON    = 0x00000080, // player loses specified morph weapon only (not "whichever they have when unmorphing")
              MORPH_NEWTIDBEHAVIOUR     = 0x00000100, // actor TID is by default transferred from the old actor to the new actor
              MORPH_UNDOBYDEATH         = 0x00000200, // actor unmorphs when killed and (unless MORPH_UNDOBYDEATHSAVES) stays dead
              MORPH_UNDOBYDEATHFORCED   = 0x00000400, // actor (if unmorphed when killed) forces unmorph (not very useful with UNDOBYDEATHSAVES)
              MORPH_UNDOBYDEATHSAVES    = 0x00000800, // actor (if unmorphed when killed) regains their health and doesn't die
              MORPH_UNDOALWAYS          = 0x00001000, // ignore unmorph blocking conditions (not implemented)
              MORPH_TRANSFERTRANSLATION = 0x00002000, // transfers the actor's translation to the morphed actor (applies to players and monsters) (not implemented)
            };

            bool HaveParen = sc->Check("(");
            int Val = 0;
            do {
                   if (sc->Check("MRF_ADDSTAMINA")) Val |= MORPH_ADDSTAMINA;
              else if (sc->Check("MRF_FULLHEALTH")) Val |= MORPH_FULLHEALTH;
              else if (sc->Check("MRF_UNDOBYTOMEOFPOWER")) Val |= MORPH_UNDOBYTOMEOFPOWER;
              else if (sc->Check("MRF_UNDOBYCHAOSDEVICE")) Val |= MORPH_UNDOBYCHAOSDEVICE;
              else if (sc->Check("MRF_FAILNOTELEFRAG")) Val |= MORPH_FAILNOTELEFRAG;
              else if (sc->Check("MRF_FAILNOLAUGH")) Val |= MORPH_FAILNOLAUGH;
              else if (sc->Check("MRF_WHENINVULNERABLE")) Val |= MORPH_WHENINVULNERABLE;
              else if (sc->Check("MRF_LOSEACTUALWEAPON")) Val |= MORPH_LOSEACTUALWEAPON;
              else if (sc->Check("MRF_NEWTIDBEHAVIOUR")) Val |= MORPH_NEWTIDBEHAVIOUR;
              else if (sc->Check("MRF_UNDOBYDEATH")) Val |= MORPH_UNDOBYDEATH;
              else if (sc->Check("MRF_UNDOBYDEATHFORCED")) Val |= MORPH_UNDOBYDEATHFORCED;
              else if (sc->Check("MRF_UNDOBYDEATHSAVES")) Val |= MORPH_UNDOBYDEATHSAVES;
              else if (sc->Check("MRF_UNDOALWAYS")) Val |= MORPH_UNDOALWAYS;
              else if (sc->Check("MRF_TRANSFERTRANSLATION")) Val |= MORPH_TRANSFERTRANSLATION;
              else sc->Error(va("Bad morph style (%s)", *sc->String));
            } while (sc->Check("|"));
            if (HaveParen) sc->Expect(")");
            pdef->Field->SetInt(DefObj, Val);
          }
          break;
        case PROP_PawnWeaponSlot: // Player.WeaponSlot 1, XFist, XChainsaw
          {
            // get slot number
            sc->ExpectNumber();
            int sidx = sc->Number;
            if (!VWeaponSlotFixups::isValidSlot(sidx)) {
              GLog.Logf(NAME_Warning, "%s: invalid weapon slot number %d", *sc->GetLoc().toStringNoCol(), sidx);
              while (sc->Check(",")) sc->ExpectString();
            } else {
              VWeaponSlotFixups &wst = allocWeaponSlotsFor(newWSlots, Class);
              wst.clearSlot(sidx);
              while (sc->Check(",")) {
                sc->ExpectString();
                // filter out special name
                if (!sc->String.strEquCI("Weapon")) {
                  wst.addToSlot(sidx, sc->String);
                }
              }
              // if it is empty, mark it as empty
              if (wst.getSlotWeaponCount(sidx) == 0) wst.addToSlot(sidx, "Weapon"); // this is "empty slot" flag
            }
          }
          break;
        case PROP_Activation:
          {
            int acttype = 0;
            for (;;) {
                   if (sc->Check("THINGSPEC_Default") || sc->Check("AF_Default")) acttype |= THINGSPEC_Default;
              else if (sc->Check("THINGSPEC_ThingActs") || sc->Check("AF_ThingActs")) acttype |= THINGSPEC_ThingActs;
              else if (sc->Check("THINGSPEC_ThingTargets") || sc->Check("AF_ThingTargets")) acttype |= THINGSPEC_ThingTargets;
              else if (sc->Check("THINGSPEC_TriggerTargets") || sc->Check("AF_TriggerTargets")) acttype |= THINGSPEC_TriggerTargets;
              else if (sc->Check("THINGSPEC_MonsterTrigger") || sc->Check("AF_MonsterTrigger")) acttype |= THINGSPEC_MonsterTrigger;
              else if (sc->Check("THINGSPEC_MissileTrigger") || sc->Check("AF_MissileTrigger")) acttype |= THINGSPEC_MissileTrigger;
              else if (sc->Check("THINGSPEC_ClearSpecial") || sc->Check("AF_ClearSpecial")) acttype |= THINGSPEC_ClearSpecial;
              else if (sc->Check("THINGSPEC_NoDeathSpecial") || sc->Check("AF_NoDeathSpecial")) acttype |= THINGSPEC_NoDeathSpecial;
              else if (sc->Check("THINGSPEC_TriggerActs") || sc->Check("AF_TriggerActs")) acttype |= THINGSPEC_TriggerActs;
              else if (sc->Check("THINGSPEC_Activate") || sc->Check("AF_Activate")) acttype |= THINGSPEC_Activate;
              else if (sc->Check("THINGSPEC_Deactivate") || sc->Check("AF_Deactivate")) acttype |= THINGSPEC_Deactivate;
              else if (sc->Check("THINGSPEC_Switch") || sc->Check("AF_Switch")) acttype |= THINGSPEC_Switch;
              else sc->Error(va("Bad activaion type \"%s\"", *sc->String));
              if (!sc->Check("|")) break;
            }
            pdef->Field->SetInt(DefObj, acttype);
          }
          break;
        case PROP_SkipLineUnsupported:
          {
            if (!vcWarningsSilenced && (pdef->ShowWarning || dbg_show_decorate_unsupported)) GLog.Logf(NAME_Warning, "%s: Property '%s' in '%s' is not yet supported", *prloc.toStringNoCol(), *Prop, Class->GetName());
            sc->SkipLine();
          }
          break;
      }
      FoundProp = true;
    }
    //while (sc->Check(";")) {}
    if (FoundProp) continue;

    //k8: sorry for this
    if (Prop.strEquCI("limitwithsubcvar")) {
      sc->ExpectString();
      NewPropLimitSubCvar(Class, sc->String);
      continue;
    }
    if (Prop.strEquCI("limitwithsubint")) {
      sc->ExpectNumber();
      NewPropLimitSubInt(Class, sc->Number);
      continue;
    }

    if (decorate_fail_on_unknown) {
      sc->Error(va("Unknown property \"%s\"", *Prop));
    } else {
      if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: Unknown property \"%s\"", *prloc.toStringNoCol(), *Prop);
    }
    sc->SkipLine();
  }

  sc->SetCMode(false);

  Class->EmitStateLabels();

  // set up linked list of states
  if (States.length()) {
    Class->States = States[0];
    for (int i = 0; i < States.length()-1; ++i) States[i]->Next = States[i+1];
    for (auto &&sts : States) {
      #if defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
      sts->Define(); // this defines state functions
      #endif
      if (sts->GotoLabel != NAME_None) {
        sts->NextState = Class->ResolveStateLabel(sts->Loc, sts->GotoLabel, sts->GotoOffset);
      }
    }
  }

  if (DoomEdNum > 0) {
    /*mobjinfo_t *nfo =*/ VClass::AllocMObjId(DoomEdNum, (GameFilter ? GameFilter : GAME_Any), Class);
    //if (nfo) nfo->Class = Class;
    //GLog.Logf("DECORATE: DoomEdNum #%d assigned to '%s'", DoomEdNum, *Class->GetFullName());
    //VMemberBase::StaticDumpMObjInfo();
  }

  if (SpawnNum > 0) {
    /*mobjinfo_t *nfo =*/ VClass::AllocScriptId(SpawnNum, (GameFilter ? GameFilter : GAME_Any), Class);
    //if (nfo) nfo->Class = Class;
  }

  Class->SetFieldInt("GameFilter", (GameFilter ? GameFilter : -1));

  DoClassReplacement(ReplaceeClass, Class);

  // check for known "any" blood classes if gore mod is active
  if (!ReplaceeClass && cli_GoreMod && !enableKnownBlood) {
    bool blockSpawn = false;
    VStr kbc = DetectKnownBloodClass(Class, &blockSpawn, false);
    if (!kbc.isEmpty()) {
      GLog.Logf(NAME_Warning, "%s: %s known blood class '%s' (%s)", *Class->Loc.toStringNoCol(), (blockSpawn ? "blocked" : "skipped"), Class->GetName(), *kbc);
      if (blockSpawn) BlockedSpawnSet.put(Class->Name, true);
    }
  }

  // also, block spawning of all children of the blocked class
  if (!BlockedSpawnSet.has(Class->Name)) {
    for (VClass *cc = Class; cc; cc = cc->GetSuperClass()) {
      if (BlockedSpawnSet.has(cc->Name)) {
        GLog.Logf(NAME_Warning, "%s: blocked known blood child class '%s' (%s)", *Class->Loc.toStringNoCol(), Class->GetName(), cc->GetName());
        BlockedSpawnSet.put(Class->Name, true);
        break;
      }
    }
  }

  // check for forced replamenet
  if (cli_GoreMod && !enableKnownBlood && !ForceReplacements.has(Class->Name) && !BlockedSpawnSet.has(Class->Name)) {
    VClass *rep = FindKnowBloodForcedReplacement(Class);
    if (rep) {
      GLog.Logf(NAME_Warning, "%s: forced known blood class '%s' replacement (%s)", *Class->Loc.toStringNoCol(), Class->GetName(), rep->GetName());
      ForceReplacements.put(Class->Name, rep);
    }
  }

  /*
  if (ReplaceeClass) {
    if (cli_DecorateOldReplacement) {
      ReplaceeClass->Replacement = Class;
      Class->Replacee = ReplaceeClass;
    } else {
      VClass *repl = ReplaceeClass->GetReplacement();
      if (developer && repl != ReplaceeClass) GLog.Logf(NAME_Dev, "class `%s` replaces `%s` (original requiest is `%s`)", Class->GetName(), repl->GetName(), ReplaceeClass->GetName());
      repl->Replacement = Class;
      Class->Replacee = repl;
    }
  }
  */
}


//==========================================================================
//
//  ParseOldDecStates
//
//==========================================================================
static void ParseOldDecStates (VScriptParser *sc, TArray<VState *> &States, VClass *Class) {
  TArray<VStr> Tokens;
  sc->String.Split(",\t\r\n", Tokens);
  for (int TokIdx = 0; TokIdx < Tokens.length(); ++TokIdx) {
    const char *pFrame = *Tokens[TokIdx];
    int DurColon = Tokens[TokIdx].IndexOf(':');
    float Duration = 4;
    if (DurColon >= 0) {
      Duration = VStr::atoi(pFrame);
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
        States[States.length()-1]->Frame |= VState::FF_FULLBRIGHT;
      } else if (cc < 'A' || cc > '^') {
        sc->Error("Frames must be A-Z, [, \\, or ]");
      } else {
        GotState = true;
        VState *State = new VState(va("S_%d", States.length()), Class, sc->GetLoc());
        States.Append(State);
        State->Frame = cc-'A';
        State->Time = float(Duration)/35.0f;
      }
      ++pFrame;
    }
  }
}


//==========================================================================
//
//  ParseOldDecoration
//
//==========================================================================
static void ParseOldDecoration (VScriptParser *sc, int Type) {
  // get name of the class
  sc->ExpectString();
  VName ClassName = *sc->String;
  bool allowReplace = true;

  if (IsNoOverrideName(sc->String)) {
    ClassName = VName(*(sc->String+"_k8NoOverride"));
    allowReplace = false;
  } else if (GlobalDisableOverride) {
    AddNoOverrideName(sc->String);
  }

  // create class
  TArray<VDecorateUserVarDef> uvars;

  VClass *Class = Type == OLDDEC_Pickup ?
    FakeInventoryClass->CreateDerivedClass(ClassName, DecPkg, uvars, sc->GetLoc()) :
    ActorClass->CreateDerivedClass(ClassName, DecPkg, uvars, sc->GetLoc());
  DecPkg->ParsedClasses.Append(Class);

  if (Type == OLDDEC_Breakable) Class->SetFieldBool("bShootable", true);

  if (Type == OLDDEC_Projectile) {
    Class->SetFieldBool("bMissile", true);
    Class->SetFieldBool("bDropOff", true);
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
  float DeathHeight = 0.0f;
  int BurnStart = 0;
  int BurnEnd = 0;
  bool BurnsAway = false;
  bool SolidOnBurn = false;
  float BurnHeight = 0.0f;
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
      SpawnStart = States.length();
      ParseOldDecStates(sc, States, Class);
      SpawnEnd = States.length();
    } else if ((Type == OLDDEC_Breakable || Type == OLDDEC_Projectile) && sc->Check("DeathSprite")) {
      // death states
      sc->ExpectString();
      if (sc->String.Length() != 4) sc->Error("Sprite name must be 4 characters long");
      DeathSprite = *sc->String.ToLower();
    } else if ((Type == OLDDEC_Breakable || Type == OLDDEC_Projectile) && sc->Check("DeathFrames")) {
      sc->ExpectString();
      DeathStart = States.length();
      ParseOldDecStates(sc, States, Class);
      DeathEnd = States.length();
    } else if (Type == OLDDEC_Breakable && sc->Check("DiesAway")) {
      DiesAway = true;
    } else if (Type == OLDDEC_Breakable && sc->Check("BurnDeathFrames")) {
      sc->ExpectString();
      BurnStart = States.length();
      ParseOldDecStates(sc, States, Class);
      BurnEnd = States.length();
    } else if (Type == OLDDEC_Breakable && sc->Check("BurnsAway")) {
      BurnsAway = true;
    } else if (Type == OLDDEC_Breakable && sc->Check("IceDeathFrames")) {
      sc->ExpectString();
      IceStart = States.length();
      ParseOldDecStates(sc, States, Class);

      // make a copy of the last state for A_FreezeDeathChunks
      VState *State = new VState(va("S_%d", States.length()), Class, sc->GetLoc());
      States.Append(State);
      State->Frame = States[States.length()-2]->Frame;

      IceEnd = States.length();
    } else if (Type == OLDDEC_Breakable && sc->Check("GenericIceDeath")) {
      GenericIceDeath = true;
    }
    // misc properties
    else if (sc->Check("Radius")) {
      sc->ExpectFloat();
      Class->SetFieldFloat("Radius", sc->Float);
      // also, reset render radius
      Class->SetFieldFloat("RenderRadius", 0);
    } else if (sc->Check("Height")) {
      sc->ExpectFloat();
      Class->SetFieldFloat("Height", sc->Float);
    } else if (sc->Check("Mass")) {
      sc->ExpectFloat();
      Class->SetFieldFloat("Mass", sc->Float);
    } else if (sc->Check("Scale")) {
      sc->ExpectFloat();
      Class->SetFieldFloat("ScaleX", sc->Float);
      Class->SetFieldFloat("ScaleY", sc->Float);
    } else if (sc->Check("Alpha")) {
      sc->ExpectFloat();
      Class->SetFieldFloat("Alpha", midval(0.0f, (float)sc->Float, 1.0f));
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
      Class->SetFieldByte("RenderStyle", RenderStyle);
    } else if (sc->Check("Translation1")) {
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > 2) sc->Error("Translation1 is out of range [0, 2]");
      Class->SetFieldInt("Translation", (TRANSL_Standard<<TRANSL_TYPE_SHIFT)+sc->Number);
    } else if (sc->Check("Translation2")) {
      sc->ExpectNumber();
      if (sc->Number < 0 || sc->Number > MAX_LEVEL_TRANSLATIONS) sc->Error(va("Translation2 is out of range [0, %d]", MAX_LEVEL_TRANSLATIONS));
      Class->SetFieldInt("Translation", (TRANSL_Level<<TRANSL_TYPE_SHIFT)+sc->Number);
    }
    // breakable decoration properties
    else if (Type == OLDDEC_Breakable && sc->Check("Health")) {
      sc->ExpectNumber();
      Class->SetFieldInt("Health", sc->Number);
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
      Class->SetFieldNameValue("DeathSound", *sc->String);
    } else if (Type == OLDDEC_Breakable && sc->Check("BurnDeathSound")) {
      sc->ExpectString();
      Class->SetFieldNameValue("ActiveSound", *sc->String);
    }
    // projectile properties
    else if (Type == OLDDEC_Projectile && sc->Check("Speed")) {
      sc->ExpectFloat();
      Class->SetFieldFloat("Speed", sc->Float*35.0f);
    } else if (Type == OLDDEC_Projectile && sc->Check("Damage")) {
      sc->ExpectNumber();
      Class->SetFieldFloat("MissileDamage", sc->Number);
    } else if (Type == OLDDEC_Projectile && sc->Check("DamageType")) {
      if (sc->Check("Normal")) {
        Class->SetFieldNameValue("DamageType", NAME_None);
      } else {
        sc->ExpectString();
        Class->SetFieldNameValue("DamageType", *sc->String);
      }
    } else if (Type == OLDDEC_Projectile && sc->Check("SpawnSound")) {
      sc->ExpectString();
      Class->SetFieldNameValue("SightSound", *sc->String);
    } else if (Type == OLDDEC_Projectile && sc->Check("ExplosionRadius")) {
      sc->ExpectNumber();
      Class->SetFieldFloat("ExplosionRadius", sc->Number);
      Explosive = true;
    } else if (Type == OLDDEC_Projectile && sc->Check("ExplosionDamage")) {
      sc->ExpectNumber();
      Class->SetFieldFloat("ExplosionDamage", sc->Number);
      Explosive = true;
    } else if (Type == OLDDEC_Projectile && sc->Check("DoNotHurtShooter")) {
      Class->SetFieldBool("bExplosionDontHurtSelf", true);
    } else if (Type == OLDDEC_Projectile && sc->Check("DoomBounce")) {
      Class->SetFieldByte("BounceType", BOUNCE_Doom);
    } else if (Type == OLDDEC_Projectile && sc->Check("HereticBounce")) {
      Class->SetFieldByte("BounceType", BOUNCE_Heretic);
    } else if (Type == OLDDEC_Projectile && sc->Check("HexenBounce")) {
      Class->SetFieldByte("BounceType", BOUNCE_Hexen);
    }
    // pickup properties
    else if (Type == OLDDEC_Pickup && sc->Check("PickupMessage")) {
      sc->ExpectString();
      Class->SetFieldStr("PickupMessage", sc->String);
    } else if (Type == OLDDEC_Pickup && sc->Check("PickupSound")) {
      sc->ExpectString();
      Class->SetFieldNameValue("PickupSound", *sc->String);
    } else if (Type == OLDDEC_Pickup && sc->Check("Respawns")) {
      Class->SetFieldBool("bRespawns", true);
    }
    // compatibility flags
    else if (sc->Check("LowGravity")) {
      Class->SetFieldFloat("Gravity", 0.125f);
    } else if (sc->Check("FireDamage")) {
      Class->SetFieldNameValue("DamageType", "Fire");
    }
    // flags
    else if (sc->Check("Solid")) Class->SetFieldBool("bSolid", true);
    else if (sc->Check("NoSector")) Class->SetFieldBool("bNoSector", true);
    else if (sc->Check("NoBlockmap")) Class->SetFieldBool("bNoBlockmap", true);
    else if (sc->Check("SpawnCeiling")) Class->SetFieldBool("bSpawnCeiling", true);
    else if (sc->Check("NoGravity")) Class->SetFieldBool("bNoGravity", true);
    else if (sc->Check("Shadow")) Class->SetFieldBool("bShadow", true);
    else if (sc->Check("NoBlood")) Class->SetFieldBool("bNoBlood", true);
    else if (sc->Check("CountItem")) Class->SetFieldBool("bCountItem", true);
    else if (sc->Check("WindThrust")) Class->SetFieldBool("bWindThrust", true);
    else if (sc->Check("FloorClip")) Class->SetFieldBool("bFloorClip", true);
    else if (sc->Check("SpawnFloat")) Class->SetFieldBool("bSpawnFloat", true);
    else if (sc->Check("NoTeleport")) Class->SetFieldBool("bNoTeleport", true);
    else if (sc->Check("Ripper")) Class->SetFieldBool("bRip", true);
    else if (sc->Check("Pushable")) Class->SetFieldBool("bPushable", true);
    else if (sc->Check("SlidesOnWalls")) Class->SetFieldBool("bSlide", true);
    else if (sc->Check("CanPass")) Class->SetFieldBool("bPassMobj", true);
    else if (sc->Check("CannotPush")) Class->SetFieldBool("bCannotPush", true);
    else if (sc->Check("ThruGhost")) Class->SetFieldBool("bThruGhost", true);
    else if (sc->Check("NoDamageThrust")) Class->SetFieldBool("bNoDamageThrust", true);
    else if (sc->Check("Telestomp")) Class->SetFieldBool("bTelestomp", true);
    else if (sc->Check("FloatBob")) Class->SetFieldBool("bFloatBob", true);
    else if (sc->Check("ActivateImpact")) Class->SetFieldBool("bActivateImpact", true);
    else if (sc->Check("CanPushWalls")) Class->SetFieldBool("bActivatePushWall", true);
    else if (sc->Check("ActivateMCross")) Class->SetFieldBool("bActivateMCross", true);
    else if (sc->Check("ActivatePCross")) Class->SetFieldBool("bActivatePCross", true);
    else if (sc->Check("Reflective")) Class->SetFieldBool("bReflective", true);
    else if (sc->Check("FloorHugger")) Class->SetFieldBool("bIgnoreFloorStep", true);
    else if (sc->Check("CeilingHugger")) Class->SetFieldBool("bIgnoreCeilingStep", true);
    else if (sc->Check("DontSplash")) Class->SetFieldBool("bNoSplash", true);
    else {
      if (decorate_fail_on_unknown) {
        Sys_Error("Unknown property '%s'", *sc->String);
      } else {
        if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "Unknown property '%s'", *sc->String);
      }
      sc->SkipLine();
      continue;
    }
  }

  if (SpawnEnd == 0) sc->Error(va("%s has no Frames definition", *ClassName));
  if (Type == OLDDEC_Breakable && DeathEnd == 0) sc->Error(va("%s has no DeathFrames definition", *ClassName));
  if (GenericIceDeath && IceEnd != 0) sc->Error("IceDeathFrames and GenericIceDeath are mutually exclusive");

  if (allowReplace) {
    if (DoomEdNum > 0) {
      /*mobjinfo_t *nfo =*/ VClass::AllocMObjId(DoomEdNum, (GameFilter ? GameFilter : GAME_Any), Class);
      //if (nfo) nfo->Class = Class;
    }

    if (SpawnNum > 0) {
      /*mobjinfo_t *nfo =*/ VClass::AllocScriptId(SpawnNum, (GameFilter ? GameFilter : GAME_Any), Class);
      //if (nfo) nfo->Class = Class;
    }
  }

  // set up linked list of states
  Class->States = States[0];
  for (int i = 0; i < States.length()-1; ++i) States[i]->Next = States[i+1];

  // set up default sprite for all states
  for (int i = 0; i < States.length(); ++i) States[i]->SpriteName = Sprite;

  // set death sprite if it's defined
  if (DeathSprite != NAME_None && DeathEnd != 0) {
    for (int i = DeathStart; i < DeathEnd; ++i) States[i]->SpriteName = DeathSprite;
  }

  // set up links of spawn states
  if (States.length() == 1) {
    States[SpawnStart]->Time = -1.0f;
  } else {
    for (int i = SpawnStart; i < SpawnEnd-1; ++i) States[i]->NextState = States[i+1];
    States[SpawnEnd-1]->NextState = States[SpawnStart];
  }
  Class->SetStateLabel("Spawn", States[SpawnStart]);

  // set up links of death states
  if (DeathEnd != 0) {
    for (int i = DeathStart; i < DeathEnd-1; ++i) States[i]->NextState = States[i+1];
    if (!DiesAway && Type != OLDDEC_Projectile) States[DeathEnd-1]->Time = -1.0f;
    if (Type == OLDDEC_Projectile) {
      if (Explosive) {
        //States[DeathStart]->Function = FuncA_ExplodeParms;
        SetupOldStyleFunction(sc, Class, States[DeathStart], FuncA_ExplodeParms);
      }
    } else {
      // first death state plays death sound, second makes it non-blocking unless it should stay solid
      //States[DeathStart]->Function = FuncA_Scream;
      SetupOldStyleFunction(sc, Class, States[DeathStart], FuncA_Scream);
      if (!SolidOnDeath) {
        if (DeathEnd-DeathStart > 1) {
          //States[DeathStart+1]->Function = FuncA_NoBlocking;
          SetupOldStyleFunction(sc, Class, States[DeathStart+1], FuncA_NoBlocking);
        } else {
          //States[DeathStart]->Function = FuncA_ScreamAndUnblock;
          SetupOldStyleFunction(sc, Class, States[DeathStart], FuncA_ScreamAndUnblock);
        }
      }
      if (!DeathHeight) DeathHeight = Class->GetFieldFloat("Height");
      Class->SetFieldFloat("DeathHeight", DeathHeight);
    }
    Class->SetStateLabel("Death", States[DeathStart]);
  }

  // set up links of burn death states
  if (BurnEnd != 0) {
    for (int i = BurnStart; i < BurnEnd-1; ++i) States[i]->NextState = States[i+1];
    if (!BurnsAway) States[BurnEnd-1]->Time = -1.0f;
    // First death state plays active sound, second makes it non-blocking unless it should stay solid
    //States[BurnStart]->Function = FuncA_ActiveSound;
    SetupOldStyleFunction(sc, Class, States[BurnStart], FuncA_ActiveSound);
    if (!SolidOnBurn) {
      if (BurnEnd-BurnStart > 1) {
        //States[BurnStart+1]->Function = FuncA_NoBlocking;
        SetupOldStyleFunction(sc, Class, States[BurnStart+1], FuncA_NoBlocking);
      } else {
        //States[BurnStart]->Function = FuncA_ActiveAndUnblock;
        SetupOldStyleFunction(sc, Class, States[BurnStart], FuncA_ActiveAndUnblock);
      }
    }

    if (!BurnHeight) BurnHeight = Class->GetFieldFloat("Height");
    Class->SetFieldFloat("BurnHeight", BurnHeight);

    TArray<VName> Names;
    Names.Append("Death");
    Names.Append("Fire");
    Class->SetStateLabel(Names, States[BurnStart]);
  }

  // set up links of ice death states
  if (IceEnd != 0) {
    for (int i = IceStart; i < IceEnd-1; ++i) States[i]->NextState = States[i+1];

    States[IceEnd-2]->Time = 5.0f/35.0f;
    //States[IceEnd-2]->Function = FuncA_FreezeDeath;
    SetupOldStyleFunction(sc, Class, States[IceEnd-2], FuncA_FreezeDeath);

    States[IceEnd-1]->NextState = States[IceEnd-1];
    States[IceEnd-1]->Time = 1.0f/35.0f;
    //States[IceEnd-1]->Function = FuncA_FreezeDeathChunks;
    SetupOldStyleFunction(sc, Class, States[IceEnd-1], FuncA_FreezeDeathChunks);

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

  // postprocess states
  Class->EmitStateLabels();
  for (auto &&sts : States) {
    #if defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
    sts->Define(); // this defines state functions
    #endif
    if (sts->GotoLabel != NAME_None) {
      sts->NextState = Class->ResolveStateLabel(sts->Loc, sts->GotoLabel, sts->GotoOffset);
    }
  }
}


//==========================================================================
//
//  ParseDamageType
//
//==========================================================================
static void ParseDamageType (VScriptParser *sc) {
  //if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "%s: 'DamageType' in decorate is not implemented yet!", *sc->GetLoc().toStringNoCol());
  //sc->SkipBracketed();
  sc->ExpectString(); // name
  if (sc->String.strEquCI("Normal")) sc->String = "None";
  VStr dfname = sc->String;
  sc->Expect("{");
  float factor = 1.0f;
  bool noarmor = false;
  bool replace = false;
  while (!sc->Check("}")) {
    if (sc->Check("NoArmor")) {
      noarmor = true;
      continue;
    }
    if (sc->Check("ReplaceFactor")) {
      replace = true;
      continue;
    }
    if (sc->Check("Factor")) {
      sc->ExpectFloat();
      factor = sc->Float;
      continue;
    }
    sc->Error(va("unknown DamageType field '%s'", *sc->String));
  }

  // add or replace damage type
  VDamageFactor *fc = nullptr;
  for (auto &&df : CustomDamageFactors) {
    if (dfname.strEquCI(*df.DamageType)) {
      fc = &df;
      break;
    }
  }
  if (!fc) {
    fc = &CustomDamageFactors.alloc();
    memset((void *)fc, 0, sizeof(VDamageFactor));
    fc->DamageType = VName(*dfname);
  }
  fc->Factor = factor;
  if (replace) fc->Flags |= VDamageFactor::DF_REPLACE;
  if (noarmor) fc->Flags |= VDamageFactor::DF_NOARMOR;
}


//==========================================================================
//
//  ParseDecorate
//
//==========================================================================
static void ParseDecorate (VScriptParser *sc, TArray<VClassFixup> &ClassFixups, TArray<VWeaponSlotFixups> &newWSlots) {
  dcTotalSourceSize += sc->GetScriptSize();
  while (!sc->AtEnd()) {
    if (sc->Check("#region") || sc->Check("#endregion")) {
      //GLog.Logf(NAME_Warning, "REGION: crossed=%d", (sc->Crossed ? 1 : 0));
      sc->SkipLine();
      continue;
    }
    if (sc->Check("#include")) {
      sc->ExpectString();
      int Lump = /*W_CheckNumForFileName*/W_CheckNumForFileNameInSameFile(mainDecorateLump, sc->String);
      // check WAD lump only if it's no longer than 8 characters and has no path separator
      if (Lump < 0 && sc->String.Length() <= 8 && sc->String.IndexOf('/') < 0) {
        if (mainDecorateLump < 0) {
          Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
        } else {
          Lump = W_CheckNumForNameInFile(VName(*sc->String, VName::AddLower8), W_LumpFile(mainDecorateLump));
        }
      }
      if (Lump < 0) sc->Error(va("Lump %s not found", *sc->String));
      ParseDecorate(new VScriptParser(/*sc->String*/W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassFixups, newWSlots);
    } else if (sc->Check("const")) {
      ParseConst(sc, DecPkg);
    } else if (sc->Check("enum")) {
      ParseEnum(sc, DecPkg);
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
    } else if (sc->Check("k8vavoom")) {
      // special k8vavoom section
      sc->Expect("{");
      while (!sc->Check("}")) {
        if (sc->Check("basepak")) {
          sc->Expect("=");
               if (sc->Check("true") || sc->Check("tan")) thisIsBasePak = true;
          else if (sc->Check("false") || sc->Check("ona")) thisIsBasePak = false;
          else sc->Error("boolean expected for k8vavoom command 'basepak'");
          continue;
        }
        if (sc->Check("AllowBloodReplacement")) {
          sc->Expect("=");
               if (sc->Check("true") || sc->Check("tan")) bloodOverrideAllowed = true;
          else if (sc->Check("false") || sc->Check("ona")) bloodOverrideAllowed = false;
          else sc->Error("boolean expected for k8vavoom command 'AllowBloodReplacement'");
          continue;
        }
        if (sc->Check("GlobalDisableOverride")) { GlobalDisableOverride = true; continue; }
        if (sc->Check("GlobalEnableOverride")) { GlobalDisableOverride = false; continue; }
        sc->Error(va("invalid k8vavoom command '%s'", *sc->String));
      }
    } else {
      ParseOldDecoration(sc, OLDDEC_Decoration);
    }
  }
  delete sc;
  sc = nullptr;
}


//==========================================================================
//
//  ReadLineSpecialInfos
//
//==========================================================================
void ReadLineSpecialInfos () {
  VStream *Strm = FL_OpenFileRead("line_specials.txt");
  vassert(Strm);
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
}


/*
static void dumpFieldDefs (VClass *cls) {
  if (!cls || !cls->Fields) return;
  GLog.Logf("=== CLASS:%s ===", cls->GetName());
  for (VField *fi = cls->Fields; fi; fi = fi->Next) {
    if (fi->Type.Type == TYPE_Int) {
      GLog.Logf("  %s: %d v=%d", fi->GetName(), fi->Ofs, *(vint32 *)(cls->Defaults+fi->Ofs));
    } else if (fi->Type.Type == TYPE_Float) {
      GLog.Logf("  %s: %d v=%f", fi->GetName(), fi->Ofs, *(float *)(cls->Defaults+fi->Ofs));
    } else {
      GLog.Logf("  %s: %d", fi->GetName(), fi->Ofs);
    }
  }
}
*/


//==========================================================================
//
//  SetupLimiters
//
//  setups all instance limit flags and such
//  should be called after parsing all decorate code
//
//==========================================================================
static void SetupLimiters () {
  NumberLimitedClasses.clear();

  if (limitSubs.length() == 0) return;

  // setup base limits
  for (auto &&lsb : limitSubs) {
    vassert(lsb.baseClass);
    if (!lsb.baseClass->IsChildOf(EntityClass)) continue;
    if (lsb.isInt) {
      vassert(lsb.amount > 0);
      lsb.baseClass->InstanceLimitWithSub = lsb.amount;
    } else {
      vassert(!lsb.cvar.isEmpty());
      lsb.baseClass->InstanceLimitWithSubCvar = lsb.cvar;
    }
    lsb.baseClass->SetLimitInstancesWithSub(true);
    NumberLimitedClasses.append(lsb.baseClass);
  }

  // now link all classes to base limits
  VClass::ForEachClass([](VClass *cls) -> FERes {
    VClass *bc = cls;
    while (bc) {
      if (!bc->InstanceLimitBaseClass && bc->GetLimitInstancesWithSub()) break;
      bc = bc->GetSuperClass();
    }
    if (bc) {
      // yay, i found good base class!
      for (VClass *cc = cls; cc != bc; cc = cc->GetSuperClass()) {
        cc->SetLimitInstancesWithSub(true);
        cc->InstanceLimitBaseClass = bc;
      }
    }
    return FERes::FOREACH_NEXT;
  });

  // free memory
  limitSubs.clear();
}


//==========================================================================
//
//  ProcessDecorateScripts
//
//==========================================================================
void ProcessDecorateScripts () {
#ifndef VAVOOM_K8_DEVELOPER
  // no wai
  vcWarningsSilenced = 0;
#endif
  if (!disableBloodReplaces && fsys_DisableBloodReplacement) disableBloodReplaces = true;

  RegisterDecorateMethods();

  for (int Lump = W_IterateFile(-1, "decorate_ignore.txt"); Lump != -1; Lump = W_IterateFile(Lump, "decorate_ignore.txt")) {
    GLog.Logf(NAME_Init, "Parsing DECORATE ignore file '%s'", *W_FullLumpName(Lump));
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    vassert(Strm);
    VScriptParser *sc = new VScriptParser(W_FullLumpName(Lump), Strm);
    while (sc->GetString()) {
      if (sc->String.length() == 0) continue;
      IgnoredDecorateActions.put(sc->String, true);
    }
    delete sc;
    delete Strm;
  }

  for (auto &&fname : cli_DecorateIgnoreFiles) {
    if (Sys_FileExists(fname)) {
      VStream *Strm = FL_OpenSysFileRead(fname);
      if (Strm) {
        GLog.Logf(NAME_Init, "Parsing DECORATE ignore file '%s'", *fname);
        VScriptParser *sc = new VScriptParser(fname, Strm);
        while (sc->GetString()) {
          if (sc->String.length() == 0) continue;
          IgnoredDecorateActions.put(sc->String, true);
        }
        delete sc;
        delete Strm;
      }
    }
  }

  GLog.Log(NAME_Init, "Parsing DECORATE definition files");
  for (int Lump = W_IterateFile(-1, "vavoom_decorate_defs.xml"); Lump != -1; Lump = W_IterateFile(Lump, "vavoom_decorate_defs.xml")) {
    //GLog.Logf(NAME_Init, "  %s", *W_FullLumpName(Lump));
    VStream *Strm = W_CreateLumpReaderNum(Lump);
    vassert(Strm);
    VXmlDocument *Doc = new VXmlDocument();
    Doc->Parse(*Strm, "vavoom_decorate_defs.xml");
    delete Strm;
    ParseDecorateDef(*Doc);
    delete Doc;
  }

  GLog.Log(NAME_Init, "Parsing known blood definition files");
  LoadKnownBlood();

  GLog.Log(NAME_Init, "Parsing known class ignores definition files");
  LoadKnownClassIgnores();

  GLog.Log(NAME_Init, "Processing DECORATE scripts");

  DecPkg = new VPackage(NAME_decorate);

  // find classes
  EntityClass = VClass::FindClass("Entity");
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
  PowerSpeedClass = VClass::FindClass("PowerSpeed");

  // find methods used by old style decorations
  FuncA_Scream = ActorClass->FindMethodChecked("A_Scream");
  FuncA_NoBlocking = ActorClass->FindMethodChecked("A_NoBlocking");
  FuncA_ScreamAndUnblock = ActorClass->FindMethodChecked("A_ScreamAndUnblock");
  FuncA_ActiveSound = ActorClass->FindMethodChecked("A_ActiveSound");
  FuncA_ActiveAndUnblock = ActorClass->FindMethodChecked("A_ActiveAndUnblock");
  FuncA_ExplodeParms = ActorClass->FindMethodChecked("A_ExplodeParms");
  FuncA_FreezeDeath = ActorClass->FindMethodChecked("A_FreezeDeath");
  FuncA_FreezeDeathChunks = ActorClass->FindMethodChecked("A_FreezeDeathChunks");

  // collect decorate scripts
  TArray<int> decoLumps;
  for (auto &&it : WadNSNameIterator(NAME_decorate, WADNS_Global)) {
    decoLumps.append(it.lump);
  }

  // find "after_iwad" scripts
  TArray<int> afterIWadDecLumps;
  for (auto &&it : WadNSNameIterator(NAME_decorate, WADNS_AfterIWad)) {
    afterIWadDecLumps.append(it.lump);
  }

  // insert "after iwad" after last iwad (or at the end)
  if (afterIWadDecLumps.length()) {
    // find last iwad position
    int lastiwad = -1;
    for (int f = 0; f < decoLumps.length(); ++f) {
      //GCon->Logf(NAME_Debug, "%d: <%s>; iwad=%d", f, *W_FullLumpName(decoLumps[f]), (int)W_IsIWADLump(decoLumps[f]));
      if (W_IsIWADLump(decoLumps[f])) {
        if (!W_FullPakNameForLump(decoLumps[f]).endsWithCI("/basepak.pk3")) lastiwad = f+1;
      }
    }
    // insert additional decorate lumps
    if (lastiwad < 0 || lastiwad >= decoLumps.length()) {
      // at the end
      GLog.Logf(NAME_Init, "Adding \"after iwad\" decorates at the end of the list");
      for (auto &&lmp : afterIWadDecLumps) decoLumps.append(lmp);
    } else {
      // at `lastiwad`
      GLog.Log(NAME_Init, "Adding \"after iwad\" decorates between:");
      GLog.Logf(NAME_Init, "   '%s', and", *W_FullLumpName(decoLumps[lastiwad-1]));
      GLog.Logf(NAME_Init, "   '%s'", *W_FullLumpName(decoLumps[lastiwad]));
      for (auto &&lmp : afterIWadDecLumps) { decoLumps.insert(lastiwad, lmp); ++lastiwad; }
    }
    afterIWadDecLumps.clear();
  }

  // parse scripts
  TArray<VClassFixup> ClassFixups;
  TArray<VWeaponSlotFixups> newWSlots;
  int lastDecoFile = -1;

  double dcParseTime = -Sys_Time();
  for (auto &&Lump : decoLumps) {
    mainDecorateLump = Lump;
    GLog.Logf(NAME_Init, "Parsing decorate script '%s'", *W_FullLumpName(Lump));
    if (lastDecoFile != W_LumpFile(Lump)) {
      lastDecoFile = W_LumpFile(Lump);
      ResetReplacementBase();
    }
    thisIsBasePak = false;
    bloodOverrideAllowed = false;
    GlobalDisableOverride = false;
    ParseDecorate(new VScriptParser(W_FullLumpName(Lump), W_CreateLumpReaderNum(Lump)), ClassFixups, newWSlots);
    thisIsBasePak = false; // reset it
    mainDecorateLump = -1;
  }
  dcParseTime += Sys_Time();

  decoLumps.clear();
  NoOverrideNames.clear();

  //VMemberBase::StaticDumpMObjInfo();
  ClearReplacementBase();

  // make sure all import classes were defined
  if (VMemberBase::GDecorateClassImports.length()) {
    for (int i = 0; i < VMemberBase::GDecorateClassImports.length(); ++i) {
      GLog.Logf(NAME_Warning, "Undefined DECORATE class `%s`", VMemberBase::GDecorateClassImports[i]->GetName());
    }
    Sys_Error("Not all DECORATE class imports were defined");
  }

  GLog.Logf(NAME_Init, "Post-procesing decorate code");
  //VMemberBase::StaticDumpMObjInfo();

  // set class properties
  TMap<VStr, bool> powerfixReported;
  for (int i = 0; i < ClassFixups.length(); ++i) {
    VClassFixup &CF = ClassFixups[i];
    if (!CF.ReqParent) Sys_Error("Invalid decorate class fixup (no parent); class is '%s', offset is %d, name is '%s'", (CF.Class ? *CF.Class->GetFullName() : "None"), CF.Offset, *CF.Name);
    vassert(CF.ReqParent);
    //GLog.Logf("*** FIXUP (class='%s'; name='%s'; ofs=%d)", CF.Class->GetName(), *CF.Name, CF.Offset);
    if (CF.Name.length() == 0 || CF.Name.ICmp("None") == 0) {
      *(VClass **)(CF.Class->Defaults+CF.Offset) = nullptr;
    } else {
      //VStr loname = CF.Name.toLowerCase();
      VClass *C = nullptr;
      if (CF.PowerPrefix.length() /*&& !loname.startsWith(CF.PowerPrefix.toLowerCase())*/) {
        // try prepended
        VStr pwfix = (CF.PowerPrefix+CF.Name).toLowerCase();
        C = VClass::FindClassNoCase(*pwfix);
        if (C) {
          if (!powerfixReported.find(pwfix)) {
            powerfixReported.put(pwfix, true);
            if (cli_WAll > 0 || cli_DecorateWarnPowerupRename > 0) {
              GLog.Logf(NAME_Warning, "Decorate powerup fixup in effect: `%s` -> `%s`", *CF.Name, *(CF.PowerPrefix+CF.Name));
            }
          }
        }
      }
      if (!C) C = VClass::FindClassNoCase(*CF.Name);
      if (!C) {
        if (dbg_show_missing_classes || CF.PowerPrefix.length() != 0) {
          if (CF.PowerPrefix.length() != 0) {
            GLog.Logf(NAME_Warning, "DECORATE: No such powerup class `%s`", *CF.Name);
          } else {
            GLog.Logf(NAME_Warning, "DECORATE: No such class `%s`", *CF.Name);
          }
        }
        //*(VClass **)(CF.Class->Defaults+CF.Offset) = nullptr;
      } else if (!C->IsChildOf(CF.ReqParent)) {
        GLog.Logf(NAME_Warning, "DECORATE: Class `%s` is not a descendant of `%s`", *CF.Name, CF.ReqParent->GetName());
      } else {
        *(VClass **)(CF.Class->Defaults+CF.Offset) = C;
      }
    }
  }

  VField *DropItemListField = ActorClass->FindFieldChecked("DropItemList");
  for (int i = 0; i < DecPkg->ParsedClasses.length(); ++i) {
    TArray<VDropItemInfo> &List = *(TArray<VDropItemInfo>*)DropItemListField->GetFieldPtr((VObject *)DecPkg->ParsedClasses[i]->Defaults);
    for (auto &&DI : List) {
      //VDropItemInfo &DI = List[j];
      if (DI.TypeName == NAME_None) { DI.Type = nullptr; continue; }
      VClass *C = VClass::FindClassNoCase(*DI.TypeName);
      if (!C) {
        if (!vcWarningsSilenced && cli_ShowDropItemMissingClasses > 0) GLog.Logf(NAME_Warning, "No such class `%s` (DropItemList for `%s`)", *DI.TypeName, *DecPkg->ParsedClasses[i]->GetFullName());
      } else if (!C->IsChildOf(ActorClass)) {
        if (!vcWarningsSilenced) GLog.Logf(NAME_Warning, "Class `%s` is not an actor class (DropItemList for `%s`)", *DI.TypeName, *DecPkg->ParsedClasses[i]->GetFullName());
        C = nullptr;
      }
      DI.Type = C;
    }
  }

  //WARNING: keep this in sync with script code!
  // setup PlayerPawn default weapon slots
  if (newWSlots.length()) {
    // store weapon slots into GameInfo class defaults
    VClass *wpnbase = VClass::FindClass("Weapon");
    if (!wpnbase) Sys_Error("`Weapon` class not found, cannot set weapon slots");
    VClass *ppawnbase = VClass::FindClass("PlayerPawn");
    if (!ppawnbase) Sys_Error("`PlayerPawn` class not found, cannot set weapon slots");
    // set it for all player pawns
    for (auto &&wsobj : newWSlots) {
      VClass *pawn = VClass::FindClassNoCase(*wsobj.getPlayerClassName());
      if (!pawn) {
        GCon->Logf(NAME_Error, "`%s` player pawn class not found, cannot set weapon slots", *wsobj.getPlayerClassName());
        continue;
      }
      if (!pawn->IsChildOf(ppawnbase)) {
        GCon->Logf(NAME_Error, "`%s` is not a player pawn class, cannot set weapon slots", *wsobj.getPlayerClassName());
        continue;
      }
      VField *fldList = pawn->FindFieldChecked(VName("decoWeaponsClasses"));
      //FIXME: type checking!
      TArray<VClass *> &dclist = *(TArray<VClass *> *)(fldList->GetFieldPtr((VObject *)pawn->Defaults));
      dclist.clear();
      for (int slot = 0; slot < NUM_WEAPON_SLOTS; ++slot) {
        const TArray<VStr> &wplist = wsobj.getSlotWeaponList(slot);
        if (wplist.length()) {
          // clear this slot
          dclist.append(wpnbase); // special value
          for (auto &&wname : wplist) {
            vassert(!wname.isEmpty());
            VClass *wc = VClass::FindClassNoCase(*wname);
            if (!wc) { GLog.Logf(NAME_Warning, "unknown weapon class `%s` in player pawn class `%s`", *wname, pawn->GetName()); continue; }
            if (!wc->IsChildOf(wpnbase)) { GLog.Logf(NAME_Warning, "class '%s' is not a weapon in player pawn class `%s`", *wname, pawn->GetName()); continue; }
            dclist.append(wc);
          }
        }
        // slot terminator
        dclist.append(nullptr);
      }
    }
    // release memory
    newWSlots.clear();
  }

  GLog.Logf(NAME_Init, "Compiling decorate code");
  double dcCompileTime = -Sys_Time();
  // emit code
  for (auto &&dcls : DecPkg->ParsedClasses) {
    if (getDecorateDebug()) GLog.Logf("Emiting Class %s", *dcls->GetFullName());
    //dumpFieldDefs(DecPkg->ParsedClasses[i]);
    dcls->DecorateEmit();
    #if defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
    for (VState *sts = dcls->States; sts; sts = sts->Next) sts->Emit();
    #endif
  }
  dcCompileTime += Sys_Time();

  GLog.Logf(NAME_Init, "Generating decorate code");
  double dcCodegenTime = -Sys_Time();
  // compile and set up for execution
  for (auto &&dcls : DecPkg->ParsedClasses) {
    if (getDecorateDebug()) GLog.Logf("Compiling Class %s", *dcls->GetFullName());
    //dumpFieldDefs(DecPkg->ParsedClasses[i]);
    dcls->DecoratePostLoad();
    //dumpFieldDefs(dcls);
    #if defined(VC_DECORATE_ACTION_BELONGS_TO_STATE)
    // generate code for state actions
    for (VState *sts = dcls->States; sts; sts = sts->Next) {
      // `VState::Emit()` clears `FunctionName` for direct calls
      if (sts->Function && sts->FunctionName == NAME_None) {
        if (!sts->funcIsCopy) {
          //GLog.Logf(NAME_Debug, "%s: generating code for `%s`", *sts->Loc.toString(), *sts->Function->GetFullName());
          sts->Function->PostLoad();
          vassert(sts->Function->VTableIndex >= -1);
        } else {
          //GLog.Logf(NAME_Debug, "%s: don't generate code for copy `%s`", *sts->Loc.toString(), *sts->Function->GetFullName());
        }
      }
    }
    #endif
  }
  dcCodegenTime += Sys_Time();

  if (vcErrorCount) BailOut();
  //VMemberBase::StaticDumpMObjInfo();

  VClass::StaticReinitStatesLookup();

  SetupLimiters();

  // clear memory
  ShutdownKnownBlood();
  ShutdownKnownClassIgnores();
  //!TLocation::ClearSourceFiles();

  {
    const int pms = secs2msecs(dcParseTime);
    //const int cms = secs2msecs(dcCompileTime);
    //const int gms = secs2msecs(dcCodegenTime);
    int pspeed = dcTotalSourceSize/pms/1024;
    pspeed += !pspeed;
    GLog.Logf(NAME_Init, "parsed %s decorate bytes in %s (%s mb/sec), compiled in %s, codegen took %s",
      comatoze(dcTotalSourceSize), secs2timestr(dcParseTime), comatoze(pspeed), secs2timestr(dcCompileTime), secs2timestr(dcCodegenTime));
  }
}


//==========================================================================
//
//  ShutdownDecorate
//
//==========================================================================
void ShutdownDecorate () {
  FlagList.Clear();
  LineSpecialInfos.Clear();
}


static TMap<VStr, bool> decoFlagsWarned;


//==========================================================================
//
//  VEntity::SetDecorateFlag
//
//==========================================================================
bool VEntity::SetDecorateFlag (VStr Flag, bool Value) {
  //GCon->Logf(NAME_Debug, "%s:%u: VEntity::SetDecorateFlag: flag='%s'; value=%d", GetClass()->GetName(), GetUniqueId(), *Flag, (int)Value);

  VStr ClassFilter;
  int DotPos = Flag.indexOf('.');
  if (DotPos >= 0) {
    ClassFilter = Flag.left(DotPos);
    Flag.chopLeft(DotPos+1);
  }

  //if (ClassFilter.strEquCI("POWERSPEED")) return true;
  if (ClassFilter.strEquCI("Player")) ClassFilter = "PlayerPawn";

  for (auto &&ClassDef : FlagList) {
    if (!IsA(ClassDef.Class)) continue;
    if (!ClassFilter.isEmpty() && !ClassFilter.strEquCI(*ClassDef.Class->Name)) continue;
    VFlagDef *fdef = ClassDef.FindFlag(Flag);
    if (!fdef) continue;
    bool doRelink = fdef->RelinkToWorld;
    bool didset = true;
    switch (fdef->Type) {
      case FLAG_Bool:
        if (doRelink) doRelink = (fdef->Field->GetBool(this) != Value);
        fdef->Field->SetBool(this, Value);
        // worker fields for notickers
        if (Value && Flag.strEquCI("k8vavoominternalnotickgrav")) {
          StateTime = -1;
          LastMoveTime = 0;
          PlaneAlpha = 0;
        }
        break;
      case FLAG_BoolInverted:
        if (doRelink) doRelink = (fdef->Field->GetBool(this) == Value);
        fdef->Field->SetBool(this, !Value);
        break;
      case FLAG_Unsupported:
        doRelink = false;
        if (fdef->ShowWarning || dbg_show_decorate_unsupported) {
          VStr ws = va("Setting unsupported flag '%s' in `%s` to `%s`", *Flag, GetClass()->GetName(), (Value ? "true" : "false"));
          if (!decoFlagsWarned.has(ws)) {
            decoFlagsWarned.put(ws, true);
            GLog.Log(NAME_Warning, *ws);
          }
        }
        break;
      case FLAG_Byte:
        {
          vuint8 bv = (Value ? fdef->BTrue : fdef->BFalse);
          if (doRelink) {
            doRelink = (fdef->Field->GetByte(this) != bv);
            if (doRelink) UnlinkFromWorld(); // some flags can affect word linking, so unlink here...
          }
          fdef->Field->SetByte(this, bv);
        }
        break;
      case FLAG_Float:
        {
          float fv = (Value ? fdef->FTrue : fdef->FFalse);
          if (doRelink) {
            doRelink = (fdef->Field->GetFloat(this) != fv);
            if (doRelink) UnlinkFromWorld(); // some flags can affect word linking, so unlink here...
          }
          fdef->Field->SetFloat(this, fv);
        }
        break;
      case FLAG_Name:
        {
          VName nv = (Value ? fdef->NTrue : fdef->NFalse);
          if (doRelink) {
            doRelink = (fdef->Field->GetNameValue(this) != nv);
            if (doRelink) UnlinkFromWorld(); // some flags can affect word linking, so unlink here...
          }
          fdef->Field->SetNameValue(this, nv);
        }
        break;
      case FLAG_Class:
        {
          VClass *cv = Value ?
            fdef->NTrue != NAME_None ? VClass::FindClass(*fdef->NTrue) : nullptr :
            fdef->NFalse != NAME_None ? VClass::FindClass(*fdef->NFalse) : nullptr;
          if (doRelink) {
            doRelink = (fdef->Field->GetClassValue(this) != cv);
            if (doRelink) UnlinkFromWorld(); // some flags can affect word linking, so unlink here...
          }
          fdef->Field->SetClassValue(this, cv);
        }
        break;
      case FLAG_NoClip:
        if (doRelink) {
          doRelink = (fdef->Field->GetBool(this) == Value || fdef->Field2->GetBool(this) == Value);
          if (doRelink) UnlinkFromWorld(); // some flags can affect word linking, so unlink here...
        }
        fdef->Field->SetBool(this, !Value);
        fdef->Field2->SetBool(this, !Value);
        break;
      default: didset = false;
    }
    if (doRelink) LinkToWorld(true); // ...and link back again
    if (dbg_dump_flag_changes) GLog.Logf("SETFLAG '%s'(%s) on '%s' (relink=%d); value=%d", *Flag, *ClassFilter, *GetClass()->GetFullName(), (int)doRelink, (int)Value);
    return didset;
  }

  {
    VStr ws = va("Setting unknown flag '%s' in `%s` to `%s`", *Flag, GetClass()->GetName(), (Value ? "true" : "false"));
    if (!decoFlagsWarned.has(ws)) {
      decoFlagsWarned.put(ws, true);
      GLog.Log(NAME_Warning, *ws);
    }
  }
  return false;
}


//==========================================================================
//
//  VEntity::GetDecorateFlag
//
//==========================================================================
bool VEntity::GetDecorateFlag (VStr Flag) {
  VStr ClassFilter;
  int DotPos = Flag.indexOf('.');
  if (DotPos >= 0) {
    ClassFilter = Flag.left(DotPos);
    Flag.chopLeft(DotPos+1);
  }

  for (auto &&ClassDef : FlagList) {
    if (!IsA(ClassDef.Class)) continue;
    if (!ClassFilter.isEmpty() && !ClassFilter.strEquCI(*ClassDef.Class->Name)) continue;
    VFlagDef *fdef = ClassDef.FindFlag(Flag);
    if (!fdef) continue;
    switch (fdef->Type) {
      case FLAG_Bool: return fdef->Field->GetBool(this);
      case FLAG_BoolInverted: return !fdef->Field->GetBool(this);
      case FLAG_Unsupported:
        if (fdef->ShowWarning || dbg_show_decorate_unsupported) {
          VStr ws = va("Getting unsupported flag '%s' in `%s`", *Flag, GetClass()->GetName());
          if (!decoFlagsWarned.has(ws)) {
            decoFlagsWarned.put(ws, true);
            GLog.Log(NAME_Warning, *ws);
          }
        }
        return false;
      case FLAG_Byte: return !!fdef->Field->GetByte(this);
      case FLAG_Float: return (fdef->Field->GetFloat(this) != 0.0f);
      case FLAG_Name: return (fdef->Field->GetNameValue(this) != NAME_None);
      case FLAG_Class: return !!fdef->Field->GetClassValue(this);
      case FLAG_NoClip: return (!fdef->Field->GetBool(this) && !fdef->Field2->GetBool(this)); //FIXME??
    }
    return false;
  }

  {
    VStr ws = va("Getting unknown flag '%s' in `%s`", *Flag, GetClass()->GetName());
    if (!decoFlagsWarned.has(ws)) {
      decoFlagsWarned.put(ws, true);
      GLog.Log(NAME_Warning, *ws);
    }
  }
  return false;
}


//==========================================================================
//
//  CompilerReportMemory
//
//==========================================================================
void CompilerReportMemory () {
  VExpression::ReportLeaks(cli_CompileAndExit > 0 || cli_CompilerReport > 0);
}
