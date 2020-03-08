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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
class VNetConnection;
class VClientGameBase;

// constants for FixedColormap
enum {
  NUMCOLORMAPS    = 32,
  INVERSECOLORMAP = 32,
  GOLDCOLORMAP    = 33,
  REDCOLORMAP     = 34,
  GREENCOLORMAP   = 35,
  MONOCOLORMAP    = 36,
  BEREDCOLORMAP   = 37,
  COLORMAPS_MAX,
};


// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
enum psprnum_t {
  PS_WEAPON,
  PS_FLASH, // only DOOM uses it
  PS_WEAPON_OVL, // temporary hack
  NUMPSPRITES
};

// player states
enum playerstate_t {
  PST_LIVE, // playing or camping
  PST_DEAD, // dead on the ground, view follows killer
  PST_REBORN, // ready to restart/respawn
  PST_CHEAT_REBORN, // do not reload level, do not reset inventory
};

// button/action code definitions
enum {
  BT_ATTACK      = 0x00000001, // press "fire"
  BT_USE         = 0x00000002, // use button, to open doors, activate switches
  BT_JUMP        = 0x00000004,
  BT_ALT_ATTACK  = 0x00000008,
  BT_BUTTON_5    = 0x00000010,
  BT_BUTTON_6    = 0x00000020,
  BT_BUTTON_7    = 0x00000040,
  BT_BUTTON_8    = 0x00000080,
  BT_RELOAD      = 0x00000100,
  BT_SPEED       = 0x00000200,
  BT_STRAFE      = 0x00000400,
  BT_CROUCH      = 0x00000800,
  BT_MOVELEFT    = 0x00001000,
  BT_MOVERIGHT   = 0x00002000,
  BT_LEFT        = 0x00004000,
  BT_RIGHT       = 0x00008000,
  BT_FORWARD     = 0x00010000,
  BT_BACKWARD    = 0x00020000,
  BT_FLASHLIGHT  = 0x00100000,
  BT_SUPERBULLET = 0x00200000,
  BT_ZOOM        = 0x00400000,
};

struct VViewState {
  VState *State;
  float StateTime;
  float SX, SY;
  float OfsY;
  float BobOfsX, BobOfsY;
};

// extended player object info: player_t
class VBasePlayer : public VGameObject {
  DECLARE_CLASS(VBasePlayer, VGameObject, 0)
  NO_DEFAULT_CONSTRUCTOR(VBasePlayer)

  enum { TOCENTRE = -128 };

  VLevelInfo *Level;

  enum {
    PF_Active            = 0x0001u,
    PF_Spawned           = 0x0002u,
    PF_IsBot             = 0x0004u,
    PF_FixAngle          = 0x0008u,
    PF_AttackDown        = 0x0010u, // True if button down last tic.
    PF_UseDown           = 0x0020u,
    PF_DidSecret         = 0x0040u, // True if secret level has been done.
    PF_Centering         = 0x0080u,
    PF_IsClient          = 0x0100u, // Player on client side
    PF_AutomapRevealed   = 0x0200u,
    PF_AutomapShowThings = 0x0400u,
    PF_ReloadQueued      = 0x0800u,
    PF_ZoomDown          = 0x1000u,
  };
  vuint32 PlayerFlags;

  VNetConnection *Net;

  VStr UserInfo;

  VStr PlayerName;
  vuint8 BaseClass;
  vuint8 PClass; // player class type
  vuint8 TranslStart;
  vuint8 TranslEnd;
  vint32 Color;

  float ClientForwardMove; // *2048 for move
  float ClientSideMove; // *2048 for move
  float ForwardMove; // *2048 for move
  float SideMove; // *2048 for move
  float FlyMove; // fly up/down/centering
  /*vuint8*/vuint32 Buttons; // fire, use
  /*vuint8*/vuint32 Impulse; // weapon changes, inventory, etc
  // for ACS
  vuint32 AcsCurrButtonsPressed; // was pressed after last copy
  vuint32 AcsCurrButtons; // current state
  vuint32 AcsButtons; // what ACS will see
  vuint32 OldButtons; // previous state ACS will see
  float AcsNextButtonUpdate; // time left before copying AcsCurrButtons to AcsButtons
  float AcsPrevMouseX, AcsPrevMouseY; // previous ACS mouse movement
  float AcsMouseX, AcsMouseY; // current ACS mouse movement

