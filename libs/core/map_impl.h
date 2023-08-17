//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
//**
//**  Template for mapping keys to values.
//**
//**  Robin Hood Hash Table Implementation by Ketmar Dark
//**
//**************************************************************************
// define this to skip calling ctors and dtors on keys and values (can be useful for PODs)
// note that both keys and values will be zeroed before using anyway
//#define TMAP_NO_CLEAR

// bucket mapping algos
// see https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
//#define TMAP_BKHASH_FASTRANGE
// use hash folding for hash tables with less than 64K items
// this is slightly slower than no folding (one more branch, one addition, and one shift),
// but may help with weaker hash functions
//#define TMAP_BKHASH_FOLDING

// use seed to avoid hash attacks and problematic hashes?
// tbh, i don't think that it will do any good for us
//#define TMAP_USE_SEED


// the table will not allocate more than "load factor" entries in the entry array
// (because it will never use more entries anyway).
//
// also, note that entries array is contiguous, and will be resized when needed,
// so stored keys and values may change their addresses. do not use keys or values
// that are storing pointers to themselves, or something like that. no notification
// methods will be called on such moves.
//
// both keys and values will be *copied* to the entry array. when `put()` replaces
// the value, no key copying will be done.


// ////////////////////////////////////////////////////////////////////////// //
#ifndef TMap_Class_Name
# error do not include this directly!
#endif


