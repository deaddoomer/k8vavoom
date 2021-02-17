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

class VObject;
class VMethod;


// ////////////////////////////////////////////////////////////////////////// //
enum EType {
  TYPE_Void,
  TYPE_Int,
  TYPE_Byte,
  TYPE_Bool,
  TYPE_Float,
  TYPE_Name,
  TYPE_String,
  TYPE_Pointer,
  TYPE_Reference,
  TYPE_Class,
  TYPE_State,
  TYPE_Delegate,
  TYPE_Struct,
  TYPE_Vector,
  TYPE_Array,
  TYPE_DynamicArray,
  TYPE_SliceArray, // array consisting of pointer and length, with immutable length
  TYPE_Dictionary,
  TYPE_Unknown,
  TYPE_Automatic, // this is valid only for variable declarations, and will be resolved to actual type

  NUM_BASIC_TYPES,
};


// ////////////////////////////////////////////////////////////////////////// //
class VFieldType {
public:
  vuint8 Type;
  vuint8 InnerType; // for pointers and dictionary value pointers
  vuint8 ArrayInnerType; // for arrays and dictionary array values
  vuint8 KeyInnerType; // key for dictionaries; PtrLevel is always 1 for pointers (and pointers are `void`)
  vuint8 ValueInnerType; // value for dictionaries
  vuint8 PtrLevel; // for pointers and dictionary pointer values
  union {
    // you should never access `ArrayDimInternal` directly!
    // sign bit is used to mark "2-dim array"
    vint32 ArrayDimInternal;
    VClass *KClass; // class for class key type
    VStruct *KStruct; // struct data
    VMethod *KFunction; // function of the delegate type; can be `nullptr` for `none delegate`
  };
  // for normal types and dictionary values
  union {
    vuint32 BitMask;
    VClass *Class; // class of the reference
    VStruct *Struct; // struct data
    VMethod *Function; // function of the delegate type; can be `nullptr` for `none delegate`
  };

  VFieldType ();
  VFieldType (EType Atype);
  explicit VFieldType (VClass *InClass);
  explicit VFieldType (VStruct *InStruct);

  enum { MemSize = 6+sizeof(void *)*2 };
  static VFieldType ReadTypeMem (vuint8 *&ptr);
  void WriteTypeMem (vuint8 *&ptr) const;

  friend VStream &operator << (VStream &, VFieldType &);

  bool Equals (const VFieldType &) const;

  // used for `==` and `!=` -- allow substructs and subclasses
  bool IsCompatiblePointerRelaxed (const VFieldType &) const;

  static VFieldType MakeDictType (const VFieldType &ktype, const VFieldType &vtype, const TLocation &loc);

  VFieldType MakePointerType () const;
  VFieldType GetPointerInnerType () const;
  VFieldType MakeArrayType (int, const TLocation &) const;
  VFieldType MakeArray2DType (int d0, int d1, const TLocation &l) const;
  VFieldType MakeDynamicArrayType (const TLocation &) const;
  VFieldType MakeSliceType (const TLocation &) const;

  VFieldType GetArrayInnerType () const;
  VFieldType GetDictKeyType () const;
  VFieldType GetDictValueType () const;

  inline bool IsArray1D () const noexcept { return (ArrayDimInternal >= 0); }
  inline bool IsArray2D () const noexcept { return (ArrayDimInternal < 0); }
  // get 1d array dim (for 2d arrays this will be correctly calculated)
  inline vint32 GetArrayDim () const noexcept { return (ArrayDimInternal >= 0 ? ArrayDimInternal : GetFirstDim()*GetSecondDim()); }
  // get first dimension (or the only one for 1d array)
  inline vint32 GetFirstDim () const noexcept { return (ArrayDimInternal >= 0 ? ArrayDimInternal : ArrayDimInternal&0x7fff); }
  // get second dimension (or 1 for 1d array)
  inline vint32 GetSecondDim () const noexcept { return (ArrayDimInternal >= 0 ? 1 : (ArrayDimInternal>>16)&0x7fff); }