  VEntity *MO;
  VEntity *Camera;
  vint32 PlayerState;

  // determine POV, including viewpoint bobbing during movement
  // focal origin above r.z
  TVec ViewOrg;

  TAVec ViewAngles;

  // this is only used between levels
  // mo->health is used during levels
  vint32 Health;

  // frags, kills of other players
  vint32 Frags;
  vint32 Deaths;

  // for intermission stats
  vint32 KillCount;
  vint32 ItemCount;
  vint32 SecretCount;

  // so gun flashes light up areas
  vuint8 ExtraLight;

  // for lite-amp and invulnarability powers
  vuint8 FixedColormap;

  // color shifts for damage, powerups and content types
  vuint32 CShift;

  // overlay view sprites (gun, etc)
  VViewState ViewStates[NUMPSPRITES];
  vint32 DispSpriteFrame[NUMPSPRITES]; // see entity code for explanation
  VName DispSpriteName[NUMPSPRITES]; // see entity code for explanation
  float PSpriteSY;
  float PSpriteWeaponLowerPrev;
  float PSpriteWeaponLoweringStartTime;
  float PSpriteWeaponLoweringDuration;

  float WorldTimer; // total time the player's been playing

  vuint8 ClientNum;

  vint32 SoundEnvironment;

  VClientGameBase *ClGame;

  VPlayerReplicationInfo *PlayerReplicationInfo;

  //VField *fldPendingWeapon;
  //VField *fldReadyWeapon;

  VObject *LastViewObject[NUMPSPRITES];
  //VObject *lastReadyWeapon;
  //VState *lastReadyWeaponReadyState;

  static bool isCheckpointSpawn;

private:
  inline void UpdateDispFrameFrom (int idx, const VState *st) {
    if (st) {
      if ((st->Frame&VState::FF_KEEPSPRITE) == 0 && st->SpriteIndex != 1) {
        DispSpriteFrame[idx] = (DispSpriteFrame[idx]&~0x00ffffff)|(st->SpriteIndex&0x00ffffff);
        DispSpriteName[idx] = st->SpriteName;
      }
      if ((st->Frame&VState::FF_DONTCHANGE) == 0) DispSpriteFrame[idx] = (DispSpriteFrame[idx]&0x00ffffff)|((st->Frame&VState::FF_FRAMEMASK)<<24);
    }
  }

public:
  //VBasePlayer () : UserInfo(E_NoInit), PlayerName(E_NoInit) {}
  virtual void PostCtor () override;

  void ResetButtons ();

  const int GetEffectiveSpriteIndex (int idx) const { return DispSpriteFrame[idx]&0x00ffffff; }
  const int GetEffectiveSpriteFrame (int idx) const { return ((DispSpriteFrame[idx]>>24)&VState::FF_FRAMEMASK); }

  inline VAliasModelFrameInfo getMFI (int idx) const {
    VAliasModelFrameInfo res;
    res.sprite = DispSpriteName[idx];
    res.frame = GetEffectiveSpriteFrame(idx);
    res.index = (ViewStates[idx].State ? ViewStates[idx].State->InClassIndex : -1);
    res.spriteIndex = GetEffectiveSpriteIndex(idx);
    return res;
  }

  //  VObject interface
  virtual bool ExecuteNetMethod(VMethod *) override;

  void SpawnClient();

  void Printf(const char *, ...) __attribute__((format(printf,2,3)));
  void CenterPrintf(const char *, ...) __attribute__((format(printf,2,3)));

  void SetViewState(int, VState *);
  void AdvanceViewStates(float);

  void SetUserInfo(VStr);
  void ReadFromUserInfo();

  //  Handling of player input.
  void StartPitchDrift();
  void StopPitchDrift();
  void AdjustAngles();
  void HandleInput();
  bool Responder(event_t *);
  void ClearInput();
  int AcsGetInput(int);

