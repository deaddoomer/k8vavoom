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
//**
//**  k8vavoom object base class.
//**
//**************************************************************************

// define private default constructor
#define NO_DEFAULT_CONSTRUCTOR(cls) \
  protected: cls() {} public:

// declare the base VObject class
#define DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags) \
public: \
  /* identification */ \
  enum {StaticClassFlags = TStaticFlags|CLASS_Native}; \
  private: static VClass PrivateStaticClass; public: \
  typedef TSuperClass Super; \
  typedef TClass ThisClass; \
  static VClass *StaticClass() { return &PrivateStaticClass; }

// declare a concrete class
#define DECLARE_CLASS(TClass, TSuperClass, TStaticFlags) \
  DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags|CLASS_Native) \
  virtual ~TClass() { ConditionalDestroy(); } \
  friend inline VStream &operator<<(VStream &Strm, TClass *&Obj) { return Strm << *(VObject **)&Obj; } \
  static void InternalConstructor() { new TClass(); }

// declare an abstract class
#define DECLARE_ABSTRACT_CLASS(TClass, TSuperClass, TStaticFlags) \
  DECLARE_BASE_CLASS(TClass, TSuperClass, TStaticFlags|CLASS_Abstract) \
  virtual ~TClass() { ConditionalDestroy(); } \
  friend inline VStream &operator<<(VStream &Strm, TClass *&Obj) { return Strm << *(VObject **)&Obj; }

