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

// HEADER FILES ------------------------------------------------------------

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class VNetConnection;
class VClientGameBase;

//
//  Constants for FixedColourmap
//
enum
{
  NUMCOLOURMAPS   = 32,
  INVERSECOLOURMAP  = 32,
  GOLDCOLOURMAP   = 33,
  REDCOLOURMAP    = 34,
  GREENCOLOURMAP    = 35,
};

//
// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
//
enum psprnum_t
{
  ps_weapon,
  ps_flash, //  Only DOOM uses it
  NUMPSPRITES
};

//
// Player states.
//
enum playerstate_t
{
  // Playing or camping.
  PST_LIVE,
  // Dead on the ground, view follows killer.
  PST_DEAD,
  // Ready to restart/respawn???
  PST_REBORN
};

//
// Button/action code definitions.
//
enum
{
  BT_ATTACK     = 1,    // Press "Fire".
  BT_USE        = 2,    // Use button, to open doors, activate switches.
  BT_JUMP       = 4,
  BT_ALT_ATTACK = 8,

  BT_RELOAD     = 0x00000100,
  BT_SPEED      = 0x00000200,
  BT_STRAFE     = 0x00000400,
  BT_CROUCH     = 0x00000800,
  BT_MOVELEFT   = 0x00001000,
  BT_MOVERIGHT  = 0x00002000,
  BT_LEFT       = 0x00004000,
  BT_RIGHT      = 0x00008000,
  BT_FORWARD    = 0x00010000,
  BT_BACKWARD   = 0x00020000,
};

struct VViewState
{
  VState *State;
  float     StateTime;
  float     SX;
  float     SY;
};

//
// Extended player object info: player_t
//
class VBasePlayer : public VGameObject
{
  DECLARE_CLASS(VBasePlayer, VGameObject, 0)

  enum { TOCENTRE = -128 };

  static VField *fldPendingWeapon;

  VLevelInfo *Level;

  enum {
    PF_Active            = 0x0001,
    PF_Spawned           = 0x0002,
    PF_IsBot             = 0x0004,
    PF_FixAngle          = 0x0008,
    PF_AttackDown        = 0x0010, // True if button down last tic.
    PF_UseDown           = 0x0020,
    PF_DidSecret         = 0x0040, // True if secret level has been done.
    PF_Centreing         = 0x0080,
    PF_IsClient          = 0x0100, // Player on client side
    PF_AutomapRevealed   = 0x0200,
    PF_AutomapShowThings = 0x0400,
    PF_ReloadQueued      = 0x0800,
  };
  vuint32 PlayerFlags;

  VNetConnection *Net;

  VStr UserInfo;

  VStr PlayerName;
  vuint8 BaseClass;
  vuint8 PClass; // player class type
  vuint8 TranslStart;
  vuint8 TranslEnd;
  vint32 Colour;

  float ClientForwardMove;  // *2048 for move
  float ClientSideMove;   // *2048 for move
  float ForwardMove;  // *2048 for move
  float SideMove;   // *2048 for move
  float FlyMove;    // fly up/down/centreing
  /*vuint8*/vuint32 Buttons;    // fire, use
  /*vuint8*/vuint32 Impulse;    // weapon changes, inventory, etc
  //  For ACS
  vuint32 AcsButtons;
  /*vuint8*/vuint32 OldButtons;
  TAVec OldViewAngles;

  VEntity *MO;
  VEntity *Camera;
  vint32 PlayerState;

  // determine POV, including viewpoint bobbing during movement
  // focal origin above r.z
  TVec ViewOrg;

  TAVec ViewAngles;

  // This is only used between levels,
  // mo->health is used during levels.
  vint32 Health;

  // Frags, kills of other players.
  vint32 Frags;
  vint32 Deaths;

  // For intermission stats.
  vint32 KillCount;
  vint32 ItemCount;
  vint32 SecretCount;

  // So gun flashes light up areas.
  vuint8 ExtraLight;

  // For lite-amp and invulnarability powers
  vuint8 FixedColourmap;

  // Colour shifts for damage, powerups and content types
  vuint32 CShift;

  // Overlay view sprites (gun, etc).
  VViewState ViewStates[NUMPSPRITES];
  vint32 DispSpriteFrame; // see entity code for explanation
  float PSpriteSY;

  float WorldTimer;       // total time the player's been playing

  vuint8 ClientNum;

  vint32 SoundEnvironment;

  VClientGameBase *ClGame;

  VPlayerReplicationInfo *PlayerReplicationInfo;

  VBasePlayer()
  : UserInfo(E_NoInit)
  , PlayerName(E_NoInit)
  {}

  //  VObject interface
  virtual bool ExecuteNetMethod(VMethod*) override;

  void SpawnClient();

  void Printf(const char*, ...) __attribute__((format(printf,2,3)));
  void CentrePrintf(const char*, ...) __attribute__((format(printf,2,3)));

  void SetViewState(int, VState*);
  void AdvanceViewStates(float);

  void SetUserInfo(const VStr&);
  void ReadFromUserInfo();

