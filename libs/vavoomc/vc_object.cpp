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
#include "vc_public.h"

#define VC_GARBAGE_COLLECTOR_CHECKS
//#define VC_GARBAGE_COLLECTOR_LOGS_BASE
//#define VC_GARBAGE_COLLECTOR_LOGS_MORE
//#define VC_GARBAGE_COLLECTOR_LOGS_XTRA

// compact slot storage on each GC cycle?
//#define VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG

int VObject::cliShowReplacementMessages = 0;
int VObject::cliShowLoadingMessages = 0;
int VObject::cliShowGCMessages = 0;
int VObject::cliShowIODebugMessages = 0;
int VObject::cliDumpNameTables = 0;
int VObject::cliAllErrorsAreFatal = 0;
int VObject::cliVirtualiseDecorateMethods = 0;
int VObject::cliShowPackageLoading = 0;
int VObject::cliShowUndefinedBuiltins = 1;
int VObject::cliCaseSensitiveLocals = 1;
int VObject::cliCaseSensitiveFields = 1;
int VObject::engineAllowNotImplementedBuiltins = 0;
int VObject::standaloneExecutor = 0;
TMap<VStrCI, bool> VObject::cliAsmDumpMethods;
TArray<VObject::CanSkipReadingClassFn> VObject::CanSkipReadingClassCBList;
TMapNC<VName, VName> VObject::IOClassNameTranslation;
TArray<VObject::ClassNameTranslationFn> VObject::ClassNameTranslationCBList;

VObject::GetVCvarObjectFn VObject::GetVCvarObject = nullptr;


//==========================================================================
//
//  showGCMessages
//
//==========================================================================
#if defined(VC_GARBAGE_COLLECTOR_LOGS_BASE)
# define vdgclogf(...)  if (GCDebugMessagesAllowed && cliShowGCMessages) GLog.Logf(NAME_Debug, __VA_ARGS__)
#else
# define vdgclogf(...)  do {} while (0)
#endif


// ////////////////////////////////////////////////////////////////////////// //
// register a class at startup time
VClass VObject::PrivateStaticClass (
  EC_NativeConstructor,
  sizeof(VObject),
  VObject::StaticClassFlags,
  nullptr,
  NAME_Object,
  VObject::InternalConstructor
);
VClass *autoclassVObject = VObject::StaticClass();


// ////////////////////////////////////////////////////////////////////////// //
static vuint32 gLastUsedUniqueId = 0;
TArray<VObject *> VObject::GObjObjects;
// this is LIFO queue of free slots in `GObjObjects`
// it is not required in compacting collector
// but Spelunky Remake *may* use "immediate delete" mode, so leave it here
static int gObjFirstFree = 0; // frist free index in `GObjObjects`
int VObject::GNumDeleted = 0;
bool VObject::GInGarbageCollection = false;
static void *GNewObject = nullptr;
bool VObject::GImmediadeDelete = true;
bool VObject::GGCMessagesAllowed = false;
int VObject::GCDebugMessagesAllowed = 0;
bool (*VObject::onExecuteNetMethodCB) (VObject *obj, VMethod *func) = nullptr; // return `false` to do normal execution
bool VObject::DumpBacktraceToStdErr = false;

static VQueueLifo<vint32> gDelayDeadObjects;


VObject::GCStats VObject::gcLastStats;


/*
static void dumpFieldDefs (VClass *cls, const vuint8 *data) {
  if (!cls || !cls->Fields) return;
  GLog.Logf("=== CLASS:%s ===", cls->GetName());
  for (VField *fi = cls->Fields; fi; fi = fi->Next) {
    if (fi->Type.Type == TYPE_Int) {
      GLog.Logf("  %s: %d v=%d", fi->GetName(), fi->Ofs, *(const vint32 *)(data+fi->Ofs));
    } else if (fi->Type.Type == TYPE_Float) {
      GLog.Logf("  %s: %d v=%f", fi->GetName(), fi->Ofs, *(const float *)(data+fi->Ofs));
    } else {
      GLog.Logf("  %s: %d", fi->GetName(), fi->Ofs);
    }
  }
}
*/


//==========================================================================
//
//  VScriptIterator::~VScriptIterator
//
//==========================================================================
VScriptIterator::~VScriptIterator () {
}


//==========================================================================
//
//  VScriptIterator::Finished
//
//==========================================================================
void VScriptIterator::Finished () {
  delete this;
}


//==========================================================================
//
//  VMethodProxy::VMethodProxy
//
//==========================================================================
VMethodProxy::VMethodProxy ()
  : MethodName(nullptr)
  , Method(nullptr)
  , Class(nullptr)
{
}


//==========================================================================
//
//  VMethodProxy::VMethodProxy
//
//==========================================================================
VMethodProxy::VMethodProxy (const char *AMethod)
  : MethodName(AMethod)
  , Method(nullptr)
  , Class(nullptr)
{
}


//==========================================================================
//
//  VMethodProxy::ResolveStatic
//
//  returns `false` if static method not found
//
//==========================================================================
bool VMethodProxy::ResolveStatic (VClass *aClass) {
  if (Method) return true;
  if (!aClass || !MethodName || !MethodName[0]) return false;
  // if there is no such name, there is no such method
  VName n = VName(MethodName, VName::Find);
  if (n == NAME_None) return false;
  Class = aClass;
  Method = Class->FindAccessibleMethod(n);
  if (!Method) return false;
  // find outer class
  for (VMemberBase *o = Method->Outer; o; o = o->Outer) {
    if (o->MemberType == MEMBER_Class || o->MemberType == MEMBER_DecorateClass) {
      VClass *cls = (VClass *)o;
      // now look into parent classes
      while (cls) {
        if (!cls->FindAccessibleMethod(n)) {
          //GLog.Logf("BOO! '%s' (%s)", *n, cls->GetName());
          break;
        }
        Class = cls;
        cls = cls->ParentClass;
      }
      //GLog.Logf("MT `%s`: class is `%s`, real class is `%s`", MethodName, Self->GetClass()->GetName(), Class->GetName());
    }
  }
  //GLog.Logf("MT.Outer: `%s`", (Method->Outer ? Method->Outer->GetName() : "<null>"));
  if (Method && !Method->IsStatic()) Method = nullptr;
  return !!Method;
}


//==========================================================================
//
//  VMethodProxy::Resolve
//
//  returns `false` if method not found
//
//==========================================================================
bool VMethodProxy::Resolve (VObject *Self) {
  if (Method) return true;
  if (!Self || !MethodName || !MethodName[0]) return false;
  // if there is no such name, there is no such method
  VName n = VName(MethodName, VName::Find);
  if (n == NAME_None) return false;
  Class = Self->GetClass();
  Method = Class->FindAccessibleMethod(n);
  if (!Method) return false;
  // find outer class
  for (VMemberBase *o = Method->Outer; o; o = o->Outer) {
    if (o->MemberType == MEMBER_Class || o->MemberType == MEMBER_DecorateClass) {
      VClass *cls = (VClass *)o;
      // now look into parent classes
      while (cls) {
        if (!cls->FindAccessibleMethod(n)) {
          //GLog.Logf("BOO! '%s' (%s)", *n, cls->GetName());
          break;
        }
        Class = cls;
        cls = cls->ParentClass;
      }
      //GLog.Logf("MT `%s`: class is `%s`, real class is `%s`", MethodName, Self->GetClass()->GetName(), Class->GetName());
    }
  }
  //GLog.Logf("MT.Outer: `%s`", (Method->Outer ? Method->Outer->GetName() : "<null>"));
  return !!Method;
}


