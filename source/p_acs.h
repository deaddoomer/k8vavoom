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
#ifndef DECLARE_PCD
#define DECLARE_PCD(name) PCD_ ## name
#define DECLARING_PCD_ENUM
enum EPCD
{
#endif
  DECLARE_PCD(Nop),
  DECLARE_PCD(Terminate),
  DECLARE_PCD(Suspend),
  DECLARE_PCD(PushNumber),
  DECLARE_PCD(LSpec1),
  DECLARE_PCD(LSpec2),
  DECLARE_PCD(LSpec3),
  DECLARE_PCD(LSpec4),
  DECLARE_PCD(LSpec5),
  DECLARE_PCD(LSpec1Direct),
  DECLARE_PCD(LSpec2Direct),//10
  DECLARE_PCD(LSpec3Direct),
  DECLARE_PCD(LSpec4Direct),
  DECLARE_PCD(LSpec5Direct),
  DECLARE_PCD(Add),
  DECLARE_PCD(Subtract),
  DECLARE_PCD(Multiply),
  DECLARE_PCD(Divide),
  DECLARE_PCD(Modulus),
  DECLARE_PCD(EQ),
  DECLARE_PCD(NE),//20
  DECLARE_PCD(LT),
  DECLARE_PCD(GT),
  DECLARE_PCD(LE),
  DECLARE_PCD(GE),
  DECLARE_PCD(AssignScriptVar),
  DECLARE_PCD(AssignMapVar),
  DECLARE_PCD(AssignWorldVar),
  DECLARE_PCD(PushScriptVar),
  DECLARE_PCD(PushMapVar),
  DECLARE_PCD(PushWorldVar),//30
  DECLARE_PCD(AddScriptVar),
  DECLARE_PCD(AddMapVar),
  DECLARE_PCD(AddWorldVar),
  DECLARE_PCD(SubScriptVar),
  DECLARE_PCD(SubMapVar),
  DECLARE_PCD(SubWorldVar),
  DECLARE_PCD(MulScriptVar),
  DECLARE_PCD(MulMapVar),
  DECLARE_PCD(MulWorldVar),
  DECLARE_PCD(DivScriptVar),//40
  DECLARE_PCD(DivMapVar),
  DECLARE_PCD(DivWorldVar),
  DECLARE_PCD(ModScriptVar),
  DECLARE_PCD(ModMapVar),
  DECLARE_PCD(ModWorldVar),
  DECLARE_PCD(IncScriptVar),
  DECLARE_PCD(IncMapVar),
  DECLARE_PCD(IncWorldVar),
  DECLARE_PCD(DecScriptVar),
  DECLARE_PCD(DecMapVar),//50
  DECLARE_PCD(DecWorldVar),
  DECLARE_PCD(Goto),
  DECLARE_PCD(IfGoto),
  DECLARE_PCD(Drop),
  DECLARE_PCD(Delay),
  DECLARE_PCD(DelayDirect),
  DECLARE_PCD(Random),
  DECLARE_PCD(RandomDirect),
  DECLARE_PCD(ThingCount),
  DECLARE_PCD(ThingCountDirect),//60
  DECLARE_PCD(TagWait),
  DECLARE_PCD(TagWaitDirect),
  DECLARE_PCD(PolyWait),
  DECLARE_PCD(PolyWaitDirect),
  DECLARE_PCD(ChangeFloor),
  DECLARE_PCD(ChangeFloorDirect),
  DECLARE_PCD(ChangeCeiling),
  DECLARE_PCD(ChangeCeilingDirect),
  DECLARE_PCD(Restart),
  DECLARE_PCD(AndLogical),//70
  DECLARE_PCD(OrLogical),
  DECLARE_PCD(AndBitwise),
  DECLARE_PCD(OrBitwise),
  DECLARE_PCD(EorBitwise),
  DECLARE_PCD(NegateLogical),
  DECLARE_PCD(LShift),
  DECLARE_PCD(RShift),
  DECLARE_PCD(UnaryMinus),
  DECLARE_PCD(IfNotGoto),
  DECLARE_PCD(LineSide),//80
  DECLARE_PCD(ScriptWait),
  DECLARE_PCD(ScriptWaitDirect),
  DECLARE_PCD(ClearLineSpecial),
  DECLARE_PCD(CaseGoto),
  DECLARE_PCD(BeginPrint),
  DECLARE_PCD(EndPrint),
  DECLARE_PCD(PrintString),
  DECLARE_PCD(PrintNumber),
  DECLARE_PCD(PrintCharacter),
  DECLARE_PCD(PlayerCount),//90
  DECLARE_PCD(GameType),
  DECLARE_PCD(GameSkill),
  DECLARE_PCD(Timer),
  DECLARE_PCD(SectorSound),
  DECLARE_PCD(AmbientSound),
  DECLARE_PCD(SoundSequence),
  DECLARE_PCD(SetLineTexture),
  DECLARE_PCD(SetLineBlocking),
  DECLARE_PCD(SetLineSpecial),
  DECLARE_PCD(ThingSound),//100
  DECLARE_PCD(EndPrintBold),
  DECLARE_PCD(ActivatorSound),  // Start of the extended opcodes.
  DECLARE_PCD(LocalAmbientSound),
  DECLARE_PCD(SetLineMonsterBlocking),
  DECLARE_PCD(PlayerBlueSkull), // Start of new [Skull Tag] pcodes
  DECLARE_PCD(PlayerRedSkull),
  DECLARE_PCD(PlayerYellowSkull),
  DECLARE_PCD(PlayerMasterSkull),
  DECLARE_PCD(PlayerBlueCard),
  DECLARE_PCD(PlayerRedCard),//110
  DECLARE_PCD(PlayerYellowCard),
  DECLARE_PCD(PlayerMasterCard),
  DECLARE_PCD(PlayerBlackSkull),
  DECLARE_PCD(PlayerSilverSkull),
  DECLARE_PCD(PlayerGoldSkull),
  DECLARE_PCD(PlayerBlackCard),
  DECLARE_PCD(PlayerSilverCard),
  DECLARE_PCD(PlayerOnTeam),
  DECLARE_PCD(PlayerTeam),
  DECLARE_PCD(PlayerHealth),//120
  DECLARE_PCD(PlayerArmorPoints),
  DECLARE_PCD(PlayerFrags),
  DECLARE_PCD(PlayerExpert),
  DECLARE_PCD(BlueTeamCount),
  DECLARE_PCD(RedTeamCount),
  DECLARE_PCD(BlueTeamScore),
  DECLARE_PCD(RedTeamScore),
  DECLARE_PCD(IsOneFlagCTF),
  DECLARE_PCD(LSpec6),      // These are never used. They should probably
  DECLARE_PCD(LSpec6Direct),//130 // be given names like PCD_Dummy.
  DECLARE_PCD(PrintName),
  DECLARE_PCD(MusicChange),
  DECLARE_PCD(ConsoleCommandDirect), //DECLARE_PCD(Team2FragPoints),
  DECLARE_PCD(ConsoleCommand),
  DECLARE_PCD(SinglePlayer),    // [RH] End of Skull Tag p-codes
  DECLARE_PCD(FixedMul),
  DECLARE_PCD(FixedDiv),
  DECLARE_PCD(SetGravity),
  DECLARE_PCD(SetGravityDirect),
  DECLARE_PCD(SetAirControl),//140
  DECLARE_PCD(SetAirControlDirect),
  DECLARE_PCD(ClearInventory),
  DECLARE_PCD(GiveInventory),
  DECLARE_PCD(GiveInventoryDirect),
  DECLARE_PCD(TakeInventory),
  DECLARE_PCD(TakeInventoryDirect),
  DECLARE_PCD(CheckInventory),
  DECLARE_PCD(CheckInventoryDirect),
  DECLARE_PCD(Spawn),
  DECLARE_PCD(SpawnDirect),//150
  DECLARE_PCD(SpawnSpot),
  DECLARE_PCD(SpawnSpotDirect),
  DECLARE_PCD(SetMusic),
  DECLARE_PCD(SetMusicDirect),
  DECLARE_PCD(LocalSetMusic),
  DECLARE_PCD(LocalSetMusicDirect),
  DECLARE_PCD(PrintFixed),
  DECLARE_PCD(PrintLocalised),
  DECLARE_PCD(MoreHudMessage),
  DECLARE_PCD(OptHudMessage),//160
  DECLARE_PCD(EndHudMessage),
  DECLARE_PCD(EndHudMessageBold),
  DECLARE_PCD(SetStyle),
  DECLARE_PCD(SetStyleDirect),
  DECLARE_PCD(SetFont),
  DECLARE_PCD(SetFontDirect),
  DECLARE_PCD(PushByte),
  DECLARE_PCD(LSpec1DirectB),
  DECLARE_PCD(LSpec2DirectB),
  DECLARE_PCD(LSpec3DirectB),//170
  DECLARE_PCD(LSpec4DirectB),
  DECLARE_PCD(LSpec5DirectB),
  DECLARE_PCD(DelayDirectB),
  DECLARE_PCD(RandomDirectB),
  DECLARE_PCD(PushBytes),
  DECLARE_PCD(Push2Bytes),
  DECLARE_PCD(Push3Bytes),
  DECLARE_PCD(Push4Bytes),
  DECLARE_PCD(Push5Bytes),
  DECLARE_PCD(SetThingSpecial),//180
  DECLARE_PCD(AssignGlobalVar),
  DECLARE_PCD(PushGlobalVar),
  DECLARE_PCD(AddGlobalVar),
  DECLARE_PCD(SubGlobalVar),
  DECLARE_PCD(MulGlobalVar),
  DECLARE_PCD(DivGlobalVar),
  DECLARE_PCD(ModGlobalVar),
  DECLARE_PCD(IncGlobalVar),
  DECLARE_PCD(DecGlobalVar),
  DECLARE_PCD(FadeTo),//190
  DECLARE_PCD(FadeRange),
  DECLARE_PCD(CancelFade),
  DECLARE_PCD(PlayMovie),
  DECLARE_PCD(SetFloorTrigger),
  DECLARE_PCD(SetCeilingTrigger),
  DECLARE_PCD(GetActorX),
  DECLARE_PCD(GetActorY),
  DECLARE_PCD(GetActorZ),
  DECLARE_PCD(StartTranslation),
  DECLARE_PCD(TranslationRange1),//200
  DECLARE_PCD(TranslationRange2),
  DECLARE_PCD(EndTranslation),
  DECLARE_PCD(Call),
  DECLARE_PCD(CallDiscard),
  DECLARE_PCD(ReturnVoid),
  DECLARE_PCD(ReturnVal),
  DECLARE_PCD(PushMapArray),
  DECLARE_PCD(AssignMapArray),
  DECLARE_PCD(AddMapArray),
  DECLARE_PCD(SubMapArray),//210
  DECLARE_PCD(MulMapArray),
  DECLARE_PCD(DivMapArray),
  DECLARE_PCD(ModMapArray),
  DECLARE_PCD(IncMapArray),
  DECLARE_PCD(DecMapArray),
  DECLARE_PCD(Dup),
  DECLARE_PCD(Swap),
  DECLARE_PCD(WriteToIni),
  DECLARE_PCD(GetFromIni),
  DECLARE_PCD(Sin),//220
  DECLARE_PCD(Cos),
  DECLARE_PCD(VectorAngle),
  DECLARE_PCD(CheckWeapon),
  DECLARE_PCD(SetWeapon),
  DECLARE_PCD(TagString),
  DECLARE_PCD(PushWorldArray),
  DECLARE_PCD(AssignWorldArray),
  DECLARE_PCD(AddWorldArray),
  DECLARE_PCD(SubWorldArray),
  DECLARE_PCD(MulWorldArray),//230
  DECLARE_PCD(DivWorldArray),
  DECLARE_PCD(ModWorldArray),
  DECLARE_PCD(IncWorldArray),
  DECLARE_PCD(DecWorldArray),
  DECLARE_PCD(PushGlobalArray),
  DECLARE_PCD(AssignGlobalArray),
  DECLARE_PCD(AddGlobalArray),
  DECLARE_PCD(SubGlobalArray),
  DECLARE_PCD(MulGlobalArray),
  DECLARE_PCD(DivGlobalArray),//240
  DECLARE_PCD(ModGlobalArray),
  DECLARE_PCD(IncGlobalArray),
  DECLARE_PCD(DecGlobalArray),
  DECLARE_PCD(SetMarineWeapon),
  DECLARE_PCD(SetActorProperty),
  DECLARE_PCD(GetActorProperty),
  DECLARE_PCD(PlayerNumber),
  DECLARE_PCD(ActivatorTID),
  DECLARE_PCD(SetMarineSprite),
  DECLARE_PCD(GetScreenWidth),//250
  DECLARE_PCD(GetScreenHeight),
  DECLARE_PCD(ThingProjectile2),
  DECLARE_PCD(StrLen),
  DECLARE_PCD(SetHudSize),
  DECLARE_PCD(GetCvar),
  DECLARE_PCD(CaseGotoSorted),
  DECLARE_PCD(SetResultValue),
  DECLARE_PCD(GetLineRowOffset),
  DECLARE_PCD(GetActorFloorZ),
  DECLARE_PCD(GetActorAngle),//260
  DECLARE_PCD(GetSectorFloorZ),
  DECLARE_PCD(GetSectorCeilingZ),
  DECLARE_PCD(LSpec5Result),
  DECLARE_PCD(GetSigilPieces),
  DECLARE_PCD(GetLevelInfo),
  DECLARE_PCD(ChangeSky),
  DECLARE_PCD(PlayerInGame),
  DECLARE_PCD(PlayerIsBot),
  DECLARE_PCD(SetCameraToTexture),
  DECLARE_PCD(EndLog),//270
  DECLARE_PCD(GetAmmoCapacity),
  DECLARE_PCD(SetAmmoCapacity),
  DECLARE_PCD(PrintMapCharArray),
  DECLARE_PCD(PrintWorldCharArray),
  DECLARE_PCD(PrintGlobalCharArray),
  DECLARE_PCD(SetActorAngle),
  DECLARE_PCD(GrabInput),
  DECLARE_PCD(SetMousePointer),
  DECLARE_PCD(MoveMousePointer),
  DECLARE_PCD(SpawnProjectile),//280
  DECLARE_PCD(GetSectorLightLevel),
  DECLARE_PCD(GetActorCeilingZ),
  DECLARE_PCD(SetActorPosition),
  DECLARE_PCD(ClearActorInventory),
  DECLARE_PCD(GiveActorInventory),
  DECLARE_PCD(TakeActorInventory),
  DECLARE_PCD(CheckActorInventory),
  DECLARE_PCD(ThingCountName),
  DECLARE_PCD(SpawnSpotFacing),
  DECLARE_PCD(PlayerClass),//290
  DECLARE_PCD(AndScriptVar),
  DECLARE_PCD(AndMapVar),
  DECLARE_PCD(AndWorldVar),
  DECLARE_PCD(AndGlobalVar),
  DECLARE_PCD(AndMapArray),
  DECLARE_PCD(AndWorldArray),
  DECLARE_PCD(AndGlobalArray),
  DECLARE_PCD(EOrScriptVar),
  DECLARE_PCD(EOrMapVar),
  DECLARE_PCD(EOrWorldVar),//300
  DECLARE_PCD(EOrGlobalVar),
  DECLARE_PCD(EOrMapArray),
  DECLARE_PCD(EOrWorldArray),
  DECLARE_PCD(EOrGlobalArray),
  DECLARE_PCD(OrScriptVar),
  DECLARE_PCD(OrMapVar),
  DECLARE_PCD(OrWorldVar),
  DECLARE_PCD(OrGlobalVar),
  DECLARE_PCD(OrMapArray),
  DECLARE_PCD(OrWorldArray),//310
  DECLARE_PCD(OrGlobalArray),
  DECLARE_PCD(LSScriptVar),
  DECLARE_PCD(LSMapVar),
  DECLARE_PCD(LSWorldVar),
  DECLARE_PCD(LSGlobalVar),
  DECLARE_PCD(LSMapArray),
  DECLARE_PCD(LSWorldArray),
  DECLARE_PCD(LSGlobalArray),
  DECLARE_PCD(RSScriptVar),
  DECLARE_PCD(RSMapVar),//320
  DECLARE_PCD(RSWorldVar),
  DECLARE_PCD(RSGlobalVar),
  DECLARE_PCD(RSMapArray),
  DECLARE_PCD(RSWorldArray),
  DECLARE_PCD(RSGlobalArray),
  DECLARE_PCD(GetPlayerInfo),
  DECLARE_PCD(ChangeLevel),
  DECLARE_PCD(SectorDamage),
  DECLARE_PCD(ReplaceTextures),
  DECLARE_PCD(NegateBinary),//330
  DECLARE_PCD(GetActorPitch),
  DECLARE_PCD(SetActorPitch),
  DECLARE_PCD(PrintBind),
  DECLARE_PCD(SetActorState),
  DECLARE_PCD(ThingDamage2),
  DECLARE_PCD(UseInventory),
  DECLARE_PCD(UseActorInventory),
  DECLARE_PCD(CheckActorCeilingTexture),
  DECLARE_PCD(CheckActorFloorTexture),
  DECLARE_PCD(GetActorLightLevel),//340
  DECLARE_PCD(SetMugShotState),
  DECLARE_PCD(ThingCountSector),
  DECLARE_PCD(ThingCountNameSector),
  DECLARE_PCD(CheckPlayerCamera),
  DECLARE_PCD(MorphActor),
  DECLARE_PCD(UnmorphActor),
  DECLARE_PCD(GetPlayerInput),
  DECLARE_PCD(ClassifyActor),
  DECLARE_PCD(PrintBinary),
  DECLARE_PCD(PrintHex),//350

