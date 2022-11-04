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
//** directly included from "p_acs.cpp"
//**************************************************************************


//==========================================================================
//
//  VAcs::CallFunction
//
//==========================================================================
int VAcs::CallFunction (line_t *actline, int argCount, int funcIndex, vint32 *args) {
#if !USE_COMPUTED_GOTO
  {
    VStr dstr;
    for (const ACSF_Info *nfo = ACSF_List; nfo->name; ++nfo) {
      if (nfo->index == funcIndex) {
        dstr = va("[%d] %s", funcIndex, nfo->name);
        break;
      }
    }
    if (!dstr.length()) dstr = va("OPC[%d]", funcIndex);
    dstr += va("; argCount=%d", argCount);
    for (int f = 0; f < argCount; ++f) dstr += va("; <#%d>:%d", f, args[f]);
    GCon->Logf("ACS:  ACSF: %s", *dstr);
  }
#endif

  switch (funcIndex) {
    // int SpawnSpotForced (str classname, int spottid, int tid, int angle)
    case ACSF_SpawnSpotForced:
      //GCon->Logf("ACS: SpawnSpotForced: classname=<%s>; spottid=%d; tid=%d; angle=%f", *GetNameLowerCase(args[0]), args[1], args[2], float(args[3])*45.0f/32.0f);
      if (argCount >= 4) {
        return Level->eventAcsSpawnSpot(GetNameLowerCase(args[0]), args[1], args[2], float(args[3])*45.0f/32.0f, true); // forced
      }
      return 0;

    // int SpawnSpotFacingForced (str classname, int spottid, int tid)
    case ACSF_SpawnSpotFacingForced:
      if (argCount >= 3) {
        return Level->eventAcsSpawnSpotFacing(GetNameLowerCase(args[0]), args[1], args[2], true);
      }
      return false;

    // int SpawnForced (str classname, fixed x, fixed y, fixed z [, int tid [, int angle]])
    case ACSF_SpawnForced:
      if (argCount >= 4) {
        //GCon->Logf(NAME_Debug, "ACSF_SpawnForced: name=%s; pos=(%g,%g,%g); tid=%d; angle=%g", *GetNameLowerCase(args[0]), float(args[1])/65536.0f, float(args[2])/65536.0f, float(args[3])/65536.0f, (argCount >= 4 ? args[4] : 0), (argCount >= 5 ? float(args[5])*45.0f/32.0f : 0));
        return Level->eventAcsSpawnThing(GetNameLowerCase(args[0]),
                        TVec(float(args[1])/65536.0f, float(args[2])/65536.0f, float(args[3])/65536.0f), // x, y, z
                        (argCount >= 4 ? args[4] : 0), // tid
                        (argCount >= 5 ? float(args[5])*45.0f/32.0f : 0), // angle
                        true); // forced
      }
      return 0;


    case ACSF_CheckActorClass:
      if (argCount >= 2) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        VStr name = GetStr(args[1]);
        if (name.length() == 0) return 0;
        //GCon->Logf(NAME_Debug, "ACSF_CheckActorClass:000: `%s` : '%s'", Ent->GetClass()->GetName(), *name);
        if (name.strEquCI(Ent->GetClass()->GetName())) return 1;
        // check replaced class (but not for players)
        // skip this for players, because most of the time checking player class via ACS should be strict
        if (Ent->IsPlayer()) return 0;
        VClass *rpc = Ent->GetClass()->GetReplacee();
        if (!rpc || rpc == Ent->GetClass() || !rpc->IsChildOf(VEntity::StaticClass())) return 0;
        //GCon->Logf(NAME_Debug, "ACSF_CheckActorClass:001: `%s` : '%s'", rpc->GetName(), *name);
        return (name.strEquCI(rpc->GetName()) ? 1 : 0);
      }
      return 0;

    case ACSF_GetActorClass:
      if (argCount > 0) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          if (Ent->IsPlayer() && acs_fake_player_class.asStr().length()) {
            return ActiveObject->Level->PutNewString(acs_fake_player_class.asStr());
          }
          return ActiveObject->Level->PutNewString(*Ent->GetClass()->Name);
        }
      }
      return ActiveObject->Level->PutNewString("");

    case ACSF_GetWeapon:
      if (Activator && Activator->IsPlayer() && Activator->Player) {
        VEntity *wpn = Activator->Player->eventGetReadyWeapon();
        if (wpn) return ActiveObject->Level->PutNewString(*wpn->GetClass()->Name);
      }
      return ActiveObject->Level->PutNewString("");

    // int GetArmorType (string armortype, int playernum)
    case ACSF_GetArmorType:
      if (argCount >= 2) {
        int pidx = args[1];
        VBasePlayer *plr = SV_GetPlayerByNum(pidx);
        if (!plr || !plr->MO) return 0;
        if (!(plr->PlayerFlags&VBasePlayer::PF_Spawned)) return 0;
        if (plr->MO->Health <= 0) return 0; // it is dead
        VName atype = GetNameLowerCase(args[0]);
        if (atype == NAME_None || atype == "none") return 0;
        return plr->MO->eventGetArmorPointsForType(atype);
      }
      return 0;

    // str GetArmorInfo (int infotype)
    // int GetArmorInfo (int infotype)
    // fixed GetArmorInfo (int infotype)
    case ACSF_GetArmorInfo:
      if (argCount > 0) {
        if (!Activator || !Activator->IsPlayer() || !Activator->Player) {
          // what to do here?
          if (args[0] == ARMORINFO_CLASSNAME) return ActiveObject->Level->PutNewString("None");
          return 0;
        }
        VBasePlayer *plr = Activator->Player;
        switch (args[0]) {
          case ARMORINFO_CLASSNAME: return ActiveObject->Level->PutNewString(*plr->GetCurrentArmorClassName());
          case ARMORINFO_SAVEAMOUNT: return plr->GetCurrentArmorSaveAmount();
          case ARMORINFO_SAVEPERCENT: return (int)(plr->GetCurrentArmorSavePercent()*65536.0f);
          case ARMORINFO_MAXABSORB: return plr->GetCurrentArmorMaxAbsorb();
          case ARMORINFO_MAXFULLABSORB: return plr->GetCurrentArmorFullAbsorb();
          case ARMORINFO_ACTUALSAVEAMOUNT: return plr->GetCurrentArmorActualSaveAmount();
          default: GCon->Logf(NAME_Warning, "ACS: GetArmorInfo got invalid requiest #%d", args[0]); break;
        }
      } else {
        Host_Error("ACS: GetArmorInfo requires argument");
      }
      return 0;

    // fixed GetActorViewHeight (int tid)
    case ACSF_GetActorViewHeight:
      if (argCount >= 1) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) return (int)(Ent->GetViewHeight()*65536.0f);
      }
      return 0;


    // bool ACS_NamedTerminate (string script, int map) -- always returns `true`
    case ACSF_ACS_NamedTerminate:
      if (argCount >= 2) {
        VStr scname = VStr(GetNameLowerCase(args[0]));
        XLevel->TerminateNamedScriptThinkers(scname, args[1]);
      }
      return 1;

    // bool ACS_NamedSuspend (string script, int map) -- always returns `true`
    case ACSF_ACS_NamedSuspend:
      if (argCount >= 2) {
        VStr scname = VStr(GetNameLowerCase(args[0]));
        XLevel->SuspendNamedScriptThinkers(scname, args[1]);
      }
      return 1;

    // bool ACS_NamedExecute (string script, int map, int s_arg1, int s_arg2, int s_arg3)
    case ACSF_ACS_NamedExecute:
      if (argCount > 0) {
        //VAcsObject *ao = nullptr;
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        int ScArgs[4];
        ScArgs[0] = (argCount > 2 ? args[2] : 0);
        ScArgs[1] = (argCount > 3 ? args[3] : 0);
        ScArgs[2] = (argCount > 4 ? args[4] : 0);
        ScArgs[3] = (argCount > 5 ? args[5] : 0);
        if (!ActiveObject->Level->Start(-name.GetIndex(), (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, false/*always*/, false/*wantresult*/, false/*net*/)) return 0;
        return 1;
      }
      return 0;

    // int ACS_NamedExecuteWithResult (string script, int s_arg1, int s_arg2, int s_arg3, int s_arg4)
    case ACSF_ACS_NamedExecuteWithResult:
      if (argCount > 0) {
        //VAcsObject *ao = nullptr;
        VName name = GetNameLowerCase(args[0]);
        /*
        if (argCount > 4) {
          if (args[4] != 0) Host_Error("ACS_NamedExecuteWithResult(%s): s_arg4=%d", *name, args[4]);
        }
        */
        if (name == NAME_None) return 0;
        int ScArgs[4];
        ScArgs[0] = (argCount > 1 ? args[1] : 0);
        ScArgs[1] = (argCount > 2 ? args[2] : 0);
        ScArgs[2] = (argCount > 3 ? args[3] : 0);
        ScArgs[3] = (argCount > 4 ? args[4] : 0);
        int res = 0;
        ActiveObject->Level->Start(-name.GetIndex(), 0, ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, true/*always*/, true/*wantresult*/, false/*net*/, &res);
        return res;
      }
      return 0;

    // bool ACS_NamedExecuteAlways (string script, int map, int s_arg1, int s_arg2, int s_arg3)
    case ACSF_ACS_NamedExecuteAlways:
      if (argCount > 0) {
        //VAcsObject *ao = nullptr;
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        int ScArgs[4];
        ScArgs[0] = (argCount > 2 ? args[2] : 0);
        ScArgs[1] = (argCount > 3 ? args[3] : 0);
        ScArgs[2] = (argCount > 4 ? args[4] : 0);
        ScArgs[3] = (argCount > 5 ? args[5] : 0);
        //GCon->Logf(NAME_Debug, "ACSF_ACS_NamedExecuteAlways: script=<%s>; map=%d; args=(%d,%d,%d,%d)", *name, (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3]);
        if (!ActiveObject->Level->Start(-name.GetIndex(), (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, true/*always*/, false/*wantresult*/, false/*net*/)) {
          //GCon->Logf(NAME_Debug, "   FAILED!");
          return 0;
        }
        //GCon->Logf(NAME_Debug, "   OK!");
        return 1;
      }
      return 0;


    case ACSF_ACS_NamedLockedExecute:
      if (argCount >= 5) {
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        if (!Level->eventCheckLock(Activator, args[4], false)) return 0;
        int ScArgs[4];
        ScArgs[0] = args[2];
        ScArgs[1] = args[3];
        ScArgs[2] = 0;
        ScArgs[3] = 0;
        if (!ActiveObject->Level->Start(-name.GetIndex(), (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, false/*always*/, false/*wantresult*/, false/*net*/)) return 0;
        return 1;
      }
      return 0;

    case ACSF_ACS_NamedLockedExecuteDoor:
      if (argCount >= 5) {
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        if (!Level->eventCheckLock(Activator, args[4], true)) return 0;
        int ScArgs[4];
        ScArgs[0] = args[2];
        ScArgs[1] = args[3];
        ScArgs[2] = 0;
        ScArgs[3] = 0;
        if (!ActiveObject->Level->Start(-name.GetIndex(), (argCount > 1 ? args[1] : 0), ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], Activator, line, side, false/*always*/, false/*wantresult*/, false/*net*/)) return 0;
        return 1;
      }
      return 0;


    // bool CheckFlag (int tid, str flag)
    case ACSF_CheckFlag:
      {
        VName name = GetNameLowerCase(args[1]);
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        //sp[-1] = vint32(Ent->Origin.x * 0x10000);
        //return (Ent->eventCheckFlag(*name) ? 1 : 0);
        return Ent->GetDecorateFlag(*name);
      }

    // int SetActorFlag (int tid, str flagname, bool value);
    case ACSF_SetActorFlag:
      {
        VStr name = VStr(GetNameLowerCase(args[1]));
        int count = 0;
        if (name.length()) {
          if (args[0] == 0) {
            // activator
            if (Activator && Activator->SetDecorateFlag(name, !!args[2])) ++count;
          } else {
            for (VEntity *mobj = Level->FindMobjFromTID(args[0], nullptr); mobj; mobj = Level->FindMobjFromTID(args[0], mobj)) {
              //mobj->StartSound(sound, 0, sp[-1] / 127.0f, 1.0f, false);
              //if (mobj->eventSetFlag(name, !!args[2])) ++count;
              if (mobj->SetDecorateFlag(name, !!args[2])) ++count;
            }
          }
        }
        return count;
      }

    case ACSF_PlayActorSound:
      GCon->Logf(NAME_Error, "unimplemented ACSF function 'PlayActorSound'");
      return 0;

    // void PlaySound (int tid, str sound [, int channel [, fixed volume [, bool looping [, fixed attenuation [, bool local]]]]])
    case ACSF_PlaySound:
      {
        //GCon->Logf("ERROR: unimplemented ACSF function 'PlaySound'");
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          VName name = GetName(args[1]);
          if (name != NAME_None) {
            int chan = (argCount > 2 ? args[2] : 4)&7;
            float volume = (argCount > 3 ? (float)args[3]/(float)0x10000 : 1.0f);
            if (volume <= 0) return 0;
            if (volume > 1) volume = 1;
            bool looping = (argCount > 4 ? !!args[4] : false);
            float attenuation = (argCount > 5 ? (float)args[5]/(float)0x10000 : 1.0f);
            bool local = (argCount > 6 ? !!args[6] : (attenuation == 0.0f));
            if (attenuation < 0.0f) attenuation = 0.0f; //WARNING! zero attenuation means "local sound"
            if (local) {
              //attenuation = 0.0f;
              //Ent->StartSound(name, chan, volume, attenuation, looping);
              Ent = Ent->eventGetLocalSoundEntity();
              if (Ent) Ent->StartSound(name, chan, volume, attenuation, looping);
            } else {
              Ent->StartSound(name, chan, volume, attenuation, looping);
            }
          }
        }
        return 0;
      }

    // void StopSound (int tid, int channel);
    case ACSF_StopSound:
      {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          int chan = (argCount > 1 ? args[1] : 4)&7;
          Ent->StopSound(chan);
        }
        //GCon->Logf("ERROR: unimplemented ACSF function 'StopSound'");
        return 0;
      }

    case ACSF_SoundVolume:
      //GCon->Logf("ERROR: unimplemented ACSF function 'SoundVolume'");
      return 0;

    case ACSF_SetMusicVolume:
      //GCon->Logf("ERROR: unimplemented ACSF function 'SetMusicVolume'");
      return 0;

    case ACSF_GetActorVelX:
      {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        return (int)(Ent->Velocity.x*(65536.0f/35.0f));
      }
    case ACSF_GetActorVelY:
      {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        return (int)(Ent->Velocity.y*(65536.0f/35.0f));
      }
    case ACSF_GetActorVelZ:
      {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0;
        return (int)(Ent->Velocity.z*(65536.0f/35.0f));
      }

    //bool SetActorVelocity (int tid, fixed velx, fixed vely, fixed velz, bool add, bool setbob)
    //TODO: bob
    case ACSF_SetActorVelocity:
      //k8: all or one?
      {
        int count = 0;
        if (args[0] == 0) {
          // activator
          if (Activator) {
            ++count;
            if (args[4]) {
              // add
              Activator->Velocity.x += float(args[1])/65536.0f*35.0f;
              Activator->Velocity.y += float(args[2])/65536.0f*35.0f;
              Activator->Velocity.z += float(args[3])/65536.0f*35.0f;
            } else {
              Activator->Velocity.x = float(args[1])/65536.0f*35.0f;
              Activator->Velocity.y = float(args[2])/65536.0f*35.0f;
              Activator->Velocity.z = float(args[3])/65536.0f*35.0f;
            }
          }
        } else {
          for (VEntity *mobj = Level->FindMobjFromTID(args[0], nullptr); mobj; mobj = Level->FindMobjFromTID(args[0], mobj)) {
            ++count;
            if (args[4]) {
              // add
              mobj->Velocity.x += float(args[1])/65536.0f*35.0f;
              mobj->Velocity.y += float(args[2])/65536.0f*35.0f;
              mobj->Velocity.z += float(args[3])/65536.0f*35.0f;
            } else {
              mobj->Velocity.x = float(args[1])/65536.0f*35.0f;
              mobj->Velocity.y = float(args[2])/65536.0f*35.0f;
              mobj->Velocity.z = float(args[3])/65536.0f*35.0f;
            }
          }
        }
        return (count > 0 ? 1 : 0);
      }

    // int UniqueTID ([int tid[, int limit]])
    case ACSF_UniqueTID:
      {
        int tidstart = (argCount > 0 ? args[0] : 0);
        int limit = (argCount > 1 ? args[1] : 0);
        int res = Level->FindFreeTID(tidstart, limit);
        //GCon->Logf("UniqueTID: tidstart=%d; limit=%d; res=%d", tidstart, limit, res);
        return res;
      }

    // bool IsTIDUsed (int tid)
    case ACSF_IsTIDUsed:
      return (argCount > 0 && args[0] ? Level->IsTIDUsed(args[0]) : 1);

    case ACSF_GetActorPowerupTics:
      if (argCount > 0) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          VName name = GetName(args[1]);
          float ptime = Ent->eventFindActivePowerupTime(name);
          if (ptime == 0) return 0;
          return int(ptime/35.0f);
        }
        return 0;
      } else {
        return 0;
      }

    // void SetTranslation (int tid, str transname)
    case ACSF_SetTranslation:
      if (argCount >= 2) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          VStr name = GetStr(args[1]);
          Ent->Translation = R_FindTranslationByName(name);
        }
      }
      return 0;

    case ACSF_ChangeActorAngle:
    case ACSF_ChangeActorPitch:
    case ACSF_ChangeActorRoll:
    case ACSF_SetActorRoll:
      // ignores interpolation for now (args[2])
      if (argCount >= 2) {
        int count = 0;
        float newAngle = (float)(args[1]&0xffff)*360.0f/(float)0x10000;
        newAngle = (funcIndex == ACSF_ChangeActorPitch ? AngleMod180(newAngle) : AngleMod(newAngle));
        if (args[0] == 0) {
          VEntity *ent = EntityFromTID(args[0], Activator);
          if (ent) {
            switch (funcIndex) {
              case ACSF_ChangeActorAngle: ent->Angles.yaw = newAngle; break;
              case ACSF_ChangeActorPitch: ent->Angles.pitch = newAngle; break;
              case ACSF_ChangeActorRoll: case ACSF_SetActorRoll: ent->Angles.roll = newAngle; break;
            }
            ++count;
          }
        } else {
          for (VEntity *ent = Level->FindMobjFromTID(args[0], nullptr); ent; ent = Level->FindMobjFromTID(args[0], ent)) {
            if (ent) {
              switch (funcIndex) {
                case ACSF_ChangeActorAngle: ent->Angles.yaw = newAngle; break;
                case ACSF_ChangeActorPitch: ent->Angles.pitch = newAngle; break;
                case ACSF_ChangeActorRoll: case ACSF_SetActorRoll: ent->Angles.roll = newAngle; break;
              }
            }
          }
        }
        return count;
      }
      return 0;

    case ACSF_GetActorRoll:
      if (argCount >= 1) {
        VEntity *ent = EntityFromTID(args[0], Activator);
        if (ent) return (int)(AngleMod(ent->Angles.roll)*65536.0f/360.0f);
      }
      return 0;

    case ACSF_SetActivator:
      //GCon->Logf("ACSF_SetActivator: argc=%d; arg[0]=%d; arg[1]=%d", argCount, (argCount > 0 ? args[0] : 0), (argCount > 1 ? args[1] : 0));
      if (argCount == 0) { Activator = nullptr; return 0; } // to world
      // only tid was specified
      if (argCount == 1) {
        if (args[0] == 0) return (Activator ? 1 : 0); // to self
        Activator = EntityFromTID(args[0], Activator);
        //GCon->Logf("   new activator: <%s>", (Activator ? *Activator->GetClass()->GetFullName() : "none"));
        return (Activator ? 1 : 0);
      }
      // some flags was specified
      Activator = EntityFromTID(args[0], Activator);
      if (Activator) {
        Activator = Activator->eventDoAAPtr(args[1]);
      } else {
        if (args[1]&AAPTR_ANY_PLAYER) {
          for (int f = 0; f < 8; ++f) {
            if (!(args[1]&(AAPTR_PLAYER1<<f))) continue;
            if (!GGameInfo->Players[f]) continue;
            if (!(GGameInfo->Players[f]->PlayerFlags&VBasePlayer::PF_Spawned)) continue;
            if (!GGameInfo->Players[f]->MO) continue; // just in case
            Activator = GGameInfo->Players[f]->MO;
            break;
          }
        }
      }
      return (Activator ? 1 : 0);
      /*
      if (args[0] == 0 && argCount > 1 && args[1] == 1) {
        // to world
        Activator = nullptr;
        return 0; //???
      }
      */
      //GCon->Logf("ACSF_SetActivator: tid=%d; ptr=%d", args[0], (argCount > 1 ? args[1] : 0));

    // bool SetActivatorToTarget (int tid)
    case ACSF_SetActivatorToTarget:
      //GCon->Logf("ACSF_SetActivatorToTarget: argc=%d; arg[0]=%d", argCount, (argCount > 0 ? args[0] : 0));
      if (argCount > 0) {
        VEntity *src = EntityFromTID(args[0], Activator);
        if (!src) return 0; // oops
        VEntity *tgt = src->eventFindTargetForACS();
        if (!tgt) return 0;
        //GCon->Logf("ACSF_SetActivatorToTarget: new target is <%s>; tid=%d", *tgt->GetClass()->GetFullName(), tgt->TID);
        Activator = tgt;
        return 1;
      }
      return 0;


    case ACSF_SetLineActivation:
      if (argCount > 1) {
        int searcher = -1;
        line_t *line = XLevel->FindLine(args[0], &searcher);
        if (line) line->SpacFlags = args[2];
      }
      return 0;

    case ACSF_GetLineActivation:
      if (argCount > 0) {
        int searcher = -1;
        line_t *line = XLevel->FindLine(args[0], &searcher);
        if (line) return line->SpacFlags;
      }
      return 0;


    case ACSF_SetCVar:
      if (argCount >= 2) {
        VName name = GetName(args[0]);
        if (name == NAME_None) return 0;
        VCvar *cvar = VCvar::FindVariable(*name);
        if (cvar) {
          if (!cvar->CanBeModified(true, true)) { acsCVarWarning(name); return 0; } // modonly, noserver
        } else {
          cvar = VCvar::CreateNewInt(*name, args[1], "created from ACS", CVAR_FromMod|CVAR_ACS);
          cvar->SetACS();
        }
        //GCon->Logf("ACSF: set cvar '%s' (%f)", *name, args[1]/65536.0f);
        cvar->Set(*name, args[1] /* /65536.0f */);
        return 1;
      }
      return 0;

    //k8: this should work over network, but meh
    // args[0]: playernumber
    case ACSF_GetUserCVar:
      if (argCount >= 2) {
        VName name = GetName(args[1]);
        if (name == NAME_None) return 0;
        //GCon->Logf("ACSF: get user cvar '%s' (%f)", *name, VCvar::GetFloat(*name));
        //FIXME:return (int)(VCvar::GetFloat(*name)*65536.0f);
        return VCvar::GetInt(*name);
      }
      return 0;

    //k8: this should work over network, but meh
    // args[0]: playernumber
    case ACSF_SetUserCVar:
      if (argCount >= 3) {
        VName name = GetName(args[1]);
        if (name == NAME_None) return 0;
        VCvar *cvar = VCvar::FindVariable(*name);
        if (cvar) {
          if (!cvar->CanBeModified(true, true)) { acsCVarWarning(name); return 0; } // modonly, noserver
        } else {
          cvar = VCvar::CreateNewInt(*name, args[2], "created from ACS", CVAR_FromMod|CVAR_ACS);
          cvar->SetACS();
        }
        //GCon->Logf("ACSF: set user cvar '%s' (%f)", *name, args[2]/65536.0f);
        cvar->Set(*name, args[2] /* /65536.0f */);
        return 1;
      }
      return 0;

    // non-existent vars should return 0 here (yay, another ACS hack!)
    case ACSF_GetCVarString:
      if (argCount >= 1) {
        VName name = GetName(args[0]);
        //GCon->Logf(NAME_Debug, "GetCVARString: %s", *name);
        if (name == NAME_None) return 0; //ActiveObject->Level->PutNewString("");
        if (!VCvar::HasVar(*name)) return 0;
        //GCon->Logf("ACSF_GetCVarString: var=<%s>; value=<%s>", *name, *VCvar::GetString(*name));
        return ActiveObject->Level->PutNewString(VCvar::GetString(*name));
      }
      return 0; //ActiveObject->Level->PutNewString("");

    case ACSF_SetCVarString:
      //GCon->Logf("***ACSF_SetCVarString: var=<%s>", *GetName(args[0]));
      if (argCount >= 2) {
        VName name = GetName(args[0]);
        if (name == NAME_None) return 0;
        //GCon->Logf("ACSF_SetCVarString: var=<%s>; value=<%s>; allowed=%d", *name, *GetStr(args[1]), (int)VCvar::CanBeModified(*name, true, true));
        VCvar *cvar = VCvar::FindVariable(*name);
        if (cvar) {
          if (!cvar->CanBeModified(true, true)) { acsCVarWarning(name); return 0; } // modonly, noserver
        } else {
          cvar = VCvar::CreateNewStr(*name, "", "created from ACS", CVAR_FromMod|CVAR_ACS);
          cvar->SetACS();
        }
        VStr value = GetStr(args[1]);
        cvar->Set(*name, value);
        return 1;
      }
      return 0;

    //k8: this should work over network, but meh
    // non-existent vars should return 0 here (yay, another ACS hack!)
    case ACSF_GetUserCVarString:
      if (argCount >= 2) {
        VName name = GetName(args[1]);
        if (name == NAME_None) return 0; //ActiveObject->Level->PutNewString("");
        if (!VCvar::HasVar(*name)) return 0;
        //GCon->Logf("ACSF_GetUserCVarString: var=<%s>; value=<%s>", *name, *VCvar::GetString(*name));
        return ActiveObject->Level->PutNewString(VCvar::GetString(*name));
      }
      return 0; //ActiveObject->Level->PutNewString("");

    //k8: this should work over network, but meh
    // args[0]: playernumber
    case ACSF_SetUserCVarString:
      //GCon->Logf("***ACSF_SetUserCVarString: var=<%s>", *GetName(args[1]));
      if (argCount >= 3) {
        VName name = GetName(args[1]);
        if (name == NAME_None) return 0;
        //GCon->Logf("ACSF_SetUserCVarString: var=<%s>; value=<%s>; allowed=%d", *name, *GetStr(args[2]), (int)VCvar::CanBeModified(*name, true, true));
        VCvar *cvar = VCvar::FindVariable(*name);
        if (cvar) {
          if (!cvar->CanBeModified(true, true)) { acsCVarWarning(name); return 0; } // modonly, noserver
        } else {
          cvar = VCvar::CreateNewStr(*name, "", "created from ACS", CVAR_FromMod|CVAR_ACS);
          cvar->SetACS();
        }
        VStr value = GetStr(args[2]);
        cvar->Set(*name, value);
        return 1;
      }
      return 0;


    case ACSF_PlayerIsSpectator_Zandro:
      return 0;

    case ACSF_GetPlayerLivesLeft_Zandro:
    case ACSF_SetPlayerLivesLeft_Zandro:
      return 0;

    case ACSF_GetGamemodeState_Zandro:
      #ifdef CLIENT
      if (GGameInfo->NetMode == NM_Standalone ||
          GGameInfo->NetMode == NM_Client ||
          GGameInfo->NetMode == NM_ListenServer)
      {
        if (cl && cls.signon && cl->MO) {
          return 2; // GAMESTATE_INPROGRESS
        }
      }
      #endif
      return -1; // GAMESTATE_UNSPECIFIED

    // https://zdoom.org/wiki/ConsolePlayerNumber
    //FIXME: disconnect?
    case ACSF_ConsolePlayerNumber_Zandro:
      //GCon->Logf(NAME_Warning, "ERROR: unimplemented ACSF function #%d'", 102);
      //if (!isZandroACS()) return 0;
      #ifdef CLIENT
      if (GGameInfo->NetMode == NM_Standalone ||
          GGameInfo->NetMode == NM_Client ||
          GGameInfo->NetMode == NM_ListenServer)
      {
        if (cl && cls.signon && cl->MO) {
          //GCon->Logf(NAME_Warning, "CONPLRNUM: %d", cl->ClientNum);
          return cl->ClientNum;
        }
      }
      #endif
      return -1;

    // int RequestScriptPuke (int script[, int arg0[, int arg1[, int arg2[, int arg3]]]])
    case ACSF_RequestScriptPuke_Zandro:
      //if (!isZandroACS()) return 0;
      {
        #ifdef CLIENT
        if (argCount < 1) return 0;
        if (GGameInfo->NetMode != NM_Client && GGameInfo->NetMode != NM_Standalone && GGameInfo->NetMode != NM_TitleMap) {
          GCon->Logf(NAME_Warning, "ACS: Zandro RequestScriptPuke can be executed only by clients (%d).", GGameInfo->NetMode);
          return 0;
        }
        // ignore this in demos
        //FIXME: check `net` flag first
        if (cls.demoplayback) {
          return 1;
        }
        // do it
        if (args[0] == 0) return 1;
        int ScArgs[4];
        ScArgs[0] = (argCount > 1 ? args[1] : 0);
        ScArgs[1] = (argCount > 2 ? args[2] : 0);
        ScArgs[2] = (argCount > 3 ? args[3] : 0);
        ScArgs[3] = (argCount > 4 ? args[4] : 0);
        VEntity *plr = nullptr;
        if (GGameInfo->NetMode == NM_Standalone || GGameInfo->NetMode == NM_Client || GGameInfo->NetMode == NM_TitleMap) {
          if (cl && cls.signon && cl->MO) {
            plr = cl->MO;
            //GCon->Logf("plr: %p", plr);
          }
        }
        if (!ActiveObject->Level->Start(abs(args[0]), 0/*map*/, ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], plr, line, side, (args[0] < 0)/*always*/, false/*wantresult*/, true/*net*/)) return 0;
        return 1;
        #else
        GCon->Log(NAME_Warning, "ACS: Zandro RequestScriptPuke can be executed only by clients.");
        return 0;
        #endif
      }

    // int NamedRequestScriptPuke (str script[, int arg0[, int arg1[, int arg2[, int arg3]]]])
    case ACSF_NamedRequestScriptPuke_Zandro:
      //if (!isZandroACS()) return 0;
      {
        #ifdef CLIENT
        if (argCount < 1) return 0;
        if (GGameInfo->NetMode != NM_Client && GGameInfo->NetMode != NM_Standalone && GGameInfo->NetMode != NM_TitleMap) {
          GCon->Logf(NAME_Warning, "ACS: Zandro NamedRequestScriptPuke can be executed only by clients (%d).", GGameInfo->NetMode);
          return 0;
        }
        // ignore this in demos
        //FIXME: check `net` flag first
        if (cls.demoplayback) {
          return 1;
        }
        // do it
        VName name = GetNameLowerCase(args[0]);
        if (name == NAME_None) return 0;
        int ScArgs[4];
        ScArgs[0] = (argCount > 1 ? args[1] : 0);
        ScArgs[1] = (argCount > 2 ? args[2] : 0);
        ScArgs[2] = (argCount > 3 ? args[3] : 0);
        ScArgs[3] = (argCount > 4 ? args[4] : 0);
        VEntity *plr = nullptr;
        if (GGameInfo->NetMode == NM_Standalone || GGameInfo->NetMode == NM_Client || GGameInfo->NetMode == NM_TitleMap) {
          if (cl && cls.signon && cl->MO) plr = cl->MO;
        }
        if (!ActiveObject->Level->Start(-name.GetIndex(), 0/*map*/, ScArgs[0], ScArgs[1], ScArgs[2], ScArgs[3], plr, line, side, false/*always*/, false/*wantresult*/, true/*net*/)) return 0;
        return 1;
        #else
        GCon->Log(NAME_Warning, "ACS: Zandro NamedRequestScriptPuke can be executed only by clients.");
        return 0;
        #endif
      }

    // int PickActor (int source, fixed angle, fixed pitch, fixed distance, int tid [, int actorMask [, int wallMask [, int flags]]])
    case ACSF_PickActor:
      if (argCount < 5) return 0;
      {
        /*
        GCon->Logf("ACSF_PickActor: argc=%d; arg[0]=%d; arg[1]=%d; arg[2]=%d; arg[3]=%d; arg[4]=%d; arg[5]=%d; arg[6]=%d; arg[7]=%d", argCount,
          (argCount > 0 ? args[0] : 0), (argCount > 1 ? args[1] : 1),
          (argCount > 2 ? args[2] : 0), (argCount > 3 ? args[3] : 1),
          (argCount > 4 ? args[4] : 0), (argCount > 5 ? args[5] : 1),
          (argCount > 6 ? args[6] : 0), (argCount > 7 ? args[7] : 1));
        */
        VEntity *src = EntityFromTID(args[0], Activator);
        if (!src) return 0; // oops
        if (args[3] < 0) return 0;
        // create direction vector
        TAVec ang;
        ang.yaw = 360.0f*float(args[1])/65536.0f;
        ang.pitch = 360.0f*float(args[2])/65536.0f;
        ang.roll = 0;
        TVec dir = AngleVector(ang);
        //dir = NormaliseSafe(dir);
        //dir.normaliseInPlace();
        VEntity *hit = src->eventPickActor(
          false, TVec(0, 0, 0), // origin
          dir, float(args[3])/65536.0f,
          argCount > 5, (argCount > 5 ? args[5] : 0), // actormask
          argCount > 6, (argCount > 6 ? args[6] : 0)); // wallmask
        if (!hit) return 0;
        //GCon->Logf("ACSF_PickActor: hit=<%s>; tid=%d", *hit->GetClass()->GetFullName(), hit->TID);
        // assign tid
        int flags = (argCount > 7 ? args[7] : 0);
        int newtid = args[4];
        if (newtid < 0) { GCon->Logf(NAME_Warning, "ACS: PickActor with negative tid (%d)", newtid); return (flags&PICKAF_RETURNTID ? hit->TID : 1); }
        // assign new tid
        if ((flags&PICKAF_FORCETID) != 0 || hit->TID == 0) {
          if (newtid != 0 || (flags&PICKAF_FORCETID) != 0) {
            //int oldtid = hit->TID;
            hit->SetTID(newtid);
            //GCon->Logf("ACSF_PickActor: TID change: hit=<%s>; oldtid=%d; newtid=%d", *hit->GetClass()->GetFullName(), oldtid, hit->TID);
          }
        }
        return (flags&PICKAF_RETURNTID ? hit->TID : 1);
      }

    // void LineAttack (int tid, fixed angle, fixed pitch, int damage [, str pufftype [, str damagetype [, fixed range [, int flags [, int pufftid]]]]]);
    case ACSF_LineAttack:
      if (argCount >= 4) {
        VEntity *src = EntityFromTID(args[0], Activator);
        if (!src) return 0; // oops
        // create direction vector
        VName pufft = VName("BulletPuff");
        if (argCount > 4) pufft = GetName(args[4]);
        VName dmgt = VName("None");
        if (argCount > 5) dmgt = GetName(args[5]);
        if (dmgt == NAME_None) dmgt = VName("None");
        TAVec ang;
        ang.yaw = 360.0f*float(args[1])/65536.0f;
        ang.pitch = 360.0f*float(args[2])/65536.0f;
        ang.roll = 0;
        TVec dir = AngleVector(ang);
        //dir = NormaliseSafe(dir);
        //dir.normaliseInPlace();
        // dir, range, damage,
        // pufftype, damagetype,
        // flags, pufftid
        src->eventLineAttackACS(
          dir, (argCount > 6 ? float(args[6])/65536.0f : 2048.0f), float(args[3])/65536.0f,
          pufft, dmgt,
          (argCount > 7 ? args[7] : 0), (argCount > 8 ? args[8] : 0));
      }
      return 0;

    case ACSF_GetChar:
      if (argCount >= 2) {
        VStr s = GetStr(args[0]);
        int idx = args[1];
        if (idx >= 0 && idx < s.length()) {
          /*
          if ((vuint8)s[idx] > 32) {
            GCon->Logf("GetChar(%d:%s)=%d (%c)", idx, *s.quote(), s[idx], (vuint8)s[idx]);
          } else {
            GCon->Logf("GetChar(%d:%s)=%d", idx, *s.quote(), (vuint8)s[idx]);
          }
          */
          return (vuint8)s[idx];
        }
      }
      return 0;

    case ACSF_strcmp:
    case ACSF_stricmp:
      if (argCount >= 2) {
        int maxlen = (argCount > 2 ? args[2] : MAX_VINT32);
        VStr s0 = GetStr(args[0]);
        VStr s1 = GetStr(args[1]);
        int curpos = 0;
        while (curpos < maxlen) {
          if (curpos >= s0.length() || curpos >= s1.length()) {
            if (s0.length() == s1.length()) return 0; // equal
            return (s0.length() < s1.length() ? -1 : 1);
          }
          char c0 = s0[curpos];
          char c1 = s1[curpos];
          if (funcIndex == ACSF_stricmp) {
            c0 = VStr::upcase1251(c0);
            c1 = VStr::upcase1251(c1);
          }
          if (c0 < c1) return -1;
          if (c0 > c1) return 1;
          ++curpos;
        }
      }
      return 0;

    case ACSF_StrLeft:
      if (argCount >= 2) {
        VStr s = GetStr(args[0]);
        int newlen = args[2];
        if (newlen <= 0) return ActiveObject->Level->PutNewString("");
        if (newlen >= s.length()) return args[0];
        return ActiveObject->Level->PutNewString(s.left(newlen));
      }
      return ActiveObject->Level->PutNewString("");

    case ACSF_StrRight:
      if (argCount >= 2) {
        VStr s = GetStr(args[0]);
        int newlen = args[2];
        if (newlen <= 0) return ActiveObject->Level->PutNewString("");
        if (newlen >= s.length()) return args[0];
        return ActiveObject->Level->PutNewString(s.right(newlen));
      }
      return ActiveObject->Level->PutNewString("");

    case ACSF_StrMid:
      if (argCount >= 3) {
        VStr s = GetStr(args[0]);
        int pos = args[1];
        if (pos < 0) pos = 0;
        int newlen = args[2];
        //GCon->Logf("***StrMid: <%s> (pos=%d; newlen=%d): <%s>", *s.quote(), pos, newlen, *s.mid(pos, newlen).quote());
        if (newlen <= 0) return ActiveObject->Level->PutNewString("");
        if (pos == 0 && newlen >= s.length()) return args[0];
        return ActiveObject->Level->PutNewString(s.mid(pos, newlen));
      }
      return ActiveObject->Level->PutNewString("");


    // int SetUserVariable (int tid, str name, value)
    case ACSF_SetUserVariable:
      if (argCount >= 3) {
        VStr s = GetStr(args[1]).toLowerCase();
        if (!s.startsWith("user_")) {
          if (acs_dump_uservar_access) GCon->Logf(NAME_Warning, "ACS: cannot set user variable with name \"%s\"", *GetStr(args[1]));
          return 0;
        }
        VName fldname = VName(*s);
        int count = 0;
        if (args[0] == 0) {
          if (doSetUserVarOrArray(Activator, fldname, args[2], false)) ++count;
        } else {
          for (VEntity *mobj = Level->FindMobjFromTID(args[0], nullptr); mobj; mobj = Level->FindMobjFromTID(args[0], mobj)) {
            if (doSetUserVarOrArray(mobj, fldname, args[2], false)) ++count;
          }
        }
        return count;
      }
      return 0;

    // GetUserVariable (int tid, str name)
    case ACSF_GetUserVariable:
      if (argCount >= 2) {
        VStr s = GetStr(args[1]).toLowerCase();
        if (!s.startsWith("user_")) {
          GCon->Logf("ACS: cannot get user variable with name \"%s\"", *GetStr(args[1]));
          return 0;
        }
        VEntity *mobj = EntityFromTID(args[0], Activator);
        if (!mobj) {
          GCon->Logf("ACS: cannot get user variable with name \"%s\" from nothing", *GetStr(args[1]));
          return 0;
        }
        VName fldname = VName(*s);
        return doGetUserVarOrArray(mobj, fldname, false);
      }
      return 0;

    // int SetUserArray (int tid, str name, int pos, int value)
    case ACSF_SetUserArray:
      if (argCount >= 4) {
        VStr s = GetStr(args[1]).toLowerCase();
        if (!s.startsWith("user_")) {
          GCon->Logf(NAME_Warning, "ACS: cannot set user array with name \"%s\"", *GetStr(args[1]));
          return 0;
        }
        VName fldname = VName(*s);
        int count = 0;
        if (args[0] == 0) {
          if (doSetUserVarOrArray(Activator, fldname, args[3], true, args[2])) ++count;
        } else {
          for (VEntity *mobj = Level->FindMobjFromTID(args[0], nullptr); mobj; mobj = Level->FindMobjFromTID(args[0], mobj)) {
            if (doSetUserVarOrArray(mobj, fldname, args[3], true, args[2])) ++count;
          }
        }
        return count;
      }
      return 0;

    // GetUserArray (int tid, str name, int pos)
    case ACSF_GetUserArray:
      if (argCount >= 3) {
        VStr s = GetStr(args[1]).toLowerCase();
        if (!s.startsWith("user_")) {
          GCon->Logf("ACS: cannot get user array with name \"%s\"", *GetStr(args[1]));
          return 0;
        }
        VEntity *mobj = EntityFromTID(args[0], Activator);
        if (!mobj) {
          GCon->Logf("ACS: cannot get user array with name \"%s\" from nothing", *GetStr(args[1]));
          return 0;
        }
        VName fldname = VName(*s);
        return doGetUserVarOrArray(mobj, fldname, true, args[2]);
      }
      return 0;


    // void SetSkyScrollSpeed (int sky, fixed skyspeed);
    case ACSF_SetSkyScrollSpeed: return 0;
    // int GetAirSupply (int playernum) -- in tics
    case ACSF_GetAirSupply: return 35; // always one second
    case ACSF_SetAirSupply: return 1; // say that we did it

    case ACSF_AnnouncerSound: // ignored
      return 0;

    case ACSF_PlayerIsLoggedIn_Zandro:
      return 0; // player is never logged in
    //case ACSF_GetPlayerAccountName_Zandro: return 0; // `0` means "not implemented" //ActiveObject->Level->PutNewString(""); // always unnamed
    case ACSF_GetPlayerAccountName_Zandro:
      if (!isZandroACS()) return 0;
      return ActiveObject->Level->PutNewString(""); // always unnamed

    // bool SetPointer(int assign_slot, int tid[, int pointer_selector[, int flags]])
    case ACSF_SetPointer:
      if (argCount >= 2 && Activator) {
        return (Activator->eventSetPointerForACS(args[0], args[1], (argCount > 2 ? args[2] : 0), (argCount > 3 ? args[3] : 0)) ? 1 : 0);
      }
      return 0;

    // int Sqrt (int number)
    case ACSF_Sqrt:
      if (argCount > 0 && args[0] > 0) return (int)sqrtf((float)args[0]);
      return 0;

    // fixed FixedSqrt (fixed number)
    case ACSF_FixedSqrt:
      if (argCount > 0 && args[0] > 0) return (int)(sqrtf((float)args[0]/65536.0f)*65536.0f);
      return 0;

    //case ACSF_StrArg:
    //  return -FName(FBehavior::StaticLookupString(args[0]));

    case ACSF_Floor:
      return (argCount > 0 ? args[0]&~0xffff : 0);

    case ACSF_Ceil:
      return (argCount > 0 ? (args[0]&~0xffff)+0x10000 : 0);

    case ACSF_Round:
      return (argCount > 0 ? (args[0]+32768)&~0xffff : 0);

    case ACSF_StrArg:
      if (argCount > 0) {
        VStr s = GetStr(args[0]);
        return ActiveObject->Level->PutNewString(s);
      }
      return ActiveObject->Level->PutNewString("");

    case ACSF_CheckSight: //untested!
      if (argCount >= 2) {
        unsigned flags = VEntity::CSE_CheckBaseRegion;
        if (argCount >= 3) {
          if (args[2]&1) flags |= VEntity::CSE_IgnoreFakeFloors;
          if (args[2]&2) flags |= VEntity::CSE_IgnoreBlockAll;
        }
        //bool CanSeeEx (VEntity *Ent, unsigned flags=0);

        if (args[0] == 0) {
          if (args[1] == 0) return true;
          if (Activator) {
            for (VEntity *mobj = Level->FindMobjFromTID(args[1], nullptr); mobj; mobj = Level->FindMobjFromTID(args[1], mobj)) {
              if (Activator->CanSeeEx(mobj, flags)) return 1;
            }
          }
        } else {
          for (VEntity *src = Level->FindMobjFromTID(args[0], nullptr); src; src = Level->FindMobjFromTID(args[0], src)) {
            if (args[1] != 0) {
              for (VEntity *dest = Level->FindMobjFromTID(args[1], nullptr); dest; dest = Level->FindMobjFromTID(args[1], dest)) {
                if (src->CanSeeEx(dest, flags)) return 1;
              }
            } else {
              if (Activator && src->CanSeeEx(Activator, flags)) return 1;
            }
          }
        }
      }
      return 0;

    case ACSF_ScriptCall:
      if (argCount >= 2) {
        VStr clsname = GetStr(args[0]).toLowerCase();
        VStr funcname = GetStr(args[1]).toLowerCase();
        GCon->Logf(NAME_Error, "ACSF_ScriptCall(%s,%s,%d) -- unimplemented", *clsname, *funcname, argCount);
      }
      return 0;

    // fixed VectorLength (fixed x, fixed y)
    case ACSF_VectorLength:
      if (argCount >= 2) {
        TVec v = TVec((float)args[0]/65536.0f, (float)args[0]/65536.0f);
        float len = v.length2D();
        return (int)(len/65536.0f);
      }
      return 0;

    // bool CheckActorProperty (int tid, int property, int value)
    case ACSF_CheckActorProperty:
      if (argCount >= 3) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (!Ent) return 0; // never equals
        //if (developer) GCon->Logf(NAME_Dev, "GetActorProperty: ent=<%s>, propid=%d", Ent->GetClass()->GetName(), args[1]);
        int val = Ent->eventGetActorProperty(args[1]);
        // convert special properties
        switch (args[1]) {
          case 20: //APROP_Species
          case 21: //APROP_NameTag
            val = ActiveObject->Level->PutNewString(*VName(EName(val)));
            break;
        }
        return (val == args[2]);
      }
      return 0;

    case ACSF_CheckActorState:
      if (argCount >= 2) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          VStr stname = GetStr(args[1]);
          if (stname.isEmpty() || stname.strEquCI("none") || stname.strEquCI("null")) return 1;
          bool exact = (argCount > 2 ? !!args[2] : false);
          TArray<VName> Names;
          VMemberBase::StaticSplitStateLabel(stname, Names);
          return !!Ent->GetClass()->FindStateLabel(Names, exact);
        }
      }
      return 0;

    // void Radius_Quake2 (int tid, int intensity, int duration, int damrad, int tremrad, str sound)
    case ACSF_Radius_Quake2:
      if (argCount >= 5) {
        VName sndname = NAME_None;
        if (argCount > 5) sndname = GetNameLowerCase(args[5]);
        Level->eventAcsRadiusQuake2(Activator, args[0], args[1], args[2], args[3], args[4], sndname);
      }
      return 0;

    // void QuakeEx (int tid, int intensityX, int intensityY, int intensityZ, int duration, int damrad, int tremrad, sound sfx [, int flags [, fixed mulwavex [, fixed mulwavey [, fixed mulwavez [, int falloff [, int highpoint [, fixed rollintensity [, fixed rollwave]]]]]]]])
    case ACSF_QuakeEx:
      GCon->Logf(NAME_Warning, "partially implemented ACSF function '%s' (%d args)", "QuakeEx", argCount);
      if (argCount >= 7) {
        VName sndname = NAME_None;
        const int tid = args[0];
        //const int intensity = (args[1]+args[2]+args[3])/3; // for now
        const int intensity = max3(args[1], args[2], args[3]); // for now
        const int dur = args[4];
        const int damrad = args[5];
        const int tremrad = args[6];
        if (argCount > 7) sndname = GetNameLowerCase(args[7]);
        Level->eventAcsRadiusQuake2(Activator, tid, intensity, dur, damrad, tremrad, sndname);
      }
      return 0;

    // bool Warp (int tid, fixed xofs, fixed yofs, fixed zofs, fixed angle, int flags [, str success_state [, bool exact [, fixed heightoffset [, fixed radiusoffset [, fixed pitch]]]]])
    case ACSF_Warp:
      if (argCount >= 6) {
        const int tid = args[0];
        const float xofs = args[1]/float(0x10000);
        const float yofs = args[2]/float(0x10000);
        const float zofs = args[3]/float(0x10000);
        const float angle = args[4]/float(0x10000);
        const int flags = args[5];
        VName succState = (argCount > 6 ? GetNameLowerCase(args[6]) : NAME_None);
        const bool exact = (argCount > 7 ? !!args[7] : false);
        const float hofs = (argCount > 8 ? args[8]/float(0x10000) : 0.0f);
        const float rofs = (argCount > 9 ? args[9]/float(0x10000) : 0.0f);
        const float pitch = (argCount > 10 ? args[10]/float(0x10000) : 0.0f);
        Level->eventAcsWarp(Activator, tid, xofs, yofs, zofs, angle, flags, succState, exact, hofs, rofs, pitch);
      }
      return 0;

    case ACSF_SetHUDClipRect:
      GCon->Logf(NAME_Error, "unimplemented ACSF function '%s' (%d args)", "SetHUDClipRect", argCount);
      return 0;
    case ACSF_SetHUDWrapWidth:
      GCon->Logf(NAME_Error, "unimplemented ACSF function '%s' (%d args)", "SetHUDWrapWidth", argCount);
      return 0;

    case ACSF_CheckFont:
      if (argCount > 0) {
        #ifdef CLIENT
        return (T_IsFontExists(GetNameLowerCase(args[0])) ? 1 : 0);
        #else
        //FIXME
        //TODO
        return 1;
        #endif
      }
      return 0;

    case ACSF_CheckClass:
      if (argCount > 0) {
        VName cname = GetNameLowerCase(args[0]);
        if (cname != NAME_None) {
          VClass *cls = VClass::FindClassNoCase(*cname);
          while (cls) {
            if (VStr::strEqu(cls->GetName(), "Actor")) return 1;
            cls = cls->ParentClass;
          }
        }
      }
      return 0;

    //bool CheckProximity (int tid, str classname, float distance [, int count [, int flags [, int ptr]]])
    case ACSF_CheckProximity:
      if (argCount >= 3) {
        VEntity *ent = EntityFromTID(args[0], Activator);
        if (ent) {
          bool res = ent->doCheckProximity(GetNameLowerCase(args[1]),
                                           (float)(args[2])/65536.0f,
                                           (argCount > 3), (argCount > 3 ? args[3] : 0),
                                           (argCount > 4), (argCount > 4 ? args[4] : 0),
                                           (argCount > 5), (argCount > 5 ? args[5] : 0));
          return (res ? 1 : 0);
        }
      }
      return 0;

    case ACSF_DropItem:
      if (argCount >= 2) {
        VName itemName = GetNameLowerCase(args[1]);
        if (itemName == NAME_None || itemName == "null" || itemName == "none") return 0;
        int tid = args[0];
        int amount = (argCount > 2 ? args[2] : 0);
        float chance = (argCount > 3 ? (float)(args[3])/256.0f : 1.0f);
        int res = 0;
        for (VEntity *mobj = Level->FindMobjFromTID(tid, nullptr); mobj; mobj = Level->FindMobjFromTID(tid, mobj)) {
          if (mobj->eventDropItem(itemName, amount, chance)) ++res;
        }
        return res;
      }
      return 0;

    // void DropInventory (int tid, str itemtodrop);
    case ACSF_DropInventory:
      if (argCount >= 2) {
        VName itemName = GetNameLowerCase(args[1]);
        if (itemName == NAME_None || itemName == "null" || itemName == "none") return 0;
        int tid = args[0];
        for (VEntity *mobj = Level->FindMobjFromTID(tid, nullptr); mobj; mobj = Level->FindMobjFromTID(tid, mobj)) {
          mobj->eventACSDropInventory(itemName);
        }
      }
      return 0;

    case ACSF_GetPolyobjX:
    case ACSF_GetPolyobjY:
    case ACSF_GetPolyobjZ:
      if (argCount > 0) {
        polyobj_t *pobj = XLevel->GetPolyobj(args[0]);
        if (!pobj) return 0x7FFFFFFF; // doesn't exist
        switch (funcIndex) {
          case ACSF_GetPolyobjX: return (int)(pobj->startSpot.x*65536.0f);
          case ACSF_GetPolyobjY: return (int)(pobj->startSpot.y*65536.0f);
          case ACSF_GetPolyobjZ: return (int)(pobj->startSpot.z*65536.0f);
          default: break;
        }
      }
      return 0x7FFFFFFF; // doesn't exist

    // Polyobj_MoveEx (int po, int hspeed, int yawangle, int dist, int vspeed, int vdist, int moveflags)
    case ACSF_Polyobj_MoveEx:
      if (argCount >= 7) {
        return Level->eventAcsPolyMoveRotateEx(args[0], args[1], args[2], args[3], args[4], args[5], 0.0f/*deltaangle*/, args[6], Activator);
      }
      return 0;

    // Polyobj_MoveToEx (int po, int speed, int x, int y, int z, int moveflags)
    case ACSF_Polyobj_MoveToEx:
      if (argCount >= 6) {
        return Level->eventAcsPolyMoveToRotateEx(args[0], args[1], args[2], args[3], args[4], 0.0f/*deltaangle*/, args[5], Activator);
      }
      return 0;

    // Polyobj_MoveToSpotEx (int po, int speed, int targettid, int moveflags) -- this uses target height too
    case ACSF_Polyobj_MoveToSpotEx:
      if (argCount >= 4) {
        // use `NAN` here, to avoid rotation
        return Level->eventAcsPolyMoveToSpotRotateEx(args[0], args[1], args[2], NAN/*deltaangle*/, args[3], Activator);
      }
      return 0;

    // int Polyobj_GetFlagsEx (int po) -- -1 means "no pobj"
    case ACSF_Polyobj_GetFlagsEx:
      if (argCount >= 1) {
        polyobj_t *po = ActiveObject->Level->XLevel->GetPolyobj(args[0]);
        if (po) return po->PolyFlags;
      }
      return -1;

    // int Polyobj_SetFlagsEx (int po, int flags, int oper) -- oper: 0 means "clear", 1 means "set", -1 means "replace"
    case ACSF_Polyobj_SetFlagsEx:
      if (argCount >= 3) {
        polyobj_t *po = ActiveObject->Level->XLevel->GetPolyobj(args[0]);
        if (po) {
          vuint32 flg = (vuint32)args[1]&0xffffff;
               if (args[2] == POBJ_FLAGS_REPLACE) po->PolyFlags = flg;
          else if (args[2]) po->PolyFlags |= flg;
          else po->PolyFlags &= ~flg;
        }
      }
      return 0;

    // int Polyobj_IsBusy (int po) -- returns -1 if there is no such pobj
    case ACSF_Polyobj_IsBusy:
      if (argCount >= 1) {
        polyobj_t *po = ActiveObject->Level->XLevel->GetPolyobj(args[0]);
        if (po) return (Level->eventPolyBusy(args[0]) ? 1 : 0);
      }
      return -1;

    // fixed Polyobj_GetAngle (int po) -- returns -1 if there is no such pobj
    case ACSF_Polyobj_GetAngle:
      if (argCount >= 1) {
        return (int)(Level->eventPolyAngle(args[0])*65536.0f);
      }
      return -1;

    // Polyobj_MoveRotateEx (int po, int hspeed, int yawangle, int dist, int vspeed, int vdist, fixed deltaangle, int moveflags)
    case ACSF_Polyobj_MoveRotateEx:
      if (argCount >= 8) {
        return Level->eventAcsPolyMoveRotateEx(args[0], args[1], args[2], args[3], args[4], args[5], args[6]/65536.0f, args[7], Activator);
      }
      return 0;

    // Polyobj_MoveToRotateEx (int po, int speed, int x, int y, int z, int deltaangle, int moveflags)
    case ACSF_Polyobj_MoveToRotateEx:
      if (argCount >= 7) {
        return Level->eventAcsPolyMoveToRotateEx(args[0], args[1], args[2], args[3], args[4], args[5]/65536.0f, args[6], Activator);
      }
      return 0;

    // Polyobj_MoveToSpotRotateEx (int po, int speed, int targettid, int deltaangle, int moveflags) -- this uses target height too
    case ACSF_Polyobj_MoveToSpotRotateEx:
      if (argCount >= 5) {
        return Level->eventAcsPolyMoveToSpotRotateEx(args[0], args[1], args[2], args[3]/65536.0f, args[4], Activator);
      }
      return 0;

    // Polyobj_RotateEx (int po, int speed, fixed deltaangle, int moveflags)
    case ACSF_Polyobj_RotateEx:
      if (argCount >= 4) {
        return Level->eventAcsPolyRotateEx(args[0], args[1], args[2]/65536.0f, args[3], Activator);
      }
      return 0;


    case ACSF_SpawnParticle:
      return 0;

    case ACSF_SoundSequenceOnActor:
      if (argCount >= 2) {
        VEntity *ent = EntityFromTID(args[0], Activator);
        if (ent) ent->StartSoundSequence(GetName(args[1]), 0); // this will abort previous sequence
      }
      return 0;
    case ACSF_SoundSequenceOnSector:
      //TODO: args[2] is `location`, see https://zdoom.org/wiki/SoundSequenceOnSector
      if (argCount >= 2) {
        VName seqName = GetName(args[1]);
        sector_t *sector;
        for (int sidx = FindSectorFromTag(sector, args[0]); sidx >= 0; sidx = FindSectorFromTag(sector, args[0], sidx)) {
          Level->SectorStopSequence(sector);
          Level->SectorStartSequence(sector, seqName, 0);
        }
      }
      return 0;
    case ACSF_SoundSequenceOnPolyobj:
      if (argCount >= 2) {
        polyobj_t *po = ActiveObject->Level->XLevel->GetPolyobj(args[0]);
        if (po) {
          Level->PolyobjStopSequence(po); // abort current sequence
          Level->PolyobjStartSequence(po, GetName(args[1]), 0); // start new sequence
        }
      }
      return 0;

    // void SetSectorGlow (int tag, bool plane, int red, int green, int blue, int height)
    // plane: if true, the glow is applied on the ceiling, otherwise (i.e. if false) it is applied on the floor
    // red: if this is set to -1, the glow effect is removed (the other color compounds are ignored)
    case ACSF_SetSectorGlow:
      //GCon->Logf(NAME_Warning, "ignored ACSF `SetSectorGlow`");
      if (argCount >= 6) {
        sector_t *sector;
        const bool setIt = (args[2] >= 0 && args[5] > 0);
        const vuint32 clr = (setIt ? 0xff000000u|(clampToByte(args[2])<<16)|(clampToByte(args[3])<<8)|clampToByte(args[4]) : 0u);
        for (int sidx = FindSectorFromTag(sector, args[0]); sidx >= 0; sidx = FindSectorFromTag(sector, args[0], sidx)) {
          if (setIt) {
            // set glow
            if (args[1]) {
              // ceiling
              sector->params.lightFCFlags |= sec_params_t::LFC_CeilingLight_Glow;
              sector->params.glowCeiling = clr;
              sector->params.glowCeilingHeight = args[5];
            } else {
              // floor
              sector->params.lightFCFlags |= sec_params_t::LFC_FloorLight_Glow;
              sector->params.glowFloor = clr;
              sector->params.glowFloorHeight = args[5];
            }
          } else {
            // reset glow
            if (args[1]) {
              // ceiling
              sector->params.lightFCFlags &= ~sec_params_t::LFC_CeilingLight_Glow;
              sector->params.glowCeiling = 0; // reset color
              sector->params.glowCeilingHeight = 0; // reset height
            } else {
              // floor
              sector->params.lightFCFlags &= ~sec_params_t::LFC_FloorLight_Glow;
              sector->params.glowFloor = 0; // reset color
              sector->params.glowFloorHeight = 0; // reset height
            }
          }
        }
      }
      return 0;

    // void SetSectorDamage (int tag, int amount [, string damagetype [, int interval [, int leaky]]])
    case ACSF_SetSectorDamage:
      if (argCount >= 1) {
        const int amount = clampval((argCount > 1 ? args[1] : 0), 0, 10000);
        VName dmgType = (argCount > 2 ? GetNameLowerCase(args[2]) : NAME_None);
        const int interval = max2(0, (argCount > 3 ? args[3] : 32));
        const int leaky = clampval((argCount > 4 ? args[4] : 0), 0, 256);
        sector_t *sector;
        for (int sidx = FindSectorFromTag(sector, args[0]); sidx >= 0; sidx = FindSectorFromTag(sector, args[0], sidx)) {
          sector->Damage = amount;
          sector->DamageType = dmgType;
          sector->DamageInterval = interval;
          sector->DamageLeaky = leaky;
        }
      }
      return 0;

    case ACSF_SetSectorTerrain:
      GCon->Logf(NAME_Warning, "ACSF `SetSectorTerrain` is not implemented yet");
      return 0;

    case ACSF_GetActorFloorTerrain:
      if (argCount >= 1) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          auto tt = Ent->GetActorTerrain();
          if (tt) return ActiveObject->Level->PutNewString(tt->OrigName);
        }
      }
      return ActiveObject->Level->PutNewString(SV_GetDefaultTerrain()->OrigName);

    case ACSF_GetActorFloorTexture:
      if (argCount >= 1) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent && Ent->Sector) {
          return ActiveObject->Level->PutNewString(VStr(GTextureManager.GetTextureName(Ent->EFloor.splane->pic)));
        }
      }
      return ActiveObject->Level->PutNewString("");

    case ACSF_SetFogDensity:
      GCon->Logf(NAME_Warning, "ignored ACSF `SetFogDensity`");
      return 0;

    case ACSF_SpawnDecal:
      if (argCount >= 2) {
        VEntity *Ent = EntityFromTID(args[0], Activator);
        if (Ent) {
          VName decalname = GetNameLowerCase(args[1]);
          if (decalname != NAME_None && !VStr::strEquCI(*decalname, "none")) {
            int flags = (argCount > 2 ? args[2] : 0);
            float angle = AngleMod((argCount > 3 ? args[3] : 0)/65536.0f*360.0f);
            float zofs = (argCount > 4 ? args[4] : 0)/65536.0f;
            float dist = (argCount > 5 ? args[5] : 0)/65536.0f;
            TVec org = Ent->Origin;
            //TODO: y-flipped decal should have reverse offset
            org.z += max2(0.0f, Ent->Height)*0.5f+zofs;
            if ((flags&SDF_ABSANGLE) == 0) angle = AngleMod(angle+Ent->Angles.yaw);
            return Level->AcsSpawnDecal(Ent, decalname, org, dist, angle, !!(flags&SDF_PERMANENT));
          }
        }
      }
      return 0;

    // bool IsPointerEqual (int ptr_select1, int ptr_select2 [, int tid1 [, int tid2]]);
    case ACSF_IsPointerEqual:
      if (argCount >= 2) {
        VEntity *ent1 = EntityFromTID((argCount > 2 ? args[2] : 0), Activator);
        VEntity *ent2 = EntityFromTID((argCount > 3 ? args[3] : 0), Activator);
        if (ent1 && ent2) return (ent1->eventACSIsPointerEqual(args[0], args[1], ent2) ? 1 : 0);
      }
      return 0;

    // bool CanRaiseActor (int tid);
    case ACSF_CanRaiseActor:
      if (argCount >= 0) {
        VEntity *ent = EntityFromTID((argCount > 0 ? args[1] : 0), Activator);
        if (ent) return (ent->eventCanRaise() ? 1 : 0);
      }
      return 0;

    case ACSF_GetMaxInventory:
      GCon->Logf(NAME_Warning, "ACSF `GetMaxInventory` is not implemented");
      return 0;

    case ACSF_GetLineUDMFInt:
      if (argCount >= 2) {
        VStr key = GetUDMFKeyName(args[1]);
        int id = args[0];
        if (id == 0) {
          if (!actline) return 0;
          return Level->XLevel->getLineIdxKeyInt((int)(ptrdiff_t)(actline-&Level->XLevel->Lines[0]), *key);
        } else {
          return Level->XLevel->getLineKeyInt(id, *key);
        }
      }
      return 0;
    case ACSF_GetLineUDMFFixed:
      if (argCount >= 2) {
        VStr key = GetUDMFKeyName(args[1]);
        int id = args[0];
        if (id == 0) {
          if (!actline) return 0;
          return (int)(Level->XLevel->getLineIdxKeyFloat((int)(ptrdiff_t)(actline-&Level->XLevel->Lines[0]), *key)*65536.0f);
        } else {
          return (int)(Level->XLevel->getLineKeyFloat(id, *key)*65536.0f);
        }
      }
      return 0;

    case ACSF_GetThingUDMFInt:
      if (argCount >= 2) {
        VStr key = GetUDMFKeyName(args[1]);
        int id = args[0];
        if (id == 0 && Activator) id = Activator->TID;
        return Level->XLevel->getThingKeyInt(id, *key);
      }
      return 0;
    case ACSF_GetThingUDMFFixed:
      if (argCount >= 2) {
        VStr key = GetUDMFKeyName(args[1]);
        int id = args[0];
        if (id == 0 && Activator) id = Activator->TID;
        return (int)(Level->XLevel->getThingKeyFloat(id, *key)*65536.0f);
      }
      return 0;

    case ACSF_GetSectorUDMFInt:
      if (argCount >= 2) {
        VStr key = GetUDMFKeyName(args[1]);
        return Level->XLevel->getSectorKeyInt(args[0], *key);
      }
      return 0;
    case ACSF_GetSectorUDMFFixed:
      if (argCount >= 2) {
        VStr key = GetUDMFKeyName(args[1]);
        return (int)(Level->XLevel->getSectorKeyFloat(args[0], *key)*65536.0f);
      }
      return 0;

    case ACSF_GetSideUDMFInt:
      if (argCount >= 3 && args[1] >= 0 && args[1] <= 1) {
        VStr key = GetUDMFKeyName(args[2]);
        int id = args[0];
        if (id == 0) {
          if (!actline) return 0;
          return Level->XLevel->getSideIdxKeyInt(actline->sidenum[args[1]], *key);
        } else {
          return Level->XLevel->getSideKeyInt(args[1], id, *key);
        }
      }
      return 0;
    case ACSF_GetSideUDMFFixed:
      if (argCount >= 3 && args[1] >= 0 && args[1] <= 1) {
        VStr key = GetUDMFKeyName(args[2]);
        int id = args[0];
        if (id == 0 && actline) id = actline->lineTag;
        if (id == 0) {
          if (!actline) return 0;
          return (int)(Level->XLevel->getSideIdxKeyFloat(actline->sidenum[args[1]], *key)*65536.0f);
        } else {
          return (int)(Level->XLevel->getSideKeyFloat(args[1], id, *key)*65536.0f);
        }
      }
      return 0;

    // int DamageActor (int targettid, int targetptr, int inflictortid, int inflictorptr, int damage, str damagetype);
    case ACSF_DamageActor:
      if (argCount >= 6) {
        return Level->eventAcsDamageActor(args[0], args[1], args[2], args[3], args[4], GetName(args[5]), Activator);
      }
      return -1;

    // int CalcActorLight (int tid, int tidptr, int flags)
    case ACSF_CalcActorLight:
      if (argCount >= 3) {
        VEntity *act = EntityFromTID(args[0], Activator);
        if (act) act = act->eventDoAAPtr(args[1]);
        if (act) return act->XLevel->CalcEntityLight(act, args[2]);
      }
      return 0;

    // int CalcPlayerLight (int flags)
    case ACSF_CalcPlayerLight:
      #ifdef CLIENT
      if (argCount >= 1) {
        if (GGameInfo->NetMode == NM_Standalone ||
            GGameInfo->NetMode == NM_Client ||
            GGameInfo->NetMode == NM_ListenServer)
        {
          if (cl && cls.signon && cl->MO) return cl->MO->XLevel->CalcEntityLight(cl->MO, args[0]);
        }
      }
      #endif
      return 0;
  }

  for (const ACSF_Info *nfo = ACSF_List; nfo->name; ++nfo) {
    if (nfo->index == funcIndex) {
      if (acs_abort_on_unknown_acsf) {
        Host_Error("unimplemented ACSF function '%s' (%d args)", nfo->name, argCount);
      } else {
        if (!unimpACSFWarned.put(nfo->index, true)) {
          GCon->Logf(NAME_Error, "unimplemented ACSF function '%s' (%d args)", nfo->name, argCount);
        }
      }
      return 0;
    }
  }

  if (acs_abort_on_unknown_acsf) {
    Host_Error("unimplemented ACSF function #%d (%d args)", funcIndex, argCount);
  } else {
    if (!unimpACSFWarned.put(funcIndex, true)) {
      GCon->Logf(NAME_Error, "unimplemented ACSF function #%d (%d args)", funcIndex, argCount);
    }
  }

  return 0;
}