//==========================================================================
//
//  VMethodProxy::ResolveChecked
//
//==========================================================================
void VMethodProxy::ResolveChecked (VObject *Self) {
  if (!Resolve(Self)) VPackage::InternalFatalError(va("cannot find method `%s` in class `%s`", (MethodName ? MethodName : "<unnamed>"), (Class ? Class->GetName() : "<unnamed>")));
}


//==========================================================================
//
//  VMethodProxy::ResolveStaticChecked
//
//==========================================================================
void VMethodProxy::ResolveStaticChecked (VClass *aClass) {
  if (!ResolveStatic(aClass)) VPackage::InternalFatalError(va("cannot find static method `%s` in class `%s`", (MethodName ? MethodName : "<unnamed>"), (aClass ? aClass->GetName() : "<unnamed>")));
}


//==========================================================================
//
//  VMethodProxy::Execute
//
//==========================================================================
VFuncRes VMethodProxy::Execute (VObject *Self) {
  ResolveChecked(Self);
  if (!(Method->Flags&FUNC_Static)) {
    vassert(Self);
    if (!Self->IsA(Class)) {
      //VObject::VMDumpCallStack();
      VPackage::InternalFatalError(va("object of class `%s` is not a subclass of `%s` for method `%s`", Self->GetClass()->GetName(), Class->GetName(), MethodName));
    }
  }
  if (Method->VTableIndex < -1) VPackage::InternalFatalError(va("method `%s` in class `%s` wasn't postloaded", (MethodName ? MethodName : "<unnamed>"), (Class ? Class->GetName() : "<unnamed>")));
  if (Method->VTableIndex != -1) {
    return VObject::ExecuteFunction(Self->vtable[Method->VTableIndex]);
  } else {
    return VObject::ExecuteFunction(Method);
  }
}


//==========================================================================
//
//  VMethodProxy::ExecuteNoCheck
//
//  this doesn't check is `Self` isa `Class`
//
//==========================================================================
VFuncRes VMethodProxy::ExecuteNoCheck (VObject *Self) {
  ResolveChecked(Self);
  if (Method->VTableIndex < -1) VPackage::InternalFatalError(va("method `%s` in class `%s` wasn't postloaded", (MethodName ? MethodName : "<unnamed>"), (Class ? Class->GetName() : "<unnamed>")));
  if (Method->VTableIndex != -1) {
    vassert(Self);
    return VObject::ExecuteFunction(Self->vtable[Method->VTableIndex]);
  } else {
    return VObject::ExecuteFunction(Method);
  }
}


//==========================================================================
//
//  VMethodProxy::ExecuteStatic
//
//==========================================================================
VFuncRes VMethodProxy::ExecuteStatic (VClass *aClass) {
  ResolveStaticChecked(aClass);
  vassert(Method->Flags&FUNC_Static);
  if (Method->VTableIndex < -1) VPackage::InternalFatalError(va("method `%s` in class `%s` wasn't postloaded", (MethodName ? MethodName : "<unnamed>"), (Class ? Class->GetName() : "<unnamed>")));
  return VObject::ExecuteFunction(Method);
}



//==========================================================================
//
//  VObject::VObject
//
//==========================================================================
VObject::VObject () noexcept : Index(-1), UniqueId(0) {
}


//==========================================================================
//
//  VObject::PostCtor
//
//==========================================================================
void VObject::PostCtor () {
  //fprintf(stderr, "VObject::PostCtor(%p): '%s'\n", this, GetClass()->GetName());
}


//==========================================================================
//
//  VObject::~VObject
//
//==========================================================================
VObject::~VObject () {
  vassert(Index >= 0); // the thing that should be

  ConditionalDestroy();
  GObjObjects[Index] = nullptr;

  if (!GInGarbageCollection) {
    vassert(GNumDeleted > 0);
    vassert(ObjectFlags&VObjFlag_Destroyed);
    --GNumDeleted;
    --gcLastStats.markedDead;
    ObjectFlags |= VObjFlag_CleanupRef;
    vdgclogf("marked object(%u) #%d: %p (%s)", UniqueId, Index, this, GetClass()->GetName());
    //fprintf(stderr, "Cleaning up for `%s`\n", *this->GetClass()->Name);
    // no need to delete index from queues, next GC cycle will take care of that
    const int ilen = gObjFirstFree;
    VObject **goptr = GObjObjects.ptr();
    for (int i = 0; i < ilen; ++i, ++goptr) {
      VObject *obj = *goptr;
      if (!obj || (obj->ObjectFlags&VObjFlag_Destroyed)) continue;
      obj->GetClass()->CleanObject(obj);
    }
    ++gcLastStats.lastCollected;
    // cheap check, why not?
    while (gObjFirstFree > 0 && !GObjObjects[gObjFirstFree-1]) --gObjFirstFree;
  }
}


//==========================================================================
//
//  VObject::operator new
//
//==========================================================================
void *VObject::operator new (size_t) {
  //VPackage::InternalFatalError("do not use `new` on `VObject`");
  vassert(GNewObject);
  return GNewObject;
}


//==========================================================================
//
//  VObject::operator new
//
//==========================================================================
void *VObject::operator new (size_t, const char *, int) {
  //VPackage::InternalFatalError("do not use `new` on `VObject`");
  vassert(GNewObject);
  return GNewObject;
}


//==========================================================================
//
//  VObject::operator delete
//
//==========================================================================
void VObject::operator delete (void *Object) {
  Z_Free(Object);
}


//==========================================================================
//
//  VObject::operator delete
//
//==========================================================================
void VObject::operator delete (void *Object, const char *, int) {
  Z_Free(Object);
}


//==========================================================================
//
//  VObject::StaticInit
//
//==========================================================================
void VObject::StaticInit () {
  VMemberBase::StaticInit();
  memset(&gcLastStats, 0, sizeof(gcLastStats)); // easy way
}


//==========================================================================
//
//  VObject::StaticExit
//
//==========================================================================
void VObject::StaticExit () {
  VMemberBase::StaticExit();
}


