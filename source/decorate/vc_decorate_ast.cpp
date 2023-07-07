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
// this directly included from "vc_decorate.cpp"


//==========================================================================
//
//  VDecorateInvocation
//
//==========================================================================
class VDecorateInvocation : public VExpression {
public:
  VName Name;
  VMethod *Func; // if this is not null, use it instead of searching by `Name` (yet `Name` still be valid!)
  int NumArgs;
  VState *CallerState; // used for `Func`
  VExpression *Args[VMethod::MAX_PARAMS+1];

  VDecorateInvocation (VName AName, const TLocation &ALoc, int ANumArgs, VExpression *AArgs[]);
  VDecorateInvocation (VMethod *AFunc, VName AName, const TLocation &ALoc, int ANumArgs, VExpression *AArgs[]);
  virtual ~VDecorateInvocation () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &) override;
  virtual void Emit (VEmitContext &) override;

  virtual VStr toString () const override;

  void FixKnownShit (VEmitContext &ec, const char *FuncName);

protected:
  VDecorateInvocation () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;

  bool HasFloatArg (VEmitContext &ec);
};


//==========================================================================
//
//  VDecorateUserVar
//
//  this generates call to _get/_set uservar methods
//
//==========================================================================
class VDecorateUserVar : public VExpression {
public:
  VStr fldname;
  VExpression *index; // array index, if not nullptr

  VDecorateUserVar (VStr afldname, const TLocation &aloc);
  VDecorateUserVar (VStr afldname, VExpression *aindex, const TLocation &aloc);
  virtual ~VDecorateUserVar () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  //virtual VExpression *ResolveAssignmentTarget (VEmitContext &ec) override;
  virtual VExpression *ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) override;
  virtual void Emit (VEmitContext &ec) override;
  virtual bool IsDecorateUserVar () const override;
  virtual VStr toString () const override;

protected:
  VDecorateUserVar () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};


//==========================================================================
//
//  VDecorateAJump
//
//  as `A_Jump()` can have insane amounts of arguments, we'll generate
//  VM code directly instead
//
//==========================================================================
class VDecorateAJump : public VExpression {
private:
  //VExpression *xstc; // bool(`XLevel.StateCall`) access expression
  //VExpression *xass; // XLevel.StateCall->Result = false
  VExpression *crnd0; // first call to P_Random()
  VExpression *crnd1; // second call to P_Random()

public:
  VExpression *prob; // probability
  TArray<VExpression *> labels; // jump labels
  VState *CallerState;

public:
  VDecorateAJump (const TLocation &aloc);
  virtual ~VDecorateAJump () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual void Emit (VEmitContext &ec) override;
  virtual VStr toString () const override;

protected:
  VDecorateAJump () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};



//==========================================================================
//
//  VDecorateRndPick
//
//  as `[f]randompick()` can have insane amounts of arguments, we'll
//  generate VM code directly instead
//
//==========================================================================
class VDecorateRndPick : public VExpression {
private:
  VExpression *crnd0; // first call to P_Random()

public:
  TArray<VExpression *> numbers; // jump labels
  bool asFloat;

public:
  VDecorateRndPick (bool aAsFloat, const TLocation &aloc);
  virtual ~VDecorateRndPick () override;
  virtual VExpression *SyntaxCopy () override;
  virtual VExpression *DoResolve (VEmitContext &ec) override;
  virtual void Emit (VEmitContext &ec) override;
  virtual VStr toString () const override;

protected:
  VDecorateRndPick () {}
  virtual void DoSyntaxCopyTo (VExpression *e) override;
};



//==========================================================================
//
//  tryStringAsFloat
//
//==========================================================================
static bool tryStringAsFloat (float &resf, const char *str) {
  resf = 0.0f;
  if (!str || !str[0]) return false;
  if (VStr::convertFloat(str, &resf)) {
    if (isFiniteF(resf)) {
      return true;
    }
  }
  resf = 0.0f;
  return false;
}


//==========================================================================
//
//  tryStringAsInt
//
//==========================================================================
static bool tryStringAsInt (int &resi, const char *str, const VFieldType &destType) {
  // try int
  resi = 0;
  if (!str || !str[0]) return false;
  // loose conversion
  if (VStr::convertInt(str, &resi, true)) {
    switch (destType.Type) {
      case TYPE_Byte: resi = clampToByte(resi); break;
      case TYPE_Bool: resi = (resi ? 1 : 0); break;
    }
    return true;
  }
  resi = 0;
  return false;
}


//==========================================================================
//
//  tryStringAsFloat2Int
//
//==========================================================================
static bool tryStringAsFloat2Int (int &resi, const char *str, const VFieldType &destType) {
  resi = 0;
  if (!str || !str[0]) return false;
  // try float (sigh)
  float resf = 0.0f;
  if (VStr::convertFloat(str, &resf)) {
    if (isFiniteF(resf)) {
      // arbitrary limits
           if (resf < -0x3fffffff) resi = -0x3fffffff;
      else if (resf > +0x3fffffff) resi = +0x3fffffff;
      else resi = (int)resf;
      switch (destType.Type) {
        case TYPE_Byte: resi = clampToByte(resi); break;
        case TYPE_Bool: resi = (resi ? 1 : 0); break;
      }
      return true;
    }
  }
  return false;
}


