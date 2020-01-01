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
#include "gamedefs.h"
#include "network.h"


//==========================================================================
//
//  VPlayerChannel::VPlayerChannel
//
//==========================================================================
VPlayerChannel::VPlayerChannel (VNetConnection *AConnection, vint32 AIndex, vuint8 AOpenedLocally)
  : VChannel(AConnection, CHANNEL_Player, AIndex, AOpenedLocally)
  , Plr(nullptr)
  , OldData(nullptr)
  , NewObj(false)
  , FieldCondValues(nullptr)
{
}


//==========================================================================
//
//  VPlayerChannel::~VPlayerChannel
//
//==========================================================================
VPlayerChannel::~VPlayerChannel () {
  SetPlayer(nullptr);
}


//==========================================================================
//
//  VPlayerChannel::SetPlayer
//
//==========================================================================
void VPlayerChannel::SetPlayer (VBasePlayer *APlr) {
  if (Plr) {
    if (OldData) {
      for (VField *F = Plr->GetClass()->NetFields; F; F = F->NextNetField) {
        VField::DestructField(OldData+F->Ofs, F->Type);
      }
      delete[] OldData;
      OldData = nullptr;
    }
    if (FieldCondValues) {
      delete[] FieldCondValues;
      FieldCondValues = nullptr;
    }
  }

  Plr = APlr;

  if (Plr) {
    VBasePlayer *Def = (VBasePlayer *)Plr->GetClass()->Defaults;
    OldData = new vuint8[Plr->GetClass()->ClassSize];
    memset(OldData, 0, Plr->GetClass()->ClassSize);
    for (VField *F = Plr->GetClass()->NetFields; F; F = F->NextNetField) {
      VField::CopyFieldValue((vuint8 *)Def+F->Ofs, OldData+F->Ofs, F->Type);
    }
    FieldCondValues = new vuint8[Plr->GetClass()->NumNetFields];
    NewObj = true;
  }
}


//==========================================================================
//
//  VPlayerChannel::EvalCondValues
//
//==========================================================================
void VPlayerChannel::EvalCondValues (VObject *Obj, VClass *Class, vuint8 *Values) {
  if (Class->GetSuperClass()) EvalCondValues(Obj, Class->GetSuperClass(), Values);
  for (int i = 0; i < Class->RepInfos.Num(); ++i) {
    bool Val = VObject::ExecuteFunctionNoArgs(Obj, Class->RepInfos[i].Cond, true).getBool(); // allow VMT lookups
    for (int j = 0; j < Class->RepInfos[i].RepFields.Num(); ++j) {
      if (Class->RepInfos[i].RepFields[j].Member->MemberType != MEMBER_Field) continue;
      Values[((VField *)Class->RepInfos[i].RepFields[j].Member)->NetIndex] = Val;
    }
  }
}


//==========================================================================
//
//  VPlayerChannel::Update
//
//==========================================================================
void VPlayerChannel::Update () {
  EvalCondValues(Plr, Plr->GetClass(), FieldCondValues);

  VMessageOut Msg(this);
  Msg.bReliable = true;
  vuint8 *Data = (vuint8*)Plr;
  for (VField *F = Plr->GetClass()->NetFields; F; F = F->NextNetField) {
    if (!FieldCondValues[F->NetIndex]) continue;
    if (VField::IdenticalValue(Data+F->Ofs, OldData+F->Ofs, F->Type)) continue;
    if (F->Type.Type == TYPE_Array) {
      VFieldType IntType = F->Type;
      IntType.Type = F->Type.ArrayInnerType;
      int InnerSize = IntType.GetSize();
      for (int i = 0; i < F->Type.GetArrayDim(); ++i) {
        if (VField::IdenticalValue(Data+F->Ofs+i*InnerSize, OldData+F->Ofs+i*InnerSize, IntType)) continue;
        Msg.WriteInt(F->NetIndex/*, Plr->GetClass()->NumNetFields*/);
        Msg.WriteInt(i/*, F->Type.GetArrayDim()*/);
        if (VField::NetSerialiseValue(Msg, Connection->ObjMap, Data+F->Ofs+i*InnerSize, IntType)) {
          VField::CopyFieldValue(Data+F->Ofs+i*InnerSize, OldData+F->Ofs+i*InnerSize, IntType);
        }
      }
    } else {
      Msg.WriteInt(F->NetIndex/*, Plr->GetClass()->NumNetFields*/);
      if (VField::NetSerialiseValue(Msg, Connection->ObjMap, Data+F->Ofs, F->Type)) {
        VField::CopyFieldValue(Data+F->Ofs, OldData+F->Ofs, F->Type);
      }
    }
  }

  if (Msg.GetNumBits()) SendMessage(&Msg);
}


//==========================================================================
//
//  VPlayerChannel::ParsePacket
//
//==========================================================================
void VPlayerChannel::ParsePacket (VMessageIn &Msg) {
  while (!Msg.AtEnd()) {
    int FldIdx = Msg.ReadInt(/*Plr->GetClass()->NumNetFields*/);
    VField *F = nullptr;
    for (VField *CF = Plr->GetClass()->NetFields; CF; CF = CF->NextNetField) {
      if (CF->NetIndex == FldIdx) {
        F = CF;
        break;
      }
    }
    if (F) {
      if (F->Type.Type == TYPE_Array) {
        int Idx = Msg.ReadInt(/*F->Type.GetArrayDim()*/);
        VFieldType IntType = F->Type;
        IntType.Type = F->Type.ArrayInnerType;
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)Plr+F->Ofs+Idx*IntType.GetSize(), IntType);
      } else {
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)Plr+F->Ofs, F->Type);
      }
      continue;
    }

    if (ReadRpc(Msg, FldIdx, Plr)) continue;

    Sys_Error("Bad net field %d", FldIdx);
  }
}
