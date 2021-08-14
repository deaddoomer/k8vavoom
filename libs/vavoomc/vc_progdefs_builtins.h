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
//** directly included from "vc_progdefs.h"
//**************************************************************************
  DECLARE_OPC_BUILTIN(IntAbs),
  DECLARE_OPC_BUILTIN(FloatAbs),
  DECLARE_OPC_BUILTIN(IntSign),
  DECLARE_OPC_BUILTIN(FloatSign),
  DECLARE_OPC_BUILTIN(IntMin),
  DECLARE_OPC_BUILTIN(IntMax),
  DECLARE_OPC_BUILTIN(FloatMin),
  DECLARE_OPC_BUILTIN(FloatMax),
  DECLARE_OPC_BUILTIN(IntClamp),
  DECLARE_OPC_BUILTIN(FloatClamp),
  DECLARE_OPC_BUILTIN(FloatIsNaN),
  DECLARE_OPC_BUILTIN(FloatIsInf),
  DECLARE_OPC_BUILTIN(FloatIsFinite),
  DECLARE_OPC_BUILTIN(DegToRad),
  DECLARE_OPC_BUILTIN(RadToDeg),
  DECLARE_OPC_BUILTIN(AngleMod360),
  DECLARE_OPC_BUILTIN(AngleMod180),
  DECLARE_OPC_BUILTIN(Sin),
  DECLARE_OPC_BUILTIN(Cos),
  DECLARE_OPC_BUILTIN(SinCos),
  DECLARE_OPC_BUILTIN(Tan),
  DECLARE_OPC_BUILTIN(ASin),
  DECLARE_OPC_BUILTIN(ACos),
  DECLARE_OPC_BUILTIN(ATan), // slope
  DECLARE_OPC_BUILTIN(Sqrt),
  DECLARE_OPC_BUILTIN(ATan2), // y, x
  DECLARE_OPC_BUILTIN(FMod),
  DECLARE_OPC_BUILTIN(FModPos),
  DECLARE_OPC_BUILTIN(FPow),
  DECLARE_OPC_BUILTIN(FLog2),
  DECLARE_OPC_BUILTIN(FloatEquEps), // v0, v1, eps
  DECLARE_OPC_BUILTIN(FloatEquEpsLess), // v0, v1, eps
  DECLARE_OPC_BUILTIN(VecLength),
  DECLARE_OPC_BUILTIN(VecLengthSquared),
  DECLARE_OPC_BUILTIN(VecLength2D),
  DECLARE_OPC_BUILTIN(VecLength2DSquared),
  DECLARE_OPC_BUILTIN(VecNormalize),
  DECLARE_OPC_BUILTIN(VecNormalize2D),
  DECLARE_OPC_BUILTIN(VecDot),
  DECLARE_OPC_BUILTIN(VecDot2D),
  DECLARE_OPC_BUILTIN(VecCross),
  DECLARE_OPC_BUILTIN(VecCross2D),
  DECLARE_OPC_BUILTIN(RoundF2I),
  DECLARE_OPC_BUILTIN(RoundF2F),
  DECLARE_OPC_BUILTIN(TruncF2I),
  DECLARE_OPC_BUILTIN(TruncF2F),
  DECLARE_OPC_BUILTIN(FloatCeil),
  DECLARE_OPC_BUILTIN(FloatFloor),
  DECLARE_OPC_BUILTIN(FloatLerp),
  DECLARE_OPC_BUILTIN(IntLerp),
  DECLARE_OPC_BUILTIN(FloatSmoothStep),
  DECLARE_OPC_BUILTIN(FloatSmoothStepPerlin),
  DECLARE_OPC_BUILTIN(NameToIIndex),
  DECLARE_OPC_BUILTIN(VectorClampF),
  DECLARE_OPC_BUILTIN(VectorClampV),
  DECLARE_OPC_BUILTIN(VectorClampScaleF),
  DECLARE_OPC_BUILTIN(VectorMinV),
  DECLARE_OPC_BUILTIN(VectorMaxV),
  DECLARE_OPC_BUILTIN(VectorMinF),
  DECLARE_OPC_BUILTIN(VectorMaxF),
  DECLARE_OPC_BUILTIN(VectorAbs),
  // cvar getters/setters with runtime-defined names
  // should be sequential in exactly this order!
  DECLARE_OPC_BUILTIN(GetCvarIntRT),
  DECLARE_OPC_BUILTIN(GetCvarFloatRT),
  DECLARE_OPC_BUILTIN(GetCvarStrRT),
  DECLARE_OPC_BUILTIN(GetCvarBoolRT),
  DECLARE_OPC_BUILTIN(SetCvarIntRT),
  DECLARE_OPC_BUILTIN(SetCvarFloatRT),
  DECLARE_OPC_BUILTIN(SetCvarStrRT),
  DECLARE_OPC_BUILTIN(SetCvarBoolRT),
  // cvar optimisations
  // in the bytecode, this builtin is followed with name index, and `VCvar*`; it can be `nullptr`, and can be hotpached
  // should be sequential in exactly this order!
  DECLARE_OPC_BUILTIN(GetCvarInt),
  DECLARE_OPC_BUILTIN(GetCvarFloat),
  DECLARE_OPC_BUILTIN(GetCvarStr),
  DECLARE_OPC_BUILTIN(GetCvarBool),
  DECLARE_OPC_BUILTIN(SetCvarInt),
  DECLARE_OPC_BUILTIN(SetCvarFloat),
  DECLARE_OPC_BUILTIN(SetCvarStr),
  DECLARE_OPC_BUILTIN(SetCvarBool),
  // `write` and `writeln`
  //DECLARE_OPC_BUILTIN(DoWriteOne/*, Type*/), // this has special bytecode arg: `OPCARGS_Type`
  //DECLARE_OPC_BUILTIN(DoWriteFlush/*, None*/),