  //  Handling of player input.
  void StartPitchDrift();
  void StopPitchDrift();
  void AdjustAngles();
  void HandleInput();
  bool Responder(event_t*);
  void ClearInput();
  int AcsGetInput(int);

  //  Implementation of server to client events.
  void DoClientStartSound(int, TVec, int, int, float, float, bool);
  void DoClientStopSound(int, int);
  void DoClientStartSequence(TVec, int, VName, int);
  void DoClientAddSequenceChoice(int, VName);
  void DoClientStopSequence(int);
  void DoClientPrint(VStr);
  void DoClientCentrePrint(VStr);
  void DoClientSetAngles(TAVec);
  void DoClientIntermission(VName);
  void DoClientPause(bool);
  void DoClientSkipIntermission();
  void DoClientFinale(VStr);
  void DoClientChangeMusic(VName);
  void DoClientSetServerInfo(VStr, VStr);
  void DoClientHudMessage(const VStr&, VName, int, int, int, const VStr&,
    float, float, int, int, float, float, float);

  void WriteViewData();

  DECLARE_FUNCTION(cprint)
  DECLARE_FUNCTION(centreprint)
  DECLARE_FUNCTION(GetPlayerNum)
  DECLARE_FUNCTION(ClearPlayer)
  DECLARE_FUNCTION(SetViewObject)
  DECLARE_FUNCTION(SetViewObjectIfNone)
  DECLARE_FUNCTION(SetViewState)
  DECLARE_FUNCTION(AdvanceViewStates)
  DECLARE_FUNCTION(DisconnectBot)

  DECLARE_FUNCTION(ClientStartSound)
  DECLARE_FUNCTION(ClientStopSound)
  DECLARE_FUNCTION(ClientStartSequence)
  DECLARE_FUNCTION(ClientAddSequenceChoice)
  DECLARE_FUNCTION(ClientStopSequence)
  DECLARE_FUNCTION(ClientPrint)
  DECLARE_FUNCTION(ClientCentrePrint)
  DECLARE_FUNCTION(ClientSetAngles)
  DECLARE_FUNCTION(ClientIntermission)
  DECLARE_FUNCTION(ClientPause)
  DECLARE_FUNCTION(ClientSkipIntermission)
  DECLARE_FUNCTION(ClientFinale)
  DECLARE_FUNCTION(ClientChangeMusic)
  DECLARE_FUNCTION(ClientSetServerInfo)
  DECLARE_FUNCTION(ClientHudMessage)

  DECLARE_FUNCTION(ServerSetUserInfo)

