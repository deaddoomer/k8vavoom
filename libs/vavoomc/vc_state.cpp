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
//**  Copyright (C) 2018-2023 Ketmar Dark
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
#include "vc_local.h"

VState *VState::mNoJumpState = nullptr;
VState *VState::mInvalidState = nullptr;


//==========================================================================
//
//  VState::VState
//
//==========================================================================
VState::VState (VName AName, VMemberBase *AOuter, TLocation ALoc)
  : VMemberBase(MEMBER_State, AName, AOuter, ALoc)
  , Type(Vavoom)
  , TicType(TicKind::TCK_Normal)
  , SpriteName(NAME_None)
  , Frame(0)
  , Time(0)
  , Misc1(0)
  , Misc2(0)
  , Arg1(0)
  , Arg2(0)
  , NextState(nullptr)
  , Function(nullptr)
  , Next(nullptr)
  , GotoLabel(NAME_None)
  , GotoOffset(0)
  , FunctionName(NAME_None)
  , frameWidth(-1)
  , frameHeight(-1)
  , frameOfsX(0)
  , frameOfsY(0)
  , frameAction(0)
  , SpriteIndex(0)
  , InClassIndex(-1)
  , NetId(-1)
  , NetNext(nullptr)
  , LightInited(false)
  , LightDef(nullptr)
  , LightName(VStr())
  , infoFlags(0)
  , validcount(0)
  , funcIsCopy(false)
{
}


//==========================================================================
//
//  VState::~VState
//
//==========================================================================
VState::~VState () {
}


//==========================================================================
//
//  VState::StaticInit
//
//  called from `VBaseMember::StaticInit()`
//
//==========================================================================
void VState::StaticInit () {
  mNoJumpState = new VState("<:nojump_state:>", /*VClass*/nullptr, TLocation());
  mInvalidState = new VState("<:invalid_state:>", /*VClass*/nullptr, TLocation());
  //InClass->AddState(mNoJumpState);
  mNoJumpState->SpriteName = NAME_None;
  mNoJumpState->Frame = 0|VState::FF_SKIPOFFS|VState::FF_SKIPMODEL|VState::FF_DONTCHANGE|VState::FF_KEEPSPRITE;
  mNoJumpState->Time = 0;
}


//==========================================================================
//
//  VState::PostLoad
//
//==========================================================================
void VState::PostLoad () {
  SpriteIndex = (SpriteName != NAME_None ? VClass::FindAddSprite(SpriteName) : 1);
  NetNext = Next;
}


//==========================================================================
//
//  VState::Define
//
//==========================================================================
bool VState::Define () {
  bool Ret = true;
  if (!funcIsCopy) {
    if (Function && !Function->Define()) Ret = false;
  }
  return Ret;
}


//==========================================================================
//
//  VState::Emit
//
//==========================================================================
void VState::Emit () {
  VEmitContext ec(this);
  if (GotoLabel != NAME_None) {
    //fprintf(stderr, "state `%s` label resolve: %s\n", *GetFullName(), *GotoLabel);
    NextState = ((VClass *)Outer)->ResolveStateLabel(Loc, GotoLabel, GotoOffset);
  }

  if (Function) {
    if (!Function->IsDefined()) {
      ParseWarning(Loc, "State function `%s` wasn't properly defined", *Function->GetFullName());
    }
    //GLog.Logf(NAME_Debug, "%s: emiting function `%s` (%s)", *Loc.toString(), *Function->GetFullName(), *Function->Loc.toString());
    if (!funcIsCopy) Function->Emit();
    FunctionName = NAME_None; // need this for decorate
  } else if (FunctionName != NAME_None) {
    funcIsCopy = true; // set it here, so decorate code won't try to `PostLoad()` this state's function
    Function = ((VClass *)Outer)->FindMethod(FunctionName);
    if (!Function) {
      ParseError(Loc, "No such method `%s` in class `%s`", *FunctionName, ((VClass *)Outer)->GetName());
    } else {
      if (Function->Flags&FUNC_VarArgs) ParseError(Loc, "State method must not have varargs");
      // executor will properly remove the result, and some decorate methods should return something
      //if (Function->ReturnType.Type != TYPE_Void) ParseError(Loc, "State method must not return a value");
      if (!Function->IsGoodStateMethod()) ParseError(Loc, "State method must be callable without arguments");
      // ok, state methods can be virtual and static now
      if (Type != Vavoom && (Function->Flags&FUNC_Static) != 0) ParseError(Loc, "State method must not be static");
      /*
      if (Function->Flags&FUNC_Static) ParseError(Loc, "State method must not be static");
      if (Type == Vavoom) {
        if (!(Function->Flags&FUNC_Final)) ParseError(Loc, "State method must be final"); //k8: why?
      }
      */
      //GLog.Logf(NAME_Debug, "%s: direct function `%s` (%s)", *Loc.toString(), *Function->GetFullName(), *Function->Loc.toString());
    }
  }
}


//==========================================================================
//
//  VState::IsInRange
//
//==========================================================================
bool VState::IsInRange (VState *Start, VState *End, int MaxDepth) {
  int Depth = 0;
  VState *check = Start;
  do {
    if (check == this) return true;
    if (check) check = check->Next;
    ++Depth;
  } while (Depth < MaxDepth && check != End);
  return false;
}


//==========================================================================
//
//  VState::IsInSequence
//
//==========================================================================
bool VState::IsInSequence (VState *Start) {
  for (VState *check = Start; check; check = (check->Next == check->NextState ? check->Next : nullptr)) {
    if (check == this) return true;
  }
  return false;
}


//==========================================================================
//
//  VState::GetPlus
//
//==========================================================================
VState *VState::GetPlus (int Offset, bool IgnoreJump) {
  if (Offset < 0) return nullptr;
  vassert(Offset >= 0);
  VState *S = this;
  int Count = Offset;
  while (S && Count--) {
    if (!IgnoreJump && S->Next != S->NextState) return nullptr;
    if (S->Frame&VState::FF_SKIPOFFS) ++Count;
    S = S->Next;
  }
  return S;
}
