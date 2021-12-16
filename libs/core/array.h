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
//**  Dynamic array template.
//**
//**************************************************************************
//#define VAVOOM_CORELIB_ARRAY_MINIMAL_RESIZE

template<class T, bool usectordtor> class TArrayBase {
public:
  typedef TArrayBase<T, usectordtor> MyType;

private:
  // 2d info is from `VScriptArray`
  int ArrNum; // if bit 31 is set, this is 1st dim of 2d array
  int ArrSize; // if bit 31 is set in `ArrNum`, this is 2nd dim of 2d array
  T *ArrData;

public:
  VVA_ALWAYS_INLINE TArrayBase () noexcept : ArrNum(0), ArrSize(0), ArrData(nullptr) {}
  // This is a dummy constructor that does nothing. The purpose of this
  // is so you can create a global TArray in the data segment that gets
  // used by code before startup without worrying about the constructor
  // resetting it after it's already been used. You MUST NOT use it for
  // heap- or stack-allocated TArrays.
  //VVA_ALWAYS_INLINE TArrayBase (ENoInit) noexcept {}
  // alas, i cannot remove this
  VVA_ALWAYS_INLINE TArrayBase (const MyType &other) noexcept : ArrNum(0), ArrSize(0), ArrData(nullptr) { operator=(other); }

  VVA_ALWAYS_INLINE ~TArrayBase () noexcept { clear(); }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE int length1D () const noexcept { return (ArrNum >= 0 ? ArrNum : (ArrNum&0x7fffffff)*(ArrSize&0x7fffffff)); }

  // this is from `VScriptArray`
  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE bool Is2D () const noexcept { return (ArrNum < 0); }
  VVA_ALWAYS_INLINE void Flatten () noexcept { if (Is2D()) { int oldlen = length1D(); ArrSize = ArrNum = oldlen; } }

  template<bool AllowDtor=true> inline void clear () noexcept {
    if (ArrData) {
      if (AllowDtor && usectordtor) {
        Flatten(); // just in case
        for (int i = 0; i < ArrNum; ++i) ArrData[i].~T();
      }
      Z_Free(ArrData);
    }
    ArrData = nullptr;
    ArrNum = ArrSize = 0;
  }
  template<bool AllowDtor=true> VVA_ALWAYS_INLINE void Clear () noexcept { return clear<AllowDtor>(); }

  // doesn't free the array itself
  template<bool AllowDtor=true> inline void reset () noexcept {
    if (AllowDtor && usectordtor) {
      Flatten(); // just in case
      for (int f = 0; f < ArrNum; ++f) ArrData[f].~T();
    }
    ArrNum = 0;
  }

  // doesn't free the array itself
  inline void resetNoDtor () noexcept { return reset<false>(); }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE int Num () const noexcept { return ArrNum; }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE int Length () const noexcept { return ArrNum; }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE int length () const noexcept { return ArrNum; }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE int NumAllocated () const noexcept { return ArrSize; }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE int numAllocated () const noexcept { return ArrSize; }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE int capacity () const noexcept { return ArrSize; }

  // don't do any sanity checks here, `ptr()` can be used even on empty arrays
  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE T *Ptr () noexcept { return ArrData; }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE const T *Ptr () const noexcept { return ArrData; }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE T *ptr () noexcept { return ArrData; }
  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE const T *ptr () const noexcept { return ArrData; }

  inline void SetPointerData (void *adata, int datalen) noexcept {
    vassert(datalen >= 0);
    if (datalen == 0) {
      if (ArrData && adata) {
        Flatten();
        vassert((uintptr_t)adata < (uintptr_t)ArrData || (uintptr_t)adata >= (uintptr_t)(ArrData+ArrSize));
      }
      clear();
      if (adata) Z_Free(adata);
    } else {
      vassert(adata);
      vassert(datalen%(int)sizeof(T) == 0);
      if (ArrData) {
        Flatten();
        vassert((uintptr_t)adata < (uintptr_t)ArrData || (uintptr_t)adata >= (uintptr_t)(ArrData+ArrSize));
        clear();
      }
      ArrData = (T *)adata;
      ArrNum = ArrSize = datalen/(int)sizeof(T);
    }
  }

  // this changes only capacity, length will not be increased (but can be decreased)
  // new memory will not be cleared and inited (there's no need to do that here)
  void resize (int NewSize) noexcept {
    vassert(NewSize >= 0);

    if (NewSize <= 0) { clear(); return; }

    Flatten(); // just in case
    if (NewSize == ArrSize) return;

    // free unused elements
    if (usectordtor) for (int i = NewSize; i < ArrNum; ++i) ArrData[i].~T();

    // realloc buffer
    ArrData = (T *)Z_Realloc(ArrData, NewSize*sizeof(T));

    // no need to init new data, allocator will do it later
    ArrSize = NewSize;
    if (ArrNum > NewSize) ArrNum = NewSize;
  }
  VVA_ALWAYS_INLINE void Resize (int NewSize) noexcept { return resize(NewSize); }

  // reserve memory for at least the given number of items
  // will not shrink the array
  VVA_ALWAYS_INLINE void reserve (int maxsize) noexcept { if (maxsize > ArrSize) return resize(maxsize); }

  template<bool DoResize=true, bool DoReserve=false> void setLength (int NewNum) noexcept {
    vassert(NewNum >= 0);
    Flatten(); // just in case
    if (DoResize || NewNum > ArrSize) {
      #ifdef VAVOOM_CORELIB_ARRAY_MINIMAL_RESIZE
      resize(NewNum+(DoReserve ? 64 : 0));
      #else
      resize(NewNum+(DoReserve ? NewNum*3/8+(NewNum < 64 ? 64 : 0) : 0));
      #endif
    }
    vassert(ArrSize >= NewNum);
    if (ArrNum > NewNum) {
      // destroy freed elements
      if (usectordtor) for (int i = NewNum; i < ArrNum; ++i) ArrData[i].~T();
    } else if (ArrNum < NewNum) {
      // initialize new elements
      // it is faster to clear the whole block first
      if (usectordtor) {
        memset((void *)(ArrData+ArrNum), 0, (NewNum-ArrNum)*sizeof(T));
        for (int i = ArrNum; i < NewNum; ++i) new(&ArrData[i], E_ArrayNew, E_NoInit) T;
      }
    }
    ArrNum = NewNum;
  }
  //template<bool DoResize=true, bool DoReserve=false> VVA_ALWAYS_INLINE void SetNum (int NewNum) noexcept { return setLength<DoResize, DoReserve>(NewNum); }
  //template<bool DoResize=true, bool DoReserve=false> VVA_ALWAYS_INLINE void setNum (int NewNum) noexcept { return setLength<DoResize, DoReserve>(NewNum); }
  //template<bool DoResize=true, bool DoReserve=false> VVA_ALWAYS_INLINE void SetLength (int NewNum) noexcept { return setLength<DoResize, DoReserve>(NewNum); }

  // don't resize, if it is not necessary
  //template<bool DoResize=false> VVA_ALWAYS_INLINE void SetNumWithReserve (int NewNum) noexcept { return setLength<DoResize, true>(NewNum); }
  template<bool DoResize=false> VVA_ALWAYS_INLINE void setLengthReserve (int NewNum) noexcept { return setLength<DoResize, true>(NewNum); }

  VVA_ALWAYS_INLINE void setLengthNoResize (int NewNum) noexcept { return setLength<false, false>(NewNum); }

  inline void Condense () noexcept { return resize(ArrNum); }
  inline void condense () noexcept { return resize(ArrNum); }

  // this won't copy capacity (there is no reason to do it)
  // alas, i cannot remove this
  void operator = (const MyType &other) noexcept {
    if (&other == this) return; // oops
    vassert(!other.Is2D());
    clear();
    const int newsz = other.ArrNum;
    if (newsz) {
      ArrNum = ArrSize = newsz;
      ArrData = (T *)Z_MallocNoClear(newsz*sizeof(T));
      if (usectordtor) {
        // it is faster to clear the whole block first
        memset((void *)ArrData, 0, (size_t)newsz*sizeof(T));
        for (int i = 0; i < newsz; ++i) {
          new(&ArrData[i], E_ArrayNew, E_NoInit) T;
          ArrData[i] = other.ArrData[i];
        }
      } else {
        memcpy((void *)ArrData, (void *)other.ArrData, (size_t)newsz*sizeof(T));
      }
    }
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE T &operator [] (int index) noexcept {
    vassert(index >= 0 && index < ArrNum);
    return ArrData[index];
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE const T &operator [] (int index) const noexcept {
    vassert(index >= 0 && index < ArrNum);
    return ArrData[index];
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE T &last () noexcept {
    vassert(!Is2D());
    vassert(ArrNum > 0);
    return ArrData[ArrNum-1];
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT VVA_PURE const T &last () const noexcept {
    vassert(!Is2D());
    vassert(ArrNum > 0);
    return ArrData[ArrNum-1];
  }

  VVA_ALWAYS_INLINE VVA_CHECKRESULT T pop () noexcept {
    vassert(ArrNum > 0);
    T res = ArrData[ArrNum-1];
    drop();
    return res;
  }

  inline void drop () noexcept {
    vassert(!Is2D());
    if (ArrNum > 0) {
      --ArrNum;
      if (usectordtor) ArrData[ArrNum].~T();
    }
  }

  // slow!
  inline void chopFirst () noexcept {
    vassert(!Is2D());
    if (ArrNum > 0) removeAt(0);
  }

  inline void insert (int index, const T &item) noexcept {
    vassert(!Is2D());
    const int oldlen = ArrNum;
    setLengthReserve(oldlen+1);
    for (int i = oldlen; i > index; --i) ArrData[i] = ArrData[i-1];
    ArrData[index] = item;
  }
  VVA_ALWAYS_INLINE void Insert (int index, const T &item) noexcept { return insert(index, item); }

  inline int append (const T &item) noexcept {
    vassert(!Is2D());
    const int oldlen = ArrNum;
    setLengthReserve(oldlen+1);
    ArrData[oldlen] = item;
    return oldlen;
  }
  VVA_ALWAYS_INLINE int Append (const T &item) noexcept { return append(item); }

  template<bool DoClear=true>inline T &alloc () noexcept {
    vassert(!Is2D());
    const int oldlen = ArrNum;
    setLengthReserve(oldlen+1);
    if (!usectordtor && DoClear) memset((void *)(ArrData+oldlen), 0, sizeof(T));
    return ArrData[oldlen];
  }
  VVA_ALWAYS_INLINE T &Alloc () noexcept { return alloc(); }

  inline void removeAt (int index) noexcept {
    vassert(ArrData != nullptr);
    vassert(index >= 0 && index < ArrNum);
    Flatten(); // just in case
    --ArrNum;
    for (int i = index; i < ArrNum; ++i) ArrData[i] = ArrData[i+1];
    if (usectordtor) ArrData[ArrNum].~T();
  }
  VVA_ALWAYS_INLINE void RemoveIndex (int index) noexcept { return removeAt(index); }

  inline int remove (const T &item) noexcept {
    Flatten(); // just in case
    int count = 0;
    for (int i = 0; i < ArrNum; ++i) {
      if (ArrData[i] == item) { ++count; RemoveIndex(i--); }
    }
    return count;
  }
  VVA_ALWAYS_INLINE int Remove (const T &item) noexcept { return remove(item); }

  friend VStream &operator << (VStream &Strm, MyType &Array) /*noexcept*/ {
    vassert(!Array.Is2D());
    int NumElem = Array.length();
    Strm << STRM_INDEX(NumElem);
    if (Strm.IsLoading()) Array.setLength(NumElem);
    for (int i = 0; i < Array.length(); ++i) Strm << Array[i];
    return Strm;
  }

public:
  // range iteration
  // WARNING! don't add/remove array elements in iterator loop!

  VVA_ALWAYS_INLINE VVA_PURE T *begin () noexcept { return (length1D() > 0 ? ArrData : nullptr); }
  VVA_ALWAYS_INLINE VVA_PURE const T *begin () const noexcept { return (length1D() > 0 ? ArrData : nullptr); }
  VVA_ALWAYS_INLINE VVA_PURE T *end () noexcept { return (length1D() > 0 ? ArrData+length1D() : nullptr); }
  VVA_ALWAYS_INLINE VVA_PURE const T *end () const noexcept { return (length1D() > 0 ? ArrData+length1D() : nullptr); }

  #define VARR_DEFINE_ITEMS_ITERATOR(xconst_)  \
    class xconst_##RevIterator { \
    public: \
      xconst_ T *stvalue; \
      xconst_ T *currvalue; \
    public: \
      VVA_ALWAYS_INLINE xconst_##RevIterator (xconst_ MyType *arr) noexcept { \
        if (arr->length1D() > 0) { \
          stvalue = arr->ArrData; \
          currvalue = arr->ArrData+arr->length1D()-1; \
        } else { \
          stvalue = currvalue = nullptr; \
        } \
      } \
      VVA_ALWAYS_INLINE xconst_##RevIterator (const xconst_##RevIterator &it) noexcept : stvalue(it.stvalue), currvalue(it.currvalue) {} \
      VVA_ALWAYS_INLINE xconst_##RevIterator (const xconst_##RevIterator &/*it*/, bool /*asEnd*/) noexcept : stvalue(nullptr), currvalue(nullptr) {} \
      VVA_ALWAYS_INLINE xconst_##RevIterator begin () noexcept { return xconst_##RevIterator(*this); } \
      VVA_ALWAYS_INLINE xconst_##RevIterator end () noexcept { return xconst_##RevIterator(*this, true); } \
      VVA_ALWAYS_INLINE VVA_PURE bool operator == (const xconst_##RevIterator &b) const noexcept { return (currvalue == b.currvalue); } \
      VVA_ALWAYS_INLINE VVA_PURE bool operator != (const xconst_##RevIterator &b) const noexcept { return (currvalue != b.currvalue); } \
      VVA_ALWAYS_INLINE VVA_PURE xconst_ T& operator * () const noexcept { return *currvalue; } /* required for iterator */ \
      /*VVA_ALWAYS_INLINE VVA_PURE xconst_ T* operator -> () const noexcept { return currvalue; }*/ /* required for iterator */ \
      VVA_ALWAYS_INLINE void operator ++ () noexcept { if (currvalue && currvalue != stvalue) --currvalue; else currvalue = stvalue = nullptr; } /* this is enough for iterator */ \
    }; \
    VVA_ALWAYS_INLINE xconst_##RevIterator reverse () xconst_ noexcept { return xconst_##RevIterator(this); }

  VARR_DEFINE_ITEMS_ITERATOR()
  VARR_DEFINE_ITEMS_ITERATOR(const)
  #undef VARR_DEFINE_ITEMS_ITERATOR

  #define VARR_DEFINE_ITEMS_ITERATOR(xconst_)  \
    class xconst_##IndexIterator { \
    public: \
      xconst_ T *currvalue; \
      xconst_ T *endvalue; \
      int currindex; \
    public: \
      VVA_ALWAYS_INLINE xconst_##IndexIterator (xconst_ MyType *arr) noexcept { \
        if (arr->length1D() > 0) { \
          currvalue = arr->ArrData; \
          endvalue = currvalue+arr->length1D(); \
        } else { \
          currvalue = endvalue = nullptr; \
        } \
        currindex = 0; \
      } \
      VVA_ALWAYS_INLINE xconst_##IndexIterator (const xconst_##IndexIterator &it) noexcept : currvalue(it.currvalue), endvalue(it.endvalue), currindex(it.currindex) {} \
      VVA_ALWAYS_INLINE xconst_##IndexIterator (const xconst_##IndexIterator &it, bool /*asEnd*/) noexcept : currvalue(it.endvalue), endvalue(it.endvalue), currindex(it.currindex) {} \
      VVA_ALWAYS_INLINE xconst_##IndexIterator begin () noexcept { return xconst_##IndexIterator(*this); } \
      VVA_ALWAYS_INLINE xconst_##IndexIterator end () noexcept { return xconst_##IndexIterator(*this, true); } \
      VVA_ALWAYS_INLINE VVA_PURE bool operator == (const xconst_##IndexIterator &b) const noexcept { return (currvalue == b.currvalue); } \
      VVA_ALWAYS_INLINE VVA_PURE bool operator != (const xconst_##IndexIterator &b) const noexcept { return (currvalue != b.currvalue); } \
      VVA_ALWAYS_INLINE VVA_PURE xconst_##IndexIterator operator * () const noexcept { return xconst_##IndexIterator(*this); } /* required for iterator */ \
      VVA_ALWAYS_INLINE void operator ++ () noexcept { ++currvalue; ++currindex; } /* this is enough for iterator */ \
      VVA_ALWAYS_INLINE VVA_PURE xconst_ T &value () noexcept { return *currvalue; } \
      /*VVA_ALWAYS_INLINE VVA_PURE const T &value () const noexcept { return *currvalue; }*/ \
      VVA_ALWAYS_INLINE VVA_PURE int index () const noexcept { return currindex; } \
    }; \
    VVA_ALWAYS_INLINE xconst_##IndexIterator itemsIdx () xconst_ noexcept { return xconst_##IndexIterator(this); }

  VARR_DEFINE_ITEMS_ITERATOR()
  VARR_DEFINE_ITEMS_ITERATOR(const)
  #undef VARR_DEFINE_ITEMS_ITERATOR

  #define VARR_DEFINE_ITEMS_ITERATOR(xconst_)  \
    class xconst_##IndexIteratorRev { \
    public: \
      xconst_ MyType *arr; \
      int currindex; \
    public: \
      VVA_ALWAYS_INLINE xconst_##IndexIteratorRev (xconst_ MyType *aarr) noexcept : arr(aarr) { currindex = (arr->length1D() > 0 ? arr->length1D()-1 : -1); } \
      VVA_ALWAYS_INLINE xconst_##IndexIteratorRev (const xconst_##IndexIteratorRev &it) noexcept : arr(it.arr), currindex(it.currindex) {} \
      VVA_ALWAYS_INLINE xconst_##IndexIteratorRev (const xconst_##IndexIteratorRev &it, bool /*asEnd*/) noexcept : arr(it.arr), currindex(-1) {} \
      VVA_ALWAYS_INLINE xconst_##IndexIteratorRev begin () noexcept { return xconst_##IndexIteratorRev(*this); } \
      VVA_ALWAYS_INLINE xconst_##IndexIteratorRev end () noexcept { return xconst_##IndexIteratorRev(*this, true); } \
      VVA_ALWAYS_INLINE VVA_PURE bool operator == (const xconst_##IndexIteratorRev &b) const noexcept { return (arr == b.arr && currindex == b.currindex); } \
      VVA_ALWAYS_INLINE VVA_PURE bool operator != (const xconst_##IndexIteratorRev &b) const noexcept { return (arr != b.arr || currindex != b.currindex); } \
      VVA_ALWAYS_INLINE VVA_PURE xconst_##IndexIteratorRev operator * () const noexcept { return xconst_##IndexIteratorRev(*this); } /* required for iterator */ \
      VVA_ALWAYS_INLINE void operator ++ () noexcept { --currindex; } /* this is enough for iterator */ \
      VVA_ALWAYS_INLINE VVA_PURE xconst_ T &value () noexcept { return arr->ArrData[currindex]; } \
      /*VVA_ALWAYS_INLINE VVA_PURE const T &value () const noexcept { return arr->ArrData[currindex]; }*/ \
      VVA_ALWAYS_INLINE VVA_PURE int index () const noexcept { return currindex; } \
    }; \
    VVA_ALWAYS_INLINE xconst_##IndexIteratorRev itemsIdxRev () xconst_ noexcept { return xconst_##IndexIteratorRev(this); }

  VARR_DEFINE_ITEMS_ITERATOR()
  VARR_DEFINE_ITEMS_ITERATOR(const)
  #undef VARR_DEFINE_ITEMS_ITERATOR
};


// ////////////////////////////////////////////////////////////////////////// //
// this is used to hold "delay destroy" VObject indicies
// didn't found a better place for this
// this does allocations in 8KB chunks
// `T` should be a POD, as it won't be properly copied/destructed
// blocks are never moved on allocation
template<class T> class VQueueLifo {
private:
  /* block format:
     two last pointer-sized cells are reserved for "prev block" and "next block" pointers.
     everything else is used for objects.
   */
  enum {
    BlockSize = 8192U, // 8KB blocks
    ItemsPerBlock = (unsigned)((BlockSize-2*sizeof(void **))/sizeof(T)),
    PrevIndex = (unsigned)(BlockSize/sizeof(void **)-2),
    NextIndex = (unsigned)(BlockSize/sizeof(void **)-1),
  };

private:
  T *first; // first alloted block
  T *currblock; // currently using block
  unsigned blocksAlloted; // number of allocated blocks, for stats
  unsigned used; // total number of elements pushed

private:
  inline static T *getPrevBlock (T *blk) noexcept { return (blk ? (T *)(((void **)blk)[PrevIndex]) : nullptr); }
  inline static T *getNextBlock (T *blk) noexcept { return (blk ? (T *)(((void **)blk)[NextIndex]) : nullptr); }

  inline static void setPrevBlock (T *blk, T* ptr) noexcept { if (blk) ((void **)blk)[PrevIndex] = ptr; }
  inline static void setNextBlock (T *blk, T* ptr) noexcept { if (blk) ((void **)blk)[NextIndex] = ptr; }

  inline unsigned freeInCurrBlock () const noexcept { return (used%ItemsPerBlock ?: ItemsPerBlock); }

public:
  VV_DISABLE_COPY(VQueueLifo)
  inline VQueueLifo () noexcept : first(nullptr), currblock(nullptr), blocksAlloted(0), used(0) {}
  inline ~VQueueLifo () noexcept { clear(); }

  inline T operator [] (const int idx) noexcept {
    vassert(idx >= 0 && idx < used);
    unsigned bnum = (unsigned)idx/ItemsPerBlock;
    T *blk = first;
    while (bnum--) blk = getNextBlock(blk);
    return blk[(unsigned)idx%ItemsPerBlock];
  }

  inline int length () const noexcept { return (int)used; }
  inline int capacity () const noexcept { return (int)(blocksAlloted*ItemsPerBlock); }

  // free all pool memory
  void clear () noexcept {
    while (first) {
      T *nb = getNextBlock(first);
      Z_Free(first);
      first = nb;
    }
    first = currblock = nullptr;
    blocksAlloted = 0;
    used = 0;
  }

  // reset pool, but don't deallocate memory
  inline void reset () noexcept {
    currblock = first;
    used = 0;
  }

  // allocate new element to fill
  // won't clear it
  inline T *alloc () noexcept {
    if (currblock) {
      if (used) {
        const unsigned cbpos = freeInCurrBlock();
        if (cbpos < ItemsPerBlock) {
          // can use it
          ++used;
          return (currblock+cbpos);
        }
        // has next allocated block?
        T *nb = getNextBlock(currblock);
        if (nb) {
          currblock = nb;
          ++used;
          vassert(freeInCurrBlock() == 1);
          return nb;
        }
      } else {
        // no used items, yay
        vassert(currblock == first);
        ++used;
        return currblock;
      }
    } else {
      vassert(used == 0);
    }
    // need a new block
    vassert(getNextBlock(currblock) == nullptr);
    ++blocksAlloted;
    T *nblk = (T *)Z_Malloc(BlockSize);
    setPrevBlock(nblk, currblock);
    setNextBlock(nblk, nullptr);
    setNextBlock(currblock, nblk);
    if (!first) {
      vassert(used == 0);
      first = nblk;
    }
    currblock = nblk;
    ++used;
    vassert(freeInCurrBlock() == 1);
    return nblk;
  }

  inline void push (const T &value) noexcept { (*alloc()) = value; }
  inline void append (const T &value) noexcept { (*alloc()) = value; }

  // forget last element
  inline void pop () noexcept {
    vassert(used);
    if (freeInCurrBlock() == 1) {
      // go to previous block (but don't go beyond first one)
      T *pblk = getPrevBlock(currblock);
      if (pblk) currblock = pblk;
    }
    --used;
  }

  // forget last element
  inline T popValue () noexcept {
    vassert(used);
    T *res = getLast();
    pop();
    return *res;
  }

  // get pointer to last element (or `nullptr`)
  inline T *getLast () noexcept { return (used ? currblock+freeInCurrBlock()-1 : nullptr); }
};

template<typename T> using TArray = TArrayBase<T, true>;
template<typename T> using TArrayNC = TArrayBase<T, false>;