  int GetStackSize () const noexcept; // in 4-byte slots (this is wrong for x86_64, but the code is used to that)
  inline int GetStackSlotCount () const noexcept { return GetStackSize()>>2; }

  int GetSize () const noexcept;
  int GetAlignment () const noexcept;

  bool CheckPassable (const TLocation &, bool raiseError=true) const;
  bool CheckReturnable (const TLocation &, bool raiseError=true) const;
  bool CheckMatch (bool asRef, const TLocation &loc, const VFieldType &, bool raiseError=true) const;

  VStr GetName () const;

  inline bool IsAnyArray () const noexcept { return (Type == TYPE_Array || Type == TYPE_DynamicArray || Type == TYPE_SliceArray || Type == TYPE_Dictionary); }
  inline bool IsAnyIndexableArray () const noexcept { return (Type == TYPE_Array || Type == TYPE_DynamicArray || Type == TYPE_SliceArray); }
  inline bool IsStruct () const noexcept { return (Type == TYPE_Struct); }
  inline bool IsAnyArrayOrStruct () const noexcept { return (IsAnyArray() || IsStruct()); }
  inline bool IsPointer () const noexcept { return (Type == TYPE_Pointer); } // useless, but nice
  inline bool IsVoidPointer () const noexcept { return (Type == TYPE_Pointer && PtrLevel == 1 && InnerType == TYPE_Void); }
  inline bool IsPointerType (EType tp) const noexcept { return (Type == TYPE_Pointer && PtrLevel == 1 && InnerType == tp); }
  inline bool IsNormalOrPointerType (EType tp) const noexcept { return (Type == tp || (Type == TYPE_Pointer && PtrLevel == 1 && InnerType == tp)); }

  // this is used to force-zero some types even if they were properly destructed
  // this can be the case when we got a reused slot for something like dictionary or dynarray
  // while dtor frees their memory, we should still play safe and zero 'em
  bool NeedZeroingOnSlotReuse () const noexcept;

  // returns `true` if this type requires a wrapping struct
  // for example, it is impossible to hold `array!(array!int)` directly, so
  // compiler should create an anonymous wrapping struct for it
  bool NeedWrapStruct () const noexcept;

  // is this "wrapper struct" type?
  bool IsWrapStruct () const noexcept;

  VFieldType WrapStructType () const;
};


// ////////////////////////////////////////////////////////////////////////// //
struct VObjectDelegate {
  VObject *Obj;
  VMethod *Func;
};


// ////////////////////////////////////////////////////////////////////////// //
// dynamic array object, used in script executor
class VScriptArray {
private:
  int ArrNum; // if bit 31 is set, this is 1st dim of 2d array
  int ArrSize; // if bit 31 is set in `ArrNum`, this is 2nd dim of 2d array
  vuint8 *ArrData;

public:
  VScriptArray (const TArray<VStr> &xarr);

  inline int Num () const { return (ArrNum >= 0 ? ArrNum : (ArrNum&0x7fffffff)*(ArrSize&0x7fffffff)); }
  inline int length () const { return (ArrNum >= 0 ? ArrNum : (ArrNum&0x7fffffff)*(ArrSize&0x7fffffff)); }
  inline int length1 () const { return (ArrNum&0x7fffffff); }
  inline int length2 () const { return (ArrNum >= 0 ? (ArrNum ? 1 : 0) : (ArrSize&0x7fffffff)); }
  inline vuint8 *Ptr () { return ArrData; }
  inline bool Is2D () const { return (ArrNum < 0); }
  inline void Flatten () { if (Is2D()) { vint32 oldlen = length(); ArrSize = ArrNum = oldlen; } }
  void Clear (const VFieldType &Type);
  void Reset (const VFieldType &Type); // clear array, but don't resize
  void Resize (int NewSize, const VFieldType &Type);
  void SetNum (int NewNum, const VFieldType &Type, bool doShrink=true); // will convert to 1d
  void SetNumMinus (int NewNum, const VFieldType &Type);
  void SetNumPlus (int NewNum, const VFieldType &Type);
  void Insert (int Index, int Count, const VFieldType &Type);
  void Remove (int Index, int Count, const VFieldType &Type);
  void SetSize2D (int dim1, int dim2, const VFieldType &Type);
  vuint8 *Alloc (const VFieldType &Type);