//==========================================================================
//
//  VObject::StaticInitOptions
//
//==========================================================================
void VObject::StaticInitOptions (VParsedArgs &pargs) {
  pargs.RegisterFlagSet("-vc-dev-replacement", "!show replacement debug messages", &cliShowReplacementMessages);
  pargs.RegisterFlagSet("-vc-dev-loading", "!show various package loading messages", &cliShowLoadingMessages);
  #if defined(VC_GARBAGE_COLLECTOR_LOGS_BASE)
  pargs.RegisterFlagSet("-vc-dev-gc", "!debug GC", &cliShowGCMessages);
  #endif
  pargs.RegisterFlagSet("-vc-io-debug", "!show debug info from serialiser", &cliShowIODebugMessages);
  pargs.RegisterFlagSet("-vc-dev-dump-name-tables", "!debug dump of name maps", &cliDumpNameTables);
  pargs.RegisterFlagSet("-vc-all-errors-are-fatal", "!abort the engine instead of only host server on errors (do not use)", &cliAllErrorsAreFatal);

  pargs.RegisterFlagSet("-vc-show-package-loading", "!log loaded packages", &cliShowPackageLoading);
  pargs.RegisterFlagReset("-vc-no-show-package-loading", "!do not log loaded packages", &cliShowPackageLoading);

  pargs.RegisterFlagSet("-vc-show-undefined-builtins", "!show undefined builtins", &cliShowUndefinedBuiltins);
  pargs.RegisterFlagReset("-vc-no-show-undefined-builtins", "!do not show undefined builtins", &cliShowUndefinedBuiltins);

  pargs.RegisterFlagSet("-vc-case-sensitive-locals", "!case-sensitive locals", &cliCaseSensitiveLocals);
  pargs.RegisterFlagReset("-vc-case-insensitive-locals", "!case-insensitive locals", &cliCaseSensitiveLocals);

  pargs.RegisterFlagSet("-vc-case-sensitive-fields", "!case-sensitive locals", &cliCaseSensitiveFields);
  pargs.RegisterFlagReset("-vc-case-insensitive-fields", "!case-insensitive locals", &cliCaseSensitiveFields);

  pargs.RegisterFlagSet("-vc-lax-override", "!allow omiting `override` keyword for methods", &VMemberBase::optDeprecatedLaxOverride);
  pargs.RegisterFlagSet("-vc-lax-states", "!missing actor state is not an error", &VMemberBase::optDeprecatedLaxStates);

  pargs.RegisterFlagSet("-vc-allow-unsafe", "!allow unsafe VC operations", &VMemberBase::unsafeCodeAllowed);
  pargs.RegisterFlagReset("-vc-disable-unsafe", "!do not allow unsafe VC operations", &VMemberBase::unsafeCodeAllowed);

  pargs.RegisterFlagSet("-vc-warn-unsafe", "!warn about unsafe VC operations", &VMemberBase::unsafeCodeWarning);
  pargs.RegisterFlagReset("-vc-no-warn-unsafe", "!do not warn about unsafe VC operations", &VMemberBase::unsafeCodeWarning);

  pargs.RegisterFlagSet("-vc-legacy-korax", "!Scattered Evil compatibility option", &VMemberBase::koraxCompatibility);
  pargs.RegisterFlagReset("-vc-legacy-korax-no-warnings", "!Scattered Evil compatibility option", &VMemberBase::koraxCompatibilityWarnings);

  pargs.RegisterFlagSet("-vc-gc-debug", "!show debug GC messages", &VObject::GCDebugMessagesAllowed);

  pargs.RegisterCallback("-vc-asm-dump-methods", "!must be followed by method names; shows disasm", [] (VArgs &args, int idx) -> int {
    for (++idx; !VParsedArgs::IsArgBreaker(args, idx); ++idx) {
      VStr mn = args[idx];
      if (!mn.isEmpty()) cliAsmDumpMethods.put(mn, true);
    }
    return idx;
  });

  pargs.RegisterFlagSet("-vc-silence-warnings", "!suppress most warnings", &vcWarningsSilenced);

  pargs.RegisterFlagSet("-vc-virtualise-decorate", "!virtualise all `[decorate]` methods and properties", &cliVirtualiseDecorateMethods);
}


//==========================================================================
//
//  VObject::StaticSpawnObject
//
//==========================================================================
VObject *VObject::StaticSpawnObject (VClass *AClass, bool skipReplacement) {
  vassert(AClass);

  VObject *Obj = nullptr;
  try {
    // actually, spawn a replacement
    if (!skipReplacement) {
      VClass *newclass = AClass->GetReplacement();
      if (newclass && newclass->IsChildOf(AClass)) AClass = newclass;
    }

    // allocate memory
    Obj = (VObject *)Z_Calloc(AClass->ClassSize);

    // find native class
    VClass *NativeClass = AClass;
    while (NativeClass != nullptr && !(NativeClass->ObjectFlags&CLASSOF_Native)) {
      NativeClass = NativeClass->GetSuperClass();
    }
    vassert(NativeClass);

    // call constructor of the native class to set up C++ virtual table
    GNewObject = Obj;
    NativeClass->ClassConstructor();

    // copy values from the default object
    vassert(AClass->Defaults);
    //GLog.Logf(NAME_Debug, "000: INITIALIZING fields of `%s`", AClass->GetName());
    AClass->DeepCopyObject((vuint8 *)Obj, AClass->Defaults);
    //GLog.Logf(NAME_Debug, "001: DONE INITIALIZING fields of `%s`", AClass->GetName());

    // set up object fields
    Obj->Class = AClass;
    Obj->vtable = AClass->ClassVTable;
  } catch (...) {
    Z_Free(Obj);
    GNewObject = nullptr;
    throw;
  }
  GNewObject = nullptr;
  vassert(Obj);

  // register in pool
  // this sets `Index` and `UniqueId`
  Obj->Register();

  try {
    // postinit
    Obj->PostCtor();
  } catch (...) {
    VPackage::InternalFatalError(va("PostCtor for class `%s` aborted", AClass->GetName()));
  }

  // we're done
  return Obj;
}


//==========================================================================
//
//  VObject::Register
//
//==========================================================================
void VObject::Register () {
  UniqueId = ++gLastUsedUniqueId;
  if (gLastUsedUniqueId == 0) gLastUsedUniqueId = 1;
  if (gObjFirstFree < GObjObjects.length()) {
    Index = gObjFirstFree;
    GObjObjects[gObjFirstFree++] = this;
    gcLastStats.firstFree = gObjFirstFree;
  } else {
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
    vassert(gObjFirstFree == GObjObjects.length());
#endif
    Index = GObjObjects.append(this);
    gObjFirstFree = Index+1;
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
    vassert(gObjFirstFree == GObjObjects.length());
#endif
    gcLastStats.firstFree = gObjFirstFree;
    gcLastStats.poolSize = gObjFirstFree;
    gcLastStats.poolAllocated = GObjObjects.NumAllocated();
  }
  vdgclogf("created object(%u) #%d: %p (%s)", UniqueId, Index, this, GetClass()->GetName());
  ++gcLastStats.alive;
  IncrementInstanceCounters();
}


//==========================================================================
//
//  VObject::SetFlags
//
//==========================================================================
void VObject::SetFlags (vuint32 NewFlags) noexcept {
  if ((NewFlags&VObjFlag_Destroyed) && !(ObjectFlags&VObjFlag_Destroyed)) {
    // new dead object
    // no need to remove from delayed deletion list, GC cycle will take care of that
    // set "cleanup ref" flag here, 'cause why not?
    NewFlags |= VObjFlag_CleanupRef;
    ++GNumDeleted;
    ++gcLastStats.markedDead;
    vdgclogf("marked object(%u) #%d: %p (%s)", UniqueId, Index, this, GetClass()->GetName());
  } else if (VObject::standaloneExecutor) {
    if ((NewFlags&VObjFlag_DelayedDestroy) && !(ObjectFlags&VObjFlag_DelayedDestroy)) {
      // "delayed destroy" flag is set, put it into delayed list
      gDelayDeadObjects.push(Index);
      ++GNumDeleted;
    }
  }
  ObjectFlags |= NewFlags;
}


//==========================================================================
//
//  VObject::ConditionalDestroy
//
//==========================================================================
void VObject::ConditionalDestroy () noexcept {
  if (!(ObjectFlags&VObjFlag_Destroyed)) {
    SetFlags(VObjFlag_Destroyed);
    DecrementInstanceCounters();
    Destroy();
  }
}


//==========================================================================
//
//  VObject::Destroy
//
//==========================================================================
void VObject::Destroy () {
  Class->DestructObject(this);
  if (!(ObjectFlags&VObjFlag_Destroyed)) {
    DecrementInstanceCounters();
    SetFlags(VObjFlag_Destroyed);
  }
  vdgclogf("destroyed object(%u) #%d: %p (%s)", UniqueId, Index, this, GetClass()->GetName());
}


//==========================================================================
//
//  VObject::ClearReferences
//
//==========================================================================
void VObject::ClearReferences () {
  GetClass()->CleanObject(this);
}


//==========================================================================
//
//  VObject::MinimiseUniqueId
//
//  this can be called on map unloading, for example,
//  to combat UniqueId overflow (somewhat)
//
//==========================================================================
void VObject::MinimiseUniqueId () noexcept {
  // loop over all objects (INCLUDING dying ones), determine first free unique id
  vuint32 maxuid = 0;
  for (VObject *obj : GObjObjects) {
    if (obj) maxuid = max2(maxuid, obj->UniqueId);
  }
  gLastUsedUniqueId = maxuid;
}