//==========================================================================
//
//  VInvocation::MassageDecorateArg
//
//  this will try to coerce some decorate argument to something sensible
//
//  `argnum` starts with `1`
//
//==========================================================================
VExpression *VExpression::MassageDecorateArg (VEmitContext &ec, VInvocation *invokation, VState *CallerState, const char *funcName,
                                              int argnum, const VFieldType &destType, bool isOptional,
                                              const TLocation *aloc, bool *massaged)
{
  //FIXME: move this to separate method
  // simplify a little:
  //   replace `+number` with `number`
  if (IsUnaryMath()) {
    VUnary *un = (VUnary *)this;
    if (un->op) {
      if (un->Oper == VUnary::Plus && (un->op->IsIntConst() || un->op->IsFloatConst())) {
        VExpression *enew = un->op;
        //fprintf(stderr, "SIMPLIFIED! <%s> -> <%s>\n", *un->toString(), *etmp->toString());
        un->op = nullptr;
        delete this;
        return enew->MassageDecorateArg(ec, invokation, CallerState, funcName, argnum, destType, isOptional, aloc, massaged);
      }
    }
  }

  // flags
  // note that `invokation` may be `nullptr` if we called from `VDecorateAJump()`, for example
  if (invokation) {
    for (auto &&FlagName : invokation->Func->Params[argnum-1].NamedFlags) {
      // hack for idiotic mod authors (hello, LCA!)
      // [soundchannel]
      if (FlagName == "soundchannel") {
        if (destType.Type == TYPE_Int) {
          if (IsStrConst() || IsNameConst() || IsDecorateSingleName()) {
            VStr s = (IsDecorateSingleName() ? *((VDecorateSingleName *)this)->Name : IsStrConst() ? GetStrConst(ec.Package) : VStr(GetNameConst()));
            int v = 0;
            if (!VStr::convertInt(*s, &v)) {
              // convert digits
              const char *str = *s;
              v = 0;
              while (*str && digitInBase(*str, 10) < 0) ++str;
              if (*str) {
                while (*str) {
                  int d = digitInBase(*str, 10);
                  if (d < 0) break;
                  v = v*10+d;
                  ++str;
                }
              } else {
                VStr t = s;
                while (t.length()) {
                  if (t[0] == '_' || (vuint8)(t[0]) <= ' ') { t.chopLeft(1); continue; }
                  if (t.startsWithNoCase("CHANNEL")) { t.chopLeft(7); continue; }
                  if (t.startsWithNoCase("CHAN")) { t.chopLeft(4); continue; }
                  if (t.startsWithNoCase("SoundSlot")) { t.chopLeft(9); continue; }
                  if (t.startsWithNoCase("Slot")) { t.chopLeft(4); continue; }
                  if (t.startsWithNoCase("Sound")) { t.chopLeft(5); continue; }
                  break;
                }
                     if (t.ICmp("Auto") == 0) v = 0;
                else if (t.ICmp("Voice") == 0) v = 1;
                else if (t.ICmp("Weapon") == 0) v = 2;
                else if (t.ICmp("Item") == 0) v = 3;
                else if (t.ICmp("Body") == 0) v = 4;
                else v = 0;
              }
            }
            if (v < 0 || v > 127) v = 0;
            //GLog.Logf(NAME_Debug, "*** FUNC:`%s`; arg #%d (%s): soundchannel! '%s' -> %d", *Func->GetFullName(), i+1, *Func->Params[i].Name, *s, v);
            VExpression *enew = new VIntLiteral(v, Loc);
            delete this;
            return enew->MassageDecorateArg(ec, invokation, CallerState, funcName, argnum, destType, isOptional, aloc, massaged);
          }
        }
        continue;
      }
      // [color] or [color0]
      if (FlagName == "color0" || FlagName == "color") {
        if (destType.Type == TYPE_String) {
          // integer zero? pass empty string
          if (IsIntConst()) {
            if (GetIntConst() == 0 && FlagName == "color0") {
              VExpression *enew = new VStringLiteral(VStr(), ec.Package->FindString(""), Loc);
              delete this;
              return enew->MassageDecorateArg(ec, invokation, CallerState, funcName, argnum, destType, isOptional, aloc, massaged);
            } else {
              // hex colors
              VStr s = va("#%06x", GetIntConst()&0xffffff);
              VExpression *enew = new VStringLiteral(s, ec.Package->FindString(*s), Loc);
              delete this;
              return enew->MassageDecorateArg(ec, invokation, CallerState, funcName, argnum, destType, isOptional, aloc, massaged);
            }
          }
        }
        continue;
      }
      // unknown flag
      ParseWarningAsError(Loc, "function `%s`, argument #%d has unknown argument flag `%s`!", funcName, argnum, *FlagName);
    }
  }

  if (massaged) *massaged = true;
  switch (destType.Type) {
    case TYPE_Int:
    case TYPE_Byte:
    case TYPE_Float:
    case TYPE_Bool:
      if (IsStrConst()) {
        VStr str = GetStrConst(ec.Package);
        if (str.isEmpty() || str.strEquCI("none") || str.strEquCI("null") || str.strEquCI("nil") || str.strEquCI("false")) {
          ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be a number (replaced with 0); PLEASE, FIX THE CODE!", funcName, argnum);
          VExpression *enew = new VIntLiteral(0, Loc);
          delete this;
          return enew;
        }
        if (str.strEquCI("true")) {
          ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be a number (replaced with 1); PLEASE, FIX THE CODE!", funcName, argnum);
          VExpression *enew = new VIntLiteral(1, Loc);
          delete this;
          return enew;
        }
        // doomrl arsenal author is a... well, you know it
        if (argnum == 3 && VStr::strEqu(funcName, "decorate_A_CheckFlag") && str.strEquCI("Nope")) {
          ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be an aptr (replaced with AAPTR_DEFAULT, mod author is a mo...dder)!", funcName, argnum);
          VExpression *enew = new VIntLiteral(0, Loc);
          delete this;
          return enew;
        }
        // `A_SpawnParticle()` color
        if (argnum == 1 && VStr::strEqu(funcName, "A_SpawnParticle")) {
          vuint32 clr = M_ParseColor(*str, /*retZeroIfInvalid*/true);
          if (clr == 0) {
            ParseWarning((aloc ? *aloc : Loc), "color argument \"%s\" to `%s` is not a valid color (replaced with black)!", *str, funcName);
          }
          VExpression *enew = new VIntLiteral((vint32)clr, Loc);
          delete this;
          return enew;
        }
        // ok, try to convert the string to a number... please, Invisible Pink Unicorn, why did you created so many morons?!
        if (destType.Type == TYPE_Float) {
          float resf = 0.0f;
          if (tryStringAsFloat(resf, *str)) {
            ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be `float`, not a string (coerced, mod author is a mo...dder)!", funcName, argnum);
            VExpression *enew = new VFloatLiteral(resf, Loc);
            delete this;
            return enew;
          }
        } else {
          // try int
          int resi = 0;
          if (tryStringAsInt(resi, *str, destType)) {
            ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be `%s`, not a string (coerced, mod author is a mo...dder)!", funcName, argnum, *destType.GetName());
            VExpression *enew = new VIntLiteral(resi, Loc);
            delete this;
            return enew;
          }
          if (tryStringAsFloat2Int(resi, *str, destType)) {
            ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be `%s`, not a float-as-string (coerced, mod author is a mo...dder)!", funcName, argnum, *destType.GetName());
            VExpression *enew = new VIntLiteral(resi, Loc);
            delete this;
            return enew;
          }
        }
      }
      // `A_SpawnParticle()` color
      if (argnum == 1 && IsDecorateSingleName() && VStr::strEqu(funcName, "A_SpawnParticle")) {
        VDecorateSingleName *e = (VDecorateSingleName *)this;
        vuint32 clr = M_ParseColor(*e->Name, /*retZeroIfInvalid*/true);
        if (clr == 0) {
          ParseWarning((aloc ? *aloc : Loc), "color argument \"%s\" to `%s` is not a valid color (replaced with black)!", *e->Name, funcName);
        }
        VExpression *enew = new VIntLiteral((vint32)clr, Loc);
        delete this;
        return enew;
      }
      // none as literal?
      if (IsNoneLiteral()) {
        ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be a number (replaced with 0); PLEASE, FIX THE CODE!", funcName, argnum);
        VExpression *enew = new VIntLiteral(0, Loc);
        delete this;
        return enew;
      }

      if (IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)this;
        VStr str = VStr(e->Name);
        // ok, try to convert the string to a number... please, Invisible Pink Unicorn, why did you created so many morons?!
        if (destType.Type == TYPE_Float) {
          float resf = 0.0f;
          if (tryStringAsFloat(resf, *str)) {
            ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be `float`, not string (coerced, mod author is a mo...dder)!", funcName, argnum);
            VExpression *enew = new VFloatLiteral(resf, Loc);
            delete this;
            return enew;
          }
        } else {
          // try int
          int resi = 0;
          if (tryStringAsInt(resi, *str, destType)) {
            ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be `%s`, not string (coerced, mod author is a mo...dder)!", funcName, argnum, *destType.GetName());
            VExpression *enew = new VIntLiteral(resi, Loc);
            delete this;
            return enew;
          }
          if (tryStringAsFloat2Int(resi, *str, destType)) {
            ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be `%s`, not float-as-string (coerced, mod author is a mo...dder)!", funcName, argnum, *destType.GetName());
            VExpression *enew = new VIntLiteral(resi, Loc);
            delete this;
            return enew;
          }
        }
      }

      // convert string literal to identifier
      if (IsStrConst()) {
        VStr str = GetStrConst(ec.Package);
        if (!str.isEmpty()) {
          ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be `%s`! (trying constant `%s`)", funcName, argnum, *destType.GetName(), *str);
          VDecorateSingleName *enew = new VDecorateSingleName(str, Loc);
          delete this;
          return enew;
        }
      }

      break;

    case TYPE_Name:
      // identifier?
      if (IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)this;
        VExpression *enew = new VNameLiteral(*e->Name, Loc);
        delete this;
        return enew;
      }
      // string?
      if (IsStrConst()) {
        VStr val = GetStrConst(ec.Package);
        VExpression *enew = new VNameLiteral(*val, Loc);
        delete this;
        return enew;
      }
      // integer zero?
      if (IsIntConst() && GetIntConst() == 0) {
        // "false" or "0" means "empty"
        ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be a string (replaced `0` with empty string); PLEASE, FIX THE CODE!", funcName, argnum);
        VExpression *enew = new VNameLiteral(NAME_None, Loc);
        delete this;
        return enew;
      }
      // idiotic call (forgotten "damagetype")
      if (invokation && argnum == invokation->NumArgs && argnum < 16 && IsIntConst() && (GetIntConst() == 1 || GetIntConst() == 0) &&
          (VStr::strEqu(funcName, "A_CustomMeleeAttack") || VStr::strEqu(funcName, "A_CustomComboAttack")))
      {
        ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be a DamageType (inserted default value); MOD AUTHOR IS A MO...DDER!", funcName, argnum);
        VExpression *enew = new VNameLiteral(NAME_None, Loc);
        // append "bleed"
        invokation->Args[invokation->NumArgs] = new VIntLiteral(GetIntConst(), Loc);
        ++invokation->NumArgs;
        delete this;
        return enew;
      }
      // none as literal?
      if (IsNoneLiteral()) {
        VExpression *enew = new VNameLiteral("none", Loc);
        delete this;
        return enew;
      }
      break;

    case TYPE_String:
      // identifier?
      if (IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)this;
        VExpression *enew = new VStringLiteral(VStr(e->Name), ec.Package->FindString(*e->Name), Loc);
        delete this;
        return enew;
      }
      // integer zero?
      if (IsIntConst() && GetIntConst() == 0) {
        // "false" or "0" means "empty"
        // don't warn for `A_CustomRailgun()`, number `0` seems to be a valid value there
        /*
        if ((argnum == 3 || argnum == 4) && VStr::strEquCI(funcName, "A_CustomRailgun")) {
          // do nothing
        } else
        */
        {
          ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be a string (replaced `0` with empty string); PLEASE, FIX THE CODE!", funcName, argnum);
        }
        VExpression *enew = new VStringLiteral(VStr(), ec.Package->FindString(""), Loc);
        delete this;
        return enew;
      }
      // none as literal?
      if (IsNoneLiteral()) {
        VExpression *enew = new VStringLiteral("none", ec.Package->FindString(""), Loc);
        delete this;
        return enew;
      }
      // `A_CustomRailgun()` hex colors
      /*
      if (IsIntConst() && (argnum == 3 || argnum == 4) && VStr::strEquCI(funcName, "A_CustomRailgun")) {
        VStr s = va("#%06x", GetIntConst()&0xffffff);
        VExpression *enew = new VStringLiteral(s, ec.Package->FindString(*s), Loc);
        delete this;
        return enew;
      }
      */
      break;

    case TYPE_Class:
      // identifier?
      if (IsDecorateSingleName()) {
        VDecorateSingleName *e = (VDecorateSingleName *)this;
        VExpression *enew = new VStringLiteral(VStr(e->Name), ec.Package->FindString(*e->Name), Loc);
        delete this;
        return enew->MassageDecorateArg(ec, invokation, CallerState, funcName, argnum, destType, isOptional, aloc, massaged);
      }
      // string?
      if (IsStrConst()) {
        VStr CName = GetStrConst(ec.Package);
        VStr CNameStp = CName.xstrip();
        //TLocation ALoc = Loc;
        if (CNameStp.isEmpty() || CNameStp.strEquCI("None") || CNameStp.strEquCI("nil") || CNameStp.strEquCI("null")) {
          //ParseWarning(ALoc, "NONE CLASS `%s`", CName);
          VExpression *enew = new VNoneLiteral(Loc);
          delete this;
          return enew;
        } else {
          VClass *Cls = VClass::FindClassNoCase(*CName);
          if (!Cls && CNameStp.length() && CNameStp.length() != CName.length()) Cls = VClass::FindClassNoCase(*CNameStp);
          if (!Cls) {
            if (!destType.Class) {
              ParseWarningAsError((aloc ? *aloc : Loc), "No such class `%s` for argument #%d of `%s`", *CName, argnum, funcName);
              VExpression *enew = new VNoneLiteral(Loc);
              delete this;
              return enew;
            }
            if (cli_ShowClassRTRouting > 0) ParseWarning((aloc ? *aloc : Loc), "No such class `%s` for argument #%d of `%s` (rt-routed)", *CName, argnum, funcName);
            //k8: hack it with runtime class searching
            /*
            VExpression *enew = new VNoneLiteral(Loc);
            delete this;
            return enew;
            */
            //GLog.Logf("********* %s (%s)", *destType.GetName(), (destType.Class ? destType.Class->GetName() : "<none>"));
            VExpression *TmpArgs[2];
            TmpArgs[0] = new VClassConstant(destType.Class, Loc);
            TmpArgs[1] = new VNameLiteral(VName(*CName), Loc);
            VExpression *enew = new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindClassNoCaseEx"), nullptr, false, false, Loc, 2, TmpArgs);
            delete this;
            return enew;
          }
          if (destType.Class && !Cls->IsChildOf(destType.Class)) {
            // exception: allow `RandomSpawner` for inventories
            bool stillError = true;
            if (argnum == 1 && VStr::strEqu("A_GiveInventory", funcName)) {
              static VClass *invCls = nullptr;
              if (invCls == nullptr) { invCls = VClass::FindClass("Inventory"); vassert(invCls); }
              static VClass *rndCls = nullptr;
              if (rndCls == nullptr) { rndCls = VClass::FindClass("RandomSpawner"); vassert(rndCls); }
              if (destType.Class->IsChildOf(invCls) && Cls->IsChildOf(rndCls)) {
                stillError = false;
                ParseWarningAsError((aloc ? *aloc : Loc), "using class `%s` (RandomSpawner) for argument #%d of `%s`", Cls->GetName(), argnum, funcName);
              }
            }
            if (stillError) {
              ParseWarningAsError((aloc ? *aloc : Loc), "Class `%s` is not a descendant of `%s` for argument #%d of `%s`", *CName, destType.Class->GetName(), argnum, funcName);
              //for (VClass *cc = Cls; cc; cc = cc->GetSuperClass()) GLog.Logf(NAME_Debug, "  %s", cc->GetName());
              VExpression *enew = new VNoneLiteral(Loc);
              delete this;
              return enew;
            }
          }
          VExpression *enew = new VClassConstant(Cls, Loc);
          delete this;
          return enew;
        }
        break;
      }
      // integer zero?
      if (IsIntConst() && GetIntConst() == 0) {
        // "false" or "0" means "empty"
        ParseWarningAsError((aloc ? *aloc : Loc), "`%s` argument #%d should be class (replaced with `none`); PLEASE, FIX THE CODE!", funcName, argnum);
        VExpression *enew = new VNoneLiteral(Loc);
        delete this;
        return enew;
      }
      break;

    case TYPE_State:
      // some very bright person does this: `A_JumpIfTargetInLOS("1")` -- brilliant!
      // string?
      if (IsStrConst() || IsDecorateSingleName()) {
        const TLocation ALoc = Loc;
        VStr lblName;
        if (IsStrConst()) lblName = GetStrConst(ec.Package); else { VDecorateSingleName *e = (VDecorateSingleName *)this; lblName = VStr(e->Name); }
        lblName = lblName.xstrip();
        // any special names will be resolved by rt-router
        // try to convert a string to a number
        int lbl = -1;
        // string-as-a-number?
        if (lblName.convertInt(&lbl)) {
          // try to find the corresponding label
          bool useNumber = true;
          if (ec.SelfClass) {
            VStateLabel *StLbl = ec.SelfClass->FindStateLabel(VName(*lblName), NAME_None, true);
            if (StLbl) {
              ParseWarningAsError((aloc ? *aloc : ALoc), "do not use numeric labels in `%s` (argument #%d, label '%s')", funcName, argnum, *lblName);
              useNumber = false;
            }
          }
          if (useNumber) {
            if (lbl < 0) {
              ParseError((aloc ? *aloc : ALoc), "Negative state jumps are not allowed (function `%s`, argument #%d)", funcName, argnum);
              delete this;
              return nullptr;
            }
            ParseWarningAsError((aloc ? *aloc : ALoc), "`%s` argument #%d should be number %d instead of string \"%s\"; PLEASE, FIX THE CODE!", funcName, argnum, lbl, *lblName.quote());
            VExpression *TmpArgs[1];
            TmpArgs[0] = new VIntLiteral(lbl, ALoc);
            delete this;
            return new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpStateOfs"), nullptr, false, false, ALoc, 1, TmpArgs);
          }
        }
        //k8: don't resolve any "::" here, our dynamic resolver will do this for us
        // it's a virtual state jump
        //ParseWarning(Args[i]->Loc, "***VSJMP `%s`: <%s>", Func->GetName(), *lblName);
        VExpression *TmpArgs[2];
        //FIXME: support user arrays here!
        if (lblName.startsWithCI("user_")) {
          //ParseWarningAsError(ALoc, "uservars for state jumps are not supported (arg #%d for action `%s`). please, don't do this, this is a misfeature!", argnum, funcName);
          // uservar; route with `FindJumpStateOfs()`
          //GCon->Logf(NAME_Debug, "DECORATE: %s: routing uservar (%s) state with `FindJumpStateOfs()`", *ALoc.toStringNoCol(), *lblName);
          delete this;
          TmpArgs[0] = new VDecorateUserVar(lblName, ALoc);
          return new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpStateOfs"), nullptr, false, false, ALoc, 1, TmpArgs);
        } else {
          // final state FindJumpState (name Label, int offset);
          //TmpArgs[0] = this;
          delete this;
          TmpArgs[0] = new VNameLiteral(VName(*lblName), ALoc);
          TmpArgs[1] = new VIntLiteral(0, ALoc);
          return new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpState"), nullptr, false, false, ALoc, 2, TmpArgs);
        }
      }
      // integer?
      if (IsIntConst()) {
        const TLocation ALoc = Loc;
        int Offs = GetIntConst();
        if (Offs < 0) {
          ParseError((aloc ? *aloc : ALoc), "Negative state jumps are not allowed (function `%s`, argument #%d)", funcName, argnum);
          delete this;
          return nullptr;
        }
        VExpression *TmpArgs[1];
        TmpArgs[0] = this;
        return new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpStateOfs"), nullptr, false, false, ALoc, 1, TmpArgs);
      }
      // none as literal? this usually means "no jump"
      // but let rt-router decide
      if (IsNoneLiteral()) {
        const TLocation ALoc = Loc;
        VExpression *TmpArgs[2];
        /*
        TmpArgs[0] = new VIntLiteral(0, ALoc);
        delete this;
        return new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpStateOfs"), nullptr, false, false, ALoc, 1, TmpArgs);
        */
        // final state FindJumpState (name Label, int offset);
        delete this;
        TmpArgs[0] = new VNameLiteral(VName("none"), ALoc);
        TmpArgs[1] = new VIntLiteral(0, ALoc);
        return new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpState"), nullptr, false, false, ALoc, 2, TmpArgs);
      }
      // support things like `A_Jump(n, func())`
      if (Type.Type != TYPE_State) {
        const TLocation ALoc = Loc;
        //GCon->Logf("A_Jump: type=%s; expr=<%s>", *lbl->Type.GetName(), *lbl->toString());
        VGagErrors gag;
        VExpression *lx = this->SyntaxCopy()->Resolve(ec);
        if (lx) {
          if (lx->Type.Type != TYPE_State) {
            const bool isGoodType = (lx->Type.Type == TYPE_Int || lx->Type.Type == TYPE_Byte || lx->Type.Type == TYPE_Bool || lx->Type.Type == TYPE_Float);
            if (isGoodType) {
              //GCon->Logf("A_Jump: type=%s; expr=<%s>", *lbl->Type.GetName(), *lbl->toString());
              VExpression *TmpArgs[1];
              TmpArgs[0] = this->SyntaxCopy();
              if (lx->Type.Type == TYPE_Float) {
                ParseWarningAsError(ALoc, "jump offset argument #%d for `%s` should be integer, not float! PLEASE, FIX THE CODE!", argnum, funcName);
                TmpArgs[0] = new VScalarToInt(TmpArgs[0]); // not resolved
              }
              VExpression *eres = new VInvocation(nullptr, ec.SelfClass->FindMethodChecked("FindJumpStateOfs"), nullptr, false, false, ALoc, 1, TmpArgs);
              //GCon->Logf("   NEW: type=%s; expr=<%s>", *lbl->Type.GetName(), *lbl->toString());
              delete this;
              return eres;
            }
          }
          delete lx;
        }
      }
      break;
  }
  if (massaged) *massaged = false;
  return this;
}