  //  Implementation of server to client events.
  void DoClientStartSound(int, TVec, int, int, float, float, bool);
  void DoClientStopSound(int, int);
  void DoClientStartSequence(TVec, int, VName, int);
  void DoClientAddSequenceChoice(int, VName);
  void DoClientStopSequence(int);
  void DoClientPrint(VStr);
  void DoClientChatPrint(VStr nick, VStr text);
  void DoClientCenterPrint(VStr);
  void DoClientSetAngles(TAVec);
  void DoClientIntermission(VName);
  void DoClientPause(bool);
  void DoClientSkipIntermission();
  void DoClientFinale(VStr);
  void DoClientChangeMusic(VName);
  void DoClientSetServerInfo(VStr, VStr);
  void DoClientHudMessage(VStr, VName, int, int, int, VStr,
    float, float, int, int, float, float, float);

  void WriteViewData();

  // append player commands with the given prefix
  void ListConCommands (TArray<VStr> &list, VStr pfx);

  bool IsConCommand (VStr name);

  // returns `true` if command was found and executed
  // uses VCommand command line
  bool ExecConCommand ();

  // returns `true` if command was found (autocompleter may be still missing)
  // autocompleter should filter list
  bool ExecConCommandAC (TArray<VStr> &args, bool newArg, TArray<VStr> &aclist);

  void CallDumpInventory ();

  DECLARE_FUNCTION(get_IsCheckpointSpawn)

  DECLARE_FUNCTION(cprint)
  DECLARE_FUNCTION(centerprint)
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
  DECLARE_FUNCTION(ClientChatPrint)
  DECLARE_FUNCTION(ClientCenterPrint)
  DECLARE_FUNCTION(ClientSetAngles)
  DECLARE_FUNCTION(ClientIntermission)
  DECLARE_FUNCTION(ClientPause)
  DECLARE_FUNCTION(ClientSkipIntermission)
  DECLARE_FUNCTION(ClientFinale)
  DECLARE_FUNCTION(ClientChangeMusic)
  DECLARE_FUNCTION(ClientSetServerInfo)
  DECLARE_FUNCTION(ClientHudMessage)

  DECLARE_FUNCTION(ServerSetUserInfo)

  DECLARE_FUNCTION(QS_PutInt);
  DECLARE_FUNCTION(QS_PutName);
  DECLARE_FUNCTION(QS_PutStr);
  DECLARE_FUNCTION(QS_PutFloat);

  DECLARE_FUNCTION(QS_GetInt);
  DECLARE_FUNCTION(QS_GetName);
  DECLARE_FUNCTION(QS_GetStr);
  DECLARE_FUNCTION(QS_GetFloat);

  bool IsCheckpointPossible () { static VMethodProxy method("IsCheckpointPossible"); vobjPutParamSelf(); VMT_RET_BOOL(method); }

