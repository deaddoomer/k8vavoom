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

#define VVC_OPTIMIZE_SWIZZLE
//#define VVC_OPTIMIZE_SWIZZLE_DEBUG


// builtin codes
#define BUILTIN_OPCODE_INFO
#include "vc_progdefs.h"

#define DICTDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"

#define DYNARRDISPATCH_OPCODE_INFO
#include "vc_progdefs.h"


//==========================================================================
//
//  VFieldBase
//
//==========================================================================
VFieldBase::VFieldBase (VExpression *AOp, VName AFieldName, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , FieldName(AFieldName)
{
}

//==========================================================================
//
//  VFieldBase::~VFieldBase
//
//==========================================================================
VFieldBase::~VFieldBase () {
  if (op) { delete op; op = nullptr; }
}

//==========================================================================
//
//  VFieldBase::DoSyntaxCopyTo
//
//==========================================================================
void VFieldBase::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VFieldBase *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->FieldName = FieldName;
}


//==========================================================================
//
//  VPointerField::VPointerField
//
//==========================================================================
VPointerField::VPointerField (VExpression *AOp, VName AFieldName, const TLocation &ALoc)
  : VFieldBase(AOp, AFieldName, ALoc)
{
}


//==========================================================================
//
//  VPointerField::SyntaxCopy
//
//==========================================================================
VExpression *VPointerField::SyntaxCopy () {
  auto res = new VPointerField();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VPointerField::TryUFCS
//
//==========================================================================
VExpression *VPointerField::TryUFCS (VEmitContext &ec, AutoCopy &opcopy, const char *errdatatype, VMemberBase *mb) {
  // try UFCS
  VExpression *pp = opcopy.extract();
  if (ec.SelfClass) {
    pp = new VPushPointed(pp, pp->Loc);
    //HACK! for structs
    if (mb->MemberType == MEMBER_Struct) {
      // class
      VStruct *st = (VStruct *)mb;
      VMethod *mt = st->FindAccessibleMethod(FieldName, ec.SelfStruct);
      if (mt) {
        VExpression *dotinv = new VDotInvocation(pp, FieldName, Loc, 0, nullptr);
        delete this;
        return dotinv->Resolve(ec);
      }
    }
    //HACK! for classes
    if (mb->MemberType == MEMBER_Class) {
      // class
      VClass *cls = (VClass *)mb;
      VMethod *mt = cls->FindAccessibleMethod(FieldName, ec.SelfClass);
      if (mt) {
        VExpression *dotinv = new VDotInvocation(pp, FieldName, Loc, 0, nullptr);
        delete this;
        return dotinv->Resolve(ec);
      }
    }
    // normal UFCS
    VExpression *ufcsArgs[2/*VMethod::MAX_PARAMS+1*/];
    ufcsArgs[0] = pp;
    if (VInvocation::FindMethodWithSignature(ec, FieldName, 1, ufcsArgs)) {
      VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
      delete this;
      return call->Resolve(ec);
    }
  }
  delete pp;
  ParseError(Loc, "No such field `%s` in %s `%s`", *FieldName, errdatatype, *mb->GetFullName());
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VPointerField::DoResolve
//
//==========================================================================
VExpression *VPointerField::DoResolve (VEmitContext &ec) {
  AutoCopy opcopy(op);

  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

  if (op->Type.Type != TYPE_Pointer) {
    ParseError(Loc, "Pointer type required on left side of ->");
    delete this;
    return nullptr;
  }

  VFieldType type = op->Type.GetPointerInnerType();

  if (type.Type == TYPE_Struct) {
    if (!type.Struct) {
      ParseError(Loc, "Not a structure/reference type");
      delete this;
      return nullptr;
    }
  } else if (type.Type == TYPE_Vector) {
    // vectors are such structs
    if (!type.Struct) type.Struct = VMemberBase::StaticFindTVec();
  } else if (type.Type == TYPE_Reference) {
    // this can came from dictionary
    if (!type.Class) {
      ParseError(Loc, "Not a structure/reference type");
      delete this;
      return nullptr;
    }
  } else {
    ParseError(Loc, "Expected structure/reference type, but got `%s`", *op->Type.GetName());
    delete this;
    return nullptr;
  }

  VField *field;
  if (type.Type == TYPE_Struct || type.Type == TYPE_Vector) {
    vassert(type.Struct);
    field = type.Struct->FindField(FieldName);
    if (!field) return TryUFCS(ec, opcopy, "struct", type.Struct);
  } else {
    vassert(type.Type == TYPE_Reference);
    vassert(type.Class);
    field = type.Class->FindField(FieldName);
    if (!field) return TryUFCS(ec, opcopy, "class", type.Class);
  }

  VExpression *e = new VFieldAccess(op, field, Loc, 0);
  op = nullptr;
  delete this;

  return e->Resolve(ec);
}


//==========================================================================
//
//  VPointerField::Emit
//
//==========================================================================
void VPointerField::Emit (VEmitContext &) {
  ParseError(Loc, "Should not happen (VPointerField)");
}


//==========================================================================
//
//  VPointerField::toString
//
//==========================================================================
VStr VPointerField::toString () const {
  return e2s(op)+"->"+VStr(FieldName);
}



//==========================================================================
//
//  VDotField::VDotField
//
//==========================================================================
VDotField::VDotField (VExpression *AOp, VName AFieldName, const TLocation &ALoc)
  : VFieldBase(AOp, AFieldName, ALoc)
  , builtin(-1)
{
}


//==========================================================================
//
//  VDotField::IsDotField
//
//==========================================================================
bool VDotField::IsDotField () const { return true; }


//==========================================================================
//
//  VDotField::SyntaxCopy
//
//==========================================================================
VExpression *VDotField::SyntaxCopy () {
  auto res = new VDotField();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDotField::InternalResolve
//
//==========================================================================
VExpression *VDotField::DoPropertyResolve (VEmitContext &ec, VProperty *Prop, AssType assType) {
  if (assType == AssType::AssTarget) {
    if (Prop->SetFunc) {
      VExpression *e;
      if ((Prop->SetFunc->Flags&FUNC_Static) != 0) {
        if (op->Type.Type != TYPE_Class || !op->Type.Class) {
          ParseError(Loc, "Class name expected at the left side of `.`");
          delete this;
          return nullptr;
        }
        // statics has no self
        e = new VPropertyAssign(nullptr, Prop->SetFunc, false, Loc);
      } else {
        if (op->Type.Type == TYPE_Class) {
          ParseError(Loc, "Trying to use non-static property as static");
          delete this;
          return nullptr;
        }
        e = new VPropertyAssign(op, Prop->SetFunc, true, Loc);
        op = nullptr;
      }
      delete this;
      // assignment will call resolve
      return e;
    } else if (Prop->WriteField) {
      if (op->Type.Type == TYPE_Class) {
        ParseError(Loc, "Static fields are not supported yet");
        delete this;
        return nullptr;
      }
      VExpression *e = new VFieldAccess(op, Prop->WriteField, Loc, 0);
      op = nullptr;
      delete this;
      return e->ResolveAssignmentTarget(ec);
    } else {
      ParseError(Loc, "Property `%s` cannot be set", *FieldName);
      delete this;
      return nullptr;
    }
  } else {
    if (op->IsDefaultObject()) {
      if (!Prop->DefaultField) {
        ParseError(Loc, "Property `%s` has no default field set", *FieldName);
        delete this;
        return nullptr;
      }
      VExpression *e = new VFieldAccess(op, Prop->DefaultField, Loc, FIELD_ReadOnly);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else {
      if (Prop->GetFunc) {
        VExpression *e;
        if ((Prop->GetFunc->Flags&FUNC_Static) != 0) {
          //e = new VInvocation(op, Prop->GetFunc, nullptr, true, false, Loc, 0, nullptr);
          if (op->Type.Type != TYPE_Class || !op->Type.Class) {
            ParseError(Loc, "Class name expected at the left side of `.`");
            delete this;
            return nullptr;
          }
          // statics has no self
          e = new VInvocation(nullptr, Prop->GetFunc, nullptr, false, false, Loc, 0, nullptr);
        } else {
          if (op->Type.Type == TYPE_Class) {
            ParseError(Loc, "Trying to use non-static property as static");
            delete this;
            return nullptr;
          }
          e = new VInvocation(op, Prop->GetFunc, nullptr, true, false, Loc, 0, nullptr);
          op = nullptr;
        }
        delete this;
        return e->Resolve(ec);
      } else if (Prop->ReadField) {
        VExpression *e = new VFieldAccess(op, Prop->ReadField, Loc, 0);
        op = nullptr;
        delete this;
        return e->Resolve(ec);
      } else {
        ParseError(Loc, "Property `%s` cannot be read", *FieldName);
        delete this;
        return nullptr;
      }
    }
  }
}


//==========================================================================
//
//  VDotField::InternalResolve
//
//==========================================================================
VExpression *VDotField::InternalResolve (VEmitContext &ec, VDotField::AssType assType) {
  // try enum constant
  if (op && op->IsSingleName()) {
    VName ename = ((VSingleName *)op)->Name;
    // in this class
    if (ec.SelfClass && ec.SelfClass->IsKnownEnum(ename)) {
      VConstant *Const = ec.SelfClass->FindConstant(FieldName, ename);
      if (Const) {
        if (assType == AssType::AssTarget) {
          ParseError(Loc, "Cannot assign member `%s` of enum `%s`", *FieldName, *ename);
          delete this;
          return nullptr;
        }
        VExpression *e = new VConstantValue(Const, Loc);
        delete this;
        return e->Resolve(ec);
      } else {
        ParseError(Loc, "Unknown member `%s` in enum `%s`", *FieldName, *ename);
        delete this;
        return nullptr;
      }
    }
    // in this package
    //fprintf(stderr, "ENUM TRY: <%s %s>\n", *ename, *FieldName);
    if (ec.Package->IsKnownEnum(ename)) {
      //fprintf(stderr, "  <%s %s>\n", *ename, *FieldName);
      VConstant *Const = ec.Package->FindConstant(FieldName, ename);
      if (Const) {
        if (assType == AssType::AssTarget) {
          ParseError(Loc, "Cannot assign member `%s` of enum `%s`", *FieldName, *ename);
          delete this;
          return nullptr;
        }
        VExpression *e = new VConstantValue(Const, Loc);
        delete this;
        return e->Resolve(ec);
      } else {
        ParseError(Loc, "Unknown member `%s` in enum `%s`", *FieldName, *ename);
        delete this;
        return nullptr;
      }
    }
    // in any package (this does package imports)
    VConstant *Const = (VConstant *)VMemberBase::StaticFindMember(FieldName, ANY_PACKAGE, MEMBER_Const, ename);
    if (Const) {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot assign member `%s` of enum `%s`", *FieldName, *ename);
        delete this;
        return nullptr;
      }
      VExpression *e = new VConstantValue(Const, Loc);
      delete this;
      return e->Resolve(ec);
    }
  }

  // try enum constant in other class: `C::E.x`
  if (op && op->IsDoubleName()) {
    VDoubleName *dn = (VDoubleName *)op;
    VClass *Class = VMemberBase::StaticFindClass(dn->Name1);
    //fprintf(stderr, "Class=%s; n0=%s; n1=%s (fld=%s)\n", Class->GetName(), *dn->Name1, *dn->Name2, *FieldName);
    //!if (Class && Class != ec.SelfClass && !Class->Defined) Class->Define();
    if (Class && Class->IsKnownEnum(dn->Name2)) {
      VConstant *Const = Class->FindConstant(FieldName, dn->Name2);
      if (Const) {
        if (assType == AssType::AssTarget) {
          ParseError(Loc, "Cannot assign member `%s` of enum `%s:%s`", *FieldName, *dn->Name1, *dn->Name2);
          delete this;
          return nullptr;
        }
        VExpression *e = new VConstantValue(Const, Loc);
        delete this;
        return e->Resolve(ec);
      } else {
        ParseError(Loc, "Unknown member `%s` in enum `%s::%s`", *FieldName, *dn->Name1, *dn->Name2);
        delete this;
        return nullptr;
      }
    }
  }

  // we need a copy in case this is a pointer thingy
  AutoCopy opcopy(op);

  if (op) op = op->Resolve(ec);
  if (!op) {
    delete this;
    return nullptr;
  }

  if (assType == AssType::AssTarget) {
    if (op->IsSelfLiteral() && ec.CurrentFunc && (ec.CurrentFunc->Flags&FUNC_ConstSelf)) {
      ParseError(Loc, "`self` is read-only (const method `%s`)", *ec.CurrentFunc->GetFullName());
      delete this;
      return nullptr;
    }
    if (op->IsSelfClassLiteral()) {
      ParseError(Loc, "`default` is read-only");
      delete this;
      return nullptr;
    }
  }

  // allow dotted access for dynamic arrays
  if (op->Type.Type == TYPE_Pointer) {
    auto oldflags = op->Flags;
    if (op->Type.InnerType == TYPE_DynamicArray) {
      auto loc = op->Loc;
      delete op;
      op = nullptr;
      op = (new VPushPointed(opcopy.SyntaxCopy(), loc))->Resolve(ec);
      if (!op) { delete this; return nullptr; }
      op->Flags |= oldflags&FIELD_ReadOnly;
    } else {
      delete op;
      op = nullptr;
      VPointerField *e = new VPointerField(opcopy.extract(), FieldName, Loc);
      e->Flags |= oldflags&FIELD_ReadOnly;
      delete this;
      return e->Resolve(ec);
    }
  }

  if (op->Type.Type == TYPE_Reference) {
    // object properties
    if (FieldName == "isDestroyed" || FieldName == "IsDestroyed") {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change `IsDestroyed` property (TODO)");
        delete this;
        return nullptr;
      }
      VExpression *e = new VObjectPropGetIsDestroyed(opcopy.extract(), Loc);
      delete this;
      return e->Resolve(ec);
    }

    if (op->Type.Class) {
      // field first
      VField *field = op->Type.Class->FindField(FieldName, Loc, ec.SelfClass);
      if (field) {
        // generate field access
        VExpression *e = new VFieldAccess(op, field, Loc, (op->IsDefaultObject() ? FIELD_ReadOnly : 0));
        op = nullptr;
        delete this;
        return e->Resolve(ec);
      }

      VProperty *Prop = op->Type.Class->FindProperty(FieldName);
      if (Prop) return DoPropertyResolve(ec, Prop, assType);

      //!if (op->Type.Class && op->Type.Class != ec.SelfClass && !op->Type.Class->Defined) op->Type.Class->Define();
      VMethod *M = op->Type.Class->FindAccessibleMethod(FieldName, ec.SelfClass, &Loc);
      if (M) {
        if (M->Flags&FUNC_Iterator) {
          ParseError(Loc, "Iterator methods can only be used in foreach statements");
          delete this;
          return nullptr;
        }
        VExpression *e;
        // rewrite as invoke
        if ((M->Flags&FUNC_Static) != 0) {
          e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 0, nullptr);
        } else {
          e = new VDotInvocation(opcopy.extract(), FieldName, Loc, 0, nullptr);
        }
        delete this;
        return e->Resolve(ec);
      }

      /*
      VProperty *Prop = op->Type.Class->FindProperty(FieldName);
      if (Prop) return DoPropertyResolve(ec, Prop, assType);
      */
    }

    // convert to method, 'cause why not?
    if (assType != AssType::AssTarget && ec.SelfClass && ec.SelfClass->FindMethod(FieldName)) {
      VExpression *ufcsArgs[1];
      ufcsArgs[0] = opcopy.extract();
      VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
      delete this;
      return call->Resolve(ec);
    }

    ParseError(Loc, "No such field `%s`", *FieldName);
    delete this;
    return nullptr;
  }

  if (op->Type.Type == TYPE_Struct || op->Type.Type == TYPE_Vector) {
    VFieldType type = op->Type;
    // `auto v = vector(a, b, c)` is vector without struct, for example, hence the check
    if (!type.Struct || type.Struct == VMemberBase::StaticFindTVec()) {
      // try to parse swizzle
      const char *s = *FieldName;
      if (!s[1] && (s[0] == 'x' || s[0] == 'y' || s[0] == 'z')) {
        // field access
        // turned off, because of things like `ref vec.x`
        if (!type.Struct && assType != AssType::AssTarget) {
          int idx = (s[0] == 'x' ? 0 : s[0] == 'y' ? 1 : 2);
          VExpression *e = new VVectorDirectFieldAccess(op, idx, Loc);
          op = nullptr;
          delete this;
          return e->Resolve(ec);
        } else {
          // process as normal, we can assign to local field
          //ParseError(Loc, "Cannot assign to local vector field `%s`", *FieldName);
          //delete this;
          //return nullptr;
        }
      } else {
        int swidx = VVectorSwizzleExpr::ParseSwizzles(s);
        if (swidx >= 0) {
          // vector swizzling
          if (assType != AssType::AssTarget) {
            VExpression *e = new VVectorSwizzleExpr(op, swidx, true, Loc);
            op = nullptr;
            delete this;
            return e->Resolve(ec);
          } else {
            ParseError(Loc, "Cannot assign to local vector swizzle `%s`", *FieldName);
            delete this;
            return nullptr;
          }
        }
      }
      // bad swizzle or field access, process with normal resolution
    }
    // struct properties
    if (VStr::strEquCI(*FieldName, "InstanceSize")) {
      VStruct *st = op->Type.Struct;
      // `auto v = vector(a, b, c)` is vector without struct, for example, hence the check
      if (!st) st = VMemberBase::StaticFindTVec();
      VExpression *e = new VIntLiteral(op->Type.GetSize(), Loc);
      delete this;
      return e->Resolve(ec);
    }
    // struct method
    /*if (op->Type.Type == TYPE_Struct || op->Type.Type == TYPE_Vector)*/
    {
      VStruct *parentStruct = type.Struct;
      if (!parentStruct) {
        vassert(op->Type.Type == TYPE_Vector);
        parentStruct = VMemberBase::StaticFindTVec();
      }
      VMethod *M = parentStruct->FindAccessibleMethod(FieldName, /*type.Struct*/ec.SelfStruct, &Loc);
      if (M) {
        if (M->IsIterator()) {
          ParseError(Loc, "Iterator methods can only be used in foreach statements");
          delete this;
          return nullptr;
        }
        // rewrite as invoke
        VExpression *e;
        if (M->IsStatic()) {
          e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 0, nullptr);
        } else {
          e = new VDotInvocation(opcopy.extract(), FieldName, Loc, 0, nullptr);
        }
        delete this;
        return e->Resolve(ec);
      }
    }
    // field
    VField *field = (type.Struct ? type.Struct->FindField(FieldName) : (op->Type.Type == TYPE_Vector ? VMemberBase::StaticFindTVec()->FindField(FieldName) : nullptr));
    if (!field) {
      // try to parse swizzle
      const char *s = *FieldName;
      int swidx = VVectorSwizzleExpr::ParseSwizzles(s);
      if (swidx >= 0) {
        vassert(type.Struct);
        // vector swizzling
        if (assType != AssType::AssTarget) {
          //int opflags = op->Flags;
          op->ResetReadOnly(); // required for taking address
          op->RequestAddressOf();
          VExpression *e = new VVectorSwizzleExpr(op, swidx, false, Loc);
          op = nullptr;
          delete this;
          return e->Resolve(ec);
        } else {
          ParseError(Loc, "Cannot assign to vector swizzle `%s`", *FieldName);
          delete this;
          return nullptr;
        }
      }
      // convert to method, 'cause why not?
      if (assType != AssType::AssTarget) {
        VExpression *ufcsArgs[1];
        ufcsArgs[0] = opcopy.extract();
        VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
        delete this;
        return call->Resolve(ec);
      } else {
        ParseError(Loc, "No such field `%s`", *FieldName);
        delete this;
        return nullptr;
      }
    }
    int opflags = op->Flags;
    //!!!if (assType != AssType::AssTarget) op->Flags &= ~FIELD_ReadOnly;
    op->RequestAddressOf();
    VExpression *e = new VFieldAccess(op, field, Loc, Flags&FIELD_ReadOnly);
    e->Flags |= (opflags|field->Flags)&FIELD_ReadOnly;
    op = nullptr;
    delete this;
    return e->Resolve(ec);
  }

  // class properties
  if (op->Type.Type == TYPE_Class) {
    VClass *cls = (op->Type.Class ?: ec.OuterClass);
    if (cls) {
      VName newname = cls->FindInPropMap(TYPE_Class, FieldName);
      if (newname != NAME_None) {
        // i found her!
        if (assType == AssType::AssTarget) {
          ParseError(Loc, "Cannot assign to read-only property `%s`", *FieldName);
          delete this;
          return nullptr;
        }
        // method
        VMethod *M = cls->FindAccessibleMethod(newname, ec.SelfClass, &Loc);
        if (!M) {
          ParseError(Loc, "Method `%s` not found in class `%s` (for property `%s`)", *newname, cls->GetName(), *FieldName);
          delete this;
          return nullptr;
        }
        if (M->IsIterator()) {
          ParseError(Loc, "Iterator methods can only be used in foreach statements");
          delete this;
          return nullptr;
        }
        if (!M->IsStatic()) {
          ParseError(Loc, "Only static methods can be called with this syntax");
          delete this;
          return nullptr;
        }
        // statics has no self
        VExpression *ufcsArgs[1];
        ufcsArgs[0] = opcopy.extract();
        VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 1, ufcsArgs);
        delete this;
        return e->Resolve(ec);
      }
    }
  }

  // dynarray properties
  if (op->Type.Type == TYPE_DynamicArray &&
      (FieldName == NAME_Num || FieldName == NAME_Length || FieldName == NAME_length ||
       FieldName == "length1" || FieldName == "length2"))
  {
    //VFieldType type = op->Type;
    if (assType == AssType::AssTarget) {
      if (FieldName == "length1" || FieldName == "length2") {
        ParseError(Loc, "Use `setLength` method to create 2d dynamic array");
        delete this;
        return nullptr;
      }
      if (op->IsReadOnly()) {
        ParseError(Loc, "Cannot change length of read-only array");
        delete this;
        return nullptr;
      }
    }
    op->RequestAddressOf();
    if (assType == AssType::AssTarget) {
      // op1 will be patched in assign resolver, so it is ok
      VExpression *e = new VDynArraySetNum(op, nullptr, nullptr, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else {
      VExpression *e = new VDynArrayGetNum(op, (FieldName == "length1" ? 1 : FieldName == "length2" ? 2 : 0), Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    }
  }

  // array properties
  if (op->Type.Type == TYPE_Array &&
      (FieldName == NAME_Num || FieldName == NAME_Length || FieldName == NAME_length ||
       FieldName == "length1" || FieldName == "length2"))
  {
    if (assType == AssType::AssTarget) {
      ParseError(Loc, "Cannot change length of static array");
      delete this;
      return nullptr;
    }
    VExpression *e;
         if (FieldName == "length1") e = new VIntLiteral(op->Type.GetFirstDim(), Loc);
    else if (FieldName == "length2") e = new VIntLiteral(op->Type.GetSecondDim(), Loc);
    else e = new VIntLiteral(op->Type.GetArrayDim(), Loc);
    delete this;
    return e->Resolve(ec);
  }

  // string properties
  if (op->Type.Type == TYPE_String) {
    if (FieldName == NAME_Num || FieldName == NAME_Length || FieldName == NAME_length) {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change string length via assign yet");
        delete this;
        return nullptr;
      }
      if (!op->IsStrConst()) op->RequestAddressOf();
      VExpression *e = new VStringGetLength(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    }
    if (ec.OuterClass) {
      VName newname = ec.OuterClass->FindInPropMap(TYPE_String, FieldName);
      if (newname != NAME_None) {
        // i found her!
        //GLog.Logf(NAME_Debug, "<%s> : <%s>", *FieldName, *newname);
        if (assType == AssType::AssTarget) {
          ParseError(Loc, "Cannot assign to read-only property `%s`", *FieldName);
          delete this;
          return nullptr;
        }
        // let UFCS do the work
        FieldName = newname;
      }
    }
  }

  // name properties
  if (op->Type.Type == TYPE_Name) {
    if (ec.OuterClass) {
      VName newname = ec.OuterClass->FindInPropMap(TYPE_Name, FieldName);
      if (newname != NAME_None) {
        // i found her!
        //GLog.Logf(NAME_Debug, "<%s> : <%s>", *FieldName, *newname);
        if (assType == AssType::AssTarget) {
          ParseError(Loc, "Cannot assign to read-only property `%s`", *FieldName);
          delete this;
          return nullptr;
        }
        // let UFCS do the work
        FieldName = newname;
      }
    }
  }

  // float properties
  if (op->Type.Type == TYPE_Float) {
         if (FieldName == "isnan" || FieldName == "isNan" || FieldName == "isNaN" || FieldName == "isNAN") builtin = OPC_Builtin_FloatIsNaN;
    else if (FieldName == "isinf" || FieldName == "isInf") builtin = OPC_Builtin_FloatIsInf;
    else if (FieldName == "isfinite" || FieldName == "isFinite") builtin = OPC_Builtin_FloatIsFinite;
    if (builtin >= 0) {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot assign to read-only property");
        delete this;
        return nullptr;
      }
      // optimization for float literals
      if (op->IsFloatConst()) {
        VExpression *e = nullptr;
        switch (builtin) {
          case OPC_Builtin_FloatIsNaN: e = new VIntLiteral((isNaNF(op->GetFloatConst()) ? 1 : 0), op->Loc); break;
          case OPC_Builtin_FloatIsInf: e = new VIntLiteral((isInfF(op->GetFloatConst()) ? 1 : 0), op->Loc); break;
          case OPC_Builtin_FloatIsFinite: e = new VIntLiteral((isFiniteF(op->GetFloatConst()) ? 1 : 0), op->Loc); break;
          default: VCFatalError("VC: internal compiler error (float property `%s`)", *FieldName);
        }
        delete this;
        return e->Resolve(ec);
      }
      //FIXME: use int instead of bool here, it generates faster opcode, and doesn't matter for now
      //Type = VFieldType(TYPE_Bool); Type.BitMask = 1;
      Type = VFieldType(TYPE_Int);
      SetResolved();
      return this;
    }
  }

  // slice properties
  if (op->Type.Type == TYPE_SliceArray) {
    if (FieldName == NAME_length) {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change slice length");
        delete this;
        return nullptr;
      }
      op->RequestAddressOf();
      VExpression *e = new VSliceGetLength(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else if (FieldName == "ptr") {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change slice ptr");
        delete this;
        return nullptr;
      }
      op->RequestAddressOf();
      VExpression *e = new VSliceGetPtr(op, Loc);
      e->Flags |= (Flags|op->Flags)&FIELD_ReadOnly;
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else {
      ParseError(Loc, "No field `%s` in slice", *FieldName);
      delete this;
      return nullptr;
    }
  }

  // dictionary properties
  if (op->Type.Type == TYPE_Dictionary) {
    if (FieldName == NAME_length) {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change dictionary length");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDictGetLength(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else if (FieldName == "capacity") {
      if (assType == AssType::AssTarget) {
        ParseError(Loc, "Cannot change dictionary capacity");
        delete this;
        return nullptr;
      }
      VExpression *e = new VDictGetCapacity(op, Loc);
      op = nullptr;
      delete this;
      return e->Resolve(ec);
    } else {
      ParseError(Loc, "No field `%s` in dictionary", *FieldName);
      delete this;
      return nullptr;
    }
  }

  // convert to method, 'cause why not?
  if (assType != AssType::AssTarget) {
    // Class.Method -- for static methods
    if (op->Type.Type == TYPE_Class) {
      if (!op->Type.Class) {
        ParseError(Loc, "Class name expected at the left side of `.` for calling `%s`", *FieldName);
        delete this;
        return nullptr;
      }
      // read property
      VProperty *Prop = op->Type.Class->FindProperty(FieldName);
      if (Prop) return DoPropertyResolve(ec, Prop, assType);
      // method
      VMethod *M = op->Type.Class->FindAccessibleMethod(FieldName, ec.SelfClass, &Loc);
      if (!M) {
        ParseError(Loc, "Method `%s` not found in class `%s`", *FieldName, op->Type.Class->GetName());
        delete this;
        return nullptr;
      }
      if (M->Flags&FUNC_Iterator) {
        ParseError(Loc, "Iterator methods can only be used in foreach statements");
        delete this;
        return nullptr;
      }
      if ((M->Flags&FUNC_Static) == 0) {
        ParseError(Loc, "Only static methods can be called with this syntax");
        delete this;
        return nullptr;
      }
      // statics has no self
      VExpression *e = new VInvocation(nullptr, M, nullptr, false, false, Loc, 0, nullptr);
      delete this;
      return e->Resolve(ec);
    }
    // convert to `func(op)`
    if (ec.SelfClass) {
      VExpression *ufcsArgs[1];
      ufcsArgs[0] = opcopy.extract();
      if (VInvocation::FindMethodWithSignature(ec, FieldName, 1, ufcsArgs)) {
        VCastOrInvocation *call = new VCastOrInvocation(FieldName, Loc, 1, ufcsArgs);
        delete this;
        return call->Resolve(ec);
      }
    }
  } else if (assType == AssType::AssTarget) {
    if (op->Type.Type == TYPE_Class) {
      if (!op->Type.Class) {
        ParseError(Loc, "Class name expected at the left side of `.`");
        delete this;
        return nullptr;
      }
      // write property
      VProperty *Prop = op->Type.Class->FindProperty(FieldName);
      if (Prop) return DoPropertyResolve(ec, Prop, assType);
    }
  }

  ParseError(Loc, "Reference, struct or vector expected on left side of `.` (got `%s`)", *op->Type.GetName());
  delete this;
  return nullptr;
}


//==========================================================================
//
//  VDotField::DoResolve
//
//==========================================================================
VExpression *VDotField::DoResolve (VEmitContext &ec) {
  return InternalResolve(ec, AssType::Normal);
}


//==========================================================================
//
//  VDotField::ResolveAssignmentTarget
//
//==========================================================================
VExpression *VDotField::ResolveAssignmentTarget (VEmitContext &ec) {
  if (IsReadOnly()) {
    ParseError(Loc, "Cannot assign to read-only destination");
    delete this;
    return nullptr;
  }
  return InternalResolve(ec, AssType::AssTarget);
}


//==========================================================================
//
//  VDotField::ResolveAssignmentValue
//
//==========================================================================
VExpression *VDotField::ResolveAssignmentValue (VEmitContext &ec) {
  return InternalResolve(ec, AssType::AssValue);
}


//==========================================================================
//
//  VDotField::Emit
//
//==========================================================================
void VDotField::Emit (VEmitContext &ec) {
  EmitCheckResolved(ec);
  if (builtin < 0) {
    ParseError(Loc, "Should not happen (VDotField)");
  } else {
    if (op) op->Emit(ec);
    ec.AddBuiltin(builtin, Loc);
  }
}


//==========================================================================
//
//  VDotField::toString
//
//==========================================================================
VStr VDotField::toString () const {
  return e2s(op)+"."+VStr(FieldName);
}



//==========================================================================
//
//  VVectorDirectFieldAccess::VVectorDirectFieldAccess
//
//==========================================================================
VVectorDirectFieldAccess::VVectorDirectFieldAccess (VExpression *AOp, int AIndex, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , index(AIndex)
{
}


//==========================================================================
//
//  VVectorDirectFieldAccess::~VVectorDirectFieldAccess
//
//==========================================================================
VVectorDirectFieldAccess::~VVectorDirectFieldAccess () {
  delete op; op = nullptr;
}


//==========================================================================
//
//  VVectorDirectFieldAccess::SyntaxCopy
//
//==========================================================================
VExpression *VVectorDirectFieldAccess::SyntaxCopy () {
  auto res = new VVectorDirectFieldAccess();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VVectorDirectFieldAccess::DoSyntaxCopyTo
//
//==========================================================================
void VVectorDirectFieldAccess::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VVectorDirectFieldAccess *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->index = index;
}


//==========================================================================
//
//  VVectorDirectFieldAccess::DoResolve
//
//==========================================================================
VExpression *VVectorDirectFieldAccess::DoResolve (VEmitContext &ec) {
  // op is already resolved
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  Type = VFieldType(TYPE_Float);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VVectorDirectFieldAccess::Emit
//
//==========================================================================
void VVectorDirectFieldAccess::Emit (VEmitContext &ec) {
  if (!op) return;
  EmitCheckResolved(ec);
  op->Emit(ec);
  ec.AddStatement(OPC_VectorDirect, index, Loc);
}


//==========================================================================
//
//  VVectorDirectFieldAccess::toString
//
//==========================================================================
VStr VVectorDirectFieldAccess::toString () const {
  return e2s(op)+"."+(index == 0 ? VStr("x") : index == 1 ? VStr("y") : index == 2 ? VStr("z") : VStr(index));
}



//==========================================================================
//
//  VVectorDirectFieldAccess::ParseOneSwizzle
//
//  returns swizzle or -1
//
//==========================================================================
int VVectorSwizzleExpr::ParseOneSwizzle (const char *&s) {
  if (!s) return -1;
  while (*s && s[0] == '_') ++s;
  if (!s[0]) return -1;
  bool negated = (s[0] == 'm');
  if (negated) ++s;
  int res = (negated ? VCVSE_Negate : VCVSE_NothingZero);
  switch (s[0]) {
    case '0': ++s; res = VCVSE_Zero; break; // ignore negation
    case '1': ++s; res |= VCVSE_One; break;
    case 'x': ++s; res |= VCVSE_X; break;
    case 'y': ++s; res |= VCVSE_Y; break;
    case 'z': ++s; res |= VCVSE_Z; break;
    default: return -1;
  }
  while (*s && s[0] == '_') ++s;
  return res;
}


//==========================================================================
//
//  VVectorSwizzleExpr::SwizzleToStr
//
//==========================================================================
VStr VVectorSwizzleExpr::SwizzleToStr (int index) {
  if (index == -1) return VStr("invalid");
  VStr res;
  for (unsigned spidx = 0; spidx <= 2; ++spidx) {
    switch (index&VCVSE_Mask) {
      case VCVSE_Zero: res += "0"; break;
      case VCVSE_Zero|VCVSE_Negate: res += "m0"; break;
      case VCVSE_One: res += "1"; break;
      case VCVSE_One|VCVSE_Negate: res += "m1"; break;
      case VCVSE_X: res += "x"; break;
      case VCVSE_Y: res += "y"; break;
      case VCVSE_Z: res += "z"; break;
      case VCVSE_X|VCVSE_Negate: res += "mx"; break;
      case VCVSE_Y|VCVSE_Negate: res += "my"; break;
      case VCVSE_Z|VCVSE_Negate: res += "mz"; break;
      default: res += "*"; break;
    }
    index >>= VCVSE_Shift;
  }
  return res;
}


//==========================================================================
//
//  VVectorSwizzleExpr::ParseSwizzles
//
//==========================================================================
int VVectorSwizzleExpr::ParseSwizzles (const char *s) {
  int sv1 = ParseOneSwizzle(s);
  if (sv1 < 0) return -1;
  int sv2 = (*s ? ParseOneSwizzle(s) : 0);
  if (sv2 < 0) return -1;
  int sv3 = (*s ? ParseOneSwizzle(s) : 0);
  if (sv3 < 0) return -1;
  return sv1|(sv2<<VCVSE_Shift)|(sv3<<(VCVSE_Shift*2));
}


//==========================================================================
//
//  VVectorSwizzleExpr::VVectorSwizzleExpr
//
//==========================================================================
VVectorSwizzleExpr::VVectorSwizzleExpr (VExpression *AOp, int ASwizzle, bool ADirect, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , index(ASwizzle)
  , direct(ADirect)
{
}


//==========================================================================
//
//  VVectorSwizzleExpr::~VVectorSwizzleExpr
//
//==========================================================================
VVectorSwizzleExpr::~VVectorSwizzleExpr () {
  delete op; op = nullptr;
}


//==========================================================================
//
//  VVectorSwizzleExpr::SyntaxCopy
//
//==========================================================================
VExpression *VVectorSwizzleExpr::SyntaxCopy () {
  auto res = new VVectorSwizzleExpr();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VVectorSwizzleExpr::DoSyntaxCopyTo
//
//==========================================================================
void VVectorSwizzleExpr::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VVectorSwizzleExpr *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->index = index;
  res->direct = direct;
}


//==========================================================================
//
//  VVectorSwizzleExpr::GetSwizzleConstant
//
//==========================================================================
TVec VVectorSwizzleExpr::GetSwizzleConstant () const noexcept {
  TVec res(0.0f, 0.0f, 0.0f);
  switch (GetSwizzleX()) {
    case VCVSE_Zero: case VCVSE_Zero|VCVSE_Negate: res.x = 0.0f; break;
    case VCVSE_One: res.x = 1.0f; break;
    case VCVSE_One|VCVSE_Negate: res.x = -1.0f; break;
    default: break;
  }
  switch (GetSwizzleY()) {
    case VCVSE_Zero: case VCVSE_Zero|VCVSE_Negate: res.y = 0.0f; break;
    case VCVSE_One: res.y = 1.0f; break;
    case VCVSE_One|VCVSE_Negate: res.y = -1.0f; break;
    default: break;
  }
  switch (GetSwizzleZ()) {
    case VCVSE_Zero: case VCVSE_Zero|VCVSE_Negate: res.z = 0.0f; break;
    case VCVSE_One: res.z = 1.0f; break;
    case VCVSE_One|VCVSE_Negate: res.z = -1.0f; break;
    default: break;
  }
  return res;
}


//==========================================================================
//
//  VVectorSwizzleExpr::DoResolve
//
//==========================================================================
TVec VVectorSwizzleExpr::DoSwizzle (TVec v) const noexcept {
  float res[3];
  int sw = index;
  for (unsigned spidx = 0; spidx <= 2; ++spidx) {
    switch (sw&VCVSE_ElementMask) {
      case VCVSE_Zero: res[spidx] = 0.0f; break;
      case VCVSE_One: res[spidx] = 1.0f; break;
      case VCVSE_X: res[spidx] = v.x; break;
      case VCVSE_Y: res[spidx] = v.y; break;
      case VCVSE_Z: res[spidx] = v.z; break;
      default: VPackage::InternalFatalError(va("Invalid vector access mask (0x%01x)", sw&VCVSE_Mask));
    }
    if (sw&VCVSE_Negate) res[spidx] = -res[spidx];
    sw >>= VCVSE_Shift;
  }
  return TVec(res[0], res[1], res[2]);
}


//==========================================================================
//
//  VVectorSwizzleExpr::OptimiseSwizzleConsts
//
//  modify constructor with constants
//  this may convert vector ctor to constant vector ctor
//
//  can be used only on non-const vector ctors
//  (const ctors are processed elsewhere)
//
//==========================================================================
bool VVectorSwizzleExpr::OptimiseSwizzleConsts (VEmitContext &ec) {
  if (!op || !op->IsVectorCtor()) return false;
  VVectorExpr *vex = (VVectorExpr *)op;
  if (!vex->op1 || !vex->op2 || !vex->op3) return false;
  bool res = false;
  int sw = index;
  for (int spidx = 0; spidx < 3; ++spidx) {
    VExpression *vop = vex->getOp(spidx);
    vassert(vop);
    VExpression *newop = nullptr;
    switch (sw&VCVSE_Mask) {
      case VCVSE_Zero:
      case VCVSE_Zero|VCVSE_Negate:
        if (!vop->IsFloatConst() || vop->GetFloatConst() != 0.0f) newop = new VFloatLiteral(0.0f, Loc);
        break;
      case VCVSE_One:
        if (!vop->IsFloatConst() || vop->GetFloatConst() != 1.0f) newop = new VFloatLiteral(1.0f, Loc);
        break;
      case VCVSE_One|VCVSE_Negate:
        if (!vop->IsFloatConst() || vop->GetFloatConst() != -1.0f) newop = new VFloatLiteral(-1.0f, Loc);
        break;
      default: break;
    }
    if (newop) {
      newop = newop->Resolve(ec);
      vassert(newop);
      res = true;
      delete vop;
      vex->setOp(spidx, newop);
    }
    sw >>= VCVSE_Shift;
  }
  return res;
}


//==========================================================================
//
//  VVectorSwizzleExpr::OptimiseSwizzleSwizzle
//
//  optimze swizzle of swizzle
//
//==========================================================================
bool VVectorSwizzleExpr::OptimiseSwizzleSwizzle (VEmitContext &) {
  if (!op || !op->IsSwizzle()) return false;
  VVectorSwizzleExpr *swe = (VVectorSwizzleExpr *)op;
  if (!swe->op) return false; // just in case
  // .yxz.zxy -> zyx
  // i.e. shuffle `swe` index with this index (because `swe` is applied first)
  int swx = swe->GetSwizzleX();
  int swy = swe->GetSwizzleY();
  int swz = swe->GetSwizzleZ();
  int newswx = swx, newswy = swy, newswz = swz;
  for (int si = 0; si <= 2; ++si) {
    const int csw = (si == 0 ? GetSwizzleX() : si == 1 ? GetSwizzleY() : GetSwizzleZ());
    int nsw = -666;
    switch (csw) {
      case VCVSE_Zero:
      case VCVSE_Zero|VCVSE_Negate:
      case VCVSE_One:
      case VCVSE_One|VCVSE_Negate:
        nsw = csw;
        break;
      case VCVSE_X: nsw = swx; break;
      case VCVSE_X|VCVSE_Negate: nsw = swx^VCVSE_Negate; break;
      case VCVSE_Y: nsw = swy; break;
      case VCVSE_Y|VCVSE_Negate: nsw = swy^VCVSE_Negate; break;
      case VCVSE_Z: nsw = swz; break;
      case VCVSE_Z|VCVSE_Negate: nsw = swz^VCVSE_Negate; break;
      default: VPackage::InternalFatalError(va("Invalid vector access mask (0x%01x)", csw));
    }
    vassert(nsw >= 0);
    switch (si) {
      case 0: newswx = nsw; break;
      case 1: newswy = nsw; break;
      case 2: newswz = nsw; break;
    }
  }
  // our `op` is `swe`, it is deleted below
  op = swe->op;
  swe->op = nullptr;
  const int newidx = newswx|(newswy<<VCVSE_Shift)|(newswz<<(VCVSE_Shift*2));
  //GLog.Logf(NAME_Debug, "optimized swizzle: %s : %s -> %s", *SwizzleToStr(swe->index), *SwizzleToStr(index), *SwizzleToStr(newidx));
  index = newidx;
  delete swe;
  return true;
}


//==========================================================================
//
//  VVectorSwizzleExpr::OptimiseSwizzleCtor
//
//  modify vector constructor
//  limited form: only with non-repeating operands
//
//  can be used only on non-const vector ctors
//  (const ctors are processed elsewhere)
//
//==========================================================================
bool VVectorSwizzleExpr::OptimiseSwizzleCtor (VEmitContext &ec) {
  if (!op || !op->IsVectorCtor() || op->IsConstVectorCtor()) return false;
  if (IsSwizzleIdentity()) return false;
  VVectorExpr *vex = (VVectorExpr *)op;
  if (!vex->op1 || !vex->op2 || !vex->op3) return false;

  // check for valid transformation
  int usecount[3] = {0, 0, 0};
  bool valid = true;

  int sw = index;
  for (int spidx = 0; spidx <= 2; ++spidx) {
    int useop = -1;
    switch (sw&VCVSE_Mask) {
      case VCVSE_X:
      case VCVSE_X|VCVSE_Negate:
        useop = 0;
        break;
      case VCVSE_Y:
      case VCVSE_Y|VCVSE_Negate:
        useop = 1;
        break;
      case VCVSE_Z:
      case VCVSE_Z|VCVSE_Negate:
        useop = 2;
        break;
      default: break;
    }
    if (useop >= 0) {
      VExpression *vop = vex->getOp(useop);
      vassert(vop);
      // constants can be safely duplicated
      if (!vop->IsFloatConst() && !vop->IsIntConst()) {
        if (usecount[useop]) { valid = false; break; }
        ++usecount[useop];
      }
    }
    sw >>= VCVSE_Shift;
  }

  if (!valid) return false; // invalid transformation

  // create new operand list for vector ctor
  bool used[3] = {false, false, false}; // old unused operands must be deleted
  bool res[3] = {false, false, false}; // new operands must be resolved
  VExpression *ops[3] = {nullptr, nullptr, nullptr}; // new operands will be put here
  sw = index;
  for (int spidx = 0; spidx <= 2; ++spidx) {
    int useop = -1;
    switch (sw&VCVSE_Mask) {
      case VCVSE_Zero:
      case VCVSE_Zero|VCVSE_Negate:
        ops[spidx] = new VFloatLiteral(0.0f, Loc);
        res[spidx] = true;
        break;
      case VCVSE_One:
        ops[spidx] = new VFloatLiteral(1.0f, Loc);
        res[spidx] = true;
        break;
      case VCVSE_One|VCVSE_Negate:
        ops[spidx] = new VFloatLiteral(-1.0f, Loc);
        res[spidx] = true;
        break;
      case VCVSE_X:
      case VCVSE_X|VCVSE_Negate:
        useop = 0;
        break;
      case VCVSE_Y:
      case VCVSE_Y|VCVSE_Negate:
        useop = 1;
        break;
      case VCVSE_Z:
      case VCVSE_Z|VCVSE_Negate:
        useop = 2;
        break;
      default: VPackage::InternalFatalError(va("Invalid vector access mask (0x%01x)", sw&VCVSE_Mask));
    }
    if (useop >= 0) {
      VExpression *vop = vex->getOp(useop);
      vassert(vop);
      if (vop->IsIntConst()) {
        // operand is an integer constant
        float v = (float)vop->GetIntConst();
        if (sw&VCVSE_Negate) v = -v;
        ops[spidx] = new VFloatLiteral(v, vop->Loc);
        res[spidx] = true;
      } else if (vop->IsFloatConst()) {
        // operand is a float constant
        float v = (float)vop->GetFloatConst();
        if (sw&VCVSE_Negate) v = -v;
        ops[spidx] = new VFloatLiteral(v, vop->Loc);
        res[spidx] = true;
      } else {
        // operand is something complex
        vassert(!used[useop]);
        used[useop] = true;
        if (sw&VCVSE_Negate) {
          ops[spidx] = new VUnary(VUnary::Minus, vop, vop->Loc, true/*resolved*/);
          res[spidx] = true;
        } else {
          ops[spidx] = vop;
        }
      }
    }
    sw >>= VCVSE_Shift;
  }

  // resolve new operands
  for (int ff = 0; ff < 3; ++ff) {
    vassert(ops[ff]);
    if (res[ff]) {
      VExpression *e = ops[ff]->Resolve(ec);
      if (!e) { delete op; op = nullptr; return true; } // this will abort resolving in the caller
      ops[ff] = e;
    }
  }

  // delete unused operands
  for (int ff = 0; ff < 3; ++ff) {
    if (!used[ff]) {
      VExpression *vop = vex->getOp(ff);
      delete vop;
    }
  }

  // set new operands
  for (int ff = 0; ff < 3; ++ff) vex->setOp(ff, ops[ff]);

  // now we're just an identity
  index = (VCVSE_X|(VCVSE_Y<<VCVSE_Shift)|(VCVSE_Z<<(VCVSE_Shift*2)));

  return true; // rewritten
}


//==========================================================================
//
//  VVectorSwizzleExpr::DoResolve
//
//==========================================================================
VExpression *VVectorSwizzleExpr::DoResolve (VEmitContext &ec) {
  // op is already resolved, but check it just in case
  if (op && !op->IsResolved()) op = op->Resolve(ec);
  if (!op) { delete this; return nullptr; }
  if (index == -1) { delete this; return nullptr; }

#ifdef VVC_OPTIMIZE_SWIZZLE
  #ifdef VVC_OPTIMIZE_SWIZZLE_DEBUG
  GLog.Logf(NAME_Debug, "SWIZZLE: %s", *toString());
  #endif
  for (;;) {
    if (!op) { delete this; return nullptr; } // just in case
    if (index == -1) { delete this; return nullptr; } // just in case

    #ifdef VVC_OPTIMIZE_SWIZZLE_DEBUG
    GLog.Logf(NAME_Debug, "SWIZZLE:  preop: %s", *toString());
    #endif

    // identity swizzle can be ignored
    if (IsSwizzleIdentity()) {
      VExpression *e = op;
      op = nullptr;
      delete this;
      #ifdef VVC_OPTIMIZE_SWIZZLE_DEBUG
      GLog.Logf(NAME_Debug, "SWIZZLE:  DONE-IDENTITY: %s", *e->toString());
      #endif
      return e;
    }

    // constant swizze can be performed in-place
    if (op->IsConstVectorCtor()) {
      TVec v = ((VVectorExpr *)op)->GetConstValue();
      v = DoSwizzle(v);
      VExpression *e = new VVectorExpr(v, Loc);
      delete this;
      #ifdef VVC_OPTIMIZE_SWIZZLE_DEBUG
      GLog.Logf(NAME_Debug, "SWIZZLE:  DONE-INPLACE: %s", *e->toString());
      #endif
      return e->Resolve(ec);
    }

    // if at least one optimisation was done, try all optimisations again
    if (OptimiseSwizzleSwizzle(ec)) {
      #ifdef VVC_OPTIMIZE_SWIZZLE_DEBUG
      GLog.Logf(NAME_Debug, "SWIZZLE:    swsw: %s", *toString());
      #endif
      continue;
    }
    if (OptimiseSwizzleConsts(ec)) {
      #ifdef VVC_OPTIMIZE_SWIZZLE_DEBUG
      GLog.Logf(NAME_Debug, "SWIZZLE:    swcc: %s", *toString());
      #endif
      continue;
    }
    if (OptimiseSwizzleCtor(ec)) {
      #ifdef VVC_OPTIMIZE_SWIZZLE_DEBUG
      GLog.Logf(NAME_Debug, "SWIZZLE:    swct: %s", *toString());
      #endif
      continue;
    }

    #ifdef VVC_OPTIMIZE_SWIZZLE_DEBUG
    GLog.Logf(NAME_Debug, "SWIZZLE:  DONE: %s", *toString());
    #endif
    // cannot optimise anything
    break;
  }

  // negate swizzle can be converted to vector negation
  if (IsSwizzleIdentityNeg()) {
    VExpression *e = new VUnary(VUnary::Minus, op, Loc, true/*resolved*/);
    op = nullptr;
    delete this;
    return e->Resolve(ec);
  }
#endif

  Type = VFieldType(TYPE_Vector);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VVectorSwizzleExpr::Emit
//
//==========================================================================
void VVectorSwizzleExpr::Emit (VEmitContext &ec) {
  if (!op) return;
  EmitCheckResolved(ec);
  op->Emit(ec);
  // for indirect, load a vector, and then issue direct swizzle
  if (!direct) ec.AddStatement(OPC_VFieldValue, (VField *)nullptr, Loc);
  ec.AddStatement(OPC_VectorSwizzleDirect, index, Loc);
}


//==========================================================================
//
//  VVectorSwizzleExpr::IsSwizzle
//
//==========================================================================
bool VVectorSwizzleExpr::IsSwizzle () const {
  return true;
}


//==========================================================================
//
//  VVectorSwizzleExpr::toString
//
//==========================================================================
VStr VVectorSwizzleExpr::toString () const {
  return e2s(op)+va("_swizzle(%s:%c)", *SwizzleToStr(index), (direct ? 'd' : 'i'));
}



//==========================================================================
//
//  VFieldAccess::VFieldAccess
//
//==========================================================================
VFieldAccess::VFieldAccess (VExpression *AOp, VField *AField, const TLocation &ALoc, int ExtraFlags)
  : VExpression(ALoc)
  , op(AOp)
  , field(AField)
  , AddressRequested(false)
{
  Flags = /*field->Flags|*/ExtraFlags;
}


//==========================================================================
//
//  VFieldAccess::~VFieldAccess
//
//==========================================================================
VFieldAccess::~VFieldAccess () {
  if (op) {
    delete op;
    op = nullptr;
  }
}


//==========================================================================
//
//  VFieldAccess::SyntaxCopy
//
//==========================================================================
VExpression *VFieldAccess::SyntaxCopy () {
  auto res = new VFieldAccess();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VFieldAccess::DoSyntaxCopyTo
//
//==========================================================================
void VFieldAccess::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VFieldAccess *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->field = field;
  res->AddressRequested = AddressRequested;
}


//==========================================================================
//
//  VFieldAccess::DoResolve
//
//==========================================================================
VExpression *VFieldAccess::DoResolve (VEmitContext &) {
  Type = field->Type;
  RealType = field->Type;
  if (Type.Type == TYPE_Byte || Type.Type == TYPE_Bool) Type = VFieldType(TYPE_Int);
  SetResolved();
  return this;
}


//==========================================================================
//
//  VFieldAccess::RequestAddressOf
//
//==========================================================================
void VFieldAccess::RequestAddressOf () {
  if (AddressRequested) ParseError(Loc, "Multiple address of field (%s)", *toString());
  AddressRequested = true;
}


//==========================================================================
//
//  VFieldAccess::Emit
//
//==========================================================================
void VFieldAccess::Emit (VEmitContext &ec) {
  if (!op) return; //k8: don't segfault
  EmitCheckResolved(ec);
  op->Emit(ec);
  if (AddressRequested) {
    ec.AddStatement(OPC_Offset, field, Loc);
  } else {
    switch (field->Type.Type) {
      case TYPE_Int:
      case TYPE_Float:
      case TYPE_Name:
        ec.AddStatement(OPC_FieldValue, field, Loc);
        break;
      case TYPE_Byte:
        ec.AddStatement(OPC_ByteFieldValue, field, Loc);
        break;
      case TYPE_Bool:
             if (field->Type.BitMask&0x000000ff) ec.AddStatement(OPC_Bool0FieldValue, field, (int)(field->Type.BitMask), Loc);
        else if (field->Type.BitMask&0x0000ff00) ec.AddStatement(OPC_Bool1FieldValue, field, (int)(field->Type.BitMask>>8), Loc);
        else if (field->Type.BitMask&0x00ff0000) ec.AddStatement(OPC_Bool2FieldValue, field, (int)(field->Type.BitMask>>16), Loc);
        else ec.AddStatement(OPC_Bool3FieldValue, field, (int)(field->Type.BitMask>>24), Loc);
        break;
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        ec.AddStatement(OPC_PtrFieldValue, field, Loc);
        break;
      case TYPE_Vector:
        ec.AddStatement(OPC_VFieldValue, field, Loc);
        break;
      case TYPE_String:
        ec.AddStatement(OPC_StrFieldValue, field, Loc);
        break;
      case TYPE_Delegate:
        ec.AddStatement(OPC_Offset, field, Loc);
        ec.AddStatement(OPC_PushPointedDelegate, Loc);
        break;
      case TYPE_SliceArray:
        ec.AddStatement(OPC_SliceFieldValue, field, Loc);
        break;
      default:
        ParseError(Loc, "Invalid operation on field of this type");
    }
  }
}


//==========================================================================
//
//  VFieldAccess::IsFieldAccess
//
//==========================================================================
bool VFieldAccess::IsFieldAccess () const {
  return true;
}


//==========================================================================
//
//  VFieldAccess::toString
//
//==========================================================================
VStr VFieldAccess::toString () const {
  return e2s(op)+"->"+(field ? VStr(field->Name) : e2s(nullptr));
}



//==========================================================================
//
//  VDelegateVal::VDelegateVal
//
//==========================================================================
VDelegateVal::VDelegateVal (VExpression *AOp, VMethod *AM, const TLocation &ALoc)
  : VExpression(ALoc)
  , op(AOp)
  , M(AM)
{
}


//==========================================================================
//
//  VDelegateVal::~VDelegateVal
//
//==========================================================================
VDelegateVal::~VDelegateVal () {
  if (op) {
    delete op;
    op = nullptr;
  }
}


//==========================================================================
//
//  VDelegateVal::SyntaxCopy
//
//==========================================================================
VExpression *VDelegateVal::SyntaxCopy () {
  auto res = new VDelegateVal();
  DoSyntaxCopyTo(res);
  return res;
}


//==========================================================================
//
//  VDelegateVal::DoSyntaxCopyTo
//
//==========================================================================
void VDelegateVal::DoSyntaxCopyTo (VExpression *e) {
  VExpression::DoSyntaxCopyTo(e);
  auto res = (VDelegateVal *)e;
  res->op = (op ? op->SyntaxCopy() : nullptr);
  res->M = M;
}


//==========================================================================
//
//  VDelegateVal::DoResolve
//
//==========================================================================
VExpression *VDelegateVal::DoResolve (VEmitContext &ec) {
  if (!ec.SelfClass) { ParseError(Loc, "delegate should be in class"); delete this; return nullptr; }
  // parser creates this with unresolved self expression
  if (op && !op->IsResolved()) {
    op = op->Resolve(ec);
    if (!op) { delete this; return nullptr; }
  }
  bool wasError = false;
  if ((M->Flags&FUNC_Static) != 0) { wasError = true; ParseError(Loc, "delegate should not be static"); }
  if ((M->Flags&FUNC_VarArgs) != 0) { wasError = true; ParseError(Loc, "delegate should not be vararg"); }
  if ((M->Flags&FUNC_Iterator) != 0) { wasError = true; ParseError(Loc, "delegate should not be iterator"); }
  // sadly, `FUNC_NonVirtual` is not set yet, so use slower check
  // meh, delegates can get non-virtual functions now
  //if (ec.SelfClass->isNonVirtualMethod(M->Name)) { wasError = true; ParseError(Loc, "delegate should not be final"); }
  if (wasError) { delete this; return nullptr; }
  Type = TYPE_Delegate;
  Type.Function = M;
  SetResolved();
  return this;
}


//==========================================================================
//
//  VDelegateVal::Emit
//
//==========================================================================
void VDelegateVal::Emit (VEmitContext &ec) {
  if (!op) return;
  EmitCheckResolved(ec);
  op->Emit(ec);
  // call class postload, so `FUNC_NonVirtual` will be set
  //ec.SelfClass->PostLoad();
  if (!M->Outer) { ParseError(Loc, "VDelegateVal::Emit: Method has no outer!"); return; }
  if (M->Outer->MemberType != MEMBER_Class) { ParseError(Loc, "VDelegateVal::Emit: Method outer is not a class!"); return; }
  M->Outer->PostLoad();
  if (!M->IsPostLoaded()) { ParseError(Loc, "VDelegateVal::Emit: Method `%s` is not postloaded!", *M->GetFullName()); return; }
  //fprintf(stderr, "MT: %s (rf=%d)\n", *M->GetFullName(), (M->Flags&FUNC_NonVirtual ? 1 : 0));
  if (/*M->Flags&FUNC_NonVirtual*/M->IsNonVirtual()) {
    ec.AddStatement(OPC_PushFunc, M, Loc);
  } else {
    ec.AddStatement(OPC_PushVFunc, M, Loc);
  }
}


//==========================================================================
//
//  VDelegateVal::toString
//
//==========================================================================
VStr VDelegateVal::toString () const {
  return VStr("&(")+e2s(op)+"."+(M ? VStr(M->Name) : e2s(nullptr))+")";
}