//==========================================================================
//
//  VDecorateInvocation::VDecorateInvocation
//
//==========================================================================
VDecorateInvocation::VDecorateInvocation (VName AName, const TLocation &ALoc, int ANumArgs, VExpression **AArgs)
  : VExpression(ALoc)
  , Name(AName)
  , Func(nullptr)
  , NumArgs(ANumArgs)
  , CallerState(nullptr)
{
  memset(Args, 0, sizeof(Args));
  for (int i = 0; i < NumArgs; ++i) Args[i] = AArgs[i];
}


//==========================================================================
//
//  VDecorateInvocation::VDecorateInvocation
//
//==========================================================================
VDecorateInvocation::VDecorateInvocation (VMethod *AFunc, VName AName, const TLocation &ALoc, int ANumArgs, VExpression *AArgs[])
  : VExpression(ALoc)
  , Name(AName)
  , Func(AFunc)
  , NumArgs(ANumArgs)
  , CallerState(nullptr)
{
  memset(Args, 0, sizeof(Args));
  for (int i = 0; i < NumArgs; ++i) Args[i] = AArgs[i];
}


//==========================================================================
//
//  VDecorateInvocation::~VDecorateInvocation
//
//==========================================================================
VDecorateInvocation::~VDecorateInvocation () {
  for (int i = 0; i < NumArgs; ++i) {
    if (Args[i]) {
      delete Args[i];
      Args[i] = nullptr;
    }
  }
}