// register a class at startup time
#define IMPLEMENT_CLASS(Pre,TClass) \
  VClass Pre##TClass::PrivateStaticClass \
  ( \
    EC_NativeConstructor, \
    sizeof(Pre##TClass), \
    Pre##TClass::StaticClassFlags, \
    Pre##TClass::Super::StaticClass(), \
    NAME_##TClass, \
    Pre##TClass::InternalConstructor \
  ); \
  VClass *autoclass##Pre##TClass = Pre##TClass::StaticClass();

// register a class at startup time
#define IMPLEMENT_CLASS_NAMED(TClass,XName) \
  VClass TClass::PrivateStaticClass \
  ( \
    EC_NativeConstructor, \
    sizeof(TClass), \
    TClass::StaticClassFlags, \
    TClass::Super::StaticClass(), \
    NAME_##XName, \
    TClass::InternalConstructor \
  ); \
  VClass *autoclass##TClass = TClass::StaticClass();

// use this to declare VavoomC native function in some object
#define DECLARE_FUNCTION(func) \
  static FBuiltinInfo funcinfo##func; \
  static void exec##func();

// use this to implement VavoomC native function in some object
#define IMPLEMENT_FUNCTION(TClass,Func) \
  FBuiltinInfo TClass::funcinfo##Func(#Func, TClass::StaticClass(), TClass::exec##Func); \
  void TClass::exec##Func()

// use this to implement VavoomC native function as free function (it will still belong to the given VC object)
#define IMPLEMENT_FREE_FUNCTION(TClass,Func) \
  static void vc_free_exec_##TClass_##Func(); \
  /*static*/ FBuiltinInfo vc_free_funcinfo_##TClass_##Func(#Func, TClass::StaticClass(), &vc_free_exec_##TClass_##Func); \
  static void vc_free_exec_##TClass_##Func()

// use this to implement VavoomC native function as free function (it will still belong to the given VC struct)
#define IMPLEMENT_FREE_STRUCT_FUNCTION(ClassName,StructName,Func) \
  static void vc_free_struct_exec_##ClassName_##StructName_##Func(); \
  /*static*/ FBuiltinInfo vc_free_funcinfo_##ClassName_##StructName_##Func(#Func, #ClassName, #StructName, &vc_free_struct_exec_##ClassName_##StructName_##Func); \
  static void vc_free_struct_exec_##ClassName_##StructName_##Func()


// ////////////////////////////////////////////////////////////////////////// //
// keys and buttons
enum {
  K_ESCAPE = 27,
  K_ENTER = 13,
  K_TAB = 9,
  K_BACKSPACE = 8,

  K_SPACE = 32,

  K_N0 = 48, K_N1, K_N2, K_N3, K_N4, K_N5, K_N6, K_N7, K_N8, K_N9,

  K_a = 97, K_b, K_c, K_d, K_e, K_f,  K_g, K_h, K_i, K_j, K_k, K_l,
  K_m, K_n, K_o, K_p, K_q, K_r, K_s, K_t, K_u, K_v, K_w, K_x, K_y, K_z,

  K_FIRST_CONTROL_KEYCODE = 0x80,

  K_UPARROW = 0x80, K_LEFTARROW, K_RIGHTARROW, K_DOWNARROW,
  K_INSERT, K_DELETE, K_HOME, K_END, K_PAGEUP, K_PAGEDOWN,

  K_PAD0, K_PAD1, K_PAD2, K_PAD3, K_PAD4, K_PAD5, K_PAD6, K_PAD7, K_PAD8, K_PAD9,

  K_NUMLOCK,
  K_PADDIVIDE, K_PADMULTIPLE,
  K_PADMINUS, K_PADPLUS,
  K_PADENTER, K_PADDOT,

  K_CAPSLOCK,
  K_BACKQUOTE,

  K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,

  K_LSHIFT, K_RSHIFT,
  K_LCTRL, K_RCTRL,
  K_LALT, K_RALT,

  K_LWIN, K_RWIN,
  K_MENU,

  K_PRINTSCRN,
  K_SCROLLLOCK,
  K_PAUSE,

  // mouse
  K_MOUSE1, // left
  K_MOUSE2, // right
  K_MOUSE3, // middle
  K_MOUSE4, // x1
  K_MOUSE5, // x2
  K_MOUSE6, K_MOUSE7, K_MOUSE8, K_MOUSE9,
  K_MWHEELUP, K_MWHEELDOWN,

  // joysticks
  K_JOY1, K_JOY2, K_JOY3, K_JOY4, K_JOY5, K_JOY6, K_JOY7, K_JOY8, K_JOY9,
  K_JOY10, K_JOY11, K_JOY12, K_JOY13, K_JOY14, K_JOY15, K_JOY16,

  // controllers
  K_AXIS_LEFTX, K_AXIS_LEFTY,
  K_AXIS_RIGHTX, K_AXIS_RIGHTY,
  K_AXIS_TRIGGERLEFT, K_AXIS_TRIGGERRIGHT,

  K_BUTTON_A, K_BUTTON_B,
  K_BUTTON_X, K_BUTTON_Y,
  K_BUTTON_BACK, K_BUTTON_GUIDE, K_BUTTON_START,
  K_BUTTON_LEFTSTICK, K_BUTTON_RIGHTSTICK,
  K_BUTTON_LEFTSHOULDER, K_BUTTON_RIGHTSHOULDER,
  K_BUTTON_DPAD_UP, K_BUTTON_DPAD_DOWN,
  K_BUTTON_DPAD_LEFT, K_BUTTON_DPAD_RIGHT,
  K_BUTTON_TRIGGER_LEFT, K_BUTTON_TRIGGER_RIGHT,

  // intervals
  K_MOUSE_FIRST = K_MOUSE1,
  K_MOUSE_LAST = K_MWHEELDOWN,

  K_MOUSE_BUTTON_FIRST = K_MOUSE1,
  K_MOUSE_BUTTON_LAST = K_MOUSE9,

  K_JOY_FIRST = K_JOY1,
  K_JOY_LAST = K_JOY16,

  K_CONTROLLER_FIRST = K_AXIS_LEFTX,
  K_CONTROLLER_LAST = K_BUTTON_TRIGGER_RIGHT,

  K_CONTROLLER_AXIS_FIRST = K_AXIS_LEFTX,
  K_CONTROLLER_AXIS_LAST = K_AXIS_TRIGGERRIGHT,

  K_CONTROLLER_BUTTON_FIRST = K_BUTTON_A,
  K_CONTROLLER_BUTTON_LAST = K_BUTTON_TRIGGER_RIGHT,

  K_KEYCODE_MAX,
};

static_assert(K_KEYCODE_MAX < 256, "too many keycodes");


// input event types
enum {
  ev_keydown,
  ev_keyup,
  ev_mouse, // "mouse moved" event, with deltas in `dx`/`dy`
  ev_joystick,
  ev_click, // click or doubleclick (see `clickcnt`); keycode is K_MOUSEn; `x` and `y` holds widget-local coords
  ev_enter, // mouse enters the widget; `x` and `y` holds widget-local coords
  ev_leave, // mouse leaves the widget; `x` and `y` holds widget-local coords
  // extended events for vcc_run
  ev_uimouse, // "mouse moved" event, with actual coords in `x`/`y` (widget-locals)
  ev_winfocus,
  // only for vccrun
  ev_timer, // data1: timer id
  ev_closequery, // may be send when window close request is received
  ev_quit, // send to stop current event loop
  // socket library
  ev_socket,
  // sent when resolution changed; no data
  ev_resolution,
  // this event will be broadcased to all widgets
  // it cannot be canceled or consumed, and all modifications will be reverted
  // note that is will be broadcasted to disabled and invisible widgets too
  ev_broadcast,
  // for neoUI library
  ev_neoui = 69,
  ev_user = 666,
};


enum {
  EFlag_Eaten = 1U<<0,
  EFlag_Cancelled = 1U<<1,
  EFlag_Bubbling = 1U<<2, // this event is "bubbling up"
};


enum {
  bCtrl = 1U<<0,
  bAlt = 1U<<1,
  bShift = 1U<<2,
  bHyper = 1U<<3,
  bLMB = 1U<<4,
  bMMB = 1U<<5,
  bRMB = 1U<<6,
  bCtrlLeft = 1U<<7,
  bAltLeft = 1U<<8,
  bShiftLeft = 1U<<9,
  bCtrlRight = 1U<<10,
  bAltRight = 1U<<11,
  bShiftRight = 1U<<12,
};


// event structure
struct event_t {
  vint32 type; // event type
  // unions are actually aliases
  union {
    vint32 data1; // keys / mouse / joystick buttons
    vint32 keycode; // ev_keyXXX
    vint32 focused; // ev_winfocus
    vint32 timerid; // ev_timer
    vint32 sockev; // ev_socket
    // for ev_mouse
    vint32 msx;
  };
  union {
    vint32 data2; // mouse / joystick x move
    vint32 dx; // ev_mouse
    vint32 x; // ev_uimouse; mouse button down/up
    vint32 sockid; // ev_socket
  };
  union {
    vint32 data3; // mouse / joystick y move
    vint32 dy; // ev_mouse
    vint32 y; // ev_uimouse; mouse button down/up
    vint32 sockdata; // ev_socket
  };
  union {
    vint32 data4;
    vint32 clickcnt; // 1 for click, 2 for doubleclick
    // for ev_mouse
    vint32 msy;
    // for ev_joystick
    vint32 joyidx; // 0 or 1
  };
  vuint32 flags; // EFlag_XXX
  VObject *obj;
  VObject *dest;
  vuint32 modflags;

  inline void clear () noexcept { memset((void *)this, 0, sizeof(event_t)); }

  inline bool isEaten () const noexcept { return !!(flags&EFlag_Eaten); }
  inline void setEaten () noexcept { flags |= EFlag_Eaten; }

  inline bool isCancelled () const noexcept { return !!(flags&EFlag_Cancelled); }
  inline void setCancelled () noexcept { flags |= EFlag_Cancelled; }

  inline bool isEatenOrCancelled () const noexcept { return !!(flags&(EFlag_Eaten|EFlag_Cancelled)); }

  inline bool isBubbling () const noexcept { return !!(flags&EFlag_Bubbling); }
  inline void setBubbling () noexcept { flags |= EFlag_Bubbling; }
  inline void resetBubbling () noexcept { flags &= ~EFlag_Bubbling; }

  inline bool isSinking () const noexcept { return !(flags&EFlag_Bubbling); }
  inline void setSinking () noexcept { flags &= ~EFlag_Bubbling; }
  inline void resetSinking () noexcept { flags |= EFlag_Bubbling; }

  inline bool isCtrlDown () const noexcept { return !!(modflags&bCtrl); }
  inline bool isAltDown () const noexcept { return !!(modflags&bAlt); }
  inline bool isShiftDown () const noexcept { return !!(modflags&bShift); }
  inline bool isHyperDown () const noexcept { return !!(modflags&bHyper); }

  inline bool isLMBDown () const noexcept { return !!(modflags&bLMB); }
  inline bool isMMBDown () const noexcept { return !!(modflags&bMMB); }
  inline bool isRMBDown () const noexcept { return !!(modflags&bRMB); }

  inline bool isMBDown (int index) const noexcept { return (index >= 0 && index <= 2 ? !!(modflags&(bLMB<<(index&0x0f))) : false); }

  inline bool isLCtrlDown () const noexcept { return !!(modflags&bCtrlLeft); }
  inline bool isLAltDown () const noexcept { return !!(modflags&bAltLeft); }
  inline bool isLShiftDown () const noexcept { return !!(modflags&bShiftLeft); }

  inline bool isRCtrlDown () const noexcept { return !!(modflags&bCtrlRight); }
  inline bool isRAltDown () const noexcept { return !!(modflags&bAltRight); }
  inline bool isRShiftDown () const noexcept { return !!(modflags&bShiftRight); }

  inline bool isAnyMouseEvent () const noexcept {
    return
      type == ev_mouse ||
      type == ev_uimouse ||
      type == ev_click ||
      type == ev_enter ||
      type == ev_leave ||
      ((type == ev_keydown || type == ev_keyup) && (keycode >= K_MOUSE_FIRST && keycode <= K_MOUSE_LAST));
  }

  inline bool isAnyMouseButtonEvent () const noexcept {
    return ((type == ev_keydown || type == ev_keyup) && (keycode >= K_MOUSE1 && keycode <= K_MOUSE9));
  }

  inline bool isAnyMouseButtonDownEvent () const noexcept {
    return (type == ev_keydown && (keycode >= K_MOUSE1 && keycode <= K_MOUSE9));
  }

  inline bool isAnyMouseButtonUpEvent () const noexcept {
    return (type == ev_keyup && (keycode >= K_MOUSE1 && keycode <= K_MOUSE9));
  }
};


// ////////////////////////////////////////////////////////////////////////// //
// `ExecuteFunction()` returns this
// we cannot return arrays
// returning pointers is not implemented yet (except VClass and VObject)
struct VFuncRes {
private:
  EType type;
  bool pointer;
  /*const*/ void *ptr;
  int ival; // and bool
  float fval;
  TVec vval;
  VStr sval;
  VName nval;

public:
  VFuncRes () : type(TYPE_Void), pointer(false), ptr(nullptr), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) {}
  VFuncRes (const VFuncRes &b) : type(TYPE_Void), pointer(false), ptr(nullptr), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) { *this = b; }

  VFuncRes (bool v) : type(TYPE_Int), pointer(false), ptr(nullptr), ival(v ? 1 : 0), fval(v ? 1 : 0), vval(0, 0, 0), sval(), nval(NAME_None) {}

  VFuncRes (int v) : type(TYPE_Int), pointer(false), ptr(nullptr), ival(v), fval(v), vval(0, 0, 0), sval(), nval(NAME_None) {}
  //VFuncRes (const int *v) : type(TYPE_Int), pointer(true), ptr(v), ival(v ? *v : 0), fval(v ? float(*v) : 0.0f), vval(0, 0, 0), sval(), nval(NAME_None) {}

  VFuncRes (float v) : type(TYPE_Float), pointer(false), ptr(nullptr), ival(v), fval(v), vval(0, 0, 0), sval(), nval(NAME_None) {}
  //VFuncRes (const float *v) : type(TYPE_Float), pointer(true), ptr(v), ival(*v ? int(v) : 0), fval(v ? *v : 0.0f), vval(0, 0, 0), sval(), nval(NAME_None) {}

  VFuncRes (const TVec &v) : type(TYPE_Vector), pointer(false), ptr(nullptr), ival(0), fval(0), vval(v.x, v.y, v.z), sval(), nval(NAME_None) {}
  //VFuncRes (const TVec *&v) : type(TYPE_Vector), pointer(true), ptr(v), ival(0), fval(0), vval(v ? v->x : 0.0f, v ? v->y : 0.0f, v ? v->z : 0.0f), sval(), nval(NAME_None) {}
  VFuncRes (const float x, const float y, const float z) : type(TYPE_Vector), pointer(false), ptr(nullptr), ival(0), fval(0), vval(x, y, z), sval(), nval(NAME_None) {}

  VFuncRes (VStr v) : type(TYPE_String), pointer(false), ptr(nullptr), ival(0), fval(0), vval(0, 0, 0), sval(v), nval(NAME_None) {}
  //VFuncRes (const VStr *&v) : type(TYPE_String), pointer(true), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(v ? *v : VStr::EmptyString), nval(NAME_None) {}

  VFuncRes (VName v) : type(TYPE_Name), pointer(false), ptr(nullptr), ival(0), fval(0), vval(0, 0, 0), sval(), nval(v) {}
  //VFuncRes (const VName *&v) : type(TYPE_Name), pointer(true), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(), nval() { if (v) nval = VName(*v); else nval = NAME_None; }

  VFuncRes (VClass *v) : type(TYPE_Class), pointer(false), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) {}
  VFuncRes (VObject *v) : type(TYPE_Reference), pointer(false), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) {}
  VFuncRes (VState *v) : type(TYPE_State), pointer(false), ptr(v), ival(0), fval(0), vval(0, 0, 0), sval(), nval(NAME_None) {}

  inline void operator = (const VFuncRes &b) {
    if (&b == this) return;
    type = b.type;
    pointer = b.pointer;
    ptr = b.ptr;
    ival = b.ival;
    fval = b.fval;
    vval = b.vval;
    sval = b.sval;
    nval = b.nval;
  }

  inline EType getType () const { return type; }
  inline bool isPointer () const { return pointer; }

  inline bool isVoid () const { return (type == TYPE_Void); }

  inline bool isNumber () const { return (type == TYPE_Int || type == TYPE_Float); }
  inline bool isInt () const { return (type == TYPE_Int); }
  inline bool isFloat () const { return (type == TYPE_Float); }
  inline bool isVector () const { return (type == TYPE_Vector); }
  inline bool isStr () const { return (type == TYPE_String); }
  inline bool isName () const { return (type == TYPE_Name); }
  inline bool isClass () const { return (type == TYPE_Class); }
  inline bool isObject () const { return (type == TYPE_Reference); }
  inline bool isState () const { return (type == TYPE_State); }

  inline const void *getPtr () const { return ptr; }
  inline VClass *getClass () const { return (VClass *)ptr; }
  inline VObject *getObject () const { return (VObject *)ptr; }
  inline VState *getState () const { return (VState *)ptr; }

  inline int getInt () const { return ival; }
  inline float getFloat () const { return fval; }
  inline const TVec &getVector () const { return vval; }
  inline VStr getStr () const { return sval; }
  inline VName getName () const { return nval; }

  inline bool getBool () const {
    switch (type) {
      case TYPE_Int: return (ival != 0);
      case TYPE_Float: return (fval != 0);
      case TYPE_Vector: return (vval.x != 0 && vval.y != 0 && vval.z != 0);
      case TYPE_String: return (sval.length() != 0);
      case TYPE_Name: return (nval != NAME_None);
      default: break;
    }
    return false;
  }

  inline operator bool () const { return getBool(); }
};