  // player events
  void eventPutClientIntoServer () { static VMethodProxy method("PutClientIntoServer"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventSpawnClient () { static VMethodProxy method("SpawnClient"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventNetGameReborn () { static VMethodProxy method("NetGameReborn"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventDisconnectClient () { static VMethodProxy method("DisconnectClient"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventUserinfoChanged () { static VMethodProxy method("UserinfoChanged"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPlayerExitMap (bool clusterChange) { static VMethodProxy method("PlayerExitMap"); vobjPutParamSelf(clusterChange); VMT_RET_VOID(method); }
  void eventPlayerBeforeExitMap () { static VMethodProxy method("PlayerBeforeExitMap"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPlayerTick (float deltaTime) { static VMethodProxy method("PlayerTick"); vobjPutParamSelf(deltaTime); VMT_RET_VOID(method); }
  void eventClientTick (float deltaTime) { static VMethodProxy method("ClientTick"); vobjPutParamSelf(deltaTime); VMT_RET_VOID(method); }
  void eventSetViewPos () { static VMethodProxy method("SetViewPos"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPreTravel () { static VMethodProxy method("PreTravel"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventUseInventory (VStr Inv) { static VMethodProxy method("UseInventory"); vobjPutParamSelf(Inv); VMT_RET_VOID(method); }
  bool eventCheckDoubleFiringSpeed () { static VMethodProxy method("CheckDoubleFiringSpeed"); vobjPutParamSelf(); VMT_RET_BOOL(method); }

  void eventResetInventory () { static VMethodProxy method("ResetInventory"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventResetHealth () { static VMethodProxy method("ResetHealth"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventRemoveKeys () { static VMethodProxy method("RemoveKeys"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventPreraiseWeapon () { static VMethodProxy method("PreraiseWeapon"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventResetToDefaults () { static VMethodProxy method("ResetToDefaults"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventOnSaveLoaded () { static VMethodProxy method("eventOnSaveLoaded"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void eventOnBeforeSave (bool autosave, bool checkpoint) { static VMethodProxy method("eventOnBeforeSave"); vobjPutParamSelf(autosave, checkpoint); VMT_RET_VOID(method); }
  void eventOnAfterSave (bool autosave, bool checkpoint) { static VMethodProxy method("eventOnAfterSave"); vobjPutParamSelf(autosave, checkpoint); VMT_RET_VOID(method); }

  void eventAfterUnarchiveThinkers () { static VMethodProxy method("eventAfterUnarchiveThinkers"); vobjPutParamSelf(); VMT_RET_VOID(method); }

  void QS_Save () { static VMethodProxy method("QS_Save"); vobjPutParamSelf(); VMT_RET_VOID(method); }
  void QS_Load () { static VMethodProxy method("QS_Load"); vobjPutParamSelf(); VMT_RET_VOID(method); }

  // cheats
  /*
  void eventCheat_VScriptCommand (TArray<VStr> &args) {
    static VMethodProxy method("Cheat_VScriptCommand");
    vobjPutParamSelf((void *)&args);
    VMT_RET_VOID(method);
  }
  */

  // server to client events
  void eventClientStartSound (int SoundId, TVec Org, int OriginId,
                              int Channel, float Volume, float Attenuation, bool Loop)
  {
    static VMethodProxy method("ClientStartSound");
    vobjPutParamSelf(SoundId, Org, OriginId, Channel, Volume, Attenuation, Loop);
    VMT_RET_VOID(method);
  }
  void eventClientStopSound (int OriginId, int Channel) {
    static VMethodProxy method("ClientStopSound");
    vobjPutParamSelf(OriginId, Channel);
    VMT_RET_VOID(method);
  }
  void eventClientStartSequence (TVec Origin, int OriginId, VName Name, int ModeNum) {
    static VMethodProxy method("ClientStartSequence");
    vobjPutParamSelf(Origin, OriginId, Name, ModeNum);
    VMT_RET_VOID(method);
  }
  void eventClientAddSequenceChoice (int OriginId, VName Choice) {
    static VMethodProxy method("ClientAddSequenceChoice");
    vobjPutParamSelf(OriginId, Choice);
    VMT_RET_VOID(method);
  }
  void eventClientStopSequence (int OriginId) {
    static VMethodProxy method("ClientStopSequence");
    vobjPutParamSelf(OriginId);
    VMT_RET_VOID(method);
  }
  void eventClientPrint (VStr Str) {
    static VMethodProxy method("ClientPrint");
    vobjPutParamSelf(Str);
    VMT_RET_VOID(method);
  }
  void eventClientChatPrint (VStr nick, VStr str) {
    static VMethodProxy method("ClientChatPrint");
    vobjPutParamSelf(nick, str);
    VMT_RET_VOID(method);
  }
  void eventClientCenterPrint (VStr Str) {
    static VMethodProxy method("ClientCenterPrint");
    vobjPutParamSelf(Str);
    VMT_RET_VOID(method);
  }
  void eventClientSetAngles (TAVec Angles) {
    static VMethodProxy method("ClientSetAngles");
    vobjPutParamSelf(Angles);
    VMT_RET_VOID(method);
  }
  void eventClientIntermission (VName NextMap) {
    static VMethodProxy method("ClientIntermission");
    vobjPutParamSelf(NextMap);
    VMT_RET_VOID(method);
  }
  void eventClientPause (bool Paused) {
    static VMethodProxy method("ClientPause");
    vobjPutParamSelf(Paused);
    VMT_RET_VOID(method);
  }
  void eventClientSkipIntermission () {
    static VMethodProxy method("ClientSkipIntermission");
    vobjPutParamSelf();
    VMT_RET_VOID(method);
  }
  void eventClientFinale (VStr Type) {
    static VMethodProxy method("ClientFinale");
    vobjPutParamSelf(Type);
    VMT_RET_VOID(method);
  }
  void eventClientChangeMusic (VName Song) {
    static VMethodProxy method("ClientChangeMusic");
    vobjPutParamSelf(Song);
    VMT_RET_VOID(method);
  }
  void eventClientSetServerInfo (VStr Key, VStr Value) {
    static VMethodProxy method("ClientSetServerInfo");
    vobjPutParamSelf(Key, Value);
    VMT_RET_VOID(method);
  }
  void eventClientHudMessage (VStr Message, VName Font, int Type,
                              int Id, int Color, VStr ColorName, float x, float y,
                              int HudWidth, int HudHeight, float HoldTime, float Time1, float Time2)
  {
    static VMethodProxy method("ClientHudMessage");
    vobjPutParamSelf(Message, Font, Type, Id, Color, ColorName, x, y, HudWidth, HudHeight, HoldTime, Time1, Time2);
    VMT_RET_VOID(method);
  }

  // client to server events
  void eventServerImpulse (int AImpulse) {
    static VMethodProxy method("ServerImpulse");
    vobjPutParamSelf(AImpulse);
    VMT_RET_VOID(method);
  }
  void eventServerSetUserInfo (VStr Info) {
    static VMethodProxy method("ServerSetUserInfo");
    vobjPutParamSelf(Info);
    VMT_RET_VOID(method);
  }

  bool eventIsReadyWeaponByName (VStr classname, bool allowReplace) {
    static VMethodProxy method("eventIsReadyWeaponByName");
    vobjPutParamSelf(classname, allowReplace);
    VMT_RET_BOOL(method);
  }

  VEntity *eventFindInventoryWeapon (VStr classname, bool allowReplace) {
    static VMethodProxy method("eventFindInventoryWeapon");
    vobjPutParamSelf(classname, allowReplace);
    VMT_RET_REF(VEntity, method);
  }

  VEntity *eventGetReadyWeapon () {
    static VMethodProxy method("eventGetReadyWeapon");
    vobjPutParamSelf();
    VMT_RET_REF(VEntity, method);
  }

  void eventSetReadyWeapon (VEntity *ent, bool instant) {
    static VMethodProxy method("eventSetReadyWeapon");
    vobjPutParamSelf(ent, instant);
    VMT_RET_VOID(method);
  }

  bool eventSetPendingWeapon (VEntity *ent) {
    static VMethodProxy method("eventSetPendingWeapon");
    vobjPutParamSelf(ent);
    VMT_RET_BOOL(method);
  }

  VStr GetCurrentArmorClassName () { static VMethodProxy method("GetCurrentArmorClassName"); vobjPutParamSelf(); VMT_RET_STR(method); }
  int GetCurrentArmorSaveAmount () { static VMethodProxy method("GetCurrentArmorSaveAmount"); vobjPutParamSelf(); VMT_RET_INT(method); }
  float GetCurrentArmorSavePercent () { static VMethodProxy method("GetCurrentArmorSavePercent"); vobjPutParamSelf(); VMT_RET_FLOAT(method); }
  int GetCurrentArmorMaxAbsorb () { static VMethodProxy method("GetCurrentArmorMaxAbsorb"); vobjPutParamSelf(); VMT_RET_INT(method); }
  int GetCurrentArmorFullAbsorb () { static VMethodProxy method("GetCurrentArmorFullAbsorb"); vobjPutParamSelf(); VMT_RET_INT(method); }
  int GetCurrentArmorActualSaveAmount () { static VMethodProxy method("GetCurrentArmorActualSaveAmount"); vobjPutParamSelf(); VMT_RET_INT(method); }
};