  DECLARE_PCD(CallFunc),//351

  DECLARE_PCD(SaveString),//352

  DECLARE_PCD(PrintMapChRange),//353
  DECLARE_PCD(PrintWorldChRange),//354
  DECLARE_PCD(PrintGlobalChRange),//355

  DECLARE_PCD(StrCpyToMapChRange),//356, UNINMPLEMENTED
  DECLARE_PCD(StrCpyToWorldChRange),//357, UNINMPLEMENTED
  DECLARE_PCD(StrCpyToGlobalChRange),//358, UNINMPLEMENTED

  DECLARE_PCD(PushFunction),//359

  DECLARE_PCD(CallStack),//360

  DECLARE_PCD(ScriptWaitNamed),//361

  DECLARE_PCD(TranslationRange3),//362, UNINMPLEMENTED

  DECLARE_PCD(GotoStack),//363

  DECLARE_PCD(AssignScriptArray),//364
  DECLARE_PCD(PushScriptArray),//365
  DECLARE_PCD(AddScriptArray),//366
  DECLARE_PCD(SubScriptArray),//367
  DECLARE_PCD(MulScriptArray),//368
  DECLARE_PCD(DivScriptArray),//369
  DECLARE_PCD(ModScriptArray),//370
  DECLARE_PCD(IncScriptArray),//371
  DECLARE_PCD(DecScriptArray),//372
  DECLARE_PCD(AndScriptArray),//373
  DECLARE_PCD(EOrScriptArray),//374
  DECLARE_PCD(OrScriptArray),//375
  DECLARE_PCD(LSScriptArray),//376
  DECLARE_PCD(RSScriptArray),//377
  DECLARE_PCD(PrintScriptChArray),//378
  DECLARE_PCD(PrintScriptChRange),//379

  DECLARE_PCD(StrCpyToScriptChRange),//380, UNINMPLEMENTED

  DECLARE_PCD(LSpec5Ex),//381
  DECLARE_PCD(LSpec5ExResult),//382
  DECLARE_PCD(TranslationRange4),//383, UNINMPLEMENTED
  DECLARE_PCD(TranslationRange5),//384, UNINMPLEMENTED

#ifdef DECLARING_PCD_ENUM
  PCODE_COMMAND_COUNT
};
#undef DECLARE_PCD
#undef DECLARING_PCD_ENUM
#endif