// flags describing an object instance
enum EObjectFlags {
  // object Destroy has already been called
  VObjFlag_Destroyed      = 1u<<0,
  // for k8vavoom: this thinker is marked for deletion on a next tick
  //               tick processor will take care of calling `Destroy()` on it
  //               set by `VThinker::DestroyThinker()`
  // for vccrun: you can call `CollectGarbage(true)` to destroy those objects
  VObjFlag_DelayedDestroy = 1u<<1,
  // this object is going to be destroyed; only GC will set this flag, and
  // you have to check it in your `ClearReferences()`
  VObjFlag_CleanupRef     = 1u<<2, // this object is going to be destroyed
};


//==========================================================================
//
//  VObject
//
//==========================================================================

// the base class of all objects
class VObject : public VInterface {
  // declarations
  DECLARE_BASE_CLASS(VObject, VObject, CLASS_Abstract|CLASS_Native)

  // friends
  friend class FObjectIterator;
  friend class VMethodProxy;

public:
  static int cliShowReplacementMessages; // default is false
  static int cliShowLoadingMessages; // default is false
  static int cliShowGCMessages; // default is false
  static int cliShowIODebugMessages; // default is false
  static int cliDumpNameTables; // default is false
  static int cliAllErrorsAreFatal; // default is false
  static int cliVirtualiseDecorateMethods; // default is false
  static int cliShowUndefinedBuiltins; // default is true

  static int cliCaseSensitiveLocals; // default is true
  static int cliCaseSensitiveFields; // default is true

  static int cliShowPackageLoading; // default is false
  static int engineAllowNotImplementedBuiltins; // default is false (and hidden classes)

  static int standaloneExecutor; // default is false

  static int ProfilerEnabled;

  static TMap<VStrCI, bool> cliAsmDumpMethods;

public: // for VM; PLEASE, DON'T MODIFY!
  static VStack *pr_stackPtr;

public:
  struct GCStats {
    int alive; // number of currently alive objects
    int markedDead; // number of objects currently marked as dead
    int lastCollected; // number of objects collected on last non-empty cycle
    int poolSize; // total number of used (including free) slots in object pool
    int poolAllocated; // total number of allocated slots in object pool
    int firstFree; // first free slot in pool
    double lastCollectDuration; // in seconds
    double lastCollectTime;
  };

private:
  // internal per-object variables
  VMethod **vtable; // Vavoom C VMT
  vint32 Index; // index of object into table
  vuint32 UniqueId; // monotonically increasing
  vuint32 ObjectFlags; // private EObjectFlags used by object manager
  VClass *Class; // class the object belongs to

  // private systemwide variables
  static TArray<VObject *> GObjObjects; // list of all objects
  static int GNumDeleted;
  static bool GInGarbageCollection;