//==========================================================================
//
//  VObject::GetCurrentUniqueId
//
//  can be used for statistics
//
//==========================================================================
vuint32 VObject::GetCurrentUniqueId () noexcept {
  return gLastUsedUniqueId;
}


//==========================================================================
//
//  VObject::CollectGarbage
//
//==========================================================================
void VObject::CollectGarbage (bool destroyDelayed) {
  if (GInGarbageCollection) return; // recursive calls are not allowed

  if (!VObject::standaloneExecutor) destroyDelayed = false; // k8vavoom engine requires this

  if (!GNumDeleted && !destroyDelayed) return;
  vassert(GNumDeleted >= 0);

  GInGarbageCollection = true;

  vdgclogf("collecting garbage");

  // destroy all delayed-destroy objects
  if (destroyDelayed) {
    while (gDelayDeadObjects.length()) {
      int idx = gDelayDeadObjects.popValue();
      vassert(idx >= 0 && idx < gObjFirstFree);
      VObject *obj = GObjObjects[idx];
      if (!obj) continue;
      if ((obj->ObjectFlags&(VObjFlag_Destroyed|VObjFlag_DelayedDestroy)) == VObjFlag_DelayedDestroy) {
        obj->ConditionalDestroy();
      }
    }
  }

  // no need to mark objects to be cleaned, `VObjFlag_CleanupRef` was set in `SetFlag()`
  int alive = 0, bodycount = 0;
  double lasttime = -Sys_Time();

  const int ilen = gObjFirstFree;
  VObject **goptr = GObjObjects.ptr();

  // we can do a trick here: instead of simply walking the array, we can do
  // both object destroying, and list compaction.
  // let's put a finger on a last used element in the array.
  // on each iteration, check if we hit a dead object, and if we did, delete it.
  // now check if the current slot is empty. if it is not, continue iteration.
  // yet if the current slot is not empty, take object from the last used slot, and
  // move it to the current slot; then check the current slot again. also,
  // move finger to the previous non-empty slot.
  // invariant: when iteration position and finger met, it means that we hit
  // the last slot, so we can stop after processing it.
  //
  // we can do this trick 'cause internal object indicies doesn't matter, and
  // object ordering doesn't matter too. if we will call GC on each frame, it
  // means that our "available slot list" is always empty, and we have no "holes"
  // in slot list.
  //
  // this is slightly complicated by the fact that we cannot really destroy objects
  // on our way (as `ClearReferences()` will check their "dead" flag). so instead of
  // destroying dead objects right away, we'll use finger to point to only alive
  // objects, and swap dead and alive objects. this way all dead objects will
  // be moved to the end of the slot area (with possible gap between dead and alive
  // objects, but it doesn't matter).

  // there is no reason to use old non-compacting collector, so i removed it
  //if (gc_use_compacting_collector)
  {
    // move finger to the first alive object
    int finger = /*gObjFirstFree*/ilen-1;
    while (finger >= 0) {
      VObject *fgobj = goptr[finger];
      if (fgobj && (fgobj->ObjectFlags&VObjFlag_Destroyed) == 0) break;
      --finger;
    }

    // sanity check
    vassert(finger < 0 || (goptr[finger] && (goptr[finger]->ObjectFlags&VObjFlag_Destroyed) == 0));

#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
    fprintf(stderr, "ilen=%d; finger=%d\n", ilen, finger);
#endif

    // now finger points to the last alive object; start iteration
    int itpos = 0;
    while (itpos <= finger) {
      VObject *obj = goptr[itpos];
      if (!obj) {
        // free slot, move alive object here
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        const int swp = finger;
#endif
        obj = (goptr[itpos] = goptr[finger]);
        obj->Index = itpos;
        goptr[finger] = nullptr; // it is moved
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
        vassert(obj && (obj->ObjectFlags&VObjFlag_Destroyed) == 0);
#endif
        // move finger
        --finger; // anyway
        while (finger > itpos) {
          VObject *fgobj = goptr[finger];
          if (fgobj && (fgobj->ObjectFlags&VObjFlag_Destroyed) == 0) break;
          --finger;
        }
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        fprintf(stderr, "  hit free slot %d; moved object %d; new finger is %d\n", itpos, swp, finger);
#endif
      } else if ((obj->ObjectFlags&VObjFlag_Destroyed) != 0) {
        // dead object, swap with alive object
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        const int swp = finger;
#endif
        VObject *liveobj = goptr[finger];
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
        vassert(obj->Index == itpos);
        vassert(liveobj && liveobj->Index == finger && (liveobj->ObjectFlags&VObjFlag_Destroyed) == 0);
#endif
        obj->Index = finger;
        liveobj->Index = itpos;
        goptr[finger] = obj;
        obj = (goptr[itpos] = liveobj);
        vassert(obj && (obj->ObjectFlags&VObjFlag_Destroyed) == 0);
        // move finger
        --finger; // anyway
        while (finger > itpos) {
          VObject *fgobj = goptr[finger];
          if (fgobj && (fgobj->ObjectFlags&VObjFlag_Destroyed) == 0) break;
          --finger;
        }
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        fprintf(stderr, "  swapped slots %d and %d; new finger is %d\n", itpos, swp, finger);
#endif
      }
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
      vassert(obj && (obj->ObjectFlags&VObjFlag_Destroyed) == 0 && obj->Index == itpos);
#endif
      // we have alive object, clear references
      obj->ClearReferences();
      ++itpos; // move to the next object
    }

#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
    fprintf(stderr, " done; itpos=%d; ilen=%d\n", itpos, ilen);
#endif
    // update alive count: it is equal to itpos, as we compacted slot storage
    alive = itpos;
    // update last free position; we cached it, so it is safe
    gObjFirstFree = itpos;

    // use itpos to delete dead objects
    while (itpos < ilen) {
      VObject *obj = goptr[itpos];
      if (obj) {
#ifdef VC_GARBAGE_COLLECTOR_COMPACTING_DEBUG
        fprintf(stderr, "  killing object #%d of %d (dead=%d) (%s)\n", itpos, ilen-1, (obj->ObjectFlags&VObjFlag_Destroyed ? 1 : 0), *obj->GetClass()->Name);
#endif
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
        vassert(obj->Index == itpos && (obj->ObjectFlags&VObjFlag_Destroyed) != 0);
#endif
        ++bodycount;
        delete obj;
#ifdef VC_GARBAGE_COLLECTOR_CHECKS
        vassert(goptr[itpos] == nullptr); // sanity check
#endif
      }
      ++itpos;
    }
  } // gc complete

  gcLastStats.lastCollectTime = Sys_Time();
  lasttime += gcLastStats.lastCollectTime;

  GNumDeleted = 0;

  // shring object pool, why not?
  if (GObjObjects.length() > 8192 && gObjFirstFree+8192 < GObjObjects.length()/2) {
    GObjObjects.setLength(gObjFirstFree+8192);
  }

  // update GC stats
  gcLastStats.alive = alive;
  if (bodycount) {
    gcLastStats.lastCollected = bodycount;
    gcLastStats.lastCollectDuration = lasttime;
  }
  gcLastStats.poolSize = GObjObjects.length();
  gcLastStats.poolAllocated = GObjObjects.NumAllocated();
  gcLastStats.firstFree = gObjFirstFree;

  vdgclogf("garbage collection complete in %d msecs; %d objects deleted, %d objects live, %d of %d array slots used; firstfree=%d",
    (int)(gcLastStats.lastCollectDuration*1000), gcLastStats.lastCollected, gcLastStats.alive, gcLastStats.poolSize, gcLastStats.poolAllocated, gObjFirstFree);

  if (GGCMessagesAllowed && bodycount) {
    const char *msg = va("GC: %d objects deleted, %d objects left; array:[%d/%d]; firstfree=%d", bodycount, alive, GObjObjects.length(), GObjObjects.NumAllocated(), gObjFirstFree);
    GLog.Log(msg);
  }

  GInGarbageCollection = false;
}


