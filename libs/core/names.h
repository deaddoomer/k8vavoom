//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
//**  Copyright (C) 1999-2010 Jānis Legzdiņš
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
//**
//**  Header file registering global hardcoded Vavoom names.
//**
//**************************************************************************

// define a message as an enumeration
#ifndef REGISTER_NAME
  #define REGISTER_NAME(name) NAME_##name,
  #define REGISTER_NAME_EX(name,str_) NAME_##name,
  #define REGISTERING_ENUM
  enum EName {
    // special zero value, meaning no name
    NAME_none,
#endif

#define NAME_None  NAME_none


// ////////////////////////////////////////////////////////////////////////// //
// hardcoded names

// log messages
REGISTER_NAME(Log)
REGISTER_NAME(Init)
REGISTER_NAME(Dev)
REGISTER_NAME(DevNet)
REGISTER_NAME(Warning)
REGISTER_NAME(Error)
REGISTER_NAME(Debug)
REGISTER_NAME(DebugGL)
REGISTER_NAME(Chat)
REGISTER_NAME(Bot)
REGISTER_NAME(BotDev)
REGISTER_NAME(BotDevAI)
REGISTER_NAME(BotDevRoam)
REGISTER_NAME(BotDevCheckPos)
REGISTER_NAME(BotDevItems)
REGISTER_NAME(BotDevAttack)
REGISTER_NAME(BotDevPath)
REGISTER_NAME(BotDevCrumbs)
REGISTER_NAME(BotDevPlanPath)

// native class names
REGISTER_NAME(Object)
REGISTER_NAME(GameObject)
REGISTER_NAME(Thinker)
REGISTER_NAME(LevelInfo)
REGISTER_NAME(WorldInfo)
REGISTER_NAME(GameInfo)
REGISTER_NAME(Entity)
REGISTER_NAME(EntityEx)
REGISTER_NAME(BasePlayer)
REGISTER_NAME(PlayerReplicationInfo)
REGISTER_NAME(ViewEntity)
REGISTER_NAME(Acs)
REGISTER_NAME(ThinkButton)
REGISTER_NAME(Level)
REGISTER_NAME(VLevel)
REGISTER_NAME(Widget)
REGISTER_NAME(RootWidget)
REGISTER_NAME(ActorDisplayWindow)
REGISTER_NAME(ClientGameBase)
REGISTER_NAME(ClientState)
REGISTER_NAME(ScriptsParser)
REGISTER_NAME(Actor)

// special struct names
REGISTER_NAME(TVec)
REGISTER_NAME(TAVec)

// Thinker events
REGISTER_NAME(Tick)
REGISTER_NAME(ClientTick)

// Entity events
REGISTER_NAME(OnMapSpawn)
REGISTER_NAME(BeginPlay)
REGISTER_NAME(Destroyed)
REGISTER_NAME(Touch)
REGISTER_NAME(BlastedHitLine)
REGISTER_NAME(CheckForPushSpecial)
REGISTER_NAME(ApplyFriction)
REGISTER_NAME(HandleFloorclip)
REGISTER_NAME(CrossSpecialLine)
REGISTER_NAME(SectorChanged)
REGISTER_NAME(RoughCheckThing)
REGISTER_NAME(ClearInventory)
REGISTER_NAME(GiveInventory)
REGISTER_NAME(TakeInventory)
REGISTER_NAME(CheckInventory)
REGISTER_NAME(UseInventoryName)
REGISTER_NAME(GetSigilPieces)
REGISTER_NAME(GetArmorPoints)
REGISTER_NAME(CheckNamedWeapon)
REGISTER_NAME(SetNamedWeapon)
REGISTER_NAME(GetAmmoCapacity)
REGISTER_NAME(SetAmmoCapacity)
REGISTER_NAME(MoveThing)
REGISTER_NAME(GetStateTime)
REGISTER_NAME(SetActorProperty)
REGISTER_NAME(GetActorProperty)
REGISTER_NAME(CheckForSectorActions)
REGISTER_NAME(SkyBoxGetAlways)
REGISTER_NAME(SkyBoxGetMate)
REGISTER_NAME(SkyBoxGetPlaneAlpha)
REGISTER_NAME(CalcFakeZMovement)
REGISTER_NAME(ClassifyActor)
REGISTER_NAME(MorphActor)
REGISTER_NAME(UnmorphActor)
REGISTER_NAME(GetViewEntRenderParams)

// LevelInfo events
REGISTER_NAME(SpawnSpecials)
REGISTER_NAME(UpdateSpecials)
REGISTER_NAME(AfterUnarchiveThinkers)
REGISTER_NAME(FindLine)
REGISTER_NAME(PolyThrustMobj)
REGISTER_NAME(PolyCrushMobj)
REGISTER_NAME(TagBusy)
REGISTER_NAME(PolyBusy)
REGISTER_NAME(ThingCount)
REGISTER_NAME(FindMobjFromTID)
REGISTER_NAME(ExecuteActionSpecial)
REGISTER_NAME(EV_ThingProjectile)
REGISTER_NAME(StartPlaneWatcher)
REGISTER_NAME(SpawnMapThing)
REGISTER_NAME(UpdateParticle)
REGISTER_NAME(AcsSpawnThing)
REGISTER_NAME(AcsSpawnSpot)
REGISTER_NAME(AcsSpawnSpotFacing)
REGISTER_NAME(SectorDamage)
REGISTER_NAME(DoThingDamage)
REGISTER_NAME(SetMarineWeapon)
REGISTER_NAME(SetMarineSprite)
REGISTER_NAME(AcsFadeRange)
REGISTER_NAME(AcsCancelFade)

// BasePlayer events
REGISTER_NAME(PutClientIntoServer)
REGISTER_NAME(SpawnClient)
REGISTER_NAME(NetGameReborn)
REGISTER_NAME(DisconnectClient)
REGISTER_NAME(UserinfoChanged)
REGISTER_NAME(PlayerExitMap)
REGISTER_NAME(PlayerBeforeExitMap)
REGISTER_NAME(PlayerTick)
REGISTER_NAME(SetViewPos)
REGISTER_NAME(PreTravel)
REGISTER_NAME(UseInventory)
REGISTER_NAME(CheckDoubleFiringSpeed)
REGISTER_NAME(Cheat_Resurrect)
REGISTER_NAME(Cheat_God)
REGISTER_NAME(Cheat_NoClip)
REGISTER_NAME(Cheat_Gimme)
REGISTER_NAME(Cheat_KillAll)
REGISTER_NAME(Cheat_Morph)
REGISTER_NAME(Cheat_NoWeapons)
REGISTER_NAME(Cheat_Class)
REGISTER_NAME(Cheat_Fly)
REGISTER_NAME(Cheat_NoTarget)
REGISTER_NAME(Cheat_Anubis)
REGISTER_NAME(Cheat_Freeze)
REGISTER_NAME(Cheat_Buddha)
REGISTER_NAME(Cheat_Summon)
REGISTER_NAME(ClientStartSound)
REGISTER_NAME(ClientStopSound)
REGISTER_NAME(ClientStartSequence)
REGISTER_NAME(ClientAddSequenceChoice)
REGISTER_NAME(ClientStopSequence)
REGISTER_NAME(ClientForceLightning)
REGISTER_NAME(ClientPrint)
REGISTER_NAME(ClientCenterPrint)
REGISTER_NAME(ClientSetAngles)
REGISTER_NAME(ClientIntermission)
REGISTER_NAME(ClientPause)
REGISTER_NAME(ClientSkipIntermission)
REGISTER_NAME(ClientFinale)
REGISTER_NAME(ClientChangeMusic)
REGISTER_NAME(ClientSetServerInfo)
REGISTER_NAME(ClientHudMessage)
REGISTER_NAME(ServerImpulse)
REGISTER_NAME(ServerSetUserInfo)

// ClientGameBase events
REGISTER_NAME(RootWindowCreated)
REGISTER_NAME(Connected)
REGISTER_NAME(Disconnected)
REGISTER_NAME(DemoPlaybackStarted)
REGISTER_NAME(DemoPlaybackStopped)
REGISTER_NAME(OnHostEndGame)
REGISTER_NAME(OnHostError)
REGISTER_NAME(StatusBarStartMap)
REGISTER_NAME(StatusBarDrawer)
REGISTER_NAME(StatusBarUpdateWidgets)
REGISTER_NAME(IintermissionStart)
REGISTER_NAME(StartFinale)
REGISTER_NAME(FinaleResponder)
REGISTER_NAME(DeactivateMenu)
REGISTER_NAME(MenuResponder)
REGISTER_NAME(MenuActive)
REGISTER_NAME(SetMenu)
REGISTER_NAME(MessageBoxDrawer)
REGISTER_NAME(MessageBoxResponder)
REGISTER_NAME(MessageBoxActive)
REGISTER_NAME(DrawViewBorder)
REGISTER_NAME(AddNotifyMessage)
REGISTER_NAME(AddCenterMessage)
REGISTER_NAME(AddHudMessage)

// Widget events
REGISTER_NAME(OnCreate)
REGISTER_NAME(OnDestroy)
REGISTER_NAME(OnChildAdded)
REGISTER_NAME(OnChildRemoved)
REGISTER_NAME(OnConfigurationChanged)
REGISTER_NAME(OnVisibilityChanged)
REGISTER_NAME(OnEnableChanged)
REGISTER_NAME(OnFocusableChanged)
REGISTER_NAME(OnFocusReceived)
REGISTER_NAME(OnFocusLost)
REGISTER_NAME(OnDraw)
REGISTER_NAME(OnPostDraw)
REGISTER_NAME(OnKeyDown)
REGISTER_NAME(OnKeyUp)
REGISTER_NAME(OnMouseMove)
REGISTER_NAME(OnMouseEnter)
REGISTER_NAME(OnMouseLeave)
REGISTER_NAME(OnMouseDown)
REGISTER_NAME(OnMouseUp)
REGISTER_NAME(OnMouseClick)
REGISTER_NAME(OnMMouseClick)
REGISTER_NAME(OnRMouseClick)

// Lump names
REGISTER_NAME(s_start)
REGISTER_NAME(s_end)
REGISTER_NAME(ss_start)
REGISTER_NAME(ss_end)
REGISTER_NAME(f_start)
REGISTER_NAME(f_end)
REGISTER_NAME(ff_start)
REGISTER_NAME(ff_end)
REGISTER_NAME(c_start)
REGISTER_NAME(c_end)
REGISTER_NAME(cc_start)
REGISTER_NAME(cc_end)
REGISTER_NAME(a_start)
REGISTER_NAME(a_end)
REGISTER_NAME(aa_start)
REGISTER_NAME(aa_end)
REGISTER_NAME(tx_start)
REGISTER_NAME(tx_end)
REGISTER_NAME(v_start)
REGISTER_NAME(v_end)
REGISTER_NAME(vv_start)
REGISTER_NAME(vv_end)
REGISTER_NAME(hi_start)
REGISTER_NAME(hi_end)
REGISTER_NAME(pr_start)
REGISTER_NAME(pr_end)
REGISTER_NAME(pnames)
REGISTER_NAME(texture1)
REGISTER_NAME(texture2)
REGISTER_NAME(f_sky)
REGISTER_NAME(f_sky001)
REGISTER_NAME(f_sky1)
REGISTER_NAME(autopage)
REGISTER_NAME(animated)
REGISTER_NAME(switches)
REGISTER_NAME(animdefs)
REGISTER_NAME(hirestex)
REGISTER_NAME(skyboxes)
REGISTER_NAME(loadacs)
REGISTER_NAME(playpal)
REGISTER_NAME(colormap)
REGISTER_NAME(fogmap)
REGISTER_NAME(x11r6rgb)
REGISTER_NAME(translat)
REGISTER_NAME(sndcurve)
REGISTER_NAME(sndinfo)
REGISTER_NAME(sndseq)
REGISTER_NAME(reverbs)
REGISTER_NAME(mapinfo)
REGISTER_NAME(terrain)
REGISTER_NAME(vterrain)
REGISTER_NAME(lockdefs)
REGISTER_NAME(things)
REGISTER_NAME(linedefs)
REGISTER_NAME(sidedefs)
REGISTER_NAME(vertexes)
REGISTER_NAME(segs)
REGISTER_NAME(ssectors)
REGISTER_NAME(nodes)
REGISTER_NAME(sectors)
REGISTER_NAME(reject)
REGISTER_NAME(blockmap)
REGISTER_NAME(behavior)
REGISTER_NAME(textmap)
REGISTER_NAME(dialogue)
REGISTER_NAME(znodes)
REGISTER_NAME(endmap)
REGISTER_NAME(gl_level)
REGISTER_NAME(gl_pvs)
REGISTER_NAME(endoom)
REGISTER_NAME(endtext)
REGISTER_NAME(endstrf)
REGISTER_NAME(teleicon)
REGISTER_NAME(saveicon)
REGISTER_NAME(loadicon)
REGISTER_NAME(ammnum0)
REGISTER_NAME(fonta_s)
REGISTER_NAME(stbfn033)
REGISTER_NAME(textcolo)
REGISTER_NAME(fontdefs)
REGISTER_NAME(engine)
REGISTER_NAME(cgame)
REGISTER_NAME(game)
REGISTER_NAME(decorate)
REGISTER_NAME(decaldef)
REGISTER_NAME(language)
REGISTER_NAME(titlemap)
REGISTER_NAME(vfxdefs)
REGISTER_NAME(gldefs)
REGISTER_NAME(doomdefs)
REGISTER_NAME(hticdefs)
REGISTER_NAME(hexndefs)
REGISTER_NAME(strfdefs)
REGISTER_NAME(keyconf)
REGISTER_NAME(textures)

// more useless lump names
REGISTER_NAME(gl_vert)
REGISTER_NAME(gl_segs)
REGISTER_NAME(gl_ssect)
REGISTER_NAME(gl_nodes)
REGISTER_NAME(script)
REGISTER_NAME(scripts)
REGISTER_NAME(zscript)
REGISTER_NAME(dehacked)
REGISTER_NAME(sprofs)

// standard font names
REGISTER_NAME(smallfont)
REGISTER_NAME(smallfont2)
REGISTER_NAME(bigfont)
REGISTER_NAME(consolefont)
REGISTER_NAME(confont)

// compiler names
REGISTER_NAME(Num)
REGISTER_NAME(Length)
REGISTER_NAME(length)
REGISTER_NAME(Insert)
REGISTER_NAME(Remove)
REGISTER_NAME(Goto)
REGISTER_NAME(Stop)
REGISTER_NAME(Wait)
REGISTER_NAME(Fail)
REGISTER_NAME(Loop)
REGISTER_NAME(Super)
REGISTER_NAME(Bright)
REGISTER_NAME(Offset)

REGISTER_NAME(Video)
REGISTER_NAME(GLVideo)
REGISTER_NAME(IniFile)
REGISTER_NAME(GLTexture)
REGISTER_NAME(FileReader)
REGISTER_NAME(FileWriter)
REGISTER_NAME(SoundSystem)
REGISTER_NAME(SoundSFXManager)

REGISTER_NAME(DFMap)

REGISTER_NAME(Console)

REGISTER_NAME(decoDoCheckFlag)
REGISTER_NAME(decoDoSetFlag)

REGISTER_NAME(Socket)

// micro a-star interface
REGISTER_NAME(MiAStarGraphBase)
REGISTER_NAME(MiAStarNodeBase)

REGISTER_NAME(EntityGridBase)

REGISTER_NAME(Level_K8BDW)
REGISTER_NAME(Level_K8Gore)

REGISTER_NAME_EX(Minus_Sign, "-")


// closing
#ifdef REGISTERING_ENUM
    NUM_HARDCODED_NAMES
  };
  #undef REGISTER_NAME
  #undef REGISTER_NAME_EX
  #undef REGISTERING_ENUM
#endif