//==========================================================================
//
//  VDecorateInvocation::toString
//
//==========================================================================
VStr VDecorateInvocation::toString () const {
  VStr res = *Name;
  res += "(";
  for (int f = 0; f < NumArgs; ++f) {
    if (f != 0) res += ", ";
    if (Args[f]) res += Args[f]->toString(); else res += "default";
  }
  res += ")";
  return res;
}


//==========================================================================
//
//  VDecorateInvocation::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateInvocation::SyntaxCopy () {
  auto res = new VDecorateInvocation();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateInvocation::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateInvocation::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateInvocation *)e;
  memset(res->Args, 0, sizeof(res->Args));
  res->Name = Name;
  res->NumArgs = NumArgs;
  for (int f = 0; f < NumArgs; ++f) res->Args[f] = (Args[f] ? Args[f]->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDecorateInvocation::HasFloatArg
//
//==========================================================================
bool VDecorateInvocation::HasFloatArg (VEmitContext &ec) {
  for (int i = 0; i < NumArgs; ++i) {
    if (!Args[i] || Args[i]->IsDefaultArg()) continue;
    VGagErrors gag;
    VExpression *e = Args[i]->SyntaxCopy()->Resolve(ec);
    const bool isFloat = (e && e->Type.Type == TYPE_Float);
    delete e;
    if (isFloat) return true;
  }
  return false;
}


//==========================================================================
//
//  VDecorateInvocation::DoResolve
//
//==========================================================================
void VDecorateInvocation::FixKnownShit (VEmitContext &ec, const char *FuncName) {
  // alot of dumbfucks cannot into wiki
  if (NumArgs == 4 && Args[3] && VStr::strEquCI(FuncName, "A_CustomMeleeAttack")) {
    VExpression *e;
    {
      VGagErrors gag;
      e = Args[3]->SyntaxCopy()->Resolve(ec);
    }
    bool doit = (e && e->Type.CheckMatch(false/*asRef*/, e->Loc, VFieldType(TYPE_Bool), false/*raiseError*/));
    delete e;
    if (doit) {
      // missing argument; morons!
      ParseWarningAsError(Loc, "`%s` is missing argument `missound`; who cares about stupid wikis?!", FuncName);
      // insert dummy empty string argument
      for (int f = 3; f >= 2; --f) Args[f+1] = Args[f];
      VStr ns = VStr("");
      int val = ec.Package->FindString(*ns);
      Args[2] = new VStringLiteral(ns, val, Args[2]->Loc);
      ++NumArgs;
      //GCon->Logf(NAME_Debug, "*** %s", *this->toString());
    }
    return;
  }
}


//==========================================================================
//
//  VDecorateInvocation::DoResolve
//
//==========================================================================
VExpression *VDecorateInvocation::DoResolve (VEmitContext &ec) {
  if (ec.SelfClass) {
    //FIXME: sanitize this!
    VMethod *M = Func;
    if (!M) {
      if (VStr::strEquCI(*Name, "va") ||
          VStr::strEquCI(*Name, "fmin") || VStr::strEquCI(*Name, "fmax") ||
          VStr::strEquCI(*Name, "fclamp") || VStr::strEquCI(*Name, "fabs"))
      {
        Name = Name.GetLower();
        M = ec.SelfClass->FindMethod(Name);
      } else if (VStr::strEquCI(*Name, "min") || VStr::strEquCI(*Name, "max") ||
                 VStr::strEquCI(*Name, "clamp") || VStr::strEquCI(*Name, "abs"))
      {
        // determine if we want an integer one
        if (HasFloatArg(ec)) {
          VStr fname = VStr("f")+(*Name);
          Name = VName(*fname, VName::AddLower);
        } else {
          Name = Name.GetLower();
        }
        M = ec.SelfClass->FindMethod(Name);
      } else {
        M = ec.SelfClass->FindDecorateStateAction(*Name);
      }
    }

    if (M) {
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements (method '%s', class '%s')", *Name, *ec.SelfClass->GetFullName());
        delete this;
        return nullptr;
      }

      FixKnownShit(ec, *Name);

      // rebuild min/max with more than two arguments into recursive expression (for now)
      //FIXME: generate better VM code for this
      if (NumArgs > 2 && (VStr::strEquCI(*Name, "fmin") || VStr::strEquCI(*Name, "fmax") ||
                          VStr::strEquCI(*Name, "min") || VStr::strEquCI(*Name, "max")))
      {
        VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 2, Args+NumArgs-2);
        if (Func && CallerState) ((VInvocation *)e)->CallerState = CallerState;
        int currlhs = NumArgs-3;
        for (; currlhs >= 0; --currlhs) {
          VExpression *newargs[2];
          newargs[0] = Args[currlhs];
          newargs[1] = e;
          e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 2, newargs);
          if (Func && CallerState) ((VInvocation *)e)->CallerState = CallerState;
        }
        //GCon->Logf(NAME_Debug, "::: %s", *e->toString()); abort();
        NumArgs = 0;
        delete this;
        return e->Resolve(ec);
      }

      VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, NumArgs, Args);
      if (Func && CallerState) ((VInvocation *)e)->CallerState = CallerState;
      NumArgs = 0;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (ec.SelfClass) {
    if (VStr::strEquCI(*Name, "A_WeaponOffset") || VStr::strEquCI(*Name, "A_Overlay")) {
      ParseWarning(Loc, "Unknown decorate action `%s` in class `%s`", *Name, *ec.SelfClass->GetFullName());
      VExpression *e = new VIntLiteral(0, Loc);
      delete this;
      return e->Resolve(ec);
    }
    ParseError(Loc, "Unknown decorate action `%s` in class `%s`", *Name, *ec.SelfClass->GetFullName());
  } else {
    ParseError(Loc, "Unknown decorate action `%s`", *Name);
  }

  delete this;
  return nullptr;
}