  static GCStats gcLastStats;

public:
  static bool GImmediadeDelete; // has any sense only for standalone executor
  static bool GGCMessagesAllowed;
  static int GCDebugMessagesAllowed;
  static bool (*onExecuteNetMethodCB) (VObject *obj, VMethod *func); // return `false` to do normal execution
  static bool DumpBacktraceToStdErr;

protected:
  inline void IncrementInstanceCounters () const noexcept {
    VClass *cc = Class;
    ++cc->InstanceCount;
    //GLog.Logf(NAME_Debug, "CTOR %s: InstanceCount=%d", cc->GetName(), cc->InstanceCount);
    for (; cc; cc = cc->GetSuperClass()) {
      ++cc->InstanceCountWithSub;
      //GLog.Logf(NAME_Debug, "  %s: InstanceCountWithSub=%d", cc->GetName(), cc->InstanceCountWithSub);
    }
  }

  inline void DecrementInstanceCounters () const noexcept {
    VClass *cc = Class;
    --cc->InstanceCount;
    //GLog.Logf(NAME_Debug, "DTOR %s: InstanceCount=%d", cc->GetName(), cc->InstanceCount);
    for (; cc; cc = cc->GetSuperClass()) {
      --cc->InstanceCountWithSub;
      //GLog.Logf(NAME_Debug, "  %s: InstanceCountWithSub=%d", cc->GetName(), cc->InstanceCountWithSub);
    }
  }

public:
  // temporary hacks
  // first, translate, then call "can skip"
  static TMapNC<VName, VName> IOClassNameTranslation; // key: original name; value: new name

  typedef bool (*CanSkipReadingClassFn) (VObject *self, VName clsname);
  static TArray<CanSkipReadingClassFn> CanSkipReadingClassCBList;

public:
  // constructors
  VObject () noexcept;
  static void InternalConstructor () { new VObject(); }

  // this is called after defaults were blit
  // `UniqueId` is already set, and the object is registered
  virtual void PostCtor ();

  // destructors
  virtual ~VObject () override;

  void *operator new (size_t);
  void *operator new (size_t, const char *, int);
  void operator delete (void *);
  void operator delete (void *, const char *, int);

  inline bool IsRefToCleanup () const noexcept { return !!(ObjectFlags&VObjFlag_CleanupRef); }
  inline bool IsDelayedDestroy () const noexcept { return !!(ObjectFlags&VObjFlag_DelayedDestroy); }
  inline bool IsGoingToDie () const noexcept { return !!(ObjectFlags&(VObjFlag_DelayedDestroy|VObjFlag_Destroyed)); }
  inline bool IsDestroyed () const noexcept { return !!(ObjectFlags&VObjFlag_Destroyed); }

  inline void SetDelayedDestroy () noexcept { SetFlags(VObjFlag_DelayedDestroy); }

  // VObject interface
  virtual void Register ();
  virtual void Destroy ();

  virtual void SerialiseFields (VStream &); // this serialises object fields
  virtual void SerialiseOther (VStream &); // this serialises other object internal data
  void Serialise (VStream &); // this calls field serialisation, then other serialisation (and writes metadata)

  virtual void ClearReferences ();
  virtual bool ExecuteNetMethod (VMethod *);

  // system-wide functions
  static void StaticInit ();
  // call this before exiting from `main()`, so VC can skip some destructor duties
  static void StaticExit ();
  // call to register options in `GParsedArgs`
  static void StaticInitOptions (VParsedArgs &pargs);

  static VObject *StaticSpawnObject (VClass *AClass, bool skipReplacement);

  inline static VObject *StaticSpawnWithReplace (VClass *AClass) { return StaticSpawnObject(AClass, false); }
  inline static VObject *StaticSpawnNoReplace (VClass *AClass) { return StaticSpawnObject(AClass, true); }

  // note that you CANNOT use `delete` on VObjects, you have to call
  // either `Destroy()` or (even better) `ConditionalDestroy()`
  // the difference is that `ConditionalDestroy()` will call `Destroy()`
  // by itself, and will do it only once
  // WARNING! DO NOT PASS `true` FROM K8VAVOOM, OR EVERYTHING *WILL* BREAK!
  static void CollectGarbage (bool destroyDelayed=false);

  // this can be called on map unloading, for example, to combat UniqueId overflow (somewhat)
  // better to call this after calling `CollectGarbage()`, so "dying" objects won't prevent cleanups
  static void MinimiseUniqueId () noexcept;
  // can be used for statistics
  static vuint32 GetCurrentUniqueId () noexcept;

  static int GetObjectsCount () noexcept;
  static VObject *GetIndexObject (int) noexcept;

  static VFuncRes ExecuteFunction (VMethod *func); // all arguments should be on the stack
  static VFuncRes ExecuteFunctionNoArgs (VObject *Self, VMethod *func, bool allowVMTLookups=true);
  static inline VStack *VMGetStackPtr () noexcept { return pr_stackPtr; }
  static inline void VMIncStackPtr () noexcept { ++pr_stackPtr; } //FIXME: checks!
  static VStack *VMCheckStack (int size) noexcept;
  static VStack *VMCheckAndClearStack (int size) noexcept;
  static void VMDumpCallStack ();
  static void VMDumpCallStackToStdErr ();
  static void ClearProfiles ();
  static void DumpProfile ();
  static void DumpProfileInternal (int type); // <0: only native; >0: only script; 0: everything

  // functions

  // this should be called instead of `Destroy()`
  // it will call `Destroy()` if necessary, and will do it only once
  // i.e. you can call `ConditionalDestroy()` as many times as you want to
  void ConditionalDestroy () noexcept;

  inline bool IsA (VClass *SomeBaseClass) const noexcept {
    for (const VClass *c = Class; c; c = c->GetSuperClass()) if (SomeBaseClass == c) return true;
    return false;
  }

  // accessors
  inline VClass *GetClass () const noexcept { return Class; }
  inline vuint32 GetFlags () const noexcept { return ObjectFlags; }
  void SetFlags (vuint32 NewFlags) noexcept;
  inline vuint32 GetUniqueId () const noexcept { return UniqueId; } // never 0

  inline VMethod *GetVFunctionIdx (int InIndex) const noexcept { return vtable[InIndex]; }
  inline VMethod *GetVFunction (VName FuncName) const noexcept { return vtable[Class->GetMethodIndex(FuncName)]; }

  static VStr NameFromVKey (int vkey) noexcept;
  static int VKeyFromName (VStr kn) noexcept;

  inline static const GCStats &GetGCStats () noexcept { return gcLastStats; }
  inline static void ResetGCStatsLastCollected () noexcept { gcLastStats.lastCollected = 0; }

  #include "vc_object_common.h"

public:
  // default fields and setters
  inline float GetFieldFloat (VName fldname) { return Class->FindFieldChecked(fldname)->GetFloat(this); }
  inline TVec GetFieldVec (VName fldname) { return Class->FindFieldChecked(fldname)->GetVec(this); }
  inline bool GetFieldBool (VName fldname) { return Class->FindFieldChecked(fldname)->GetBool(this); }
  inline vint32 GetFieldInt (VName fldname) { return Class->FindFieldChecked(fldname)->GetInt(this); }
  inline vuint8 GetFieldByte (VName fldname) { return Class->FindFieldChecked(fldname)->GetByte(this); }
  inline VName GetFieldNameValue (VName fldname) { return Class->FindFieldChecked(fldname)->GetNameValue(this); }
  inline VStr GetFieldStr (VName fldname) { return Class->FindFieldChecked(fldname)->GetStr(this); }
  inline VClass *GetFieldClassValue (VName fldname) { return Class->FindFieldChecked(fldname)->GetClassValue(this); }
  inline VObject *GetFieldObjectValue (VName fldname) { return Class->FindFieldChecked(fldname)->GetObjectValue(this); }