//==========================================================================
//
//  VObject::GetObjectsCount
//
//==========================================================================
int VObject::GetObjectsCount () noexcept {
  return GObjObjects.length();
}


//==========================================================================
//
//  VObject::GetIndexObject
//
//==========================================================================
VObject *VObject::GetIndexObject (int Index) noexcept {
  //vassert(Index >= 0 && Index < GObjObjects.length());
  return (Index >= 0 && Index < GObjObjects.length() ? GObjObjects.ptr()[Index] : nullptr);
}


//==========================================================================
//
//  VObject::SerialiseFields
//
//  this serialises object fields
//
//==========================================================================
void VObject::SerialiseFields (VStream &Strm) {
  bool debugDump = cliShowIODebugMessages;
  //GetClass()->SerialiseObject(Strm, this);
  if (Strm.IsLoading()) {
    // reading
    // read field count
    vint32 fldcount = -1;
    Strm << STRM_INDEX(fldcount);
    if (fldcount < 0) VPackage::IOError(va("invalid number of saved fields in class `%s` (%d)", GetClass()->GetName(), fldcount));
    if (fldcount == 0) return; // nothing to do
    // build field list to speedup loading
    struct FldInfo {
      VField *fld;
      bool seen;
    };
    TMapNC<VName, FldInfo> fldmap;
    for (VClass *cls = GetClass(); cls; cls = cls->GetSuperClass()) {
      for (VField *fld = cls->Fields; fld; fld = fld->Next) {
        if (fld->Flags&(FIELD_Native|FIELD_Transient)) continue;
        if (fld->Name == NAME_None) continue;
        if (fldmap.has(fld->Name)) {
          //VPackage::IOError(va("duplicate field `%s` in class `%s`", *fld->Name, GetClass()->GetName()));
          VClass *prevfld = nullptr;
          for (VClass *xcls = GetClass(); xcls && xcls != cls && !prevfld; xcls = xcls->GetSuperClass()) {
            for (VField *xfld = xcls->Fields; xfld; xfld = xfld->Next) {
              if (xfld->Name == fld->Name) {
                prevfld = xcls;
                break;
              }
            }
          }
          if (prevfld) {
            GLog.WriteLine(NAME_Warning, "duplicate field `%s` in class `%s`", *fld->Name, GetClass()->GetName());
            GLog.Logf(NAME_Warning, "  previous field at %s", *prevfld->Loc.toStringNoCol());
          } else {
            GLog.WriteLine(NAME_Warning, "duplicate field `%s` in class `%s`", *fld->Name, GetClass()->GetName());
          }
          GLog.Logf(NAME_Warning, "  field at %s", *fld->Loc.toStringNoCol());
          // do not load duplicate
        } else {
          FldInfo nfo;
          nfo.fld = fld;
          nfo.seen = false;
          fldmap.put(fld->Name, nfo);
        }
      }
    }
    // now load fields
    if (debugDump) GLog.WriteLine("VC I/O: loading %d (of %d) fields of class `%s`...", fldcount, fldmap.length(), GetClass()->GetName());
    while (fldcount--) {
      VName fldname = NAME_None;
      Strm << fldname;
      if (debugDump) GLog.WriteLine("VC I/O:   field '%s'...", *fldname);
      auto fpp = fldmap.get(fldname);
      if (!fpp) {
        GLog.WriteLine(NAME_Warning, "saved field `%s` not found in class `%s`, value ignored", *fldname, GetClass()->GetName());
        VField::SkipSerialisedValue(Strm);
      } else {
        if (fpp->seen) {
          GLog.WriteLine(NAME_Warning, "duplicate saved field `%s` in class `%s`", *fldname, GetClass()->GetName());
        }
        fpp->seen = true;
        if (debugDump) GLog.WriteLine("VC I/O: loading field `%s` of class `%s`",  *fldname, GetClass()->GetName());
        VField *fld = fpp->fld;
        VField::SerialiseFieldValue(Strm, (vuint8 *)this+fld->Ofs, fld->Type);
      }
    }
    // show missing fields
    for (auto fit = fldmap.first(); fit; ++fit) {
      if (!fit.getValue().seen) {
        VName fldname = fit.getKey();
        GLog.WriteLine(NAME_Warning, "field `%s` is missing in saved data for class `%s`", *fldname, GetClass()->GetName());
      }
    }
  } else {
    // writing
    // count fields, collect them into array
    // serialise fields
    TMapNC<VName, bool> fldseen;
    TArray<VField *> fldlist;
    if (debugDump) GLog.WriteLine("VC I/O: scanning fields of class `%s`...", GetClass()->GetName());
    for (VClass *cls = GetClass(); cls; cls = cls->GetSuperClass()) {
      for (VField *fld = cls->Fields; fld; fld = fld->Next) {
        if (fld->Flags&(FIELD_Native|FIELD_Transient)) continue;
        if (fld->Name == NAME_None) continue;
        if (fldseen.put(fld->Name, true)) {
          //GLog.WriteLine(NAME_Warning, "duplicate field `%s` in class `%s`", *fld->Name, GetClass()->GetName());
          VField *prevfld = nullptr;
          for (VClass *xcls = GetClass(); xcls && xcls != cls && !prevfld; xcls = xcls->GetSuperClass()) {
            for (VField *xfld = xcls->Fields; xfld; xfld = xfld->Next) {
              if (xfld->Name == fld->Name) {
                prevfld = xfld;
                break;
              }
            }
          }
          if (prevfld) {
            GLog.WriteLine(NAME_Warning, "duplicate field `%s` in class `%s`", *fld->Name, GetClass()->GetName());
            GLog.Logf(NAME_Warning, "  previous field at %s", *prevfld->Loc.toStringNoCol());
          } else {
            GLog.WriteLine(NAME_Warning, "duplicate field `%s` in class `%s`", *fld->Name, GetClass()->GetName());
          }
          GLog.Logf(NAME_Warning, "  field at %s", *fld->Loc.toStringNoCol());
          // do not save duplicate
        } else {
          if (debugDump) GLog.WriteLine("VC I/O:   found field `%s`",  *fld->Name);
          fldlist.append(fld);
        }
      }
    }
    // now write all fields in backwards order, so they'll appear in natural order in stream
    vint32 fldcount = fldlist.length();
    if (debugDump) GLog.WriteLine("VC I/O: writing %d fields of class `%s`...", fldcount, GetClass()->GetName());
    Strm << STRM_INDEX(fldcount);
    for (int f = fldlist.length()-1; f >= 0; --f) {
      VField *fld = fldlist[f];
      if (debugDump) GLog.WriteLine("VC I/O:   writing field `%s` of type `%s`",  *fld->Name, *fld->Type.GetName());
      Strm << fld->Name;
      VField::SerialiseFieldValue(Strm, (vuint8 *)this+fld->Ofs, fld->Type);
    }
  }
}


//==========================================================================
//
//  VObject::SerialiseOther
//
//  this serialises other object internal data
//
//==========================================================================
void VObject::SerialiseOther (VStream &/*Strm*/) {
}