//==========================================================================
//
//  VDecorateInvocation::Emit
//
//==========================================================================
void VDecorateInvocation::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen");
}


//==========================================================================
//
//  VDecorateUserVar::VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::VDecorateUserVar (VStr afldname, const TLocation &aloc)
  : VExpression(aloc)
  , fldname(afldname)
  , index(nullptr)
{
}


//==========================================================================
//
//  VDecorateUserVar::VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::VDecorateUserVar (VStr afldname, VExpression *aindex, const TLocation &aloc)
  : VExpression(aloc)
  , fldname(afldname)
  , index(aindex)
{
}


//==========================================================================
//
//  VDecorateUserVar::~VDecorateUserVar
//
//==========================================================================
VDecorateUserVar::~VDecorateUserVar () {
  delete index; index = nullptr;
  //fldname.clear(); // just4fun
}


//==========================================================================
//
//  VDecorateUserVar::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateUserVar::SyntaxCopy () {
  auto res = new VDecorateUserVar();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateUserVar::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateUserVar::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateUserVar *)e;
  res->fldname = fldname;
  res->index = (index ? index->SyntaxCopy() : nullptr);
}


//==========================================================================
//
//  VDecorateUserVar::DoResolve
//
//==========================================================================
VExpression *VDecorateUserVar::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass) Sys_Error("VDecorateUserVar::DoResolve: internal compiler error");
  //GLog.Logf(NAME_Debug, "%s: user field '%s'", *Loc.toString(), *fldname);
  VName fldnamelo = (fldname.isEmpty() ? NAME_None : VName(*fldname, VName::AddLower));
  VName fldn = ec.SelfClass->FindDecorateStateFieldTrans(fldnamelo);
  if (fldn == NAME_None) {
    ParseError(Loc, "field `%s` is not found in class `%s`", *fldname, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  // use checked field access
  VField *fld = ec.SelfClass->FindField(fldn);
  if (!fld) {
    ParseError(Loc, "field `%s` => `%s` is not found in class `%s`", *fldname, *fldn, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  VFieldType ftype = fld->Type;
  if (ftype.Type == TYPE_Array) ftype = ftype.GetArrayInnerType();
  VName mtname = (ftype.Type == TYPE_Int ? "_get_user_var_int" : "_get_user_var_float");
  VMethod *mt = ec.SelfClass->FindMethod(mtname);
  if (!mt) {
    ParseError(Loc, "internal method `%s` not found in class `%s`", *mtname, *ec.SelfClass->GetFullName());
    delete this;
    return nullptr;
  }
  VExpression *args[2];
  args[0] = new VNameLiteral(fldn, Loc);
  args[1] = index;
  VExpression *e = new VInvocation(nullptr, mt, nullptr, false, false, Loc, 2, args);
  index = nullptr;
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDecorateUserVar::ResolveCompleteAssign
//
//  this will be called before actual assign resolving
//  return `nullptr` to indicate error, or consume `val` and set `resolved`
//  to `true` if resolved
//  if `nullptr` is returned, both `this` and `val` should be destroyed
//
//==========================================================================
VExpression *VDecorateUserVar::ResolveCompleteAssign (VEmitContext &ec, VExpression *val, bool &resolved) {
  if (!ec.SelfClass) Sys_Error("VDecorateUserVar::DoResolve: internal compiler error");
  if (!val) { delete this; return nullptr; }
  resolved = true; // anyway
  VName fldnamelo = (fldname.isEmpty() ? NAME_None : VName(*fldname, VName::AddLower));
  VName fldn = ec.SelfClass->FindDecorateStateFieldTrans(fldnamelo);
  if (fldn == NAME_None) {
    ParseError(Loc, "field `%s` is not found in class `%s`", *fldname, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VField *fld = ec.SelfClass->FindField(fldn);
  if (!fld) {
    ParseError(Loc, "field `%s` => `%s` is not found in class `%s`", *fldname, *fldn, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VFieldType ftype = fld->Type;
  if (ftype.Type == TYPE_Array) ftype = ftype.GetArrayInnerType();
  VName mtname = (ftype.Type == TYPE_Int ? "_set_user_var_int" : "_set_user_var_float");
  VMethod *mt = ec.SelfClass->FindMethod(mtname);
  if (!mt) {
    ParseError(Loc, "internal method `%s` not found in class `%s`", *mtname, *ec.SelfClass->GetFullName());
    delete val;
    delete this;
    return nullptr;
  }
  VExpression *args[3];
  args[0] = new VNameLiteral(fldn, Loc);
  args[1] = val;
  args[2] = index;
  VExpression *e = new VInvocation(nullptr, mt, nullptr, false, false, Loc, 3, args);
  index = nullptr;
  //GLog.Logf(NAME_Debug, "**** %s: assign user field '%s' (%s : %s)", *Loc.toString(), *fldname, *fldnamelo, *fldn);
  delete this;
  return e->Resolve(ec);
}


//==========================================================================
//
//  VDecorateUserVar::Emit
//
//==========================================================================
void VDecorateUserVar::Emit (VEmitContext &/*ec*/) {
  Sys_Error("VDecorateUserVar::Emit: the thing that should not be!");
}


//==========================================================================
//
//  VDecorateUserVar::toString
//
//==========================================================================
VStr VDecorateUserVar::toString () const {
  VStr res = VStr(fldname);
  if (index) { res += "["; res += index->toString(); res += "]"; }
  return res;
}


//==========================================================================
//
//  VDecorateUserVar::IsDecorateUserVar
//
//==========================================================================
bool VDecorateUserVar::IsDecorateUserVar () const {
  return true;
}


//==========================================================================
//
//  VDecorateSingleName::VDecorateSingleName
//
//==========================================================================
VDecorateSingleName::VDecorateSingleName (VStr AName, const TLocation &ALoc)
  : VExpression(ALoc)
  , Name(AName)
{
  // check for local
  if (decoIsLocalName(AName)) {
    localAccess = true;
    //GCon->Logf(NAME_Debug, "%s: LOCAL: `%s`", *Loc.toStringNoCol(), *AName);
  } else {
    localAccess = false;
    //GCon->Logf(NAME_Debug, "%s: USER: `%s`", *Loc.toStringNoCol(), *AName);
  }
}


//==========================================================================
//
//  VDecorateSingleName::VDecorateSingleName
//
//==========================================================================
VDecorateSingleName::VDecorateSingleName () {
}


//==========================================================================
//
//  VDecorateSingleName::toString
//
//==========================================================================
VStr VDecorateSingleName::toString () const {
  return VStr(Name);
}


//==========================================================================
//
//  VDecorateSingleName::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateSingleName::SyntaxCopy () {
  auto res = new VDecorateSingleName();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateSingleName::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateSingleName::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateSingleName *)e;
  res->Name = Name;
  res->localAccess = localAccess;
}


//==========================================================================
//
//  VDecorateSingleName::DoResolve
//
//==========================================================================
VExpression *VDecorateSingleName::DoResolve (VEmitContext &ec) {
  // local var?
  if (localAccess) {
    VExpression *e = new VSingleName(VName(*Name, VName::AddLower), Loc);
    delete this;
    return e->Resolve(ec);
  }

  // route uservar
  if (VStr::startsWithCI(*Name, "user_")) {
    VExpression *e = new VDecorateUserVar(Name, Loc);
    delete this;
    return e->Resolve(ec);
  }

  //GLog.Logf(NAME_Debug, "%s: field '%s'", *Loc.toString(), *Name);
  if (ec.SelfClass) {
    VName ExtName = va("decorate_%s", *Name.toLowerCase());

    // prefixed constant
    VConstant *Const = ec.SelfClass->FindConstant(ExtName);
    if (Const) {
      VExpression *e = new VConstantValue(Const, Loc);
      delete this;
      return e->Resolve(ec);
    }

    // prefixed property
    VProperty *Prop = ec.SelfClass->FindDecorateProperty(Name);
    if (Prop) {
      if (!Prop->GetFunc) {
        ParseError(Loc, "Property `%s` cannot be read", *Name);
        delete this;
        return nullptr;
      }
      VExpression *e = new VInvocation(nullptr, Prop->GetFunc, nullptr, false, false, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
  }

  // non-prefixed constant
  // look only for constants defined in DECORATE scripts (and in the current class)
  {
    VConstant *Const = nullptr;
    if (ec.SelfClass) {
      Const = ec.SelfClass->FindDecorateConstant(Name);
    } else {
      // try some other classes
      if (!Const && ActorClass) Const = ActorClass->FindDecorateConstant(Name);
      if (!Const && FakeInventoryClass) Const = FakeInventoryClass->FindDecorateConstant(Name);
      if (!Const && InventoryClass) Const = InventoryClass->FindDecorateConstant(Name);
      if (!Const && AmmoClass) Const = AmmoClass->FindDecorateConstant(Name);
      if (!Const && WeaponClass) Const = WeaponClass->FindDecorateConstant(Name);
      if (!Const && PlayerPawnClass) Const = PlayerPawnClass->FindDecorateConstant(Name);
    }
    if (!Const) {
      VName CheckName = VName(*Name, VName::AddLower);
      Const = (ec.SelfClass ? ec.SelfClass->FindPackageConstant(ec.Package, CheckName) : nullptr);
      if (!Const) Const = ec.Package->FindConstant(CheckName);
    }
    if (Const) {
      VExpression *e = new VConstantValue(Const, Loc);
      delete this;
      return e->Resolve(ec);
    }
  }

  // paren-less decorate method?
  if (ec.SelfClass) {
    VMethod *M = ec.SelfClass->FindDecorateStateAction(*Name);
    if (M) {
      VExpression *e = new VDecorateInvocation(M, *Name, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
  }

  ParseError(Loc, "Illegal expression identifier `%s`", *Name);
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VDecorateSingleName::Emit
//
//==========================================================================
void VDecorateSingleName::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen");
}


//==========================================================================
//
//  VDecorateSingleName::IsDecorateSingleName
//
//==========================================================================
bool VDecorateSingleName::IsDecorateSingleName () const {
  return true;
}



//==========================================================================
//
//  VDecorateAJump::VDecorateAJump
//
//==========================================================================
VDecorateAJump::VDecorateAJump (const TLocation &aloc)
  : VExpression(aloc)
  //, xstc(nullptr)
  //, xass(nullptr)
  , crnd0(nullptr)
  , crnd1(nullptr)
  , prob(nullptr)
  , labels()
  , CallerState(nullptr)
{
}


//==========================================================================
//
//  VDecorateAJump::~VDecorateAJump
//
//==========================================================================
VDecorateAJump::~VDecorateAJump () {
  //delete xstc; xstc = nullptr;
  //delete xass; xass = nullptr;
  delete crnd0; crnd0 = nullptr;
  delete crnd1; crnd1 = nullptr;
  delete prob; prob = nullptr;
  for (int f = labels.length()-1; f >= 0; --f) delete labels[f];
  labels.clear();
  CallerState = nullptr;
}


//==========================================================================
//
//  VDecorateAJump::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateAJump::SyntaxCopy () {
  auto res = new VDecorateAJump();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateAJump::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateAJump::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateAJump *)e;
  res->prob = (prob ? prob->SyntaxCopy() : nullptr);
  res->labels.setLength(labels.length());
  for (int f = 0; f < labels.length(); ++f) {
    res->labels[f] = (labels[f] ? labels[f]->SyntaxCopy() : nullptr);
  }
  res->CallerState = CallerState;
}


//==========================================================================
//
//  VDecorateAJump::DoResolve
//
//==========================================================================
VExpression *VDecorateAJump::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass) Sys_Error("VDecorateAJump::DoResolve: internal compiler error");

  //AutoCopy probcopy(op);

  if (labels.length() > 255) {
    ParseError(Loc, "%d labels in `A_Jump` -- are you nuts?!", labels.length());
    delete this;
    return nullptr;
  }

  if (!prob) { delete this; return nullptr; }

  prob = prob->Resolve(ec);
  if (!prob) { delete this; return nullptr; }

  if (prob->Type.Type != TYPE_Int && prob->Type.Type != TYPE_Byte) {
    ParseError(Loc, "`A_Jump` argument #1 should be integer, not `%s`", *prob->Type.GetName());
    delete this;
    return nullptr;
  }

  for (int lbidx = 0; lbidx < labels.length(); ++lbidx) {
    VExpression *lbl = labels[lbidx];

    if (!lbl) {
      ParseError(Loc, "`A_Jump` cannot have default arguments");
      delete this;
      return nullptr;
    }

    bool massaged = false;
    lbl = lbl->MassageDecorateArg(ec, nullptr, CallerState, "A_Jump", lbidx+2, VFieldType(TYPE_State), false/*not optional*/, nullptr, &massaged);
    if (!lbl) { delete this; return nullptr; } // some error

    lbl = new VCastOrInvocation("DoJump", Loc, 1, &lbl);
    lbl = new VDropResult(lbl);

    labels[lbidx] = lbl->Resolve(ec);
    if (!labels[lbidx]) { delete this; return nullptr; }
  }

  /* generate this code:
    if (prob > 0) {
      if (prob > 255 || P_Random() < prob) {
        switch (P_Random()%label.length()) {
          case n: do_jump_to_label_n;
        }
      }
    }
    // this is not needed, because `DoJump()` does it for us
    //if (XLevel.StateCall) XLevel.StateCall->Result = false;

    do it by allocate local array for labels, populate it, and generate code
    for checks and sets
   */

  // create `XLevel.StateCall` access expression
  /*
  {
    VExpression *xlvl = new VSingleName("XLevel", Loc);
    xstc = new VDotField(xlvl, "StateCall", Loc);
  }
  // XLevel.StateCall->Result
  VExpression *xres = new VPointerField(xstc->SyntaxCopy(), "Result", Loc);
  // XLevel.StateCall->Result = false
  xass = new VAssignment(VAssignment::Assign, xres, new VIntLiteral(0, Loc), Loc);
  */
  // call to `P_Random()`
  crnd0 = new VCastOrInvocation("P_Random", Loc, 0, nullptr);
  crnd1 = crnd0->SyntaxCopy();

  // now resolve all generated expressions
  //xstc = xstc->ResolveBoolean(ec);
  //if (!xstc) { delete this; return nullptr; }

  //xass = xass->Resolve(ec);
  //if (!xass) { delete this; return nullptr; }
  //vassert(xass->Type.Type == TYPE_Void);

  crnd0 = crnd0->Resolve(ec);
  if (!crnd0) { delete this; return nullptr; }

  crnd1 = crnd1->Resolve(ec);
  if (!crnd1) { delete this; return nullptr; }

  if (crnd0->Type.Type != TYPE_Int && crnd0->Type.Type != TYPE_Byte) {
    ParseError(Loc, "`P_Random()` should return integer, not `%s`", *crnd0->Type.GetName());
    delete this;
    return nullptr;
  }

  if (labels.length() == 0) {
    ParseWarning(Loc, "this `A_Jump` is never taken");
  } else if (labels.length() == 1 && prob->IsIntConst() && prob->GetIntConst() > 255) {
    //k8: this is not a bug; usually, this is used to dynamically dispatch control
    //    to a label that can be defined in subclass, for example.
    //    note that `Goto` cannod do that.
    //ParseWarning(Loc, "this `A_Jump` is uncoditional; this is probably a bug (replace it with `Goto` if it isn't)");
  }

  Type.Type = TYPE_Void;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDecorateAJump::Emit
//
//==========================================================================
void VDecorateAJump::Emit (VEmitContext &ec) {
  /* generate this code:
     if (prob <= 0) goto end;
     if (prob > 255) goto doit;
     if (P_Random() >= prob) goto end;
     doit:
       switch (P_Random()%label.length()) {
         case n: do_jump_to_label_n;
       }
     end:
     if (XLevel.StateCall) XLevel.StateCall->Result = false;

     do it by allocate local array for labels, populate it, and generate code
     for checks and sets
   */

  EmitCheckResolved(ec);

  VLabel endTarget = ec.DefineLabel();
  VLabel doitTarget = ec.DefineLabel();

  bool doDrop; // do we need to drop `prob` at the end?

  if (prob->IsIntConst() && prob->GetIntConst() <= 0) {
    // jump is never taken
    doDrop = false;
    ec.AddStatement(OPC_Goto, endTarget, Loc);
  } else if (prob->IsIntConst() && prob->GetIntConst() > 255) {
    doDrop = false;
    // ...and check nothing, jump is always taken
  } else {
    doDrop = true;
    prob->Emit(ec); // prob

    if (!prob->IsIntConst()) {
      //if (prob <= 0) goto end;
      ec.AddStatement(OPC_DupPOD, Loc); // prob, prob
      ec.AddStatement(OPC_PushNumber, 0, Loc); // prob, prob, 0
      ec.AddStatement(OPC_LessEquals, Loc); // prob, flag
      ec.AddStatement(OPC_IfGoto, endTarget, Loc); // prob

      //if (prob > 255) goto doit;
      ec.AddStatement(OPC_DupPOD, Loc); // prob, prob
      ec.AddStatement(OPC_PushNumber, 255, Loc); // prob, prob, 255
      ec.AddStatement(OPC_GreaterEquals, Loc); // prob, flag
      ec.AddStatement(OPC_IfGoto, doitTarget, Loc); // prob

      ec.AddStatement(OPC_DupPOD, Loc); // prob, prob
    } else {
      // if we know prob value, we can omit most checks, and
      // we don't need to keep `prob` on the stack
      doDrop = false;
    }

    //if (P_Random() >= prob) goto end;
    //equals to: if (prob < P_Random()) goto end;
    crnd0->Emit(ec); // prob, prob, prand
    ec.AddStatement(OPC_Less, Loc); // prob, flag
    ec.AddStatement(OPC_IfGoto, endTarget, Loc); // prob
  }

  ec.MarkLabel(doitTarget); // prob

  if (labels.length() == 1) {
    labels[0]->Emit(ec); // prob
  } else if (labels.length() > 0) {
    // P_Random()%label.length()
    crnd1->Emit(ec); // prob, rnd
    ec.AddStatement(OPC_PushNumber, labels.length(), Loc); // prob, rnd, labels.length()
    ec.AddStatement(OPC_Modulus, Loc); // prob, lblidx
    // switch:
    TArray<VLabel> addrs;
    addrs.setLength(labels.length());
    for (int lidx = 0; lidx < labels.length(); ++lidx) addrs[lidx] = ec.DefineLabel();
    for (int lidx = 0; lidx < labels.length(); ++lidx) {
      if (lidx >= 0 && lidx < 256) {
        ec.AddStatement(OPC_CaseGotoB, lidx, addrs[lidx], Loc);
      }/* else if (lidx >= MIN_VINT16 && lidx < MAX_VINT16) {
        ec.AddStatement(OPC_CaseGotoS, lidx, addrs[lidx], Loc);
      } else*/ {
        ec.AddStatement(OPC_CaseGoto, lidx, addrs[lidx], Loc);
      }
    }
    // just in case (and for optimiser)
    ec.AddStatement(OPC_DropPOD, Loc); // prob (lidx dropped)
    ec.AddStatement(OPC_Goto, endTarget, Loc); // prob

    // now generate label jump code
    for (int lidx = 0; lidx < labels.length(); ++lidx) {
      ec.MarkLabel(addrs[lidx]); // prob
      labels[lidx]->Emit(ec); // prob
      if (lidx != labels.length()-1) ec.AddStatement(OPC_Goto, endTarget, Loc); // prob
    }
  }

  ec.MarkLabel(endTarget); // prob
  if (doDrop) ec.AddStatement(OPC_DropPOD, Loc);

  //if (XLevel.StateCall) XLevel.StateCall->Result = false;
  /*
  VLabel falseTarget = ec.DefineLabel();
  // expression
  xstc->EmitBranchable(ec, falseTarget, false);
  // true statement
  xass->Emit(ec);
  ec.MarkLabel(falseTarget);
  */
}


//==========================================================================
//
//  VDecorateAJump::toString
//
//==========================================================================
VStr VDecorateAJump::toString () const {
  VStr res = "A_Jump("+e2s(prob);
  for (int f = 0; f < labels.length(); ++f) {
    res += ", ";
    res += e2s(labels[f]);
  }
  res += ")";
  return res;
}



//==========================================================================
//
//  VDecorateRndPick::VDecorateRndPick
//
//==========================================================================
VDecorateRndPick::VDecorateRndPick (bool aAsFloat, const TLocation &aloc)
  : VExpression(aloc)
  , crnd0(nullptr)
  , numbers()
  , asFloat(aAsFloat)
{
}


//==========================================================================
//
//  VDecorateRndPick::~VDecorateRndPick
//
//==========================================================================
VDecorateRndPick::~VDecorateRndPick () {
  delete crnd0; crnd0 = nullptr;
  for (int f = numbers.length()-1; f >= 0; --f) delete numbers[f];
  numbers.clear();
}


//==========================================================================
//
//  VDecorateRndPick::SyntaxCopy
//
//==========================================================================
VExpression *VDecorateRndPick::SyntaxCopy () {
  auto res = new VDecorateRndPick();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDecorateRndPick::DoSyntaxCopyTo
//
//==========================================================================
void VDecorateRndPick::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDecorateRndPick *)e;
  res->crnd0 = (crnd0 ? crnd0->SyntaxCopy() : nullptr);
  res->numbers.setLength(numbers.length());
  for (int f = 0; f < numbers.length(); ++f) {
    res->numbers[f] = (numbers[f] ? numbers[f]->SyntaxCopy() : nullptr);
  }
  res->asFloat = asFloat;
}


//==========================================================================
//
//  VDecorateRndPick::DoResolve
//
//==========================================================================
VExpression *VDecorateRndPick::DoResolve (VEmitContext &ec) {
  if (numbers.length() == 0) {
    ParseError(Loc, "no choices in `%srandompick` -- are you nuts?!", (asFloat ? "f" : ""));
    delete this;
    return nullptr;
  }

  if (numbers.length() > 255) {
    ParseError(Loc, "%d choices in `%srandompick` -- are you nuts?!", numbers.length(), (asFloat ? "f" : ""));
    delete this;
    return nullptr;
  }

  // resolve numbers
  for (int lbidx = 0; lbidx < numbers.length(); ++lbidx) {
    VExpression *lbl = numbers[lbidx];

    if (!lbl) {
      ParseError(Loc, "`%srandompick` cannot have default arguments", (asFloat ? "f" : ""));
      delete this;
      return nullptr;
    }

    bool massaged = false;
    lbl = lbl->MassageDecorateArg(ec, nullptr/*invokation*/, /*CallerState*/nullptr, (asFloat ? "frandompick" : "randompick"), lbidx+1, (asFloat ? VFieldType(TYPE_Float) : VFieldType(TYPE_Int)), false/*not optional*/, nullptr, &massaged);
    if (!lbl) { delete this; return nullptr; } // some error

    numbers[lbidx] = lbl->Resolve(ec);
    lbl = numbers[lbidx];
    if (!lbl) { delete this; return nullptr; }

    if (lbl->Type.Type == TYPE_Int || lbl->Type.Type == TYPE_Byte) {
      if (asFloat) {
        numbers[lbidx] = (new VScalarToFloat(lbl))->Resolve(ec);
        if (!numbers[lbidx]) { delete this; return nullptr; }
      }
    } else if (lbl->Type.Type == TYPE_Float) {
      if (!asFloat) {
        ParseWarning(Loc, "`%srandompick()` argument #%d must be int", (asFloat ? "f" : ""), lbidx+1);
        numbers[lbidx] = (new VScalarToInt(lbl))->Resolve(ec);
        if (!numbers[lbidx]) { delete this; return nullptr; }
      }
    } else {
      ParseError(Loc, "`%srandompick()` argument #%d has invalid type `%s`", (asFloat ? "f" : ""), lbidx+1, *lbl->Type.GetName());
      delete this;
      return nullptr;
    }
  }

  // one number means "just return it"
  if (numbers.length() > 1) {
    // call to `P_Random()`
    crnd0 = new VBinary(VBinary::Modulus,
              new VCastOrInvocation("P_Random", Loc, 0, nullptr),
              new VIntLiteral(numbers.length(), Loc), Loc);

    crnd0 = crnd0->Resolve(ec);
    if (!crnd0) { delete this; return nullptr; }

    if (crnd0->Type.Type != TYPE_Int && crnd0->Type.Type != TYPE_Byte) {
      ParseError(Loc, "`P_Random()` should return integer, not `%s`", *crnd0->Type.GetName());
      delete this;
      return nullptr;
    }
  } else {
    crnd0 = nullptr; // just in case
  }

  Type.Type = (asFloat ? TYPE_Float : TYPE_Int);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDecorateRndPick::Emit
//
//==========================================================================
void VDecorateRndPick::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);

  vassert(numbers.length() > 0);
  if (numbers.length() == 1) {
    numbers[0]->Emit(ec); // number
  } else {
    VLabel endTarget = ec.DefineLabel();

    // index
    crnd0->Emit(ec);

    // switch:
    TArray<VLabel> addrs;
    addrs.setLength(numbers.length());
    for (int lidx = 0; lidx < numbers.length(); ++lidx) addrs[lidx] = ec.DefineLabel();
    for (int lidx = 0; lidx < numbers.length(); ++lidx) {
      if (lidx >= 0 && lidx < 256) {
        ec.AddStatement(OPC_CaseGotoB, lidx, addrs[lidx], Loc);
      }/* else if (lidx >= MIN_VINT16 && lidx < MAX_VINT16) {
        ec.AddStatement(OPC_CaseGotoS, lidx, addrs[lidx], Loc);
      }*/ else {
        ec.AddStatement(OPC_CaseGoto, lidx, addrs[lidx], Loc);
      }
    }
    // just in case (and for optimiser)
    ec.AddStatement(OPC_DropPOD, Loc); // (index dropped)
    ec.AddStatement(OPC_PushNumber, -1, Loc);
    ec.AddStatement(OPC_Goto, endTarget, Loc);

    // now generate label jump code
    for (int lidx = 0; lidx < numbers.length(); ++lidx) {
      ec.MarkLabel(addrs[lidx]);
      numbers[lidx]->Emit(ec); // number
      if (lidx != numbers.length()-1) ec.AddStatement(OPC_Goto, endTarget, Loc); // prob
    }

    ec.MarkLabel(endTarget);
  }
}


//==========================================================================
//
//  VDecorateRndPick::toString
//
//==========================================================================
VStr VDecorateRndPick::toString () const {
  VStr res = va("%srandompick(", (asFloat ? "f" : ""));
  bool putComma = false;
  for (auto &&n : numbers) {
    if (putComma) res += ", "; else putComma = true;
    res += e2s(n);
  }
  res += ")";
  return res;
}