  inline void SetFieldByte (VName fldname, vuint8 Value) { Class->FindFieldChecked(fldname)->SetByte(this, Value); }
  inline void SetFieldInt (VName fldname, int Value) { Class->FindFieldChecked(fldname)->SetInt(this, Value); }
  inline void SetFieldInt (VName fldname, int Value, int Idx) { Class->FindFieldChecked(fldname)->SetInt(this, Value, Idx); }
  inline void SetFieldFloat (VName fldname, float Value) { Class->FindFieldChecked(fldname)->SetFloat(this, Value); }
  inline void SetFieldFloat (VName fldname, float Value, int Idx) { Class->FindFieldChecked(fldname)->SetFloat(this, Value, Idx); }
  inline void SetFieldNameValue (VName fldname, VName Value) { Class->FindFieldChecked(fldname)->SetNameValue(this, Value); }
  inline void SetFieldStr (VName fldname, VStr Value) { Class->FindFieldChecked(fldname)->SetStr(this, Value); }
  inline void SetFieldBool (VName fldname, int Value) { Class->FindFieldChecked(fldname)->SetBool(this, Value); }
  inline void SetFieldVec (VName fldname, const TVec &Value) { Class->FindFieldChecked(fldname)->SetVec(this, Value); }
  inline void SetFieldClassValue (VName fldname, VClass *Value) { Class->FindFieldChecked(fldname)->SetClassValue(this, Value); }
  inline void SetFieldObjectValue (VName fldname, VObject *Value) { Class->FindFieldChecked(fldname)->SetObjectValue(this, Value); }

public:
  // cvar cache API

  // `canCache` is never `nullptr`, and always set to `true` on call
  // *always* return `nullptr` if there is no such cvar
  typedef VCvar *(*GetVCvarObjectFn) (VName name, bool *canCache);
  static GetVCvarObjectFn GetVCvarObject;

private:
  // check if the queue has any unprocessed events
  // returns number of events in the queue, or 0
  static int CountQueuedEventsNoLock () noexcept;

public:
  // event queue API; as it is used both in k8vavoom and in vccrun, and with
  // common `event_t` struct, there is no reason to not have it here
  //
  // it is thread-safe

  // returns `false` if the queue is full
  // add the event to the bottom of the queue
  static bool PostEvent (const event_t &ev) noexcept;

  // returns `false` if the queue is full
  // add event to the top of the queue
  static bool InsertEvent (const event_t &ev) noexcept;

  // check if the queue has any unprocessed events
  // returns number of events in the queue, or 0
  static int CountQueuedEvents () noexcept;

  // peek an event from the queue
  // event with index 0 is the top one (i.e. first posted)
  // it is safe to call this with `ev` set to `nullptr`
  // returns `false` on invalid index (and `ev` is zeroed in this case)
  static bool PeekEvent (int idx, event_t *ev) noexcept;

  // get the top event from the queue
  // returns `false` if there are no more events (and `ev` is zeroed in this case)
  // it is safe to call this with `ev` set to `nullptr`
  // (in this case the event will be dropped)
  static bool GetEvent (event_t *ev) noexcept;

  // returns maximum size of the queue
  static int GetEventQueueSize () noexcept;

  // set new maximum size of the queue
  // invalid newsize values (negative or zero) will be ignored
  // if the queue currently contanis more than `newsize` events, the API is noop
  // returns success flag (i.e. `true` when the queue was resized)
  static bool SetEventQueueSize (int newsize) noexcept;

  // unconditionally clears the queue
  static void ClearEventQueue () noexcept;

public:
  // stack routines
  static VVA_OKUNUSED inline void PR_Push (int value) noexcept {
    VObject::pr_stackPtr->i = value;
    ++VObject::pr_stackPtr;
  }

  static VVA_OKUNUSED inline void PR_PushBool (bool value) noexcept {
    VObject::pr_stackPtr->i = (value ? 1 : 0);
    ++VObject::pr_stackPtr;
  }

  static VVA_OKUNUSED inline int PR_Pop () noexcept {
    --VObject::pr_stackPtr;
    return VObject::pr_stackPtr->i;
  }

  static VVA_OKUNUSED inline void PR_Pushf (float value) noexcept {
    VObject::pr_stackPtr->f = value;
    ++VObject::pr_stackPtr;
  }

  static VVA_OKUNUSED inline float PR_Popf () noexcept {
    --VObject::pr_stackPtr;
    return VObject::pr_stackPtr->f;
  }

  static VVA_OKUNUSED inline void PR_Pushv (const TVec &v) noexcept {
    PR_Pushf(v.x);
    PR_Pushf(v.y);
    PR_Pushf(v.z);
  }

  static VVA_OKUNUSED inline void PR_Pushav (const TAVec &v) noexcept {
    PR_Pushf(v.pitch);
    PR_Pushf(v.yaw);
    PR_Pushf(v.roll);
  }

  static VVA_OKUNUSED inline TVec PR_Popv () noexcept {
    TVec v;
    v.z = PR_Popf();
    v.y = PR_Popf();
    v.x = PR_Popf();
    return v;
  }

  static VVA_OKUNUSED inline TAVec PR_Popav () noexcept {
    TAVec v;
    v.roll = PR_Popf();
    v.yaw = PR_Popf();
    v.pitch = PR_Popf();
    return v;
  }

  static VVA_OKUNUSED inline void PR_PushName (VName value) noexcept {
    VObject::pr_stackPtr->i = value.GetIndex();
    ++VObject::pr_stackPtr;
  }

  static VVA_OKUNUSED inline VName PR_PopName () noexcept {
    --VObject::pr_stackPtr;
    return *(VName *)&VObject::pr_stackPtr->i;
  }

  static VVA_OKUNUSED inline void PR_PushPtr (void *value) noexcept {
    VObject::pr_stackPtr->p = value;
    ++VObject::pr_stackPtr;
  }

  static VVA_OKUNUSED inline void *PR_PopPtr () noexcept {
    --VObject::pr_stackPtr;
    return VObject::pr_stackPtr->p;
  }

  static VVA_OKUNUSED inline VObject *PR_PopRef () noexcept {
    --VObject::pr_stackPtr;
    return (VObject *)(VObject::pr_stackPtr->p);
  }

  static VVA_OKUNUSED inline void PR_PushStr (VStr value) noexcept {
    VObject::pr_stackPtr->p = nullptr;
    *(VStr *)&VObject::pr_stackPtr->p = value;
    ++VObject::pr_stackPtr;
  }

  static VVA_OKUNUSED inline VStr PR_PopStr () noexcept {
    --VObject::pr_stackPtr;
    VStr Ret = *(VStr *)&VObject::pr_stackPtr->p;
    ((VStr *)&VObject::pr_stackPtr->p)->Clean();
    return Ret;
  }

public: // this API was in global namespace
  static volatile unsigned vmAbortBySignal;

  static void PR_Init ();
  static void PR_OnAbort ();
  static VStr PF_FormatString ();

  static void PR_WriteOne (const VFieldType &type);
  static void PR_WriteFlush ();

  // if `buf` is `nullptr`, it means "flush"
  static void (*PR_WriterCB) (const char *buf, bool debugPrint, VName wrname);