  //  Player events.
  void eventPutClientIntoServer()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_PutClientIntoServer);
  }
  void eventSpawnClient()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_SpawnClient);
  }
  void eventNetGameReborn()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_NetGameReborn);
  }
  void eventDisconnectClient()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_DisconnectClient);
  }
  void eventUserinfoChanged()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_UserinfoChanged);
  }
  void eventPlayerExitMap(bool clusterChange)
  {
    P_PASS_SELF;
    P_PASS_BOOL(clusterChange);
    EV_RET_VOID(NAME_PlayerExitMap);
  }
  void eventPlayerBeforeExitMap()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_PlayerBeforeExitMap);
  }
  void eventPlayerTick(float deltaTime)
  {
    P_PASS_SELF;
    P_PASS_FLOAT(deltaTime);
    EV_RET_VOID(NAME_PlayerTick);
  }
  void eventClientTick(float DeltaTime)
  {
    P_PASS_SELF;
    P_PASS_FLOAT(DeltaTime);
    EV_RET_VOID(NAME_ClientTick);
  }
  void eventSetViewPos()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_SetViewPos);
  }
  void eventPreTravel()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_PreTravel);
  }
  void eventUseInventory(VStr Inv)
  {
    P_PASS_SELF;
    P_PASS_STR(Inv);
    EV_RET_VOID(NAME_UseInventory);
  }
  bool eventCheckDoubleFiringSpeed()
  {
    P_PASS_SELF;
    EV_RET_BOOL(NAME_CheckDoubleFiringSpeed);
  }

  // cheats
  void eventCheat_God () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_God); }
  void eventCheat_Buddha () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_Buddha); }
  void eventCheat_Summon () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_Summon); }
  void eventCheat_NoClip () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_NoClip); }
  void eventCheat_Gimme () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_Gimme); }
  void eventCheat_KillAll () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_KillAll); }
  void eventCheat_Morph () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_Morph); }
  void eventCheat_NoWeapons () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_NoWeapons); }
  void eventCheat_Class () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_Class); }
  void eventCheat_Fly () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_Fly); }
  void eventCheat_NoTarget () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_NoTarget); }
  void eventCheat_Anubis () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_Anubis); }
  void eventCheat_Freeze () { P_PASS_SELF; EV_RET_VOID(NAME_Cheat_Freeze); }
  void eventCheat_Jumper () { P_PASS_SELF; EV_RET_VOID(VName("Cheat_Jumper")); }
  void eventCheat_ShooterKing () { P_PASS_SELF; EV_RET_VOID(VName("Cheat_ShooterKing")); }
  void eventCheat_Regeneration () { P_PASS_SELF; EV_RET_VOID(VName("Cheat_Regeneration")); }
  void eventCheat_DumpInventory () { P_PASS_SELF; EV_RET_VOID(VName("Cheat_DumpInventory")); }

  //  Server to client events.
  void eventClientStartSound(int SoundId, TVec Org, int OriginId,
    int Channel, float Volume, float Attenuation, bool Loop)
  {
    P_PASS_SELF;
    P_PASS_INT(SoundId);
    P_PASS_VEC(Org);
    P_PASS_INT(OriginId);
    P_PASS_INT(Channel);
    P_PASS_FLOAT(Volume);
    P_PASS_FLOAT(Attenuation);
    P_PASS_BOOL(Loop);
    EV_RET_VOID(NAME_ClientStartSound);
  }
  void eventClientStopSound(int OriginId, int Channel)
  {
    P_PASS_SELF;
    P_PASS_INT(OriginId);
    P_PASS_INT(Channel);
    EV_RET_VOID(NAME_ClientStopSound);
  }
  void eventClientStartSequence(TVec Origin, int OriginId, VName Name,
    int ModeNum)
  {
    P_PASS_SELF;
    P_PASS_VEC(Origin);
    P_PASS_INT(OriginId);
    P_PASS_NAME(Name);
    P_PASS_INT(ModeNum);
    EV_RET_VOID(NAME_ClientStartSequence);
  }
  void eventClientAddSequenceChoice(int OriginId, VName Choice)
  {
    P_PASS_SELF;
    P_PASS_INT(OriginId);
    P_PASS_NAME(Choice);
    EV_RET_VOID(NAME_ClientAddSequenceChoice);
  }
  void eventClientStopSequence(int OriginId)
  {
    P_PASS_SELF;
    P_PASS_INT(OriginId);
    EV_RET_VOID(NAME_ClientStopSequence);
  }
  void eventClientPrint(VStr Str)
  {
    P_PASS_SELF;
    P_PASS_STR(Str);
    EV_RET_VOID(NAME_ClientPrint);
  }
  void eventClientCentrePrint(VStr Str)
  {
    P_PASS_SELF;
    P_PASS_STR(Str);
    EV_RET_VOID(NAME_ClientCentrePrint);
  }
  void eventClientSetAngles(TAVec Angles)
  {
    P_PASS_SELF;
    P_PASS_AVEC(Angles);
    EV_RET_VOID(NAME_ClientSetAngles);
  }
  void eventClientIntermission(VName NextMap)
  {
    P_PASS_SELF;
    P_PASS_NAME(NextMap);
    EV_RET_VOID(NAME_ClientIntermission);
  }
  void eventClientPause(bool Paused)
  {
    P_PASS_SELF;
    P_PASS_BOOL(Paused);
    EV_RET_VOID(NAME_ClientPause);
  }
  void eventClientSkipIntermission()
  {
    P_PASS_SELF;
    EV_RET_VOID(NAME_ClientSkipIntermission);
  }
  void eventClientFinale(VStr Type)
  {
    P_PASS_SELF;
    P_PASS_STR(Type);
    EV_RET_VOID(NAME_ClientFinale);
  }
  void eventClientChangeMusic(VName Song)
  {
    P_PASS_SELF;
    P_PASS_NAME(Song);
    EV_RET_VOID(NAME_ClientChangeMusic);
  }
  void eventClientSetServerInfo(VStr Key, VStr Value)
  {
    P_PASS_SELF;
    P_PASS_STR(Key);
    P_PASS_STR(Value);
    EV_RET_VOID(NAME_ClientSetServerInfo);
  }
  void eventClientHudMessage(const VStr &Message, VName Font, int Type,
    int Id, int Colour, const VStr &ColourName, float x, float y,
    int HudWidth, int HudHeight, float HoldTime, float Time1, float Time2)
  {
    P_PASS_SELF;
    P_PASS_STR(Message);
    P_PASS_NAME(Font);
    P_PASS_INT(Type);
    P_PASS_INT(Id);
    P_PASS_INT(Colour);
    P_PASS_STR(ColourName);
    P_PASS_FLOAT(x);
    P_PASS_FLOAT(y);
    P_PASS_INT(HudWidth);
    P_PASS_INT(HudHeight);
    P_PASS_FLOAT(HoldTime);
    P_PASS_FLOAT(Time1);
    P_PASS_FLOAT(Time2);
    EV_RET_VOID(NAME_ClientHudMessage);
  }

  //  Client to server events.
  void eventServerImpulse(int AImpulse)
  {
    P_PASS_SELF;
    P_PASS_INT(AImpulse);
    EV_RET_VOID(NAME_ServerImpulse);
  }
  void eventServerSetUserInfo(VStr Info)
  {
    P_PASS_SELF;
    P_PASS_STR(Info);
    EV_RET_VOID(NAME_ServerSetUserInfo);
  }
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PUBLIC DATA DECLARATIONS ------------------------------------------------