//==========================================================================
//
//  VObject::Serialise
//
//  this calls field serialisation, then other serialisation
//
//==========================================================================
void VObject::Serialise (VStream &strm) {
  if (strm.IsLoading()) {
    // reading
    VName clsname = NAME_None;
    strm << clsname;
    if (strm.IsError()) VPackage::IOError(va("error reading object of class `%s`", GetClass()->GetName()));
    if (clsname == NAME_None) VPackage::IOError(va("cannot load object of `none` class"));
    // translate name
    auto tnp = IOClassNameTranslation.get(clsname);
    if (tnp) {
      clsname = *tnp;
      if (clsname == NAME_None) VPackage::IOError(va("cannot load translated object of `none` class"));
    } else {
      for (auto &&cb : ClassNameTranslationCBList) {
        VName newClassName = cb(this, clsname);
        if (newClassName != NAME_None && newClassName != clsname) {
          clsname = newClassName;
          break;
        }
      }
    }
    VClass *cls = VClass::FindClass(*clsname);
    if (!cls) {
      // can we skip it?
      bool skipit = false;
      for (auto &&cb : CanSkipReadingClassCBList) {
        if (cb(this, clsname)) { skipit = true; break; }
      }
      if (skipit) {
        // get data size
        GLog.Logf(NAME_Warning, "I/O: skipping '%s' in '%s' (this may, or may not work)", *clsname, GetClass()->GetName());
        vint32 size;
        strm << size;
        if (size < 1) VPackage::IOError(va("error reading object of class `%s` (invalid size: %d)", *clsname, size));
        auto endpos = strm.Tell()+size;
        strm.Seek(endpos);
        return;
      }
      VPackage::IOError(va("cannot load object of unknown `%s` class (%s)", *clsname, GetClass()->GetName()));
    }
    //if (!IsA(cls)) VPackage::IOError(va("cannot load object of class `%s` class (not a subclass of `%s`)", GetClass()->GetName(), *clsname));
    if (GetClass() != cls) VPackage::IOError(va("cannot load object of class `%s` (expected class `%s`)", GetClass()->GetName(), *clsname));
    // skip data size
    vint32 size;
    strm << size;
    if (size < 1) VPackage::IOError(va("error reading object of class `%s` (invalid size: %d)", *clsname, size));
    auto endpos = strm.Tell()+size;
    // read flags
    vint32 flg;
    strm << STRM_INDEX(flg);
    if (strm.IsError()) VPackage::IOError(va("error reading object of class `%s`", *clsname));
    if (flg) SetFlags(flg);
    if (ObjectFlags&VObjFlag_Destroyed) {
      // no need to read anything, just skip it all
      if (strm.Tell() > endpos) VPackage::IOError(va("error reading object of class `%s` (invalid data size)", *clsname));
      strm.Seek(endpos);
      if (strm.IsError()) VPackage::IOError(va("error reading object of class `%s`", *clsname));
      return;
    }
    // read fields
    SerialiseFields(strm);
    if (strm.IsError()) VPackage::IOError(va("error reading object of class `%s`", *clsname));
    // read other data
    SerialiseOther(strm);
    if (strm.IsError()) VPackage::IOError(va("error reading object of class `%s`", *clsname));
    // check if all data was read
    if (strm.Tell() != endpos) VPackage::IOError(va("error reading object of class `%s` (not all data read)", *clsname));
  } else {
    // writing
    VName clsname = GetClass()->Name;
    strm << clsname;
    // write dummy data size (will be fixed later)
    auto szpos = strm.Tell();
    vint32 size = 0;
    strm << size;
    // write flags
    vint32 flg = ObjectFlags;
    strm << STRM_INDEX(flg);
    // is object dead? don't write data of dead objects
    if ((ObjectFlags&VObjFlag_Destroyed) == 0) {
      // write fields
      SerialiseFields(strm);
      if (strm.IsError()) return;
      // write other data
      SerialiseOther(strm);
      if (strm.IsError()) return;
    }
    // fix data size
    auto cpos = strm.Tell();
    size = cpos-(szpos+(int)sizeof(vint32));
    strm.Seek(szpos);
    strm << size;
    strm.Seek(cpos);
  }
}


//==========================================================================
//
//  VObject::ExecuteNetMethod
//
//==========================================================================
bool VObject::ExecuteNetMethod (VMethod *func) {
  if (onExecuteNetMethodCB) return onExecuteNetMethodCB(this, func);
  return false;
}



//==========================================================================
//
//  VClassesIterator
//
//==========================================================================
class VClassesIterator : public VScriptIterator {
private:
  VClass *BaseClass;
  VClass **Out;
  int Index;

public:
  VClassesIterator (VClass *ABaseClass, VClass **AOut) : BaseClass(ABaseClass), Out(AOut), Index(0) {}

  virtual bool GetNext () override {
    while (Index < VMemberBase::GMembers.length()) {
      VMemberBase *Check = VMemberBase::GMembers[Index];
      ++Index;
      if (Check->MemberType == MEMBER_Class && ((VClass *)Check)->IsChildOf(BaseClass)) {
        *Out = (VClass *)Check;
        return true;
      }
    }
    *Out = nullptr;
    return false;
  }
};



//==========================================================================
//
//  VClassStatesIterator
//
//==========================================================================
class VClassStatesIterator : public VScriptIterator {
private:
  VState *curr;
  VState **out;

public:
  VClassStatesIterator (VClass *aclass, VState **aout) : curr(nullptr), out(aout) {
    if (aclass) curr = aclass->States;
  }

  virtual bool GetNext () override {
    *out = curr;
    if (curr) {
      curr = curr->Next;
      return true;
    }
    return false;
  }
};



//==========================================================================
//
//  VObjectsIterator
//
//==========================================================================
class VObjectsIterator : public VScriptIterator {
private:
  VClass *BaseClass;
  VObject **Out;
  int Index;

public:
  VObjectsIterator (VClass *ABaseClass, VObject **AOut) : BaseClass(ABaseClass), Out(AOut), Index(0) {}

  virtual bool GetNext () override {
    while (Index < VObject::GetObjectsCount()) {
      VObject *Check = VObject::GetIndexObject(Index);
      ++Index;
      if (Check && !Check->IsGoingToDie() && Check->IsA(BaseClass)) {
        *Out = Check;
        return true;
      }
    }
    *Out = nullptr;
    return false;
  }
};