  // calls `PR_WriterCB` if it is not empty, or does default printing
  // if `buf` is `nullptr`, it means "flush"
  static void PR_DoWriteBuf (const char *buf, bool debugPrint=false, VName wrname=NAME_None);
};


// dynamically cast an object type-safely
template<class T> T *Cast (VObject *Src) { return (Src && Src->IsA(T::StaticClass()) ? (T *)Src : nullptr); }
template<class T, class U> T *CastChecked (U *Src) {
  if (!Src || !Src->IsA(T::StaticClass())) VPackage::InternalFatalError(va("Cast `%s` to `%s` failed", (Src ? Src->GetClass()->GetName() : "none"), T::StaticClass()->GetName()));
  return (T *)Src;
}


//==========================================================================
//
//  VMethodProxy
//
//==========================================================================
class VMethodProxy {
private:
  const char *MethodName;
  VMethod *Method;
  VClass *Class;

public:
  VMethodProxy ();
  VMethodProxy (const char *AMethod);

  // returns `false` if method not found
  bool ResolveStatic (VClass *aClass);
  void ResolveStaticChecked (VClass *aClass);

  // returns `false` if method not found
  bool Resolve (VObject *Self);
  void ResolveChecked (VObject *Self);

  VFuncRes Execute (VObject *Self);
  // this doesn't check is `Self` isa `Class`
  VFuncRes ExecuteNoCheck (VObject *Self);

  VFuncRes ExecuteStatic (VClass *aClass);
};


/*----------------------------------------------------------------------------
  Object iterators.
  use like this:
    for (FObjectIterator obj(); obj; ++obj) { ... }
    for (TObjectIterator<VEntity> obj(); obj; ++obj) { ... }
----------------------------------------------------------------------------*/

// class for iterating through all objects
class FObjectIterator {
protected:
  VClass *Class;
  int Index;

public:
  FObjectIterator (VClass *InClass=VObject::StaticClass()) : Class(InClass), Index(-1) { ++*this; }

  void operator ++ () { while (++Index < VObject::GObjObjects.length() && (!VObject::GObjObjects[Index] || !VObject::GObjObjects[Index]->IsA(Class))) {} }
  // yes, they are the same
  VObject *operator * () { return VObject::GObjObjects[Index]; }
  VObject *operator -> () { return VObject::GObjObjects[Index]; }
  operator bool () { return (Index < VObject::GObjObjects.length()); }
};


// class for iterating through all objects which inherit from a specified base class
template<class T> class TObjectIterator : public FObjectIterator {
public:
  TObjectIterator () : FObjectIterator(T::StaticClass()) {}

  // yes, they are the same
  T *operator * () { return (T*)FObjectIterator::operator*(); }
  T *operator -> () { return (T*)FObjectIterator::operator->(); }
};


// object creation template
//template<class T> T *Spawn () { return (T*)VObject::StaticSpawnObject(T::StaticClass()); }
template<class T> T *SpawnWithReplace () { return (T*)VObject::StaticSpawnObject(T::StaticClass(), false); } // don't skip replacement
template<class T> T *SpawnNoReplace () { return (T*)VObject::StaticSpawnObject(T::StaticClass(), true); } // skip replacement

//inline vuint32 GetTypeHash (const VObject *Obj) { return (Obj ? Obj->GetIndex() : 0); }
inline vuint32 GetTypeHash (const VObject *Obj) { return (Obj ? hashU32(Obj->GetUniqueId()) : 0); }

// helper macros for implementing native VavoomC functions and calls to the VavoomC methods:
// this will make it simpler to port it to 64 bit platforms

// macros for passign arguments to VavoomC methods
#define P_PASS_INT(v)    VObject::PR_Push(v)
#define P_PASS_BYTE(v)   VObject::PR_Push(v)
#define P_PASS_FLOAT(v)  VObject::PR_Pushf(v)
#define P_PASS_BOOL(v)   VObject::PR_Push(v)
#define P_PASS_NAME(v)   VObject::PR_PushName(v)
#define P_PASS_STR(v)    VObject::PR_PushStr(v)
#define P_PASS_VEC(v)    VObject::PR_Pushv(v)
#define P_PASS_AVEC(v)   VObject::PR_Pushav(v)
#define P_PASS_REF(v)    VObject::PR_PushPtr(v)
#define P_PASS_PTR(v)    VObject::PR_PushPtr(v)
#define P_PASS_SELF      VObject::PR_PushPtr(this)

// macros for calling VavoomC methods with different return types
// this is for `VMethodProxy`
#define VMT_RET_VOID(v)    return (void)(v).Execute(this)
#define VMT_RET_INT(v)     return (v).Execute(this).getInt()
#define VMT_RET_BYTE(v)    return (v).Execute(this).getInt()
#define VMT_RET_FLOAT(v)   return (v).Execute(this).getFloat()
#define VMT_RET_BOOL(v)    return !!(v).Execute(this).getInt()
#define VMT_RET_NAME(v)    return (v).Execute(this).getName()
#define VMT_RET_STR(v)     return (v).Execute(this).getStr()
#define VMT_RET_VEC(v)     return (v).Execute(this).getVector()
//#define VMT_RET_AVEC(v)    VPackage::InternalFatalError("Not implemented") /*(v).ExecuteFunction(this)*/
#define VMT_RET_REF(t,v)   return (t *)(v).Execute(this).getObject()
#define VMT_RET_PTR(t,v)   return (t *)(v).Execute(this).getClass()

#define VSTATIC_RET_VOID(v)    return (void)(v).ExecuteStatic(StaticClass())
#define VSTATIC_RET_INT(v)     return (v).ExecuteStatic(StaticClass()).getInt()
#define VSTATIC_RET_BYTE(v)    return (v).ExecuteStatic(StaticClass()).getInt()
#define VSTATIC_RET_FLOAT(v)   return (v).ExecuteStatic(StaticClass()).getFloat()
#define VSTATIC_RET_BOOL(v)    return !!(v).ExecuteStatic(StaticClass()).getInt()
#define VSTATIC_RET_NAME(v)    return (v).ExecuteStatic(StaticClass()).getName()
#define VSTATIC_RET_STR(v)     return (v).ExecuteStatic(StaticClass()).getStr()
#define VSTATIC_RET_VEC(v)     return (v).ExecuteStatic(StaticClass()).getVector()
//#define VMT_RET_AVEC(v)    VPackage::InternalFatalError("Not implemented") /*(v).ExecuteFunction(StaticClass())*/
#define VSTATIC_RET_REF(t,v)   return (t *)(v).ExecuteStatic(StaticClass()).getObject()
#define VSTATIC_RET_PTR(t,v)   return (t *)(v).ExecuteStatic(StaticClass()).getClass()

// parameter get macros; parameters must be retrieved in backwards order
#define P_GET_INT(v)     vint32 v = VObject::PR_Pop()
#define P_GET_BYTE(v)    vuint8 v = VObject::PR_Pop()
#define P_GET_FLOAT(v)   float v = VObject::PR_Popf()
#define P_GET_BOOL(v)    bool v = !!VObject::PR_Pop()
#define P_GET_NAME(v)    VName v = VObject::PR_PopName()
#define P_GET_STR(v)     VStr v = VObject::PR_PopStr()
#define P_GET_VEC(v)     TVec v = VObject::PR_Popv()
#define P_GET_AVEC(v)    TAVec v = VObject::PR_Popav()
#define P_GET_REF(c,v)   c *v = (c *)VObject::PR_PopPtr()
#define P_GET_PTR(t,v)   t *v = (t *)VObject::PR_PopPtr()
#define P_GET_SELF       ThisClass *Self = (ThisClass *)VObject::PR_PopPtr()