  void SwapElements (int i0, int i1, const VFieldType &Type);
  int CallCompare (int i0, int i1, const VFieldType &Type, VObject *self, VMethod *fncmp);
  // only for flat arrays
  bool Sort (const VFieldType &Type, VObject *self, VMethod *fncmp);

  static int CallComparePtr (void *p0, void *p1, const VFieldType &Type, VObject *self, VMethod *fncmp);
};

// required for Vavoom C VM
static_assert(sizeof(VScriptArray) <= sizeof(void *)*3, "oops");


// ////////////////////////////////////////////////////////////////////////// //
// dictionary object, used in script executor
struct VScriptDictElem {
  enum {
    Flag_Destroy = 0x0001u,
  };

  void *value;
  VFieldType type;
  //vuint32 hash; // precalced
  vuint32 flags; // Flag_XXX

  VScriptDictElem () : value(nullptr), type(), /*hash(0),*/ flags(0) {}
  ~VScriptDictElem () { clear(); }
  VScriptDictElem (const VScriptDictElem &e) { e.copyTo(this); }
  VScriptDictElem &operator = (const VScriptDictElem &e) { if (&e != this) e.copyTo(this); return *this; }

  bool operator == (const VScriptDictElem &e) const;

  inline bool needDestroy () const { return !!(flags&Flag_Destroy); }
  inline void setDestroy (bool v) { if (v) flags |= Flag_Destroy; else flags &= ~Flag_Destroy; }

  // create from [pointer to] value
  // don't clear value on destroying
  static void CreateFromPtr (VScriptDictElem &e, void *ptr, const VFieldType &atype, bool calcHash);

  void clear ();
  void copyTo (VScriptDictElem *dest) const;

  // types that can be casted to/from pointer
  static inline bool isSimpleType (const VFieldType &tp) {
    switch (tp.Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Float:
      case TYPE_Bool:
      case TYPE_Name:
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
      case TYPE_String: // special
        return true;
    }
    return false;
  }

  static void streamSkip (VStream &strm);
  void Serialise (VStream &strm, const VFieldType &dtp);
  void Serialise (VStream &strm, const VFieldType &dtp) const;

  // always recalculating, never returns cached hash
  vuint32 calcHash () const;

  /*
  inline void updateHashCache () {
    hash = calcHash();
    flags |= Flag_Hashed;
  }
  */
};

vuint32 GetTypeHash (const VScriptDictElem &e);
//vuint32 GetTypeHash (VScriptDictElem &e);


class VScriptDict {
public:
  TMap<VScriptDictElem, VScriptDictElem> *map;

public:
  VScriptDict () : map(nullptr) {}
  VScriptDict (const VScriptDict &); // no wai
  VScriptDict &operator = (const VScriptDict &); // no wai

  int length () const;
  int capacity () const;

  void copyTo (VScriptDict *dest) const;

  void clear (); // this destroys `map`
  void reset (); // this resets `map`
  VScriptDictElem *find (const VScriptDictElem &key);
  // returns `true` if old value was replaced
  bool put (const VScriptDictElem &key, const VScriptDictElem &value);
  // returns `true` if value was deleted
  bool del (const VScriptDictElem &key);

  // returns `true` if something was cleaned
  bool cleanRefs ();

  //SLOW!
  VFieldType getKeyType () const;
  //SLOW!
  VFieldType getValueType () const;

  static void streamSkip (VStream &strm);
  void Serialise (VStream &strm, const VFieldType &dtp);
};

// required for Vavoom C VM
static_assert(sizeof(VScriptDict) == sizeof(void *), "oops");

VStream &operator << (VStream &strm, VScriptDict &dc);
VStream &operator << (VStream &, VFieldType &);


// ////////////////////////////////////////////////////////////////////////// //
struct FReplacedString {
  int Index;
  bool Replaced;
  VStr Old;
  VStr New;
};
