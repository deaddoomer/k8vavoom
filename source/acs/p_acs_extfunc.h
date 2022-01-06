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
  ACS_EXTFUNC_NUM(GetLineUDMFInt, 1) // implemented
  ACS_EXTFUNC(GetLineUDMFFixed) // implemented
  ACS_EXTFUNC(GetThingUDMFInt) // implemented
  ACS_EXTFUNC(GetThingUDMFFixed) // implemented
  ACS_EXTFUNC(GetSectorUDMFInt) // implemented
  ACS_EXTFUNC(GetSectorUDMFFixed) // implemented
  ACS_EXTFUNC(GetSideUDMFInt) // implemented
  ACS_EXTFUNC(GetSideUDMFFixed) // implemented
  ACS_EXTFUNC(GetActorVelX) // implemented
  ACS_EXTFUNC(GetActorVelY) // implemented
  ACS_EXTFUNC(GetActorVelZ) // implemented
  ACS_EXTFUNC(SetActivator) // implemented
  ACS_EXTFUNC(SetActivatorToTarget) // implemented
  ACS_EXTFUNC(GetActorViewHeight) // implemented
  ACS_EXTFUNC(GetChar) // implemented
  ACS_EXTFUNC(GetAirSupply) // ignored
  ACS_EXTFUNC(SetAirSupply) // ignored
  ACS_EXTFUNC(SetSkyScrollSpeed) // ignored
  ACS_EXTFUNC(GetArmorType) // implemented
  ACS_EXTFUNC(SpawnSpotForced) // implemented
  ACS_EXTFUNC(SpawnSpotFacingForced) // implemented
  ACS_EXTFUNC(CheckActorProperty) // implemented
  ACS_EXTFUNC(SetActorVelocity) // implemented
  ACS_EXTFUNC(SetUserVariable) // implemented
  ACS_EXTFUNC(GetUserVariable) // implemented
  ACS_EXTFUNC(Radius_Quake2) // implemented
  ACS_EXTFUNC(CheckActorClass) // implemented
  ACS_EXTFUNC(SetUserArray) // implemented
  ACS_EXTFUNC(GetUserArray) // implemented
  ACS_EXTFUNC(SoundSequenceOnActor) // implemented
  ACS_EXTFUNC(SoundSequenceOnSector) // partially (no location flags)
  ACS_EXTFUNC(SoundSequenceOnPolyobj) // implemented
  ACS_EXTFUNC(GetPolyobjX) // implemented
  ACS_EXTFUNC(GetPolyobjY) // implemented
  ACS_EXTFUNC(CheckSight) // implemented
  ACS_EXTFUNC(SpawnForced) // implemented
  ACS_EXTFUNC(AnnouncerSound) // skulltag, ignored
  ACS_EXTFUNC(SetPointer) // partially implemented
  ACS_EXTFUNC(ACS_NamedExecute) // implemented
  ACS_EXTFUNC(ACS_NamedSuspend) // implemented
  ACS_EXTFUNC(ACS_NamedTerminate) // implemented
  ACS_EXTFUNC(ACS_NamedLockedExecute) // implemented
  ACS_EXTFUNC(ACS_NamedLockedExecuteDoor) // implemented
  ACS_EXTFUNC(ACS_NamedExecuteWithResult) // implemented
  ACS_EXTFUNC(ACS_NamedExecuteAlways) // implemented
  ACS_EXTFUNC(UniqueTID) // implemented
  ACS_EXTFUNC(IsTIDUsed) // implemented
  ACS_EXTFUNC(Sqrt) // implemented
  ACS_EXTFUNC(FixedSqrt) // implemented
  ACS_EXTFUNC(VectorLength) // implemented
  ACS_EXTFUNC(SetHUDClipRect) // ignored
  ACS_EXTFUNC(SetHUDWrapWidth) // ignored
  ACS_EXTFUNC(SetCVar) // implemented
  ACS_EXTFUNC(GetUserCVar) // implemented
  ACS_EXTFUNC(SetUserCVar) // implemented
  ACS_EXTFUNC(GetCVarString) // implemented
  ACS_EXTFUNC(SetCVarString) // implemented
  ACS_EXTFUNC(GetUserCVarString) // implemented
  ACS_EXTFUNC(SetUserCVarString) // implemented
  ACS_EXTFUNC(LineAttack) // implemented
  ACS_EXTFUNC(PlaySound) // implemented
  ACS_EXTFUNC(StopSound) // implemented
  ACS_EXTFUNC(strcmp) // implemented
  ACS_EXTFUNC(stricmp) // implemented
  ACS_EXTFUNC(StrLeft) // implemented
  ACS_EXTFUNC(StrRight) // implemented
  ACS_EXTFUNC(StrMid) // implemented
  ACS_EXTFUNC(GetActorClass) // implemented
  ACS_EXTFUNC(GetWeapon) // implemented
  ACS_EXTFUNC(SoundVolume) // ignored
  ACS_EXTFUNC(PlayActorSound) // ignored
  ACS_EXTFUNC(SpawnDecal) // ignored
  ACS_EXTFUNC(CheckFont) // implemented partially
  ACS_EXTFUNC(DropItem) // implemented
  ACS_EXTFUNC(CheckFlag) // implemented
  ACS_EXTFUNC(SetLineActivation) // implemented
  ACS_EXTFUNC(GetLineActivation) // implemented
  ACS_EXTFUNC(GetActorPowerupTics) // implemented
  ACS_EXTFUNC(ChangeActorAngle) // implemented
  ACS_EXTFUNC(ChangeActorPitch) // 80; implemented
  ACS_EXTFUNC(GetArmorInfo) // partially implemented
  ACS_EXTFUNC(DropInventory) // implemented
  ACS_EXTFUNC(PickActor) // implemented
  ACS_EXTFUNC(IsPointerEqual) // implemented
  ACS_EXTFUNC(CanRaiseActor) // implemented
  ACS_EXTFUNC(SetActorTeleFog) // 86
  ACS_EXTFUNC(SwapActorTeleFog)
  ACS_EXTFUNC(SetActorRoll) // implemented
  ACS_EXTFUNC(ChangeActorRoll) // implemented
  ACS_EXTFUNC(GetActorRoll) // implemented
  ACS_EXTFUNC(QuakeEx) // routed to `Radius_Quake`
  ACS_EXTFUNC(Warp) // 92 implemented
  ACS_EXTFUNC(GetMaxInventory) // ignored, as does Zandronum 3.0
  ACS_EXTFUNC(SetSectorDamage) // implemented
  ACS_EXTFUNC(SetSectorTerrain) // ignored
  ACS_EXTFUNC(SpawnParticle) // ignored
  ACS_EXTFUNC(SetMusicVolume) // ignored
  ACS_EXTFUNC(CheckProximity) // implemented
  ACS_EXTFUNC(CheckActorState) // implemented 99

  // Zandronum (this is prolly as much as i want to implement)
  ACS_EXTFUNC_NUM(ResetMap_Zandro, 100)
  ACS_EXTFUNC(PlayerIsSpectator_Zandro) // ignored
  ACS_EXTFUNC(ConsolePlayerNumber_Zandro) // implemented
  ACS_EXTFUNC(GetTeamProperty_Zandro)
  ACS_EXTFUNC(GetPlayerLivesLeft_Zandro) // ignored
  ACS_EXTFUNC(SetPlayerLivesLeft_Zandro) // ignored
  ACS_EXTFUNC(ForceToSpectate_Zandro)
  ACS_EXTFUNC(GetGamemodeState_Zandro) // always returns "in progress"
  ACS_EXTFUNC(SetDBEntry_Zandro)
  ACS_EXTFUNC(GetDBEntry_Zandro)
  ACS_EXTFUNC(SetDBEntryString_Zandro)
  ACS_EXTFUNC(GetDBEntryString_Zandro)
  ACS_EXTFUNC(IncrementDBEntry_Zandro)
  ACS_EXTFUNC(PlayerIsLoggedIn_Zandro) // ignored
  ACS_EXTFUNC(GetPlayerAccountName_Zandro) // ignored
  ACS_EXTFUNC(SortDBEntries_Zandro)
  ACS_EXTFUNC(CountDBResults_Zandro)
  ACS_EXTFUNC(FreeDBResults_Zandro)
  ACS_EXTFUNC(GetDBResultKeyString_Zandro)
  ACS_EXTFUNC(GetDBResultValueString_Zandro)
  ACS_EXTFUNC(GetDBResultValue_Zandro)
  ACS_EXTFUNC(GetDBEntryRank_Zandro)
  ACS_EXTFUNC(RequestScriptPuke_Zandro) // implemented
  ACS_EXTFUNC(BeginDBTransaction_Zandro)
  ACS_EXTFUNC(EndDBTransaction_Zandro)
  ACS_EXTFUNC(GetDBEntries_Zandro)
  ACS_EXTFUNC(NamedRequestScriptPuke_Zandro) // implemented
  ACS_EXTFUNC(SystemTime_Zandro)
  ACS_EXTFUNC(GetTimeProperty_Zandro)
  ACS_EXTFUNC(Strftime_Zandro)
  ACS_EXTFUNC(SetDeadSpectator_Zandro)
  ACS_EXTFUNC(SetActivatorToPlayer_Zandro)


  ACS_EXTFUNC_NUM(CheckClass, 200) // implemented
  ACS_EXTFUNC(DamageActor) // implemented
  ACS_EXTFUNC(SetActorFlag) // implemented
  ACS_EXTFUNC(SetTranslation) // implemented
  ACS_EXTFUNC(GetActorFloorTexture) // implemented
  ACS_EXTFUNC(GetActorFloorTerrain) // implemented
  ACS_EXTFUNC(StrArg) // implemented
  ACS_EXTFUNC(Floor) // implemented
  ACS_EXTFUNC(Round) // implemented
  ACS_EXTFUNC(Ceil) // implemented
  ACS_EXTFUNC(ScriptCall) // ignored
  ACS_EXTFUNC(StartSlideshow)

  // Eternity's (i won't look into EE engine code, so nope)
  ACS_EXTFUNC_NUM(GetLineX, 300)
  ACS_EXTFUNC(GetLineY)

  // OpenGL stuff (not now)
  ACS_EXTFUNC_NUM(SetSectorGlow, 400) // ignored
  ACS_EXTFUNC(SetFogDensity) // ignored

  // k8vavoom polyobjects
  // note that angles are in degrees
  // speed is in units per second
  // Polyobj_MoveEx (int po, float hspeed, float yawangle, float dist, float vspeed, float pitchangle, float vdist, int override)
  ACS_EXTFUNC_NUM(Polyobj_MoveEx, 800) // implemented
  // Polyobj_MoveToEx (int po, float speed, float x, float y, float z, int override)
  ACS_EXTFUNC(Polyobj_MoveToEx) // implemented
  // Polyobj_MoveToSpotEx (int po, float speed, int targettid, int override) -- this uses target height too
  ACS_EXTFUNC(Polyobj_MoveToSpotEx) // implemented
  // GetPolyobjZ (int po)
  ACS_EXTFUNC(GetPolyobjZ) // implemented
  // int Polyobj_GetFlagsEx (int po) -- -1 means "no pobj"
  ACS_EXTFUNC(Polyobj_GetFlagsEx) // implemented
  // int Polyobj_SetFlagsEx (int po, int flags, int oper) -- oper: 0 means "clear", 1 means "set", -1 means "replace"
  ACS_EXTFUNC(Polyobj_SetFlagsEx) // implemented
  // int Polyobj_IsBusy (int po) -- returns -1 if there is no such pobj
  ACS_EXTFUNC(Polyobj_IsBusy) // implemented
  // fixed Polyobj_GetAngle (int po) -- returns current pobj yaw angle, in degrees
  ACS_EXTFUNC(Polyobj_GetAngle) // implemented
  // bool Polyobj_MoveRotateEx (int po, int hspeed, int yawangle, int dist, int vspeed, int vdist, fixed deltaangle, int moveflags)
  ACS_EXTFUNC(Polyobj_MoveRotateEx) // implemented
  // bool Polyobj_MoveToRotateEx (int po, int speed, int x, int y, int z, fixed deltaangle, int moveflags)
  ACS_EXTFUNC(Polyobj_MoveToRotateEx) // implemented
  // bool Polyobj_MoveToSpotRotateEx (int po, int speed, int targettid, fixed deltaangle, int moveflags) -- this uses target height too
  ACS_EXTFUNC(Polyobj_MoveToSpotRotateEx) // implemented
  // bool Polyobj_RotateEx (int po, int speed, fixed deltaangle, int moveflags)
  ACS_EXTFUNC(Polyobj_RotateEx)

  // int CalcActorLight (int tid, int tidptr, int flags)
  ACS_EXTFUNC_NUM(CalcActorLight, 860) // implemented
  // int CalcPlayerLight (int flags) -- for current player; doesn't work in MP
  ACS_EXTFUNC(CalcPlayerLight) // implemented

  // ZDaemon (not implemented, and will never be)
  ACS_EXTFUNC_NUM(GetTeamScore, 19620) // (int team)
  ACS_EXTFUNC(SetTeamScore) // (int team, int value)
