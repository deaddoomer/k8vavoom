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
  DECLARE_FUNCTION(Destroy)
  DECLARE_FUNCTION(IsA)
  //DECLARE_FUNCTION(IsDestroyed)
  DECLARE_FUNCTION(GC_CollectGarbage)
  DECLARE_FUNCTION(get_GC_AliveObjects)
  DECLARE_FUNCTION(get_GC_LastCollectedObjects)
  DECLARE_FUNCTION(get_GC_LastCollectDuration)
  DECLARE_FUNCTION(get_GC_LastCollectTime)
  DECLARE_FUNCTION(get_GC_MessagesAllowed)
  DECLARE_FUNCTION(set_GC_MessagesAllowed)

#include "vc_object_rtti.h"

  // error functions
  DECLARE_FUNCTION(Error)
  DECLARE_FUNCTION(FatalError)
  DECLARE_FUNCTION(AssertError)

  // math functions
  //DECLARE_FUNCTION(AngleMod360)
  //DECLARE_FUNCTION(AngleMod180)

  DECLARE_FUNCTION(AngleVectors)
  DECLARE_FUNCTION(AngleVector)
  DECLARE_FUNCTION(VectorAngles)
  DECLARE_FUNCTION(DirVectorsAngles)
  DECLARE_FUNCTION(VectorAngleYaw)
  DECLARE_FUNCTION(VectorAnglePitch)

  DECLARE_FUNCTION(AngleYawVector)
  DECLARE_FUNCTION(AnglePitchVector)
  DECLARE_FUNCTION(AnglesRightVector)
  DECLARE_FUNCTION(YawVectorRight)

  DECLARE_FUNCTION(RotateDirectionVector)
  DECLARE_FUNCTION(VectorRotateAroundZ)
  DECLARE_FUNCTION(RotateVectorAroundVector)

  DECLARE_FUNCTION(RotatePointAroundVector)

  DECLARE_FUNCTION(GetPlanePointZ)
  DECLARE_FUNCTION(GetPlanePointZRev)
  DECLARE_FUNCTION(PointOnPlaneSide)
  DECLARE_FUNCTION(PointOnPlaneSide2)

  DECLARE_FUNCTION(BoxOnLineSide2DV)

  DECLARE_FUNCTION(IsPlainFloor)
  DECLARE_FUNCTION(IsPlainCeiling)
  DECLARE_FUNCTION(IsSlopedFlat)
  DECLARE_FUNCTION(IsVerticalPlane)

  DECLARE_FUNCTION(PlaneProjectPoint)
  DECLARE_FUNCTION(PlanePointDistance)

  DECLARE_FUNCTION(PlaneLineIntersectTime)
  DECLARE_FUNCTION(PlaneLineIntersect)

  DECLARE_FUNCTION(PlaneForPointDir)
  DECLARE_FUNCTION(PlaneForPointNormal)
  DECLARE_FUNCTION(PlaneForLine)

  // name functions
  DECLARE_FUNCTION(nameicmp)
  DECLARE_FUNCTION(namestricmp)
  DECLARE_FUNCTION(nameEquCI)
  DECLARE_FUNCTION(nameStrEqu)
  DECLARE_FUNCTION(nameStrEquCI)

  DECLARE_FUNCTION(nameStartsWith)
  DECLARE_FUNCTION(nameStartsWithCI)
  DECLARE_FUNCTION(nameStrStartsWith)
  DECLARE_FUNCTION(nameStrStartsWithCI)

  DECLARE_FUNCTION(nameEndsWith)
  DECLARE_FUNCTION(nameEndsWithCI)
  DECLARE_FUNCTION(nameStrEndsWith)
  DECLARE_FUNCTION(nameStrEndsWithCI)

  // string functions
  DECLARE_FUNCTION(strEqu)
  DECLARE_FUNCTION(strEquCI)
  DECLARE_FUNCTION(strNameEqu)
  DECLARE_FUNCTION(strNameEquCI)
  DECLARE_FUNCTION(strlenutf8)
  DECLARE_FUNCTION(strcmp)
  DECLARE_FUNCTION(stricmp)
  //DECLARE_FUNCTION(strcat)
  DECLARE_FUNCTION(strlwr)
  DECLARE_FUNCTION(strupr)
  DECLARE_FUNCTION(substrutf8)
  DECLARE_FUNCTION(strmid)
  DECLARE_FUNCTION(strleft)
  DECLARE_FUNCTION(strright)
  DECLARE_FUNCTION(strrepeat)
  DECLARE_FUNCTION(strFromChar)
  DECLARE_FUNCTION(strFromCharUtf8)
  DECLARE_FUNCTION(strFromInt)
  DECLARE_FUNCTION(strFromFloat)
  DECLARE_FUNCTION(va)
  DECLARE_FUNCTION(atoi)
  DECLARE_FUNCTION(atof)
  DECLARE_FUNCTION(StrStartsWith)
  DECLARE_FUNCTION(StrEndsWith)
  DECLARE_FUNCTION(StrStartsWithCI)
  DECLARE_FUNCTION(StrEndsWithCI)
  DECLARE_FUNCTION(StrReplace)
  DECLARE_FUNCTION(globmatch)
  DECLARE_FUNCTION(strApproxMatch)
  DECLARE_FUNCTION(strIndexOf)
  DECLARE_FUNCTION(strLastIndexOf)
  DECLARE_FUNCTION(findExistingName)
  DECLARE_FUNCTION(strRemoveColors)
  DECLARE_FUNCTION(strXStrip)
  DECLARE_FUNCTION(strTrimAll)
  DECLARE_FUNCTION(strTrimLeft)
  DECLARE_FUNCTION(strTrimRight)

  // random numbers
  DECLARE_FUNCTION(Random)
  DECLARE_FUNCTION(FRandomFull)
  DECLARE_FUNCTION(FRandomBetween)
  DECLARE_FUNCTION(P_Random)

  DECLARE_FUNCTION(GenRandomSeedU32)
  DECLARE_FUNCTION(GenRandomU31)

  DECLARE_FUNCTION(bjprngSeedRandom)
  DECLARE_FUNCTION(bjprngSeed)
  DECLARE_FUNCTION(bjprngNext)
  DECLARE_FUNCTION(bjprngNextU31)
  DECLARE_FUNCTION(bjprngNextFloat)
  DECLARE_FUNCTION(bjprngNextFloatFull)

  DECLARE_FUNCTION(pcg3264SeedRandom)
  DECLARE_FUNCTION(pcg3264Seed)
  DECLARE_FUNCTION(pcg3264Next)
  DECLARE_FUNCTION(pcg3264NextU31)
  DECLARE_FUNCTION(pcg3264NextFloat)
  DECLARE_FUNCTION(pcg3264NextFloatFull)

  DECLARE_FUNCTION(chachaIsValid)
  DECLARE_FUNCTION(chachaGetRounds)
  DECLARE_FUNCTION(chachaSeedRandom)
  DECLARE_FUNCTION(chachaSeed)
  DECLARE_FUNCTION(chachaSeedEx)
  DECLARE_FUNCTION(chachaNext)
  DECLARE_FUNCTION(chachaNextU31)
  DECLARE_FUNCTION(chachaNextFloat)
  DECLARE_FUNCTION(chachaNextFloatFull)

  // printing in console
  DECLARE_FUNCTION(print)
  DECLARE_FUNCTION(dprint)
  DECLARE_FUNCTION(printwarn)
  DECLARE_FUNCTION(printerror)
  DECLARE_FUNCTION(printdebug)
  DECLARE_FUNCTION(printmsg)

  // class methods
  DECLARE_FUNCTION(FindClass)
  DECLARE_FUNCTION(FindClassNoCase)
  DECLARE_FUNCTION(FindClassNoCaseStr)
  DECLARE_FUNCTION(FindClassNoCaseEx)
  DECLARE_FUNCTION(FindClassNoCaseExStr)
  DECLARE_FUNCTION(ClassIsChildOf)
  DECLARE_FUNCTION(GetClassName)
  DECLARE_FUNCTION(GetFullClassName)
  DECLARE_FUNCTION(GetClassLocationStr)
  DECLARE_FUNCTION(IsAbstractClass)
  DECLARE_FUNCTION(IsNativeClass)
  DECLARE_FUNCTION(GetClassParent)
  DECLARE_FUNCTION(GetClassReplacement)
  DECLARE_FUNCTION(GetCompatibleClassReplacement)
  DECLARE_FUNCTION(GetClassReplacee)
  DECLARE_FUNCTION(FindClassState)
  DECLARE_FUNCTION(GetClassNumOwnedStates)
  DECLARE_FUNCTION(GetClassFirstState)
  DECLARE_FUNCTION(GetClassGameObjName)

  DECLARE_FUNCTION(GetClassInstanceCount)
  DECLARE_FUNCTION(GetClassInstanceCountWithSub)

  // state methods
  DECLARE_FUNCTION(GetStateLocationStr)
  DECLARE_FUNCTION(GetFullStateName)
  DECLARE_FUNCTION(StateIsInRange)
  DECLARE_FUNCTION(StateIsInSequence)
  DECLARE_FUNCTION(GetStateSpriteName)
  DECLARE_FUNCTION(GetStateSpriteFrame)
  DECLARE_FUNCTION(GetStateSpriteFrameWidth)
  DECLARE_FUNCTION(GetStateSpriteFrameHeight)
  DECLARE_FUNCTION(GetStateSpriteFrameSize)
  DECLARE_FUNCTION(GetStateDuration)
  DECLARE_FUNCTION(GetStatePlus)
  DECLARE_FUNCTION(GetNextState)
  DECLARE_FUNCTION(GetNextStateInProg)
  DECLARE_FUNCTION(StateHasAction)
  DECLARE_FUNCTION(CallStateAction)
  DECLARE_FUNCTION(GetStateSpriteFrameOfsX)
  DECLARE_FUNCTION(GetStateSpriteFrameOfsY)
  DECLARE_FUNCTION(GetStateSpriteFrameOffset)
  DECLARE_FUNCTION(GetStateMisc1)
  DECLARE_FUNCTION(GetStateMisc2)
  DECLARE_FUNCTION(SetStateMisc1)
  DECLARE_FUNCTION(SetStateMisc2)
  DECLARE_FUNCTION(GetStateTicKind)
  DECLARE_FUNCTION(GetStateArgN)
  DECLARE_FUNCTION(SetStateArgN)
  DECLARE_FUNCTION(GetStateFRN)
  DECLARE_FUNCTION(SetStateFRN)

  // Iterators
  DECLARE_FUNCTION(AllObjects)
  DECLARE_FUNCTION(AllClasses)
  DECLARE_FUNCTION(AllClassStates)

  DECLARE_FUNCTION(SpawnObject)

  DECLARE_FUNCTION(GetInputKeyStrName)
  DECLARE_FUNCTION(GetInputKeyCode)

  DECLARE_FUNCTION(GetTimeOfDay)
  DECLARE_FUNCTION(DecodeTimeVal);
  DECLARE_FUNCTION(EncodeTimeVal);

  DECLARE_FUNCTION(FindMObjId)
  DECLARE_FUNCTION(FindScriptId)
  DECLARE_FUNCTION(FindClassByGameObjName)

  // cvar functions
  DECLARE_FUNCTION(CvarExists)
  DECLARE_FUNCTION(CvarGetFlags)
  DECLARE_FUNCTION(GetCvarDefault)
  DECLARE_FUNCTION(CreateCvar)
  DECLARE_FUNCTION(CreateCvarInt)
  DECLARE_FUNCTION(CreateCvarFloat)
  DECLARE_FUNCTION(CreateCvarBool)
  DECLARE_FUNCTION(CreateCvarStr)
  DECLARE_FUNCTION(GetCvar)
  DECLARE_FUNCTION(SetCvar)
  DECLARE_FUNCTION(GetCvarF)
  DECLARE_FUNCTION(SetCvarF)
  DECLARE_FUNCTION(GetCvarS)
  DECLARE_FUNCTION(SetCvarS)
  DECLARE_FUNCTION(GetCvarB)
  DECLARE_FUNCTION(SetCvarB)
  DECLARE_FUNCTION(GetCvarHelp)
  //WARNING! must be defined by all api users separately!
  DECLARE_FUNCTION(CvarUnlatchAll)

  DECLARE_FUNCTION(SetShadowCvar)
  DECLARE_FUNCTION(SetShadowCvarF)
  DECLARE_FUNCTION(SetShadowCvarS)
  DECLARE_FUNCTION(SetShadowCvarB)

  DECLARE_FUNCTION(SetNamePutElement)
  DECLARE_FUNCTION(SetNameCheckElement)

  DECLARE_FUNCTION(RayLineIntersection2D)
  DECLARE_FUNCTION(RayLineIntersection2DDir)

  DECLARE_FUNCTION(EventClear)
  DECLARE_FUNCTION(EventIsAnyMouse)
  DECLARE_FUNCTION(EventIsMouseButton)

  DECLARE_FUNCTION(PostEvent)
  DECLARE_FUNCTION(InsertEvent)
  DECLARE_FUNCTION(CountQueuedEvents)
  DECLARE_FUNCTION(PeekEvent)
  DECLARE_FUNCTION(GetEvent)
  DECLARE_FUNCTION(GetEventQueueSize)

  DECLARE_FUNCTION(RGB)
  DECLARE_FUNCTION(RGBA)
  DECLARE_FUNCTION(RGBf)
  DECLARE_FUNCTION(RGBAf)

  DECLARE_FUNCTION(RGBGetR)
  DECLARE_FUNCTION(RGBGetG)
  DECLARE_FUNCTION(RGBGetB)
  DECLARE_FUNCTION(RGBGetA)
  DECLARE_FUNCTION(RGBSetR)
  DECLARE_FUNCTION(RGBSetG)
  DECLARE_FUNCTION(RGBSetB)
  DECLARE_FUNCTION(RGBSetA)

  DECLARE_FUNCTION(RGBGetRf)
  DECLARE_FUNCTION(RGBGetGf)
  DECLARE_FUNCTION(RGBGetBf)
  DECLARE_FUNCTION(RGBGetAf)
  DECLARE_FUNCTION(RGBSetRf)
  DECLARE_FUNCTION(RGBSetGf)
  DECLARE_FUNCTION(RGBSetBf)
  DECLARE_FUNCTION(RGBSetAf)

  DECLARE_FUNCTION(RGB2HSV)
  DECLARE_FUNCTION(HSV2RGB)

  DECLARE_FUNCTION(RGB2HSL)
  DECLARE_FUNCTION(HSL2RGB)

  DECLARE_FUNCTION(RGBDistanceSquared)
  DECLARE_FUNCTION(sRGBGamma)
  DECLARE_FUNCTION(sRGBUngamma)
  DECLARE_FUNCTION(sRGBIntensity)