#define P_GET_INT_OPT(v,d)     bool specified_##v = !!VObject::PR_Pop(); vint32 v = VObject::PR_Pop(); if (!specified_##v) v = d
#define P_GET_BYTE_OPT(v,d)    bool specified_##v = !!VObject::PR_Pop(); vuint8 v = VObject::PR_Pop(); if (!specified_##v) v = d
#define P_GET_FLOAT_OPT(v,d)   bool specified_##v = !!VObject::PR_Pop(); float v = VObject::PR_Popf(); if (!specified_##v) v = d
#define P_GET_BOOL_OPT(v,d)    bool specified_##v = !!VObject::PR_Pop(); bool v = !!VObject::PR_Pop(); if (!specified_##v) v = d
#define P_GET_NAME_OPT(v,d)    bool specified_##v = !!VObject::PR_Pop(); VName v = VObject::PR_PopName(); if (!specified_##v) v = d
#define P_GET_STR_OPT(v,d)     bool specified_##v = !!VObject::PR_Pop(); VStr v = VObject::PR_PopStr(); if (!specified_##v) v = d
#define P_GET_VEC_OPT(v,d)     bool specified_##v = !!VObject::PR_Pop(); TVec v = VObject::PR_Popv(); if (!specified_##v) v = d
#define P_GET_AVEC_OPT(v,d)    bool specified_##v = !!VObject::PR_Pop(); TAVec v = VObject::PR_Popav(); if (!specified_##v) v = d
#define P_GET_REF_OPT(c,v,d)  bool specified_##v = !!VObject::PR_Pop(); c *v = (c *)VObject::PR_PopPtr(); if (!specified_##v) v = d
#define P_GET_PTR_OPT(t,v,d)  bool specified_##v = !!VObject::PR_Pop(); t *v = (t *)VObject::PR_PopPtr(); if (!specified_##v) v = d
#define P_GET_OUT_OPT(t,v)     bool specified_##v = !!VObject::PR_Pop(); t *v = (t *)VObject::PR_PopPtr()

#define P_GET_PTR_OPT_NOSP(t,v)  VObject::PR_Pop(); t *v = (t *)VObject::PR_PopPtr()
#define P_GET_OUT_OPT_NOSP(t,v)  VObject::PR_Pop(); t *v = (t *)VObject::PR_PopPtr()

// method return macros
#define RET_VOID(v)   (void)(v)
#define RET_INT(v)    VObject::PR_Push(v)
#define RET_BYTE(v)   VObject::PR_Push((v)&0xff)
#define RET_FLOAT(v)  VObject::PR_Pushf(v)
#define RET_BOOL(v)   VObject::PR_PushBool(!!(v))
#define RET_NAME(v)   VObject::PR_PushName(v)
#define RET_STR(v)    VObject::PR_PushStr(v)
#define RET_VEC(v)    VObject::PR_Pushv(v)
#define RET_AVEC(v)   VObject::PR_Pushav(v)
#define RET_REF(v)    VObject::PR_PushPtr(v)
#define RET_PTR(v)    VObject::PR_PushPtr(v)


// ////////////////////////////////////////////////////////////////////////// //
class VScriptIterator : public VInterface {
public:
  virtual ~VScriptIterator ();
  virtual bool GetNext() = 0;
  // by default, the following does `delete this;`
  virtual void Finished ();
};


// ////////////////////////////////////////////////////////////////////////// //
static VVA_OKUNUSED inline void vobj_get_param (int &n) noexcept { n = VObject::PR_Pop(); }
static VVA_OKUNUSED inline void vobj_get_param (vuint32 &n) noexcept { n = (vuint32)VObject::PR_Pop(); }
static VVA_OKUNUSED inline void vobj_get_param (vuint8 &n) noexcept { n = (vuint8)VObject::PR_Pop(); }
static VVA_OKUNUSED inline void vobj_get_param (float &n) noexcept { n = VObject::PR_Popf(); }
static VVA_OKUNUSED inline void vobj_get_param (double &n) noexcept { n = VObject::PR_Popf(); }
static VVA_OKUNUSED inline void vobj_get_param (bool &n) noexcept { n = !!VObject::PR_Pop(); }
static VVA_OKUNUSED inline void vobj_get_param (VStr &n) noexcept { n = VObject::PR_PopStr(); }
static VVA_OKUNUSED inline void vobj_get_param (VName &n) noexcept { n = VObject::PR_PopName(); }
static VVA_OKUNUSED inline void vobj_get_param (int *&n) noexcept { n = (int *)VObject::PR_PopPtr(); }
static VVA_OKUNUSED inline void vobj_get_param (vuint32 *&n) noexcept { n = (vuint32 *)VObject::PR_PopPtr(); }
static VVA_OKUNUSED inline void vobj_get_param (vuint8 *&n) noexcept { n = (vuint8 *)VObject::PR_PopPtr(); }
static VVA_OKUNUSED inline void vobj_get_param (TVec &n) noexcept { n = VObject::PR_Popv(); }
static VVA_OKUNUSED inline void vobj_get_param (TAVec &n) noexcept { n = VObject::PR_Popav(); }
template<class C> static VVA_OKUNUSED inline void vobj_get_param (C *&n) noexcept { n = (C *)VObject::PR_PopPtr(); }

#define VC_DEFINE_OPTPARAM(tname_,type_,defval_,pop_) \
struct VOptParam##tname_ { \
  bool specified; \
  type_ value; \
  inline VOptParam##tname_ (type_ adefault=defval_) noexcept : specified(false), value(adefault) {} \
  /*inline operator type_ () noexcept { return value; }*/ \
  inline operator type_ & () noexcept { return value; } \
  inline void operator = (const type_ &v) noexcept { value = v; } \
}; \
static VVA_OKUNUSED inline void vobj_get_param (VOptParam##tname_ &n) noexcept { \
  n.specified = !!VObject::PR_Pop(); \
  if (n.specified) n.value = pop_(); else (void)pop_(); \
}

VC_DEFINE_OPTPARAM(Int, int, 0, VObject::PR_Pop) // VOptParamInt
VC_DEFINE_OPTPARAM(UInt, vuint32, 0, VObject::PR_Pop) // VOptParamUInt
VC_DEFINE_OPTPARAM(UByte, vuint8, 0, VObject::PR_Pop) // VOptParamUByte
VC_DEFINE_OPTPARAM(Float, float, 0.0f, VObject::PR_Popf) // VOptParamFloat
VC_DEFINE_OPTPARAM(Double, double, 0.0, VObject::PR_Popf) // VOptParamDouble
VC_DEFINE_OPTPARAM(Bool, bool, false, !!VObject::PR_Pop) // VOptParamBool
VC_DEFINE_OPTPARAM(Str, VStr, VStr::EmptyString, VObject::PR_PopStr) // VOptParamStr
VC_DEFINE_OPTPARAM(Name, VName, NAME_None, VObject::PR_PopName) // VOptParamName
VC_DEFINE_OPTPARAM(Vec, TVec, TVec(0.0f, 0.0f, 0.0f), VObject::PR_Popv) // VOptParamVec
VC_DEFINE_OPTPARAM(AVec, TAVec, TAVec(0.0f, 0.0f, 0.0f), VObject::PR_Popav) // VOptParamAVec

