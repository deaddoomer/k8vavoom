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
//**  Copyright (C) 2018-2022 Ketmar Dark
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
#include "../gamedefs.h"
#include "../server/sv_local.h"
#include "p_worldinfo.h"


extern VCvarI Skill;


IMPLEMENT_CLASS(V, WorldInfo)


//==========================================================================
//
//  VWorldInfo::PostCtor
//
//==========================================================================
void VWorldInfo::PostCtor () {
  Super::PostCtor();
  Acs = new VAcsGlobal;
}


//==========================================================================
//
//  VWorldInfo::SerialiseOther
//
//==========================================================================
void VWorldInfo::SerialiseOther (VStream &Strm) {
  Super::SerialiseOther(Strm);
  vuint8 xver = 0;
  Strm << xver;
  // serialise global script info
  Acs->Serialise(Strm);
}


//==========================================================================
//
//  VWorldInfo::Destroy
//
//==========================================================================
void VWorldInfo::Destroy () {
  delete Acs;
  Acs = nullptr;

  Super::Destroy();
}


//==========================================================================
//
//  VWorldInfo::SetSkill
//
//==========================================================================
void VWorldInfo::SetSkill (int ASkill) {
  //GCon->Logf(NAME_Debug, "SetSkill: ASkill=%d; GameSkill=%d; cvar=%d", ASkill, GameSkill, Skill.asInt());
       if (ASkill < 0) GameSkill = 0;
  else if (ASkill >= P_GetNumSkills()) GameSkill = P_GetNumSkills()-1;
  else GameSkill = ASkill;

  const VSkillDef *SDef = P_GetSkillDef(GameSkill);

  //GCon->Logf(NAME_Debug, "SetSkill: idx=%d; name=%s", GameSkill, *SDef->Name);

  SkillAmmoFactor = SDef->AmmoFactor;
  SkillDoubleAmmoFactor = SDef->DoubleAmmoFactor;
  SkillDamageFactor = SDef->DamageFactor;
  SkillRespawnTime = SDef->RespawnTime;
  SkillRespawnLimit = SDef->RespawnLimit;
  SkillAggressiveness = SDef->Aggressiveness;
  SkillSpawnFilter = SDef->SpawnFilter;
  SkillAcsReturn = SDef->AcsReturn;
  SkillMonsterHealth = SDef->MonsterHealth;
  SkillHealthFactor = SDef->HealthFactor;
  Flags = (Flags&0xffffff00u)|(SDef->Flags&0x0000000f);
  if (SDef->Flags&SKILLF_SlowMonsters) Flags |= WIF_SkillSlowMonsters;
  if (SDef->Flags&SKILLF_SpawnMulti) Flags |= WIF_SkillSpawnMulti;

  // copy replacements
  SkillReplacements.reset();
  for (auto &&rpl : SDef->Replacements) SkillReplacements.append(rpl);
}


//==========================================================================
//
//  VWorldInfo::GetCurrSkillName
//
//==========================================================================
const VStr VWorldInfo::GetCurrSkillName () const {
  const VSkillDef *SDef = P_GetSkillDef(GameSkill);
  return SDef->Name;
}


//==========================================================================
//
//  VWorldInfo
//
//==========================================================================
IMPLEMENT_FUNCTION(VWorldInfo, SetSkill) {
  P_GET_INT(Skill);
  P_GET_SELF;
  Self->SetSkill(Skill);
}

// native final int GetACSGlobalInt (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSGlobalInt) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_INT(Self && Self->Acs ? Self->Acs->GetGlobalVarInt(index) : 0);
}

// native final int GetACSGlobalInt (void *level, int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSGlobalStr) {
  P_GET_INT(index);
  P_GET_PTR(VAcsLevel, alvl);
  P_GET_SELF;
  RET_STR(Self && Self->Acs ? Self->Acs->GetGlobalVarStr(alvl, index) : VStr());
}

// native final float GetACSGlobalFloat (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSGlobalFloat) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_FLOAT(Self && Self->Acs ? Self->Acs->GetGlobalVarFloat(index) : 0.0f);
}

// native final void SetACSGlobalInt (int index, int value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSGlobalInt) {
  P_GET_INT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetGlobalVarInt(index, value);
}

// native final void SetACSGlobalFloat (int index, float value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSGlobalFloat) {
  P_GET_FLOAT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetGlobalVarFloat(index, value);
}

// native final int GetACSWorldInt (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSWorldInt) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_INT(Self && Self->Acs ? Self->Acs->GetWorldVarInt(index) : 0);
}

// native final float GetACSWorldFloat (int index);
IMPLEMENT_FUNCTION(VWorldInfo, GetACSWorldFloat) {
  P_GET_INT(index);
  P_GET_SELF;
  RET_FLOAT(Self && Self->Acs ? Self->Acs->GetWorldVarFloat(index) : 0.0f);
}

// native final void SetACSWorldInt (int index, int value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSWorldInt) {
  P_GET_INT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetWorldVarInt(index, value);
}

// native final void SetACSWorldFloat (int index, float value);
IMPLEMENT_FUNCTION(VWorldInfo, SetACSWorldFloat) {
  P_GET_FLOAT(value);
  P_GET_INT(index);
  P_GET_SELF;
  if (Self && Self->Acs) Self->Acs->SetWorldVarFloat(index, value);
}