//==========================================================================
//
//  VObject::NameFromVKey
//
//==========================================================================
VStr VObject::NameFromVKey (int vkey) noexcept {
  if (vkey >= K_N0 && vkey <= K_N9) return VStr(char(vkey));
  if (vkey >= K_a && vkey <= K_z) return VStr(char(vkey-32));
  switch (vkey) {
    case K_ESCAPE: return "ESCAPE";
    case K_ENTER: return "ENTER";
    case K_TAB: return "TAB";
    case K_BACKSPACE: return "BACKSPACE";

    case K_SPACE: return "SPACE";

    case K_UPARROW: return "UP";
    case K_LEFTARROW: return "LEFT";
    case K_RIGHTARROW: return "RIGHT";
    case K_DOWNARROW: return "DOWN";
    case K_INSERT: return "INSERT";
    case K_DELETE: return "DELETE";
    case K_HOME: return "HOME";
    case K_END: return "END";
    case K_PAGEUP: return "PGUP";
    case K_PAGEDOWN: return "PGDOWN";

    case K_PAD0: return "PAD0";
    case K_PAD1: return "PAD1";
    case K_PAD2: return "PAD2";
    case K_PAD3: return "PAD3";
    case K_PAD4: return "PAD4";
    case K_PAD5: return "PAD5";
    case K_PAD6: return "PAD6";
    case K_PAD7: return "PAD7";
    case K_PAD8: return "PAD8";
    case K_PAD9: return "PAD9";

    case K_NUMLOCK: return "NUM";
    case K_PADDIVIDE: return "PAD/";
    case K_PADMULTIPLE: return "PAD*";
    case K_PADMINUS: return "PAD-";
    case K_PADPLUS: return "PAD+";
    case K_PADENTER: return "PADENTER";
    case K_PADDOT: return "PAD.";

    case K_CAPSLOCK: return "CAPS";
    case K_BACKQUOTE: return "BACKQUOTE";

    case K_F1: return "F1";
    case K_F2: return "F2";
    case K_F3: return "F3";
    case K_F4: return "F4";
    case K_F5: return "F5";
    case K_F6: return "F6";
    case K_F7: return "F7";
    case K_F8: return "F8";
    case K_F9: return "F9";
    case K_F10: return "F10";
    case K_F11: return "F11";
    case K_F12: return "F12";

    case K_LSHIFT: return "LSHIFT";
    case K_RSHIFT: return "RSHIFT";
    case K_LCTRL: return "LCTRL";
    case K_RCTRL: return "RCTRL";
    case K_LALT: return "LALT";
    case K_RALT: return "RALT";

    case K_LWIN: return "LWIN";
    case K_RWIN: return "RWIN";
    case K_MENU: return "MENU";

    case K_PRINTSCRN: return "PSCRN";
    case K_SCROLLLOCK: return "SCROLL";
    case K_PAUSE: return "PAUSE";

    case K_MOUSE1: return "MOUSE1";
    case K_MOUSE2: return "MOUSE2";
    case K_MOUSE3: return "MOUSE3";
    case K_MWHEELUP: return "WHEEL_UP";
    case K_MWHEELDOWN: return "WHEEL_DOWN";

    case K_JOY1: return "JOY1";
    case K_JOY2: return "JOY2";
    case K_JOY3: return "JOY3";
    case K_JOY4: return "JOY4";
    case K_JOY5: return "JOY5";
    case K_JOY6: return "JOY6";
    case K_JOY7: return "JOY7";
    case K_JOY8: return "JOY8";
    case K_JOY9: return "JOY9";
    case K_JOY10: return "JOY10";
    case K_JOY11: return "JOY11";
    case K_JOY12: return "JOY12";
    case K_JOY13: return "JOY13";
    case K_JOY14: return "JOY14";
    case K_JOY15: return "JOY15";
    case K_JOY16: return "JOY16";

    case K_AXIS_LEFTX: return "AXIS_LEFTX";
    case K_AXIS_LEFTY: return "AXIS_LEFTY";
    case K_AXIS_RIGHTX: return "AXIS_RIGHTX";
    case K_AXIS_RIGHTY: return "AXIS_RIGHTY";
    case K_AXIS_TRIGGERLEFT: return "AXIS_TRIGGERLEFT";
    case K_AXIS_TRIGGERRIGHT: return "AXIS_TRIGGERRIGHT";

    case K_BUTTON_A: return "BUTTON_A";
    case K_BUTTON_B: return "BUTTON_B";
    case K_BUTTON_X: return "BUTTON_X";
    case K_BUTTON_Y: return "BUTTON_Y";
    case K_BUTTON_BACK: return "BUTTON_BACK";
    case K_BUTTON_GUIDE: return "BUTTON_GUIDE";
    case K_BUTTON_START: return "BUTTON_START";
    case K_BUTTON_LEFTSTICK: return "BUTTON_LSTICK";
    case K_BUTTON_RIGHTSTICK: return "BUTTON_RSTICK";
    case K_BUTTON_LEFTSHOULDER: return "LSHOULDER";
    case K_BUTTON_RIGHTSHOULDER: return "RSHOULDER";
    case K_BUTTON_DPAD_UP: return "DPAD_UP";
    case K_BUTTON_DPAD_DOWN: return "DPAD_DOWN";
    case K_BUTTON_DPAD_LEFT: return "DPAD_LEFT";
    case K_BUTTON_DPAD_RIGHT: return "DPAD_RIGHT";
    case K_BUTTON_TRIGGER_LEFT: return "LTRIGGER";
    case K_BUTTON_TRIGGER_RIGHT: return "RTRIGGER";
  }
  if (vkey > 32 && vkey < 127) return VStr(char(vkey));
  return VStr();
}


//==========================================================================
//
//  isPrefixedDigit
//
//  returns digit or -1
//  allow '_' after the prefix
//
//==========================================================================
static int isPrefixedDigit (const char *s, const char *pfx, int minpfxlen, int maxpfxlen) noexcept {
  if (!s || !s[0]) return -1;
  // check prefix
  if (pfx && pfx[0]) {
    if (!VStr::startsWithCI(s, pfx)) return -1;
    s += strlen(pfx);
  }
  // skip possible underscore or minus
  if (s[0] == '_' || s[0] == '-') ++s;
  // parse digit
  int res = 0;
  int len = 0;
  while (*s) {
    if (len > maxpfxlen) return -1; // too long
    int dig = VStr::digitInBase(*s, 10);
    if (dig < 0) return -1; // alas
    res = res*10+dig;
    ++s;
    ++len;
  }
  if (len < minpfxlen) return -1; // too short
  if (len > maxpfxlen) return -1; // just in case
  return res;
}