template<class C> struct VOptParamPtr {
  bool specified;
  bool popDummy;
  C *value;
  inline VOptParamPtr () noexcept : specified(false), popDummy(true), value(nullptr) {}
  inline VOptParamPtr (bool aPopDummy, C *adefault=nullptr) noexcept : specified(false), popDummy(aPopDummy), value(adefault) {}
  inline VOptParamPtr (C *adefault) noexcept : specified(false), popDummy(false), value(adefault) {}
  inline operator C* () noexcept { return value; }
  inline C* operator -> () noexcept { return value; }
  inline operator bool () noexcept { return !!value; }
  inline bool isNull () const noexcept { return !!value; }
};
template<class C> static VVA_OKUNUSED inline void vobj_get_param (VOptParamPtr<C> &n) noexcept {
  n.specified = !!VObject::PR_Pop();
  // optional ref pushes pointer to dummy object, so why not?
  //if (n.specified || !n.value) n.value = (C *)VObject::PR_PopPtr(); else (void)VObject::PR_PopPtr();
  if (n.specified || n.popDummy) n.value = (C *)VObject::PR_PopPtr(); else (void)VObject::PR_PopPtr();
}

/* usage of optional params:
  int ofs;
  VOptParamInt whence(SeekStart); // will retain its value if not specified
  vobjGetParamSelf(ofs, whence);
  if (whence.specified) { ... }
*/

template<typename T, typename... Args> static VVA_OKUNUSED inline void vobj_get_param (T &n, Args&... args) noexcept {
  vobj_get_param(args...);
  vobj_get_param(n);
}


template<typename... Args> static VVA_OKUNUSED inline void vobjGetParam (Args&... args) noexcept {
  vobj_get_param(args...);
}

#define vobjGetParamSelf(...)  ThisClass *Self; vobjGetParam(Self, ##__VA_ARGS__)
#define vobjDeclareSelf        ThisClass *Self


// ////////////////////////////////////////////////////////////////////////// //
static VVA_OKUNUSED inline void vobj_put_param (const int v) noexcept { VObject::PR_Push(v); }
static VVA_OKUNUSED inline void vobj_put_param (const vuint32 v) noexcept { VObject::PR_Push((vint32)v); }
static VVA_OKUNUSED inline void vobj_put_param (const vuint8 v) noexcept { VObject::PR_Push((vint32)v); }
static VVA_OKUNUSED inline void vobj_put_param (const float v) noexcept { VObject::PR_Pushf(v); }
static VVA_OKUNUSED inline void vobj_put_param (const double v) noexcept { VObject::PR_Pushf(v); }
static VVA_OKUNUSED inline void vobj_put_param (const bool v) noexcept { VObject::PR_Push(!!v); }
static VVA_OKUNUSED inline void vobj_put_param (const VStr v) noexcept { VObject::PR_PushStr(v); }
static VVA_OKUNUSED inline void vobj_put_param (const VName v) noexcept { VObject::PR_PushName(v); }
static VVA_OKUNUSED inline void vobj_put_param (const TVec v) noexcept { VObject::PR_Pushv(v); }
static VVA_OKUNUSED inline void vobj_put_param (const TAVec v) noexcept { VObject::PR_Pushav(v); }
//static VVA_OKUNUSED inline void vobj_put_param (VObject *v) noexcept { VObject::PR_PushPtr((void *)v); }
//static VVA_OKUNUSED inline void vobj_put_param (const VObject *v) noexcept { VObject::PR_PushPtr((void *)v); }
template<class C> static VVA_OKUNUSED inline void vobj_put_param (C *v) noexcept { VObject::PR_PushPtr((void *)v); }

#define VC_DEFINE_OPTPARAM_PUT(tname_,type_,defval_,push_) \
struct VOptPutParam##tname_ { \
  bool specified; \
  type_ value; \
  type_ defvalue; \
  inline VOptPutParam##tname_ () noexcept : specified(false), value(defval_), defvalue(defval_) {} \
  inline VOptPutParam##tname_ (type_ avalue, bool aspecified=true) noexcept : specified(aspecified), value(avalue), defvalue(defval_) {} \
  inline operator type_ () noexcept { return value; } \
}; \
static VVA_OKUNUSED inline void vobj_put_param (VOptPutParam##tname_ &v) noexcept { \
  if (v.specified) push_(v.value); else push_(v.defvalue); \
  VObject::PR_PushBool(v.specified); \
}

VC_DEFINE_OPTPARAM_PUT(Int, int, 0, VObject::PR_Push) // VOptPutParamInt
VC_DEFINE_OPTPARAM_PUT(UInt, vuint32, 0, VObject::PR_Push) // VOptPutParamUInt
VC_DEFINE_OPTPARAM_PUT(UByte, vuint8, 0, VObject::PR_Push) // VOptPutParamUByte
VC_DEFINE_OPTPARAM_PUT(Float, float, 0.0f, VObject::PR_Pushf) // VOptPutParamFloat
VC_DEFINE_OPTPARAM_PUT(Double, double, 0.0, VObject::PR_Pushf) // VOptPutParamDouble
VC_DEFINE_OPTPARAM_PUT(Bool, bool, false, VObject::PR_Push) // VOptPutParamBool
VC_DEFINE_OPTPARAM_PUT(Str, VStr, VStr::EmptyString, VObject::PR_PushStr) // VOptPutParamStr
VC_DEFINE_OPTPARAM_PUT(Name, VName, NAME_None, VObject::PR_PushName) // VOptPutParamName
VC_DEFINE_OPTPARAM_PUT(Vec, TVec, TVec(0.0f, 0.0f, 0.0f), VObject::PR_Pushv) // VOptPutParamVec
VC_DEFINE_OPTPARAM_PUT(AVec, TAVec, TAVec(0.0f, 0.0f, 0.0f), VObject::PR_Pushav) // VOptPutParamAVec

//WARNING! you'd better provide a dummy non-nullptr pointer here if this is not a class/object!
//         most of VC code expects that it can safely use ommited pointer argment, because
//         the compiler creates a dummy object for those.
template<class C> struct VOptPutParamPtr {
  bool specified;
  C *value;
  inline VOptPutParamPtr () noexcept : specified(false), value(nullptr) {}
  inline VOptPutParamPtr (C *avalue) noexcept : specified(true), value(avalue) {}
  inline operator C* () noexcept { return value; }
  inline C* operator -> () noexcept { return value; }
  inline operator bool () noexcept { return !!value; }
};
template<class C> static VVA_OKUNUSED inline void vobj_put_param (VOptPutParamPtr<C> v) noexcept {
  VObject::PR_PushPtr((void *)v.value);
  VObject::PR_PushBool(v.specified);
}

/* usage of optional arguments:
  static VMethodProxy method("PickActor");
  vobjPutParamSelf(
    VOptPutParamVec(orig, specified_orig), // second is boolean "specified" flag
    dir, dist,
    VOptPutParamInt(actmask, specified_actmask),
    VOptPutParamInt(wallmask, specified_wallmask)
  );
  VMT_RET_REF(VEntity, method);
*/


template<typename T, typename... Args> static VVA_OKUNUSED inline void vobj_put_param (T v, Args... args) noexcept {
  vobj_put_param(v);
  vobj_put_param(args...);
}


template<typename... Args> static VVA_OKUNUSED inline void vobjPutParam (Args... args) noexcept {
  vobj_put_param(args...);
}

#define vobjPutParamSelf(...)  vobjPutParam(this, ##__VA_ARGS__)
