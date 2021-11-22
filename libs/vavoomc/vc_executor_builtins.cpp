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
//** directrly included from "vc_executor.cpp"
//**************************************************************************
#ifdef VC_VM_LABEL_TABLE

#define DECLARE_OPC_BUILTIN(name) &&Lbl_OPC_Builtin_ ## name
static void *vm_labels_builtins[] = {
#include "vc_progdefs_builtins.h"
0 };
#undef DECLARE_OPC_BUILTIN

#else

#define PR_VMBN_SWITCH(op)  goto *vm_labels_builtins[op];
#define PR_VMBN_CASE(x)     Lbl_ ## x:
#if USE_COMPUTED_GOTO
# define PR_VMBN_BREAK  ip += 2; PR_VM_BREAK
#else
# define PR_VMBN_BREAK  goto vm_labels_builtins_done
#endif

#ifdef VCC_DEBUG_CVAR_CACHE_VMDUMP
{
  unsigned n = ReadU8(ip+1);
  GLog.Logf(NAME_Debug, "OPC_BuiltinCVar: %u", n);
  for (unsigned f = 0; f <= n; ++f) {
    if (!StatementBuiltinInfo[f].name) break;
    if (f == n) {
      GLog.Logf(NAME_Debug, "   <%s>", StatementBuiltinInfo[f].name);
      break;
    }
  }
}
#endif
PR_VMBN_SWITCH(ReadU8(ip+1)) {
  PR_VMBN_CASE(OPC_Builtin_IntAbs) if (sp[-1].i < 0.0f) sp[-1].i = -sp[-1].i; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatAbs) if (!isNaNF(sp[-1].f)) sp[-1].f = fabsf(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_IntSign) if (sp[-1].i < 0) sp[-1].i = -1; else if (sp[-1].i > 0) sp[-1].i = 1; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatSign) if (!isNaNF(sp[-1].f)) sp[-1].f = (sp[-1].f < 0.0f ? -1.0f : sp[-1].f > 0.0f ? +1.0f : 0.0f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_IntMin) if (sp[-2].i > sp[-1].i) sp[-2].i = sp[-1].i; sp -= 1; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_IntMax) if (sp[-2].i < sp[-1].i) sp[-2].i = sp[-1].i; sp -= 1; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatMin)
  if (!isNaNF(sp[-1].f) && !isNaNF(sp[-2].f)) {
    if (sp[-2].f > sp[-1].f) sp[-2].f = sp[-1].f;
  } else {
    if (!isNaNF(sp[-2].f)) sp[-2].f = sp[-1].f;
  }
  sp -= 1;
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatMax)
  if (!isNaNF(sp[-1].f) && !isNaNF(sp[-2].f)) {
    if (sp[-2].f < sp[-1].f) sp[-2].f = sp[-1].f;
  } else {
    if (!isNaNF(sp[-2].f)) sp[-2].f = sp[-1].f;
  }
  sp -= 1;
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_IntClamp) sp[-3].i = clampval(sp[-3].i, sp[-2].i, sp[-1].i); sp -= 2; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatClamp) sp[-3].f = clampval(sp[-3].f, sp[-2].f, sp[-1].f); sp -= 2; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatIsNaN) sp[-1].i = (isNaNF(sp[-1].f) ? 1 : 0); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatIsInf) sp[-1].i = (isInfF(sp[-1].f) ? 1 : 0); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatIsFinite) sp[-1].i = (isFiniteF(sp[-1].f) ? 1 : 0); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_DegToRad) sp[-1].f = DEG2RADF(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_RadToDeg) sp[-1].f = RAD2DEGF(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_AngleMod360)
  if (isFiniteF(sp[-1].f)) {
    sp[-1].f = AngleMod(sp[-1].f);
  } else {
    VObject::VMDumpCallStack();
    if (isNaNF(sp[-1].f)) VPackage::InternalFatalError("got NAN"); else VPackage::InternalFatalError("got INF");
  }
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_AngleMod180)
  if (isFiniteF(sp[-1].f)) {
    sp[-1].f = AngleMod180(sp[-1].f);
  } else {
    VObject::VMDumpCallStack();
    if (isNaNF(sp[-1].f)) VPackage::InternalFatalError("got NAN"); else VPackage::InternalFatalError("got INF");
  }
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_Sin) sp[-1].f = msin(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_Cos) sp[-1].f = mcos(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_Tan) sp[-1].f = mtan(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_ASin) sp[-1].f = masin(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_ACos) sp[-1].f = macos(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_ATan) sp[-1].f = RAD2DEGF(atanf(sp[-1].f)); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_Sqrt) sp[-1].f = sqrtf(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_ATan2) sp[-2].f = matan(sp[-2].f, sp[-1].f); sp -= 1; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_SinCos) msincos(sp[-3].f, (float *)sp[-2].p, (float *)sp[-1].p); sp -= 3; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FMod) sp[-2].f = fmodf(sp[-2].f, sp[-1].f); sp -= 1; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FModPos)
  sp[-2].f = fmodf(sp[-2].f, sp[-1].f);
  if (isFiniteF(sp[-2].f) && isFiniteF(sp[-1].f) && sp[-2].f < 0.0f) sp[-2].f += fabsf(sp[-1].f);
  sp -= 1;
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FPow) sp[-2].f = powf(sp[-2].f, sp[-1].f); sp -= 1; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FLog2) sp[-1].f = log2f(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatEquEps) /*GLog.Logf(NAME_Debug, "v0=%f; v1=%f; eps=%f; diff=%f; res=%d", sp[-3].f, sp[-2].f, sp[-1].f, fabsf(sp[-2].f-sp[-3].f), (fabsf(sp[-2].f-sp[-3].f) <= sp[-1].f));*/ sp[-3].i = (fabsf(sp[-2].f-sp[-3].f) <= sp[-1].f); sp -= 2; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatEquEpsLess) sp[-3].i = (fabsf(sp[-2].f-sp[-3].f) < sp[-1].f); sp -= 2; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_VecLength)
  if (!isFiniteF(sp[-1].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-3].f)) { cstDump(ip); VPackage::InternalFatalError("vector is INF/NAN"); }
  sp[-3].f = sqrtf(VSUM3(sp[-1].f*sp[-1].f, sp[-2].f*sp[-2].f, sp[-3].f*sp[-3].f));
  sp -= 2;
  if (!isFiniteF(sp[-1].f)) { cstDump(ip); VPackage::InternalFatalError("vector length is INF/NAN"); }
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_VecLengthSquared)
  if (!isFiniteF(sp[-1].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-3].f)) { cstDump(ip); VPackage::InternalFatalError("vector is INF/NAN"); }
  sp[-3].f = VSUM3(sp[-1].f*sp[-1].f, sp[-2].f*sp[-2].f, sp[-3].f*sp[-3].f);
  sp -= 2;
  if (!isFiniteF(sp[-1].f)) { cstDump(ip); VPackage::InternalFatalError("vector length is INF/NAN"); }
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_VecLength2D)
  if (!isFiniteF(sp[-1].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-3].f)) { cstDump(ip); VPackage::InternalFatalError("vector is INF/NAN"); }
  sp[-3].f = sqrtf(VSUM2(sp[-2].f*sp[-2].f, sp[-3].f*sp[-3].f));
  sp -= 2;
  if (!isFiniteF(sp[-1].f)) { cstDump(ip); VPackage::InternalFatalError("vector length2D is INF/NAN"); }
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_VecLength2DSquared)
  if (!isFiniteF(sp[-1].f) || !isFiniteF(sp[-2].f) || !isFiniteF(sp[-3].f)) { cstDump(ip); VPackage::InternalFatalError("vector is INF/NAN"); }
  sp[-3].f = VSUM2(sp[-2].f*sp[-2].f, sp[-3].f*sp[-3].f);
  sp -= 2;
  if (!isFiniteF(sp[-1].f)) { cstDump(ip); VPackage::InternalFatalError("vector length2D is INF/NAN"); }
  PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_VecNormalize)
  {
    TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
    if (!v.isValid() || v.isZero()) {
      v = TVec(0, 0, 0);
    } else {
      v.normaliseInPlace();
      // normalizing zero vector should produce zero, not nan/inf
      if (!v.isValid()) v = TVec(0, 0, 0);
    }
    sp[-1].f = v.z;
    sp[-2].f = v.y;
    sp[-3].f = v.x;
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VecNormalize2D)
  {
    TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
    if (!v.isValid() || v.isZero2D()) {
      v = TVec(0, 0, 0);
    } else {
      v.normalise2DInPlace();
      // normalizing zero vector should produce zero, not nan/inf
      if (!v.isValid()) v = TVec(0, 0, 0);
    }
    sp[-1].f = v.z;
    sp[-2].f = v.y;
    sp[-3].f = v.x;
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VecDot)
  {
    TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 3;
    TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 2;
    sp[-1].f = v1.dot(v2);
    if (!isFiniteF(sp[-1].f)) { cstDump(ip); VPackage::InternalFatalError("dotproduct result is INF/NAN"); }
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VecDot2D)
  {
    TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 3;
    TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 2;
    sp[-1].f = v1.dot2D(v2);
    if (!isFiniteF(sp[-1].f)) { cstDump(ip); VPackage::InternalFatalError("dotproduct2d result is INF/NAN"); }
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VecCross)
  {
    TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 3;
    TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
    v1 = v1.cross(v2);
    sp[-1].f = v1.z;
    sp[-2].f = v1.y;
    sp[-3].f = v1.x;
    if (!v1.isValid()) { cstDump(ip); VPackage::InternalFatalError("crossproduct result is INF/NAN"); }
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VecCross2D)
  {
    TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 3;
    TVec v1(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 2;
    sp[-1].f = v1.cross2D(v2);
    if (!isFiniteF(sp[-1].f)) { cstDump(ip); VPackage::InternalFatalError("crossproduct2d result is INF/NAN"); }
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_RoundF2I) sp[-1].i = (int)(roundf(sp[-1].f)); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_RoundF2F) sp[-1].f = roundf(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_TruncF2I) sp[-1].i = (int)(truncf(sp[-1].f)); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_TruncF2F) sp[-1].f = truncf(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatCeil) sp[-1].f = ceilf(sp[-1].f); PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatFloor) sp[-1].f = floorf(sp[-1].f); PR_VMBN_BREAK;
// [-3]: a; [-2]: b, [-1]: delta
  PR_VMBN_CASE(OPC_Builtin_FloatLerp) sp[-3].f = sp[-3].f+(sp[-2].f-sp[-3].f)*sp[-1].f; sp -= 2; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_IntLerp) sp[-3].i = (int)roundf(sp[-3].i+(sp[-2].i-sp[-3].i)*sp[-1].f); sp -= 2; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatSmoothStep) sp[-3].f = smoothstep(sp[-3].f, sp[-2].f, sp[-1].f); sp -= 2; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_FloatSmoothStepPerlin) sp[-3].f = smoothstepPerlin(sp[-3].f, sp[-2].f, sp[-1].f); sp -= 2; PR_VMBN_BREAK;
  PR_VMBN_CASE(OPC_Builtin_NameToIIndex) PR_VMBN_BREAK; // no, really, it is THAT easy
  PR_VMBN_CASE(OPC_Builtin_VectorClampF)
  {
    const float vmin = sp[-2].f;
    const float vmax = sp[-1].f;
    sp -= 2;
    TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
    if (v.isValid()) {
      if (isFiniteF(vmin) && isFiniteF(vmax)) {
        v.x = clampval(v.x, vmin, vmax);
        v.y = clampval(v.y, vmin, vmax);
        v.z = clampval(v.z, vmin, vmax);
      } else if (isFiniteF(vmin)) {
        v.x = min2(vmin, v.x);
        v.y = min2(vmin, v.y);
        v.z = min2(vmin, v.z);
      } else if (isFiniteF(vmax)) {
        v.x = max2(vmax, v.x);
        v.y = max2(vmax, v.y);
        v.z = max2(vmax, v.z);
      }
    }
    sp[-1].f = v.z;
    sp[-2].f = v.y;
    sp[-3].f = v.x;
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VectorClampScaleF) // (vval, fabsmax)
  {
    const float vabsmax = sp[-1].f;
    sp -= 1;
    TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
    v.clampScaleInPlace(vabsmax);
    sp[-1].f = v.z;
    sp[-2].f = v.y;
    sp[-3].f = v.x;
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VectorClampV)
  {
    TVec vmax(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 3;
    TVec vmin(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 3;
    TVec v(sp[-3].f, sp[-2].f, sp[-1].f);
    if (v.isValid()) {
      if (vmin.isValid() && vmax.isValid()) {
        v.x = clampval(v.x, vmin.x, vmax.x);
        v.y = clampval(v.y, vmin.y, vmax.y);
        v.z = clampval(v.z, vmin.z, vmax.z);
      } else if (vmin.isValid()) {
        v.x = min2(vmin.x, v.x);
        v.y = min2(vmin.y, v.y);
        v.z = min2(vmin.z, v.z);
      } else if (vmax.isValid()) {
        v.x = max2(vmax.x, v.x);
        v.y = max2(vmax.y, v.y);
        v.z = max2(vmax.z, v.z);
      }
    }
    sp[-1].f = v.z;
    sp[-2].f = v.y;
    sp[-3].f = v.x;
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VectorMinV)
  {
    TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 3;
    if (v2.isValid() && isFiniteF(sp[-1].f) && isFiniteF(sp[-2].f) && isFiniteF(sp[-3].f)) {
      sp[-1].f = min2(sp[-1].f, v2.z);
      sp[-2].f = min2(sp[-2].f, v2.y);
      sp[-3].f = min2(sp[-3].f, v2.x);
    }
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VectorMaxV)
  {
    TVec v2(sp[-3].f, sp[-2].f, sp[-1].f);
    sp -= 3;
    if (v2.isValid() && isFiniteF(sp[-1].f) && isFiniteF(sp[-2].f) && isFiniteF(sp[-3].f)) {
      sp[-1].f = max2(sp[-1].f, v2.z);
      sp[-2].f = max2(sp[-2].f, v2.y);
      sp[-3].f = max2(sp[-3].f, v2.x);
    }
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VectorMinF)
  {
    float vmin = sp[-1].f;
    sp -= 1;
    if (isFiniteF(vmin) && isFiniteF(sp[-1].f) && isFiniteF(sp[-2].f) && isFiniteF(sp[-3].f)) {
      sp[-1].f = min2(vmin, sp[-1].f);
      sp[-2].f = min2(vmin, sp[-2].f);
      sp[-3].f = min2(vmin, sp[-3].f);
    }
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VectorMaxF)
  {
    float vmax = sp[-1].f;
    sp -= 1;
    if (isFiniteF(vmax) && isFiniteF(sp[-1].f) && isFiniteF(sp[-2].f) && isFiniteF(sp[-3].f)) {
      sp[-1].f = max2(vmax, sp[-1].f);
      sp[-2].f = max2(vmax, sp[-2].f);
      sp[-3].f = max2(vmax, sp[-3].f);
    }
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_VectorAbs)
  {
    TVec v0(sp[-3].f, sp[-2].f, sp[-1].f);
    if (v0.isValid()) {
      sp[-1].f = fabsf(v0.z);
      sp[-2].f = fabsf(v0.y);
      sp[-3].f = fabsf(v0.x);
    }
    PR_VMBN_BREAK;
  }

  // cvar getters/setters with runtime-defined names
  PR_VMBN_CASE(OPC_Builtin_IsCvarExistsRT)
  {
    /*
    #ifdef VCC_DEBUG_CVAR_CACHE
    GLog.Logf(NAME_Debug, "IsCvarExistsRT: %s", *VName::CreateWithIndexSafe(sp[-1].i));
    #endif
    */
    VCvar *vp = GetRTCVar(ip, sp[-1].i, true);
    sp[-1].i = (vp ? 1 : 0);
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_GetCvarIntRT)
  {
    VCvar *vp = GetRTCVar(ip, sp[-1].i);
    sp[-1].i = vp->asInt();
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_GetCvarFloatRT)
  {
    VCvar *vp = GetRTCVar(ip, sp[-1].i);
    sp[-1].f = vp->asFloat();
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_GetCvarStrRT)
  {
    VCvar *vp = GetRTCVar(ip, sp[-1].i);
    sp[-1].p = nullptr;
    *(VStr *)&sp[-1].p = vp->asStr();
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_GetCvarBoolRT)
  {
    VCvar *vp = GetRTCVar(ip, sp[-1].i);
    sp[-1].i = (vp->asBool() ? 1 : 0);
    PR_VMBN_BREAK;
  }

  PR_VMBN_CASE(OPC_Builtin_SetCvarIntRT)
  {
    VCvar *vp = GetRTCVar(ip, sp[-2].i);
    if (!vp->IsReadOnly()) vp->SetInt(sp[-1].i);
    sp -= 2;
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_SetCvarFloatRT)
  {
    VCvar *vp = GetRTCVar(ip, sp[-2].i);
    if (!vp->IsReadOnly()) vp->SetFloat(sp[-1].f);
    sp -= 2;
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_SetCvarStrRT)
  {
    VCvar *vp = GetRTCVar(ip, sp[-2].i);
    if (!vp->IsReadOnly()) vp->SetStr(*((VStr *)&sp[-1].p));
    ((VStr *)&sp[-1].p)->clear();
    sp -= 2;
    PR_VMBN_BREAK;
  }
  PR_VMBN_CASE(OPC_Builtin_SetCvarBoolRT)
  {
    VCvar *vp = GetRTCVar(ip, sp[-2].i);
    if (!vp->IsReadOnly()) vp->SetBool(!!sp[-1].i);
    sp -= 2;
    PR_VMBN_BREAK;
  }

  // cached cvars getters/setters
  // ip+2: (vint32) name index
  // ip+2+4: vcvar ptr (pointer-sized)
  PR_VMBN_CASE(OPC_Builtin_IsCvarExists)
    {
      VCvar *vp = GetAndCacheCVar(ip, true);
      /*
      #ifdef VCC_DEBUG_CVAR_CACHE
      GLog.Logf(NAME_Debug, "IsCvarExists: vp=%p (%s)", vp, (vp ? vp->GetName() : "<none>"));
      #endif
      */
      sp[0].i = (vp ? 1 : 0);
      ++sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }
  PR_VMBN_CASE(OPC_Builtin_GetCvarInt)
    {
      VCvar *vp = GetAndCacheCVar(ip);
      sp[0].i = vp->asInt();
      ++sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }
  PR_VMBN_CASE(OPC_Builtin_GetCvarFloat)
    {
      VCvar *vp = GetAndCacheCVar(ip);
      sp[0].f = vp->asFloat();
      ++sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }
  PR_VMBN_CASE(OPC_Builtin_GetCvarStr)
    {
      VCvar *vp = GetAndCacheCVar(ip);
      sp[0].p = nullptr;
      *(VStr *)&sp[0].p = vp->asStr();
      ++sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }
  PR_VMBN_CASE(OPC_Builtin_GetCvarBool)
    {
      VCvar *vp = GetAndCacheCVar(ip);
      sp[0].i = (vp->asBool() ? 1 : 0);
      ++sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }

  PR_VMBN_CASE(OPC_Builtin_SetCvarInt)
    {
      VCvar *vp = GetAndCacheCVar(ip);
      if (!vp->IsReadOnly()) vp->SetInt(sp[-1].i);
      --sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }
  PR_VMBN_CASE(OPC_Builtin_SetCvarFloat)
    {
      VCvar *vp = GetAndCacheCVar(ip);
      if (!vp->IsReadOnly()) vp->SetFloat(sp[-1].f);
      --sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }
  PR_VMBN_CASE(OPC_Builtin_SetCvarStr)
    {
      VCvar *vp = GetAndCacheCVar(ip);
      if (!vp->IsReadOnly()) vp->SetStr(*((VStr *)&sp[-1].p));
      ((VStr *)&sp[-1].p)->clear();
      --sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }
  PR_VMBN_CASE(OPC_Builtin_SetCvarBool)
    {
      VCvar *vp = GetAndCacheCVar(ip);
      if (!vp->IsReadOnly()) vp->SetBool(!!sp[-1].i);
      --sp;
      ip += 4+sizeof(void *);
      PR_VMBN_BREAK;
    }
    return;
  /*
  PR_VMBN_DEFAULT:
    cstDump(ip);
    VPackage::InternalFatalError(va("Unknown builtin %u", ReadU8(ip+1)));
  */
}

#if USE_COMPUTED_GOTO
#else
vm_labels_builtins_done:
ip += 2;
#endif

#undef PR_VMBN_SWITCH
#undef PR_VMBN_CASE
#undef PR_VMBN_BREAK

#endif