//==========================================================================
//
//  VObject::VKeyFromName
//
//==========================================================================
int VObject::VKeyFromName (VStr kn) noexcept {
  const int oldlen = kn.length();
  kn = kn.xstrip();
  if (oldlen > 0 && kn.isEmpty()) kn = " "; // alot of spaces are equal to one space

  if (kn.isEmpty()) return 0;

  if (kn.length() == 1) {
    vuint8 ch = (vuint8)kn[0];
    if (ch >= '0' && ch <= '9') return K_N0+(ch-'0');
    if (ch >= 'A' && ch <= 'Z') return K_a+(ch-'A');
    if (ch >= 'a' && ch <= 'z') return K_a+(ch-'a');
    if (ch == ' ') return K_SPACE;
    if (ch == '`' || ch == '~') return K_BACKQUOTE;
    if (ch == 27) return K_ESCAPE;
    if (ch == 13 || ch == 10) return K_ENTER;
    if (ch == 8) return K_BACKSPACE;
    if (ch == 9) return K_TAB;
    if (ch > 32 && ch < 127) return (int)ch;
  }

  int num = isPrefixedDigit(*kn, "PAD", 1, 1);
  if (num >= 0) {
    vassert(num < 10); // just in case
    return K_PAD0+num;
  }
  num = isPrefixedDigit(*kn, "kp", 1, 1);
  if (num >= 0) {
    vassert(num < 10); // just in case
    return K_PAD0+num;
  }

  num = isPrefixedDigit(*kn, "f", 1, 2);
  if (num >= 1 && num <= 12) {
    return K_F1+num-1;
  }

  if (kn.strEquCI("kp.") || kn.strEquCI("kp-.") || kn.strEquCI("kp_.")) return K_PADDOT;
  if (kn.strEquCI("kp+") || kn.strEquCI("kp-+") || kn.strEquCI("kp_+")) return K_PADPLUS;
  if (kn.strEquCI("kp-") || kn.strEquCI("kp--") || kn.strEquCI("kp_-")) return K_PADMINUS;
  if (kn.strEquCI("kp*") || kn.strEquCI("kp-*") || kn.strEquCI("kp_*")) return K_PADMULTIPLE;
  if (kn.strEquCI("kp/") || kn.strEquCI("kp-/") || kn.strEquCI("kp_/")) return K_PADDIVIDE;

  if (kn.strEquCI("PAD/") || kn.strEquCI("PAD-/") || kn.strEquCI("PAD_/")) return K_PADDIVIDE;
  if (kn.strEquCI("PAD*") || kn.strEquCI("PAD-*") || kn.strEquCI("PAD_*")) return K_PADMULTIPLE;
  if (kn.strEquCI("PAD-") || kn.strEquCI("PAD--") || kn.strEquCI("PAD_-")) return K_PADMINUS;
  if (kn.strEquCI("PAD+") || kn.strEquCI("PAD-+") || kn.strEquCI("PAD_+")) return K_PADPLUS;
  if (kn.strEquCI("PAD.") || kn.strEquCI("PAD-.") || kn.strEquCI("PAD_.")) return K_PADDOT;

  num = isPrefixedDigit(*kn, "mouse", 1, 1);
  if (num >= 1) {
    vassert(num < 10); // just in case
    return K_MOUSE1+num-1;
  }

  num = isPrefixedDigit(*kn, "joy", 1, 2);
  if (num >= 1 && num <= 16) {
    vassert(num < 10); // just in case
    return K_JOY1+num-1;
  }

  // remove all '-' and '_'
  for (;;) {
    int mp = kn.indexOf('-');
    if (mp >= 0) {
      kn = kn.left(mp)+kn.mid(mp+1, kn.length());
      continue;
    }
    mp = kn.indexOf('_');
    if (mp >= 0) {
      kn = kn.left(mp)+kn.mid(mp+1, kn.length());
      continue;
    }
    break;
  }

  if (kn.strEquCI("kpenter")) return K_PADENTER;

  if (kn.strEquCI("ESCAPE")) return K_ESCAPE;
  if (kn.strEquCI("ENTER")) return K_ENTER;
  if (kn.strEquCI("RETURN")) return K_ENTER;
  if (kn.strEquCI("TAB")) return K_TAB;
  if (kn.strEquCI("BACKSPACE")) return K_BACKSPACE;

  if (kn.strEquCI("SPACE")) return K_SPACE;

  if (kn.strEquCI("UPARROW") || kn.strEquCI("UP")) return K_UPARROW;
  if (kn.strEquCI("LEFTARROW") || kn.strEquCI("LEFT")) return K_LEFTARROW;
  if (kn.strEquCI("RIGHTARROW") || kn.strEquCI("RIGHT")) return K_RIGHTARROW;
  if (kn.strEquCI("DOWNARROW") || kn.strEquCI("DOWN")) return K_DOWNARROW;
  if (kn.strEquCI("INSERT") || kn.strEquCI("INS")) return K_INSERT;
  if (kn.strEquCI("DELETE") || kn.strEquCI("DEL")) return K_DELETE;
  if (kn.strEquCI("HOME")) return K_HOME;
  if (kn.strEquCI("END")) return K_END;
  if (kn.strEquCI("PAGEUP") || kn.strEquCI("PGUP")) return K_PAGEUP;
  if (kn.strEquCI("PAGEDOWN") || kn.strEquCI("PGDOWN") || kn.strEquCI("PGDN")) return K_PAGEDOWN;

  if (kn.strEquCI("NUMLOCK") || kn.strEquCI("NUM")) return K_NUMLOCK;
  if (kn.strEquCI("PADDIVIDE") || kn.strEquCI("PADDIV")) return K_PADDIVIDE;
  if (kn.strEquCI("PADMULTIPLE") || kn.strEquCI("PADMULT") || kn.strEquCI("PADMUL")) return K_PADMULTIPLE;
  if (kn.strEquCI("PADMINUS")) return K_PADMINUS;
  if (kn.strEquCI("PADPLUS")) return K_PADPLUS;
  if (kn.strEquCI("PADENTER")) return K_PADENTER;
  if (kn.strEquCI("PADDOT")) return K_PADDOT;

  if (kn.strEquCI("CAPSLOCK") || kn.strEquCI("CAPS")) return K_CAPSLOCK;
  if (kn.strEquCI("BACKQUOTE")) return K_BACKQUOTE;

  if (kn.strEquCI("LSHIFT")) return K_LSHIFT;
  if (kn.strEquCI("RSHIFT")) return K_RSHIFT;
  if (kn.strEquCI("LCTRL")) return K_LCTRL;
  if (kn.strEquCI("RCTRL")) return K_RCTRL;
  if (kn.strEquCI("LALT")) return K_LALT;
  if (kn.strEquCI("RALT")) return K_RALT;

  if (kn.strEquCI("LWIN")) return K_LWIN;
  if (kn.strEquCI("RWIN")) return K_RWIN;
  if (kn.strEquCI("MENU")) return K_MENU;

  if (kn.strEquCI("PRINTSCRN") || kn.strEquCI("PSCRN") || kn.strEquCI("PRINTSCREEN") ) return K_PRINTSCRN;
  if (kn.strEquCI("SCROLLLOCK") || kn.strEquCI("SCROLL")) return K_SCROLLLOCK;
  if (kn.strEquCI("PAUSE")) return K_PAUSE;

  if (kn.strEquCI("MWHEELUP") || kn.strEquCI("WHEELUP")) return K_MWHEELUP;
  if (kn.strEquCI("MWHEELDOWN") || kn.strEquCI("WHEELDOWN")) return K_MWHEELDOWN;

  if (kn.startsWithCI("AXIS")) {
    if (kn.strEquCI("AXISLEFTX")) return K_AXIS_LEFTX;
    if (kn.strEquCI("AXISLEFTY")) return K_AXIS_LEFTY;
    if (kn.strEquCI("AXISRIGHTX")) return K_AXIS_RIGHTX;
    if (kn.strEquCI("AXISRIGHTY")) return K_AXIS_RIGHTY;
    if (kn.strEquCI("AXISTRIGGERLEFT")) return K_AXIS_TRIGGERLEFT;
    if (kn.strEquCI("AXISTRIGGERRIGHT")) return K_AXIS_TRIGGERRIGHT;
  }

  if (kn.startsWithCI("BUTTON")) {
    if (kn.strEquCI("BUTTONA")) return K_BUTTON_A;
    if (kn.strEquCI("BUTTONB")) return K_BUTTON_B;
    if (kn.strEquCI("BUTTONX")) return K_BUTTON_X;
    if (kn.strEquCI("BUTTONY")) return K_BUTTON_Y;
    if (kn.strEquCI("BUTTONBACK")) return K_BUTTON_BACK;
    if (kn.strEquCI("BUTTONGUIDE")) return K_BUTTON_GUIDE;
    if (kn.strEquCI("BUTTONSTART")) return K_BUTTON_START;
    if (kn.strEquCI("BUTTONLEFTSTICK")) return K_BUTTON_LEFTSTICK;
    if (kn.strEquCI("BUTTONRIGHTSTICK")) return K_BUTTON_RIGHTSTICK;
    if (kn.strEquCI("BUTTONLEFTSHOULDER")) return K_BUTTON_LEFTSHOULDER;
    if (kn.strEquCI("BUTTONRIGHTSHOULDER")) return K_BUTTON_RIGHTSHOULDER;
    if (kn.strEquCI("BUTTONLSTICK")) return K_BUTTON_LEFTSTICK;
    if (kn.strEquCI("BUTTONRSTICK")) return K_BUTTON_RIGHTSTICK;
  }

  if (kn.strEquCI("LEFTSHOULDER") || kn.strEquCI("LSHOULDER")) return K_BUTTON_LEFTSHOULDER;
  if (kn.strEquCI("RIGHTSHOULDER") || kn.strEquCI("RSHOULDER")) return K_BUTTON_RIGHTSHOULDER;

  if (kn.strEquCI("TRIGGERLEFT") || kn.strEquCI("LTRIGGER")) return K_BUTTON_TRIGGER_LEFT;
  if (kn.strEquCI("TRIGGERRIGHT") || kn.strEquCI("RTRIGGER")) return K_BUTTON_TRIGGER_RIGHT;

  if (kn.startsWithCI("DPAD")) {
    if (kn.strEquCI("DPADUP")) return K_BUTTON_DPAD_UP;
    if (kn.strEquCI("DPADDOWN")) return K_BUTTON_DPAD_DOWN;
    if (kn.strEquCI("DPADLEFT")) return K_BUTTON_DPAD_LEFT;
    if (kn.strEquCI("DPADRIGHT")) return K_BUTTON_DPAD_RIGHT;
  }

  return 0;
}

#include "vc_object_common.cpp"
#include "vc_object_evqueue.cpp"