// ////////////////////////////////////////////////////////////////////////// //
template<typename TK, typename TV> class TMap_Class_Name {
private:
  enum {
    InitSize = 256, // *MUST* be power of two
    LoadFactorPrc = 80, // 90 it is ok for robin hood hashes, but let's play safe
    // maximum number of buckets (entries) we can have; more than enough
    MaxBuckets = 0x40000000u,
    // "valid hash" or-mask
    ValidHashBit = 0x80000000u,
    HashMask = 0x7fffffffu,
  };
  // if we don't want to use seed, let the compiler optimise `+mSeed` to noop
  #ifndef TMAP_USE_SEED
  enum { mSeed = 0u };
  #endif

  static_assert(LoadFactorPrc > 50 && LoadFactorPrc < 100, "invalid hash table load factor");

public:
  // if `x` is already POT, returns `x`
  static VVA_FORCEINLINE VVA_CONST VVA_CHECKRESULT uint32_t nextPOTU32 (const uint32_t x) noexcept {
    uint32_t res = x;
    res |= (res>>1);
    res |= (res>>2);
    res |= (res>>4);
    res |= (res>>8);
    res |= (res>>16);
    // already pot?
    if (x != 0u && (x&(x-1)) == 0u) res &= ~(res>>1); else ++res;
    return res;
  }

  #ifdef TMAP_USE_SEED
  static VVA_FORCEINLINE VVA_CONST VVA_CHECKRESULT uint32_t hashU32 (const uint32_t a) noexcept {
    uint32_t res = a;
    res -= (res<<6);
    res = res^(res>>17);
    res -= (res<<9);
    res = res^(res<<4);
    res -= (res<<3);
    res = res^(res<<10);
    res = res^(res>>15);
    return res;
  }
  #endif

private:
  // bucket entry
  // this keeps entry hash duplicate, so we can avoid accessing entries on linear scans
  // note that finding keys almost always doing linear scans, especially on heavily loaded tables
  struct __attribute__((packed)) TBucket {
    uint32_t hash; // duplicate of bucket hash, to improve CPU cache lines locality; 0 means "empty bucket"
    uint32_t entryidx; // index in `mEntries` array when the bucket is used, nothing when the bucket is unused
  };
  static_assert(sizeof(TBucket) == sizeof(uint32_t)*2, "invalid TBucket struct size");

  // `hashOrFLink` is used as hash (when bit 31 is set), or as index to the next free entry +1
  // (i.e. 0 means "null", 1 means "entry 0", etc.)
  // as we aren't using full hash bits anyway, we can sacrifice one bit to remove one pointer
  struct TEntry {
    uint32_t hashOrFLink; // see above
    TK key; // copied key
    TV value; // copied value

    VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT bool isEmpty () const noexcept { return (hashOrFLink < ValidHashBit); }
    VVA_FORCEINLINE VVA_PURE VVA_CHECKRESULT bool isUsed () const noexcept { return (hashOrFLink >= ValidHashBit); }

    #if !defined(TMAP_NO_CLEAR)
    // will not clear hash
    VVA_FORCEINLINE void initEntry () noexcept {
      // the entry is NOT guaranteed to be zeroed, so use clearing new
      new(&key, E_ArrayNew, E_ArrayNew) TK;
      new(&value, E_ArrayNew, E_ArrayNew) TV;
    }

    // will not clear hash
    VVA_FORCEINLINE void destroyEntry () noexcept {
      key.~TK();
      value.~TV();
    }
    #endif
  } /*__attribute__((aligned(4)))*/;

private:
  uint32_t mBSize; // size of `mBuckets` array, in elements
  uint32_t mESize; // size of `mEntries` array, in elements
  TEntry *mEntries; // this array holds entries
  TBucket *mBuckets; // and this array is actual hash table "buckets" (key/entry index pairs)
  uint32_t mBucketsUsed; // total number of used buckets (number of alive entries in the hash table)
  uint32_t mFreeEntryHead; // head of the list of free entries+1 (allocated with the FIFO strategy)
  int mFirstEntry, mLastEntry; // used range of `mEntries` (`mFirstEntry` can be negative for empty table)
  uint32_t mCurrentMaxLoad; // cached current max table load (max number of used buckets before resize)
  #ifdef TMAP_USE_SEED
  uint32_t mSeed; // current seed
  #endif
  /*
    first and last entry indices are low and high (inclusive) bounds of the used part of `mEntries`.
    it is used to speedup iteration.
    for empty table, both indicies are `-1` (it is important!).

    deleted entry is added to the freelist, and will be reused on the next insertion.
    our freelist is working as a FIFO queue (much like a stack of free items).
    list head is a pointer, because entry table will be inevitably rehashed after resizing, and
    rehashing will fix the pointer. there is no need to use indexing here.

    we can split bucket array to two, holding hashes and indicies separately. this way linear scans
    will load more hashes, and we'll access the indicies array only when our hash check is passed.
    note that even with the current combined scheme we'll only have about two cache misses even for
    quite huge tables. also, bucket array is trivially addressable with a simple scaled machine
    instruction. and, when we'll find a good hash, entry index will be in CPU cache, ready to use.
    the only downside is that "resetting" the table needs to clear twice as much data, but i believe
    that it is neglible.
   */
  #ifdef CORE_MAP_TEST
  mutable uint32_t mMaxProbeCount;
  mutable uint32_t mMaxCompareCount;
  #endif

private:
  // calculate hash for the given key
  // must never return zero, because zero is used as "empty marker"

  //k8: don't even fuckin' ask me. shitplusplus is not a language,
  //k8: it is a stinking pile of shit.
  #ifdef VV_SHITPP_FUCKED_TMAP_TRAIT
  template <typename TKT> static VVA_FORCEINLINE VVA_CHECKRESULT typename fuck_enable_if<HasTypeHash<TKT>::value, uint32_t>::type calcKeyHash (const TKT &akey) noexcept {
    #ifdef VV_SHITPP_FUCKED_TMAP_TRAIT_DEBUG
    VV_SHITPP_PRINT_TPN("000");
    #endif
    return akey.TypeHash()|ValidHashBit;
  }

  template <typename TKT> static VVA_FORCEINLINE VVA_CHECKRESULT typename fuck_enable_if<!HasTypeHash<TKT>::value, uint32_t>::type calcKeyHash (const TKT &akey) noexcept {
    #ifdef VV_SHITPP_FUCKED_TMAP_TRAIT_DEBUG
    VV_SHITPP_PRINT_TPN("001");
    #endif
    return GetTypeHash(akey)|ValidHashBit;
  }
  #else
  static VVA_FORCEINLINE VVA_CHECKRESULT uint32_t calcKeyHash (const TK &akey) noexcept {
    // FUCK YOU, SHITPLUSPLUS! WHY CAN'T WE FUCKIN' DECLARE THIS FUCKIN' TEMPLATE,
    // AND THEN USE IT TO DETECT THINGS THAT WERE DECLARED LATER?!!
    //const uint32_t khash = CalcTypeHash(akey);
    // hash should always have bit 31 set
    //const uint32_t khash = GetTypeHash(akey);
    //return khash+(!khash); // avoid zero hash value
    return GetTypeHash(akey)|ValidHashBit;
  }
  #endif

  // calculate desired bucket index for the given hash
  VVA_FORCEINLINE VVA_CHECKRESULT uint32_t calcBestBucketIdx (const uint32_t hashval, const uint32_t bhigh) const noexcept {
    #if defined(TMAP_BKHASH_FASTRANGE)
    return (uint32_t)(((uint64_t)((hashval&HashMask)+mSeed)*(uint64_t)bhigh)>>32);
    #elif defined(TMAP_BKHASH_FOLDING)
    // fold hash values for hash tables with less than 64K items
    // hash folding usually gives better results than simply dropping high bits
    return ((bhigh <= 0xffffu ? foldHash32to16(hashval&HashMask) : (hashval&HashMask))+mSeed)&bhigh;
    #else
    return (hashval+mSeed)&bhigh;
    #endif
  }

  // this is done in almost any function, so let's declare it as a macro
  #define TMAP_IMPL_CALC_BUCKET_INDEX(hashval_)  \
    const uint32_t bhigh = (uint32_t)(mBSize-1u); \
    uint32_t idx = calcBestBucketIdx((hashval_), bhigh);

private:
  // when seeding is disabled, this will be optimised to noop
  #ifdef TMAP_USE_SEED
  VVA_FORCEINLINE void genNewSeed () noexcept {
    mSeed = hashU32(Z_GetHashTableSeed());
  }
  #endif

  // calculate max table load for the given bucket array size
  VVA_FORCEINLINE VVA_CONST VVA_CHECKRESULT uint32_t calcMaxLoad (const uint32_t aBSize) const noexcept {
    return aBSize*LoadFactorPrc/100u;
  }

  // calc the size of the entry array for the given bucket array size, according to desired table load factor
  VVA_FORCEINLINE VVA_CONST VVA_CHECKRESULT uint32_t calcOptimalEntriesCount (const uint32_t aBSize) const noexcept {
    return calcMaxLoad(aBSize)+2u; // 2 just in case
  }

  // calculate the minimal size of the current hash table, with respect to load factor
  // can return size bigger than the current one
  VVA_CHECKRESULT uint32_t calcCompactedSize () const noexcept {
    if (mBucketsUsed == 0) return 0u; // if we have no entries, we need no memory
    uint32_t newsz = nextPOTU32((uint32_t)mBucketsUsed);
    if (newsz < InitSize) return InitSize; // never smaller
    // this can happen when we have POT number of entries
    if (newsz == mBucketsUsed) {
      if (newsz >= MaxBuckets) return newsz;
      newsz <<= 1;
    }
    if (mBucketsUsed >= MaxBuckets) return mBSize; // cannot grow anymore
    // if new size (which is POT here) is enough for the current bucket count, use it
    if (calcMaxLoad(newsz) >= mBucketsUsed+(mBucketsUsed>>2)) return newsz;
    // sadly, it is not enough, we have to use next POT value
    if (newsz >= MaxBuckets) return newsz; // oops, too much
    return (newsz<<1);
  }

  // update `mCurrentMaxLoad` field
  VVA_FORCEINLINE void calcCurrentMaxLoad (const uint32_t aBSize) noexcept {
    mCurrentMaxLoad = calcMaxLoad(aBSize);
  }

public:
  // entry iterator
  // do not rehash/compact/destroy the table while any iterator is used
  // but you can call `removeCurrentNoAdvance()` iterator method to remove current entry (w/o advancing the iterator)
  // iterator index will point to an invalid entry in this case, so you should call `++it` immediately after that
  class TIterator {
  private:
    TMap_Class_Name *map;
    int32_t index; // `-1` means "end iterator"

  public:
    // ctor
    VVA_FORCEINLINE TIterator (TMap_Class_Name *amap) noexcept : map(amap), index(-1) { vassert(amap); resetToFirst(); }

    // special ctor that will create "end pointer"
    VVA_FORCEINLINE TIterator (const TIterator &src, bool /*dummy*/) noexcept : map(src.map), index(-1) {}

    VVA_FORCEINLINE TIterator (const TIterator &src) noexcept : map(src.map), index(src.index) {}
    VVA_FORCEINLINE void operator = (const TIterator &src) noexcept { if (&src != this) { map = src.map; index = src.index; } }

    VVA_FORCEINLINE void resetToFirst () noexcept {
      if (map->mFirstEntry >= 0) {
        index = map->mFirstEntry;
        while (index <= map->mLastEntry && map->mEntries[(unsigned)index].isEmpty()) ++index;
        if (index > map->mLastEntry) index = -1;
      } else {
        index = -1;
      }
    }

    // convert to bool
    // returns `false` for "end pointer"
    VVA_FORCEINLINE operator bool () const noexcept { return (index >= 0 && index <= map->mLastEntry); }

    // next (prefix increment)
    VVA_FORCEINLINE void operator ++ () noexcept {
      if (index >= 0 && index < map->mLastEntry) {
        ++index;
        while (index <= map->mLastEntry && map->mEntries[(unsigned)index].isEmpty()) ++index;
        if (index > map->mLastEntry) index = -1;
      } else {
        index = -1;
      }
    }

    // is this iterator valid (i.e. didn't hit the end)?
    // note that `isValid()` may return `false` after calling `removeCurrentNoAdvance()`!
    VVA_FORCEINLINE VVA_CHECKRESULT bool isValid () const noexcept { return (index >= 0 && index <= map->mLastEntry); }

    // is current entry valid (i.e. not removed)?
    VVA_FORCEINLINE VVA_CHECKRESULT bool isValidEntry () const noexcept { return (index >= 0 && index <= map->mLastEntry && map->mEntries[(unsigned)index].isUsed()); }

    // `foreach` interface
    VVA_FORCEINLINE TIterator begin () noexcept { return TIterator(*this); }
    VVA_FORCEINLINE TIterator end () noexcept { return TIterator(*this, true); }
    VVA_FORCEINLINE bool operator != (const TIterator &b) const noexcept { return (map != b.map || index != b.index); } /* used to compare with end */
    VVA_FORCEINLINE TIterator operator * () const noexcept { return TIterator(*this); } /* required for iterator */

    // key/value getters
    VVA_FORCEINLINE VVA_CHECKRESULT const TK &GetKey () const noexcept { return map->mEntries[(unsigned)index].key; }
    VVA_FORCEINLINE VVA_CHECKRESULT const TV &GetValue () const noexcept { return map->mEntries[(unsigned)index].value; }
    VVA_FORCEINLINE VVA_CHECKRESULT TV &GetValue () noexcept { return map->mEntries[(unsigned)index].value; }
    VVA_FORCEINLINE VVA_CHECKRESULT const TK &getKey () const noexcept { return map->mEntries[(unsigned)index].key; }
    VVA_FORCEINLINE VVA_CHECKRESULT const TV &getValue () const noexcept { return map->mEntries[(unsigned)index].value; }
    VVA_FORCEINLINE VVA_CHECKRESULT TV &getValue () noexcept { return map->mEntries[(unsigned)index].value; }

    VVA_FORCEINLINE VVA_CHECKRESULT const TK &key () const noexcept { return map->mEntries[(unsigned)index].key; }
    VVA_FORCEINLINE VVA_CHECKRESULT const TV &value () const noexcept { return map->mEntries[(unsigned)index].value; }
    VVA_FORCEINLINE VVA_CHECKRESULT TV &value () noexcept { return map->mEntries[(unsigned)index].value; }

    // remove current entry, but don't advance the iterator
    // it is safe to call this several times
    VVA_FORCEINLINE void removeCurrentNoAdvance () noexcept {
      if (index >= 0 && index <= map->mLastEntry && map->mEntries[(unsigned)index].isUsed()) {
        (void)map->delInternal(map->mEntries[(unsigned)index].key, map->mEntries[(unsigned)index].hashOrFLink);
      }
    }

    VVA_FORCEINLINE void RemoveCurrentNoAdvance () noexcept { return removeCurrentNoAdvance(); }
  };

  friend class TIterator;

public:
  // this is for VavoomC VM
  VVA_FORCEINLINE VVA_CHECKRESULT bool isValidIIdx (int index) const noexcept {
    return (index >= 0 && index <= mLastEntry && mEntries[index].isUsed());
  }

  // this is for VavoomC VM
  VVA_FORCEINLINE VVA_CHECKRESULT int getFirstIIdx () const noexcept {
    if (mFirstEntry >= 0) {
      int index = mFirstEntry;
      while (index <= mLastEntry && mEntries[index].isEmpty()) ++index;
      return (index <= mLastEntry ? index : -1);
    } else {
      return -1;
    }
  }

  // this is for VavoomC VM
  // <0: done
  VVA_FORCEINLINE VVA_CHECKRESULT int getNextIIdx (int index) const noexcept {
    if (index >= 0 && index <= mLastEntry) {
      ++index;
      while (index <= mLastEntry && mEntries[index].isEmpty()) ++index;
      return (index <= mLastEntry ? index : -1);
    } else {
      return -1;
    }
  }

  // this is for VavoomC VM
  VVA_FORCEINLINE VVA_CHECKRESULT int removeCurrAndGetNextIIdx (int index) noexcept {
    if (index >= 0 && index <= mLastEntry) {
      if (mEntries[index].isUsed()) {
        (void)delInternal(mEntries[index].key, mEntries[index].hashOrFLink);
      }
      return getNextIIdx(index);
    } else {
      return -1;
    }
  }

  // this is for VavoomC VM
  VVA_FORCEINLINE VVA_CHECKRESULT const TK *getKeyIIdx (int index) const noexcept {
    return (isValidIIdx(index) ? &mEntries[index].key : nullptr);
  }

  // this is for VavoomC VM
  VVA_FORCEINLINE VVA_CHECKRESULT TV *getValueIIdx (int index) const noexcept {
    return (isValidIIdx(index) ? &mEntries[index].value : nullptr);
  }

private:
  // called from `reset()` and `clear()`
  // the caller is responsive for zeroing buckets
  #if defined(TMAP_NO_CLEAR)
  VVA_FORCEINLINE
  #endif
  void freeEntries () noexcept {
    #if !defined(TMAP_NO_CLEAR)
    if (mFirstEntry >= 0) {
      const int end = mLastEntry;
      TEntry *e = &mEntries[mFirstEntry];
      for (int f = mFirstEntry; f <= end; ++f, ++e) if (e->isUsed()) e->destroyEntry();
    }
    #endif
    mFreeEntryHead = 0;
    mFirstEntry = mLastEntry = -1;
  }

  VVA_FORCEINLINE TEntry *allocEntry () noexcept {
    TEntry *res;
    if (!mFreeEntryHead) {
      // no free entries to reuse
      vassert(mESize > 0);
      // it is guaranteed to have a room for at least one entry here (see `put()`)
      ++mLastEntry; // it could be `-1`, and will become valid `0` here
      vassert((uint32_t)mLastEntry < mESize);
      if (mFirstEntry < 0) mFirstEntry = 0;
      res = &mEntries[mLastEntry];
    } else {
      res = &mEntries[mFreeEntryHead-1U];
      mFreeEntryHead = res->hashOrFLink;
      #ifdef CORE_MAP_TEST
      vassert(mFreeEntryHead < ValidHashBit);
      #endif
      // fix mFirstEntry and mLastEntry
      const int idx = (int)(ptrdiff_t)(res-&mEntries[0]);
      if (mFirstEntry < 0 || idx < mFirstEntry) mFirstEntry = idx;
      if (idx > mLastEntry) mLastEntry = idx;
    }
    //res->nextFree = nullptr; // just in case; meh, nobody cares
    // the entry is NOT guaranteed to be zeroed
    #if !defined(TMAP_NO_CLEAR)
    res->initEntry(); // this will zero the entry, and construct defaults
    #endif
    return res;
  }

  void releaseEntry (const int idx) noexcept {
    TEntry *e = &mEntries[(const uint32_t)idx];
    #if !defined(TMAP_NO_CLEAR)
    e->destroyEntry();
    #endif
    // put it into the free list (this also marks it as unused)
    e->hashOrFLink = mFreeEntryHead;
    mFreeEntryHead = (const uint32_t)idx+1U;
    // fix mFirstEntry and mLastEntry
    if (mFirstEntry == mLastEntry) {
      // it was the last used entry, the whole array is free now
      mFreeEntryHead = 0;
      mFirstEntry = mLastEntry = -1;
    } else {
      // we have at least one another non-empty entry
      // fix first entry index
      if (idx == mFirstEntry) {
        int cidx = idx+1;
        while (mEntries[cidx].isEmpty()) ++cidx;
        mFirstEntry = cidx;
      }
      // fix last entry index
      if (idx == mLastEntry) {
        int cidx = idx-1;
        while (mEntries[cidx].isEmpty()) --cidx;
        mLastEntry = cidx;
      }
    }
  }

  // calculate distance to the "ideal" bucket
  VVA_FORCEINLINE VVA_CHECKRESULT uint32_t distToDesiredBucket (const uint32_t idx, const uint32_t bhigh) const noexcept {
    const uint32_t bestidx = calcBestBucketIdx(mBuckets[idx].hash, bhigh);
    //return (bestidx <= idx ? idx-bestidx : idx+(bhigh+1u-bestidx));
    // this is exactly the same as the code above, because we need distance in "positive direction"
    // (i.e. if `bestidx` is higher than `idx`, we should walk to the end of the array, and then to `bestidx`)
    // this is basically `(idx+(bhigh+1)-bestidx)&bhigh`
    // by rewriting it, we'll get `((idx|(bhigh+1))-bestidx)&bhigh`
    // i.e. we're setting the highest bit, and then masking it away, which is noop
    return ((idx-bestidx)&bhigh);
  }

  // `put()` helper (also used in `rehash()`)
  void putEntryInternal (uint32_t swpeidx) noexcept {
    uint32_t swpehash = mEntries[swpeidx].hashOrFLink;
    #ifdef CORE_MAP_TEST
    vassert(swpehash >= ValidHashBit);
    #endif
    // `swpehash` and `swpeidx` is the "current bucket" to insert
    TMAP_IMPL_CALC_BUCKET_INDEX(swpehash)
    uint32_t pcur = 0u;
    for (uint32_t dist = 0u; dist <= bhigh; ++dist) {
      if (mBuckets[idx].hash == 0u) {
        // found free bucket, put "current bucket" there
        mBuckets[idx].hash = swpehash;
        mBuckets[idx].entryidx = swpeidx;
        ++mBucketsUsed;
        return;
      }
      const uint32_t pdist = distToDesiredBucket(idx, bhigh);
      if (pcur > pdist) {
        // swap the "current bucket" with the one at the current index
        const uint32_t tmphash = mBuckets[idx].hash;
        const uint32_t tmpidx = mBuckets[idx].entryidx;
        mBuckets[idx].hash = swpehash;
        mBuckets[idx].entryidx = swpeidx;
        swpehash = tmphash;
        swpeidx = tmpidx;
        pcur = pdist;
      }
      idx = (idx+1u)&bhigh;
      ++pcur;
    }
    __builtin_trap(); // we should not come here, ever
  }

  // helper for `del()`, and iterator deletions
  // key hash must be known
  bool delInternal (const TK &akey, const uint32_t khash) noexcept {
    if (mBucketsUsed == 0u) return false;

    TMAP_IMPL_CALC_BUCKET_INDEX(khash)

    // find the key
    if (mBuckets[idx].hash == 0u) return false; // no such key
    bool res = false;
    for (uint32_t dist = 0u; dist <= bhigh; ++dist) {
      if (mBuckets[idx].hash == 0u) break;
      const uint32_t pdist = distToDesiredBucket(idx, bhigh);
      if (dist > pdist) break;
      res = (mBuckets[idx].hash == khash && mEntries[mBuckets[idx].entryidx].key == akey);
      if (res) break;
      idx = (idx+1u)&bhigh;
    }
    if (!res) return false; // key not found

    releaseEntry((int)mBuckets[idx].entryidx);

    if (--mBucketsUsed) {
      // there were more than one item, perform backwards shift
      // we'll shift until we either find an empty bucket, or
      // until next bucket distance is not ideal; this way
      // our table will look like the removed entry was never
      // inserted, and we don't need any tombstones; this also
      // decreases distance-to-initial-bucket metric, which is
      // very important for good performance
      // see http://codecapsule.com/2013/11/17/robin-hood-hashing-backward-shift-deletion/
      uint32_t idxnext = (idx+1u)&bhigh;
      for (uint32_t dist = 0u; dist <= bhigh; ++dist) {
        if (mBuckets[idxnext].hash == 0u) { mBuckets[idx].hash = 0u; break; }
        const uint32_t pdist = distToDesiredBucket(idxnext, bhigh);
        if (pdist == 0u) { mBuckets[idx].hash = 0u; break; }
        mBuckets[idx] = mBuckets[idxnext];
        idx = idxnext; //(idx+1u)&bhigh; // `idxnext` is always next index, there's no need to recalculate it
        idxnext = (idxnext+1u)&bhigh;
      }
    } else {
      // there was only one item
      mBuckets[idx].hash = 0u;
    }

    return true;
  }

public:
  VVA_FORCEINLINE TMap_Class_Name () noexcept
    : mBSize(0)
    , mESize(0)
    , mEntries(nullptr)
    , mBuckets(nullptr)
    , mBucketsUsed(0)
    , mFreeEntryHead(0)
    , mFirstEntry(-1)
    , mLastEntry(-1)
    , mCurrentMaxLoad(0)
    #ifdef TMAP_USE_SEED
    , mSeed(0u)
    #endif
    #ifdef CORE_MAP_TEST
    , mMaxProbeCount(0u)
    , mMaxCompareCount(0u)
    #endif
  {
  }

  // no copies
  TMap_Class_Name (TMap_Class_Name &other) = delete;
  TMap_Class_Name &operator = (const TMap_Class_Name &other) = delete;

  VVA_FORCEINLINE ~TMap_Class_Name () noexcept { clear(); }

  bool operator == (const TMap_Class_Name &other) const = delete;
  bool operator != (const TMap_Class_Name &other) const = delete;
  bool operator <= (const TMap_Class_Name &other) const = delete;
  bool operator >= (const TMap_Class_Name &other) const = delete;
  bool operator < (const TMap_Class_Name &other) const = delete;
  bool operator > (const TMap_Class_Name &other) const = delete;

  // clear the table, free all table memory
  void clear () noexcept {
    // no need to call this to zero memory that will be freed anyway
    #if !defined(TMAP_NO_CLEAR)
    freeEntries();
    #endif
    if (mBuckets) Z_Free(mBuckets);
    if (mEntries) Z_Free((void *)mEntries);
    mBucketsUsed = 0u;
    mBSize = mESize = mCurrentMaxLoad = 0u;
    mBuckets = nullptr;
    mEntries = nullptr;
    mFreeEntryHead = 0;
    mFirstEntry = mLastEntry = -1;
    #ifdef CORE_MAP_TEST
    mMaxProbeCount = 0u;
    mMaxCompareCount = 0u;
    #endif
  }

  // clear the table, but don't free memory, and don't shrink arrays
  VVA_FORCEINLINE void reset () noexcept {
    freeEntries();
    if (mBucketsUsed) {
      memset(mBuckets, 0, mBSize*sizeof(TBucket)); // sadly, we have to
      mBucketsUsed = 0u;
    }
    #ifdef CORE_MAP_TEST
    mMaxProbeCount = 0u;
    mMaxCompareCount = 0u;
    #endif
  }

  // "full rehash" is used in VavoomC
  enum {
    // don't recompute key hashes
    RehashDefault = 0u,
    // recompute key hashes, leave first conflicting entry
    RehashRekey = 1u,
    // masks, should be bitored with the above
    // leave first conflicting entry
    RehashLeaveFirst = 0u,
    // leave last conflicting entry
    RehashLeaveLast = 2u,
  };

  template<unsigned Flags=RehashDefault> void rehash () noexcept {
    // clear buckets
    if (mBSize) {
      memset(mBuckets, 0, mBSize*sizeof(TBucket));
      // change seed, to minimize pathological cases
      #ifdef TMAP_USE_SEED
      genNewSeed();
      #endif
    }
    mBucketsUsed = 0u;
    // reinsert entries
    mFreeEntryHead = 0;
    if (mFirstEntry >= 0) {
      bool fixFirstLast = false; // if we destroyed some entries, perform full recalc
      vassert(mLastEntry >= mFirstEntry);
      uint32_t lastfree = 0;
      // small optimisation for empty head case
      const uint32_t stx = (uint32_t)mFirstEntry;
      TEntry *e = &mEntries[0];
      if (stx) {
        // they should be already marked due to how `releaseEntry()` works
        #ifdef CORE_MAP_TEST
        vassert(e->isEmpty());
        #endif
        lastfree = mFreeEntryHead = 1U;
        ++e;
        // `eidt` is incremented by one here
        for (uint32_t eidt = 2U; eidt <= stx; ++eidt, ++e) {
          // they should be already marked due to how `releaseEntry()` works
          #ifdef CORE_MAP_TEST
          vassert(e->isEmpty());
          #endif
          mEntries[lastfree-1U].hashOrFLink = eidt;
          lastfree = eidt;
        }
      }
      // reinsert all alive entries
      const uint32_t end = (uint32_t)mLastEntry;
      vassert(e == &mEntries[stx]);
      for (uint32_t eidx = stx; eidx <= end; ++eidx, ++e) {
        if (e->isEmpty()) {
          // add current empty entry to the free list
          if (lastfree) mEntries[lastfree-1U].hashOrFLink = eidx+1U; else mFreeEntryHead = eidx+1U;
          lastfree = eidx+1U;
          continue;
        }
        // `e` is not empty here
        if ((Flags&RehashRekey) == 0) {
          // no need to recalculate hash, just insert the entry
          putEntryInternal(eidx);
        } else {
          // need to recalculate hash
          const uint32_t khash = calcKeyHash(e->key);
          e->hashOrFLink = khash; // this also marks it as non-free
          TMAP_IMPL_CALC_BUCKET_INDEX(khash)
          // check if we already have this key
          uint32_t oldbidx = 0u; // bucketindex+1, or zero if not found
          if (mBucketsUsed && mBuckets[idx].hash) {
            for (uint32_t dist = 0u; dist <= bhigh; ++dist) {
              if (mBuckets[idx].hash == 0u) break;
              const uint32_t pdist = distToDesiredBucket(idx, bhigh);
              if (dist > pdist) break;
              if (mBuckets[idx].hash == khash && mEntries[mBuckets[idx].entryidx].key == e->key) {
                // i found her!
                oldbidx = idx+1u;
                break;
              }
              idx = (idx+1u)&bhigh;
            }
          }
          if (!oldbidx) {
            // no conflicts, just insert the entry
            putEntryInternal(eidx);
            continue;
          }
          // found conflicting entry
          fixFirstLast = true; // we'll need to perform the full recalc
          --oldbidx; // because bucket index is incremented by one in the above loop
          if (Flags&RehashLeaveLast) {
            // destroy old entry
            const uint32_t oeidx = mBuckets[oldbidx].entryidx;
            vassert(oeidx != eidx);
            #if !defined(TMAP_NO_CLEAR)
            mEntries[oeidx].destroyEntry();
            #endif
            // add old entry to the free list
            if (lastfree) mEntries[lastfree-1U].hashOrFLink = oeidx+1U; else mFreeEntryHead = oeidx+1U;
            lastfree = oeidx+1U;
            // change old entry index to the new entry index
            mBuckets[oldbidx].entryidx = eidx;
          } else {
            // destroy new entry
            #if !defined(TMAP_NO_CLEAR)
            e->destroyEntry();
            #endif
            e->hashOrFLink = 0; // mark this entry as free (and the end of the free list)
            // add new entry to the free list
            if (lastfree) mEntries[lastfree-1U].hashOrFLink = eidx+1U; else mFreeEntryHead = eidx+1U;
            lastfree = eidx+1U;
          }
        }
      }
      // done inserting, check if we need to recalc first and last indicies
      if (fixFirstLast) {
        // full recalc
        lastfree = mFreeEntryHead = 0;
        if (mBucketsUsed) {
          // trim tail
          while (mEntries[mLastEntry].isEmpty()) --mLastEntry;
          // just in case
          if (mFirstEntry > 0) {
            // i want to be double-sure that "pre-head" entries are marked as free
            // we will fix free list link below
            for (uint32_t f = 0; f < (uint32_t)mFirstEntry; ++f) mEntries[f].hashOrFLink = 0;
          }
          // process entries, build free list
          mFirstEntry = 0;
          TEntry *ee = &mEntries[0];
          for (; mFirstEntry <= mLastEntry && ee->isEmpty(); ++mFirstEntry, ++ee) {
            if (lastfree) mEntries[lastfree-1U].hashOrFLink = (uint32_t)mFirstEntry+1U; else mFreeEntryHead = (uint32_t)mFirstEntry+1U;
            lastfree = (uint32_t)mFirstEntry+1U;
          }
          // there should be at least one
          vassert(mFirstEntry < mLastEntry);
          int curre = mFirstEntry;
          for (; curre <= mLastEntry; ++curre, ++ee) {
            if (ee->isEmpty()) {
              if (lastfree) mEntries[lastfree-1U].hashOrFLink = (uint32_t)curre+1U; else mFreeEntryHead = (uint32_t)curre+1U;
              lastfree = (uint32_t)curre+1U;
            }
          }
        } else {
          // no entries
          mFirstEntry = mLastEntry = -1;
        }
      }
      // put free list sentinel
      if (lastfree) mEntries[lastfree-1U].hashOrFLink = 0; else mFreeEntryHead = 0;
    } else {
      vassert(mFirstEntry == -1);
      vassert(mLastEntry == -1);
    }
  }

  VVA_FORCEINLINE void rehashRekeyLeaveFirst () noexcept { rehash<RehashRekey|RehashLeaveFirst>(); }
  VVA_FORCEINLINE void rehashRekeyLeaveLast () noexcept { rehash<RehashRekey|RehashLeaveLast>(); }

  VVA_FORCEINLINE VVA_CHECKRESULT bool needCompact () const noexcept {
    if (mBSize == 0) return false;
    if (mBSize <= InitSize) return (mBucketsUsed == 0);
    const uint32_t newsz = calcCompactedSize();
    return (newsz < mBSize);
  }

  // call this instead of `rehash()` after a lot of deletions
  // there is no reason to move entries if no reallocation would occur
  void compact () noexcept {
    if (mBSize == 0) {
      vassert(mESize == 0);
      vassert(mFirstEntry == -1);
      vassert(mLastEntry == -1);
      return;
    }

    if (mBucketsUsed == 0) {
      if (mBSize) {
        #ifdef CORE_MAP_TEST
        printf("compact-clear; bsize=(o:%u); esize=(o:%u); used=%u; fe=%d; le=%d; cseed=(0x%08x); mpc=%u; mcomp=%u\n", mBSize, mESize, mBucketsUsed, mFirstEntry, mLastEntry, mSeed, mMaxProbeCount, mMaxProbeCount);
        #endif
        clear();
      }
      return;
    }

    const uint32_t newsz = calcCompactedSize();
    if (newsz >= mBSize) return; // nothing to do
    #ifdef CORE_MAP_TEST
    printf("compacting; bsize=(o:%u; n:%u; ldf=%u); esize=(o:%u; n:%u); used=%u; fe=%d; le=%d; cseed=(0x%08x); mpc=%u; mcomp=%u", mBSize, newsz, calcMaxLoad(newsz), mESize, calcOptimalEntriesCount(newsz), mBucketsUsed, mFirstEntry, mLastEntry, mSeed, mMaxProbeCount, mMaxProbeCount);
    //printf("\n");
    #endif

    // move all entries to the top
    if (mFirstEntry >= 0) {
      vassert(mBucketsUsed);
      vassert(mLastEntry >= mFirstEntry);
      vassert(mEntries[mFirstEntry].isUsed());
      vassert(mEntries[mLastEntry].isUsed());
      // we will fill the holes with the entries from the end of the array
      // when we'll finish, our `end` will point to the last non-empty entry
      // put our finger to the last non-empty entry
      uint32_t end = (uint32_t)mLastEntry;
      // now fill the holes
      // this loop will skip all non-holes
      // it is NOT guaranteed that entries before `mFirstEntry` are properly empty
      // so make them properly empty (we need this in the following loop)
      // they should be already marked due to how `releaseEntry()` works
      #ifdef CORE_MAP_TEST
      for (int f = 0; f < mFirstEntry; ++f) vassert(mEntries[f].isEmpty());
      #endif
      uint32_t holeidx = 0u;
      while (holeidx < end) {
        // is this a hole?
        if (mEntries[holeidx].isEmpty()) {
          // yes, we found a hole
          // sanity checks
          vassert(mEntries[holeidx].isEmpty());
          vassert(mEntries[end].isUsed());
          // zero hole entry, and then init it to some sane state
          #if !defined(TMAP_NO_CLEAR)
          mEntries[holeidx].initEntry();
          #endif
          // copy last used entry into the hole
          mEntries[holeidx] = mEntries[end];
          // destroy copied entry
          #if !defined(TMAP_NO_CLEAR)
          mEntries[end].destroyEntry();
          #endif
          // mark last entry as unused (just in case)
          mEntries[end].hashOrFLink = 0;
          // move our finger to the previous non-empty entry
          // it is guaranteed to have a non-empty entry, so no additional checks required
          // first entry is always occupied now, and `end` is never zero here
          // it may become zero after the loop, but it doesn't matter, because the loop will end in this case
          while (mEntries[--end].isEmpty()) {}
        }
        // move the hole index
        ++holeidx;
      }
      // here, the first entry is at zero index, and the last entry is at the finger index
      mFirstEntry = 0;
      mLastEntry = (int)end;
      vassert(mLastEntry >= mFirstEntry);
      vassert(mEntries[0].isUsed());
      vassert(mEntries[end].isUsed());
      // just in case
      mFreeEntryHead = 0;
    }

    // shrink arrays
    vassert((mBucketsUsed && mFirstEntry == 0) || (!mBucketsUsed && mFirstEntry == -1));
    vassert((mBucketsUsed && mLastEntry >= 0 && mLastEntry >= mFirstEntry) || (!mBucketsUsed && mLastEntry == -1));
    vassert(newsz > 0);
    vassert(mBSize > newsz);
    // bucket array
    mBSize = newsz;
    mBuckets = (TBucket *)Z_Realloc(mBuckets, mBSize*sizeof(TBucket));
    // no need to clear new buckets, `rehash()` will do that for us
    // entry array
    //const uint32_t oldESize = mESize;
    mESize = calcOptimalEntriesCount(mBSize);
    vassert(mESize > 0 && (mLastEntry+1 >= 0 && mESize >= (uint32_t)(mLastEntry+1)));
    mEntries = (TEntry *)Z_Realloc((void *)mEntries, mESize*sizeof(TEntry));
    //if (mESize > oldESize) zeroEntries(oldESize, mESize);
    #ifdef CORE_MAP_TEST
    mMaxProbeCount = 0u;
    mMaxCompareCount = 0u;
    #endif
    calcCurrentMaxLoad(mBSize);
    vassert(!mFreeEntryHead); // because we moved our entries to the top
    // reinsert entries
    rehash();
    #ifdef CORE_MAP_TEST
    printf("; newfe=%d; newle=%d; newcseed=(0x%08x)\n", mFirstEntry, mLastEntry, mSeed);
    #endif
  }

  //WARNING! returned pointer may be invalidated by any map mutation
  VVA_CHECKRESULT TV *get (const TK &akey) const noexcept {
    if (mBucketsUsed == 0u) return nullptr;
    const uint32_t khash = calcKeyHash(akey);
    TMAP_IMPL_CALC_BUCKET_INDEX(khash)
    if (mBuckets[idx].hash == 0u) return nullptr;
    #ifdef CORE_MAP_TEST
    uint32_t probeCount = 0u;
    uint32_t cmpCount = 0u;
    #endif
    for (uint32_t dist = 0u; dist <= bhigh; ++dist) {
      if (mBuckets[idx].hash == 0u) break;
      #ifdef CORE_MAP_TEST
      ++probeCount;
      if (mMaxProbeCount < probeCount) mMaxProbeCount = probeCount;
      #endif
      const uint32_t pdist = distToDesiredBucket(idx, bhigh);
      if (dist > pdist) break;
      #ifdef CORE_MAP_TEST
      if (mBuckets[idx].hash == khash) {
        ++cmpCount;
        if (mMaxCompareCount < cmpCount) mMaxCompareCount = cmpCount;
      }
      #endif
      if (mBuckets[idx].hash == khash && mEntries[mBuckets[idx].entryidx].key == akey) {
        return &(mEntries[mBuckets[idx].entryidx].value);
      }
      idx = (idx+1u)&bhigh;
    }
    return nullptr;
  }

  // check if we have this key
  VVA_FORCEINLINE VVA_CHECKRESULT bool has (const TK &akey) const noexcept { return (get(akey) != nullptr); }

  // this is used when our values are pointers, to avoid check and dereference in the caller
  VVA_FORCEINLINE VVA_CHECKRESULT const TV findptr (const TK &Key) const noexcept {
    auto res = get(Key);
    return (res ? *res : nullptr);
  }

  // returns `false` if no item was found, or `true` when something was deleted
  VVA_FORCEINLINE bool del (const TK &akey) noexcept {
    if (mBucketsUsed == 0u) return false;
    const uint32_t khash = calcKeyHash(akey);
    return delInternal(akey, khash);
  }

  // returns `true` if old value was found (and possibly replaced)
  // will replace old value if `DoReplace` is `true`
  template<bool DoReplace=true> bool put (const TK &akey, const TV &aval) noexcept {
    const uint32_t khash = calcKeyHash(akey);
    TMAP_IMPL_CALC_BUCKET_INDEX(khash)

    // check if we already have this key
    if (mBucketsUsed && mBuckets[idx].hash) {
      #ifdef CORE_MAP_TEST
      uint32_t probeCount = 0u;
      uint32_t cmpCount = 0u;
      #endif
      for (uint32_t dist = 0u; dist <= bhigh; ++dist) {
        if (mBuckets[idx].hash == 0u) break;
        #ifdef CORE_MAP_TEST
        ++probeCount;
        if (mMaxProbeCount < probeCount) mMaxProbeCount = probeCount;
        #endif
        const uint32_t pdist = distToDesiredBucket(idx, bhigh);
        if (dist > pdist) break;
        #ifdef CORE_MAP_TEST
        if (mBuckets[idx].hash == khash) {
          ++cmpCount;
          if (mMaxCompareCount < cmpCount) mMaxCompareCount = cmpCount;
        }
        #endif
        if (mBuckets[idx].hash == khash && mEntries[mBuckets[idx].entryidx].key == akey) {
          // replace element?
          if (DoReplace) mEntries[mBuckets[idx].entryidx].value = aval;
          return true; // duplicate was found
        }
        idx = (idx+1u)&bhigh;
      }
    }

    // need to resize elements table?
    if (!mBSize || mBucketsUsed >= mCurrentMaxLoad || mBucketsUsed >= mESize) {
      uint32_t newsz = (uint32_t)mBSize;
      if (newsz >= MaxBuckets) Sys_Error("out of memory for TMap!");
      newsz <<= 1;
      if (!newsz) newsz = InitSize;
      vassert(nextPOTU32(newsz) == newsz); // sanity check
      #ifdef CORE_MAP_TEST
      printf("growing; bsize=(o:%u; n:%u); esize=(o:%u; n:%u); used=%u; fe=%d; le=%d; cseed=(0x%08x); maxload=%u; mpc=%u; mcomp=%u\n", mBSize, newsz, mESize, calcOptimalEntriesCount(newsz), mBucketsUsed, mFirstEntry, mLastEntry, mSeed, mCurrentMaxLoad, mMaxProbeCount, mMaxCompareCount);
      #endif
      // resize buckets array
      mBuckets = (TBucket *)Z_Realloc(mBuckets, newsz*sizeof(TBucket));
      // no need to clear it, because `rehash()` will do it anyway
      mBSize = newsz;
      // resize entries array
      const uint32_t oldESize = mESize;
      mESize = calcOptimalEntriesCount(mBSize);
      vassert(mESize >= oldESize);
      if (mESize > oldESize) {
        mEntries = (TEntry *)Z_Realloc((void *)mEntries, mESize*sizeof(TEntry));
        //zeroEntries(oldESize, mESize);
        #ifdef CORE_MAP_TEST
        mMaxProbeCount = 0u;
        mMaxCompareCount = 0u;
        #endif
      }
      calcCurrentMaxLoad(mBSize);
      // mFreeEntryHead will be fixed in `rehash()`
      rehash();
    }

    // create new entry
    TEntry *swpe = allocEntry();
    swpe->key = akey;
    swpe->value = aval;
    swpe->hashOrFLink = khash;

    putEntryInternal((uint32_t)(ptrdiff_t)(swpe-&mEntries[0]));
    return false; // there were no duplicates
  }

  VVA_FORCEINLINE bool putReplace (const TK &akey, const TV &aval) noexcept { return put<true>(akey, aval); }
  VVA_FORCEINLINE bool putNoReplace (const TK &akey, const TV &aval) noexcept { return put<false>(akey, aval); }

  VVA_FORCEINLINE VVA_CHECKRESULT int count () const noexcept { return (int)mBucketsUsed; }
  VVA_FORCEINLINE VVA_CHECKRESULT int length () const noexcept { return (int)mBucketsUsed; }
  VVA_FORCEINLINE VVA_CHECKRESULT int capacity () const noexcept { return (int)mESize; }
  VVA_FORCEINLINE VVA_CHECKRESULT int bucketcapacity () const noexcept { return (int)mBSize; }

#undef TMAP_IMPL_CALC_BUCKET_INDEX

  #ifdef CORE_MAP_TEST
  VVA_CHECKRESULT int countItems () const noexcept {
    int res = 0;
    if (mLastEntry >= 0) {
      for (uint32_t f = 0u; f <= (uint32_t)mLastEntry; ++f) if (mEntries[f].isUsed()) ++res;
    }
    return res;
  }

  VVA_CHECKRESULT uint32_t getMaxProbeCount () const noexcept { return mMaxProbeCount; }
  VVA_CHECKRESULT uint32_t getMaxCompareCount () const noexcept { return mMaxCompareCount; }
  #endif

  VVA_FORCEINLINE VVA_CHECKRESULT TIterator first () noexcept { return TIterator(this); }
};
