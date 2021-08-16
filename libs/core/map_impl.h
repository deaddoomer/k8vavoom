//**************************************************************************
//**
//**    ##   ##    ##    ##   ##   ####     ####   ###     ###
//**    ##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**     ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**     ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**      ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**       #    ##    ##    #      ####     ####   ##       ##
//**
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
//**  Template for mapping keys to values.
//**
//**  Robin Hood Hash Table
//**
//**************************************************************************
// define this to skip calling dtors on keys and values (can be useful for PODs)
// note that both keys and values will be zeroed before using anyway
//#define TMAP_NO_CLEAR

// see https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
//#define TMAP_USE_MULTIPLY

// define this to clear items on entry pool allocation/release
// if not defined, entry pool item will be cleared right before putting a new item
//#define TMAP_CLEAR_INB4


// ////////////////////////////////////////////////////////////////////////// //
static_assert(sizeof(unsigned) >= 4, "`unsigned` should be at least 4 bytes");

template<class TK, class TV> class TMap_Class_Name {
public:
  static VVA_ALWAYS_INLINE VVA_CONST unsigned nextPOTU32 (const unsigned x) noexcept {
    unsigned res = x;
    res |= (res>>1);
    res |= (res>>2);
    res |= (res>>4);
    res |= (res>>8);
    res |= (res>>16);
    // already pot?
    if (x != 0 && (x&(x-1)) == 0) res &= ~(res>>1); else ++res;
    return res;
  }

  static VVA_ALWAYS_INLINE VVA_CONST unsigned hashU32 (const unsigned a) noexcept {
    unsigned res = (unsigned)a;
    res -= (res<<6);
    res = res^(res>>17);
    res -= (res<<9);
    res = res^(res<<4);
    res -= (res<<3);
    res = res^(res<<10);
    res = res^(res>>15);
    return res;
  }

private:
  enum {
    InitSize = 256, // *MUST* be power of two
    LoadFactorPrc = 90, // it is ok for robin hood hashes
  };

  struct TEntry {
    unsigned hash; // 0 means "empty"
    TEntry *nextFree; // next free entry
    TK key;
    TV value;

    VVA_ALWAYS_INLINE bool isEmpty () const noexcept { return (hash == 0u); }
    VVA_ALWAYS_INLINE void setEmpty () noexcept { hash = 0u; }

    VVA_ALWAYS_INLINE void initEntry () noexcept {
      new(&key, E_ArrayNew, E_ArrayNew)TK;
      new(&value, E_ArrayNew, E_ArrayNew)TV;
    }

    VVA_ALWAYS_INLINE void zeroEntry () noexcept {
      memset((void *)this, 0, sizeof(*this));
    }

    VVA_ALWAYS_INLINE void destroyEntry () noexcept {
      key.~TK();
      value.~TV();
    }
  };

  // key must not be empty!
  static VVA_ALWAYS_INLINE unsigned calcKeyHash (const TK &akey) noexcept {
    const unsigned khash = GetTypeHash(akey);
    return khash+(!khash); // avoid zero hash value
  }

  // calculate desired index for the given hash
  VVA_ALWAYS_INLINE unsigned calcBestIdx (const unsigned hashval, const unsigned bhigh) const noexcept {
    #if !defined(TMAP_USE_MULTIPLY)
    return (hashval^mSeed)&bhigh;
    #else
    return (unsigned)(((uint64_t)(hashval^mSeed)*(uint64_t)bhigh)>>32);
    #endif
  }

  // this is done in almost any function, so let's declare it as a macro
  #define TMAP_IMPL_CALC_BUCKET_INDEX(hashval_)  \
    const unsigned bhigh = (unsigned)(mEBSize-1u); \
    unsigned idx = calcBestIdx((hashval_), bhigh);

private:
  unsigned mEBSize; // size of `mEntries` and `mBuckets` arrays, in elements
  TEntry *mEntries; // this array holds entries
  TEntry **mBuckets; // and this array is actual hash table "buckets", holds pointers to elements
  unsigned mBucketsUsed; // total number of used buckets (number of entries in the hash table)
  TEntry *mFreeEntryHead; // head of the list of free entries (allocated with the FIFO strategy)
  int mFirstEntry, mLastEntry; // used range of `mEntries` (`mFirstEntry` can be negative for empty table)
  unsigned mSeed; // current seed
  unsigned mCurrentMaxLoad; // cached current max table load (max number of used buckets before resize)
  /*
    first and last entry indices are low and high (inclusive) bounds of the used part of `mEntries`.
    it is used to speedup iteration.
    for empty table, both indicies are `-1` (it is important!).

    deleted entry is added to freelist, and will be reused on the next insertion.
    freelist is woring by FIFO algorithm.
    list head is a pointer, because entry table will be inevitably rehashed after resizing, and
    rehashing will fix the pointer. there is no need to use indexing there.

    it may be better to keep hash value in buckets too (to improve CPU cache lines locality), but
    i didn't profiled that, and the code looks cleaner the way it is now.

    it is also possible to keep entry index and hash in `mBuckets` instead of pointer. this way
    we'll have size penalty for 32-bit architectures, but we could reuse entry index for free
    list index, and remove `nextFree` field from `TEntry`.
    maybe even remove `hash` field too, and force `rehash()` to recalculate all hashes. this doesn't
    look like a good idea, tho, because it may make grow operations much slower.
   */
  #ifdef CORE_MAP_TEST
  mutable unsigned mMaxProbeCount;
  #endif

private:
  VVA_ALWAYS_INLINE void genNewSeed () noexcept {
    mSeed = hashU32(Z_GetHashTableSeed());
  }

  VVA_ALWAYS_INLINE void calcCurrentMaxLoad (const unsigned aEBSize) noexcept {
    mCurrentMaxLoad = aEBSize*LoadFactorPrc/100u;
  }

  // up to, but not including last
  void initEntries (const unsigned first, const unsigned last) {
    if (first < last) {
      TEntry *ee = &mEntries[first];
      memset((void *)ee, 0, (last-first)*sizeof(TEntry));
      #if !defined(TMAP_NO_CLEAR) && defined(TMAP_CLEAR_INB4)
      for (unsigned f = first; f < last; ++f, ++ee) ee->initEntry();
      #endif
    }
    calcCurrentMaxLoad(last); // `initEntries()` called on each table resize, so it is a good place
    #ifdef CORE_MAP_TEST
    mMaxProbeCount = 0u;
    #endif
  }

public:
  class TIterator {
  private:
    TMap_Class_Name *map;
    unsigned index;

  public:
    // ctor
    VVA_ALWAYS_INLINE TIterator (TMap_Class_Name *amap) noexcept : map(amap), index(0) {
      vassert(amap);
      if (amap->mFirstEntry < 0) {
        index = amap->mEBSize;
      } else {
        index = (unsigned)amap->mFirstEntry;
        while ((int)index <= amap->mLastEntry && index < amap->mEBSize && amap->mEntries[index].isEmpty()) ++index;
        if ((int)index > amap->mLastEntry) index = amap->mEBSize;
      }
    }

    // special ctor that will create "end pointer"
    VVA_ALWAYS_INLINE TIterator (const TIterator &src, bool /*dummy*/) noexcept : map(src.map), index(src.map->mEBSize) {}

    VVA_ALWAYS_INLINE TIterator (const TIterator &src) noexcept : map(src.map), index(src.index) {}
    VVA_ALWAYS_INLINE TIterator &operator = (const TIterator &src) noexcept { if (&src != this) { map = src.map; index = src.index; } return *this; }

    // convert to bool
    VVA_ALWAYS_INLINE operator bool () const noexcept { return ((int)index <= map->mLastEntry && index < map->mEBSize); }

    // next (prefix increment)
    VVA_ALWAYS_INLINE void operator ++ () noexcept {
      if (index < map->mEBSize) {
        ++index;
        while ((int)index <= map->mLastEntry && index < map->mEBSize && map->mEntries[index].isEmpty()) ++index;
        if ((int)index > map->mLastEntry) index = map->mEBSize; // make it equal to `end`, if necessary
      }
    }

    VVA_ALWAYS_INLINE bool isValid () const noexcept { return ((int)index <= map->mLastEntry && index < map->mEBSize); }

    // `foreach` interface
    VVA_ALWAYS_INLINE TIterator begin () noexcept { return TIterator(*this); }
    VVA_ALWAYS_INLINE TIterator end () noexcept { return TIterator(*this, true); }
    VVA_ALWAYS_INLINE bool operator != (const TIterator &b) const noexcept { return (map != b.map || index != b.index); } /* used to compare with end */
    VVA_ALWAYS_INLINE TIterator operator * () const noexcept { return TIterator(*this); } /* required for iterator */

    // key/value getters
    VVA_ALWAYS_INLINE const TK &GetKey () const noexcept { return map->mEntries[index].key; }
    VVA_ALWAYS_INLINE const TV &GetValue () const noexcept { return map->mEntries[index].value; }
    VVA_ALWAYS_INLINE TV &GetValue () noexcept { return map->mEntries[index].value; }
    VVA_ALWAYS_INLINE const TK &getKey () const noexcept { return map->mEntries[index].key; }
    VVA_ALWAYS_INLINE const TV &getValue () const noexcept { return map->mEntries[index].value; }
    VVA_ALWAYS_INLINE TV &getValue () noexcept { return map->mEntries[index].value; }

    VVA_ALWAYS_INLINE const TK &key () const noexcept { return map->mEntries[index].key; }
    VVA_ALWAYS_INLINE const TV &value () const noexcept { return map->mEntries[index].value; }
    VVA_ALWAYS_INLINE TV &value () noexcept { return map->mEntries[index].value; }

    // can be used after removing item, for example
    VVA_ALWAYS_INLINE bool isValidEntry () const noexcept {
      return ((int)index <= map->mLastEntry && index < map->mEBSize && !map->mEntries[index].isEmpty());
    }

    VVA_ALWAYS_INLINE void removeCurrentNoAdvance () noexcept {
      if ((int)index <= map->mLastEntry && index < map->mEBSize) {
        if (!map->mEntries[index].isEmpty()) {
          //map->del(map->mEntries[index].key);
          (void)map->delInternal(map->mEntries[index].key, map->mEntries[index].hash);
        }
        //operator++();
      }
    }
    VVA_ALWAYS_INLINE void RemoveCurrentNoAdvance () noexcept { removeCurrentNoAdvance(); }

    VVA_ALWAYS_INLINE void resetToFirst () noexcept {
      if (map->mFirstEntry < 0) {
        index = map->mEBSize;
      } else {
        index = (unsigned)map->mFirstEntry;
        while ((int)index <= map->mLastEntry && index < map->mEBSize && map->mEntries[index].isEmpty()) ++index;
        if ((int)index > map->mLastEntry) index = map->mEBSize;
      }
    }
  };

  friend class TIterator;

public:
  // this is for VavoomC VM
  VVA_ALWAYS_INLINE bool isValidIIdx (int index) const noexcept {
    return (index >= 0 && index <= mLastEntry && index < (int)mEBSize && !mEntries[index].isEmpty());
  }

  // this is for VavoomC VM
  VVA_ALWAYS_INLINE int getFirstIIdx () const noexcept {
    if (mFirstEntry < 0) return -1;
    int index = mFirstEntry;
    while (index <= mLastEntry && index < (int)mEBSize && mEntries[index].isEmpty()) ++index;
    return (index <= mLastEntry && index < (int)mEBSize ? index : -1);
  }

  // <0: done
  VVA_ALWAYS_INLINE int getNextIIdx (int index) const noexcept {
    if (index >= 0 && index <= mLastEntry) {
      ++index;
      while (index <= mLastEntry && index < (int)mEBSize && mEntries[index].isEmpty()) ++index;
      return (index <= mLastEntry && index < (int)mEBSize ? index : -1);
    }
    return -1;
  }

  VVA_ALWAYS_INLINE int removeCurrAndGetNextIIdx (int index) noexcept {
    if (index >= 0 && index <= mLastEntry && index < (int)mEBSize) {
      if (!mEntries[index].isEmpty()) del(mEntries[index].key);
      return getNextIIdx(index);
    }
    return -1;
  }

  VVA_ALWAYS_INLINE const TK *getKeyIIdx (int index) const noexcept {
    return (isValidIIdx(index) /*&& !mEntries[index].isEmpty()*/ ? &mEntries[index].key : nullptr);
  }

  VVA_ALWAYS_INLINE TV *getValueIIdx (int index) const noexcept {
    return (isValidIIdx(index) /*&& !mEntries[index].isEmpty()*/ ? &mEntries[index].value : nullptr);
  }

private:
  void freeEntries (const bool doZero=true) noexcept {
    #if !defined(TMAP_NO_CLEAR)
    if (mFirstEntry >= 0) {
      const int end = mLastEntry;
      TEntry *e = &mEntries[mFirstEntry];
      for (int f = mFirstEntry; f <= end; ++f, ++e) if (!e->isEmpty()) e->destroyEntry();
    }
    #endif
    if (doZero && mEBSize > 0) initEntries(0, mEBSize);
    mFreeEntryHead = nullptr;
    mFirstEntry = mLastEntry = -1;
  }

  TEntry *allocEntry () noexcept {
    TEntry *res;
    if (!mFreeEntryHead) {
      // nothing was allocated, so allocate something now
      if (mEBSize == 0) {
        mEBSize = InitSize;
        mBuckets = (TEntry **)Z_Malloc(mEBSize*sizeof(TEntry *));
        memset(&mBuckets[0], 0, mEBSize*sizeof(TEntry *));
        mEntries = (TEntry *)Z_Malloc(mEBSize*sizeof(TEntry));
        initEntries(0, mEBSize);
        genNewSeed();
      }
      // it is guaranteed to have a room for at least one entry here (see `put()`)
      ++mLastEntry;
      if (mFirstEntry == -1) mFirstEntry = 0;
      res = &mEntries[mLastEntry];
    } else {
      res = mFreeEntryHead;
      mFreeEntryHead = res->nextFree;
      // fix mFirstEntry and mLastEntry
      int idx = (int)(::ptrdiff_t)(res-&mEntries[0]);
      if (mFirstEntry < 0 || idx < mFirstEntry) mFirstEntry = idx;
      if (idx > mLastEntry) mLastEntry = idx;
    }
    res->nextFree = nullptr; // just in case
    #if !defined(TMAP_NO_CLEAR) && !defined(TMAP_CLEAR_INB4)
    // the entry is guaranteed to be zeroed
    res->initEntry();
    #endif
    return res;
  }

  void releaseEntry (TEntry *e) noexcept {
    const int idx = (int)(::ptrdiff_t)(e-&mEntries[0]);
    #if !defined(TMAP_NO_CLEAR)
    e->destroyEntry();
    #endif
    e->zeroEntry();
    #if !defined(TMAP_NO_CLEAR) && defined(TMAP_CLEAR_INB4)
    e->initEntry();
    #endif
    e->nextFree = mFreeEntryHead;
    mFreeEntryHead = e;
    // fix mFirstEntry and mLastEntry
    if (mFirstEntry == mLastEntry) {
      mFreeEntryHead = nullptr;
      mFirstEntry = mLastEntry = -1;
    } else {
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

  VVA_ALWAYS_INLINE unsigned distToStIdxBH (const unsigned idx, const unsigned bhigh) const noexcept {
    const unsigned bestidx = calcBestIdx(mBuckets[idx]->hash, bhigh);
    //return (bestidx <= idx ? idx-bestidx : idx+(bhigh+1u-bestidx));
    // this is exactly the same as the code above, because we need distance in "positive direction"
    // (i.e. if `bestidx` is higher than `idx`, we should walk to the end of the array, and then to `bestidx`)
    // this is basically `(idx+(bhigh+1)-bestidx)&bhigh`
    // by rewriting it, we'll get `((idx|(bhigh+1))-bestidx)&bhigh`
    // i.e. we're setting the highest bit, and then masking it away, which is noop
    return ((idx-bestidx)&bhigh);
  }

  VVA_ALWAYS_INLINE unsigned distToStIdx (const unsigned idx) const noexcept { return distToStIdxBH(idx, mEBSize-1u); }

  void putEntryInternal (TEntry *swpe) noexcept {
    TMAP_IMPL_CALC_BUCKET_INDEX(swpe->hash)
    unsigned pcur = 0;
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) {
        // put entry
        mBuckets[idx] = swpe;
        ++mBucketsUsed;
        return;
      }
      const unsigned pdist = distToStIdxBH(idx, bhigh);
      if (pcur > pdist) {
        // swapping the current bucket with the one to insert
        TEntry *tmpe = mBuckets[idx];
        mBuckets[idx] = swpe;
        swpe = tmpe;
        pcur = pdist;
      }
      idx = (idx+1)&bhigh;
      ++pcur;
    }
    __builtin_trap(); // we should not come here, ever
  }

  // if key hash is known
  bool delInternal (const TK &akey, const unsigned khash) noexcept {
    if (mBucketsUsed == 0u) return false;

    TMAP_IMPL_CALC_BUCKET_INDEX(khash)

    // find key
    if (!mBuckets[idx]) return false; // no key
    bool res = false;
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      const unsigned pdist = distToStIdxBH(idx, bhigh);
      if (dist > pdist) break;
      res = (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey);
      if (res) break;
      idx = (idx+1)&bhigh;
    }

    if (!res) return false; // key not found

    releaseEntry(mBuckets[idx]);

    if (--mBucketsUsed) {
      // there was more than one item
      unsigned idxnext = (idx+1)&bhigh;
      for (unsigned dist = 0; dist <= bhigh; ++dist) {
        if (!mBuckets[idxnext]) { mBuckets[idx] = nullptr; break; }
        const unsigned pdist = distToStIdxBH(idxnext, bhigh);
        if (pdist == 0) { mBuckets[idx] = nullptr; break; }
        mBuckets[idx] = mBuckets[idxnext];
        idx = (idx+1)&bhigh;
        idxnext = (idxnext+1)&bhigh;
      }
    } else {
      // there was only one item
      mBuckets[idx] = nullptr;
    }

    return true;
  }

public:
  VVA_ALWAYS_INLINE TMap_Class_Name () noexcept : mEBSize(0), mEntries(nullptr), mBuckets(nullptr), mBucketsUsed(0), mFreeEntryHead(nullptr), mFirstEntry(-1), mLastEntry(-1), mSeed(0), mCurrentMaxLoad(0) {}

  VVA_ALWAYS_INLINE TMap_Class_Name (TMap_Class_Name &other) noexcept : mEBSize(0), mEntries(nullptr), mBuckets(nullptr), mBucketsUsed(0), mFreeEntryHead(nullptr), mFirstEntry(-1), mLastEntry(-1), mSeed(0), mCurrentMaxLoad(0) {
    operator=(other);
  }

  VVA_ALWAYS_INLINE ~TMap_Class_Name () noexcept { clear(); }

  bool operator == (const TMap_Class_Name &other) const = delete;
  bool operator != (const TMap_Class_Name &other) const = delete;
  bool operator <= (const TMap_Class_Name &other) const = delete;
  bool operator >= (const TMap_Class_Name &other) const = delete;
  bool operator < (const TMap_Class_Name &other) const = delete;
  bool operator > (const TMap_Class_Name &other) const = delete;

  TMap_Class_Name &operator = (const TMap_Class_Name &other) noexcept {
    if (&other != this) {
      clear();
      // copy entries
      if (other.mBucketsUsed) {
        // has some entries
        mEBSize = nextPOTU32(mBucketsUsed);
        mBuckets = (TEntry **)Z_Malloc(mEBSize*sizeof(TEntry *));
        memset(&mBuckets[0], 0, mEBSize*sizeof(TEntry *));
        mEntries = (TEntry *)Z_Malloc(mEBSize*sizeof(TEntry));
        initEntries(0, mEBSize);
        genNewSeed();
        mFirstEntry = mLastEntry = -1;
        if (other.mLastEntry >= 0) {
          const unsigned end = (unsigned)other.mLastEntry;
          unsigned didx = 0u;
          for (unsigned f = (unsigned)other.mFirstEntry; f <= end; ++f) {
            if (!other.mEntries[f].isEmpty()) {
              #if !defined(TMAP_NO_CLEAR) && !defined(TMAP_CLEAR_INB4)
              // the entry is guaranteed to be zeroed
              mEntries[didx].initEntry();
              #endif
              mEntries[didx++] = other.mEntries[f];
            }
          }
          if (didx > 0) {
            mFirstEntry = 0;
            mLastEntry = (int)didx-1;
          }
        }
        rehash();
      }
    }
    return *this;
  }

  void clear () noexcept {
    // no need to call this to zero memory that will be freed anyway
    #if !defined(TMAP_NO_CLEAR)
    freeEntries(false);
    #endif
    mFreeEntryHead = nullptr;
    Z_Free(mBuckets);
    mBucketsUsed = 0u;
    mBuckets = nullptr;
    Z_Free((void *)mEntries);
    mEBSize = mCurrentMaxLoad = 0u;
    mEntries = nullptr;
    mFreeEntryHead = nullptr;
    mFirstEntry = mLastEntry = -1;
  }

  // won't shrink buckets
  void reset () noexcept {
    freeEntries();
    if (mBucketsUsed) {
      memset(mBuckets, 0, mEBSize*sizeof(TEntry *));
      mBucketsUsed = 0u;
    }
  }

  enum {
    // don't recompute key hashes
    RehashDefault = 0u,
    // recompute key hashes, leave first conflicting record
    RehashRekey = 1u,
    // masks, should be bitored with the above
    // leave first conflicting record
    RehashLeaveFirst = 0u,
    // leave last conflicting record
    RehashLeaveLast = 2u,
  };

  void rehash (const unsigned flags=RehashDefault) noexcept {
    // clear buckets
    memset(mBuckets, 0, mEBSize*sizeof(TEntry *));
    mBucketsUsed = 0u;
    // reinsert entries
    mFreeEntryHead = nullptr;
    TEntry *lastfree = nullptr;
    if (mLastEntry >= 0) {
      // change seed, to minimize pathological cases
      //TODO: use prng to generate new hash
      genNewSeed();
      // small optimisation for empty head case
      const unsigned stx = (unsigned)mFirstEntry;
      TEntry *e;
      if (stx > 0) {
        e = &mEntries[0];
        lastfree = mFreeEntryHead = e++;
        for (unsigned idx = 1; idx < stx; ++idx, ++e) {
          lastfree->nextFree = e;
          lastfree = e;
        }
        lastfree->nextFree = nullptr;
      }
      // reinsert all alive entries
      const unsigned end = (unsigned)mLastEntry;
      e = &mEntries[stx];
      for (unsigned eidx = stx; eidx <= /*mEBSize*/end; ++eidx, ++e) {
        if (!e->isEmpty()) {
          if (flags&RehashRekey) {
            const unsigned khash = calcKeyHash(e->key);
            e->hash = khash;
            TMAP_IMPL_CALC_BUCKET_INDEX(khash)
            // check if we already have this key
            TEntry *old = nullptr;
            if (mBucketsUsed && mBuckets[idx]) {
              for (unsigned dist = 0; dist <= bhigh; ++dist) {
                TEntry *ee = mBuckets[idx];
                if (!ee) break;
                const unsigned pdist = distToStIdx(idx);
                if (dist > pdist) break;
                if (ee->hash == khash && ee->key == e->key) {
                  // i found her!
                  vassert(old != ee);
                  old = ee;
                  break;
                }
                idx = (idx+1)&bhigh;
              }
            }
            if (old) {
              if (flags&RehashLeaveLast) {
                // replace old with new
                old->destroyEntry();
                old->key = e->key;
                old->value = e->value;
                old->hash = e->hash;
              }
              e->destroyEntry();
            } else {
              putEntryInternal(e);
            }
          } else {
            // no need to recalculate hash
            putEntryInternal(e);
          }
        }
        // need additional check, because it may be destroyed
        if (e->isEmpty()) {
          if (lastfree) lastfree->nextFree = e; else mFreeEntryHead = e;
          lastfree = e;
        }
      }
      if (lastfree) lastfree->nextFree = nullptr;
    }
    calcCurrentMaxLoad(mEBSize); // why not?
  }

  VVA_ALWAYS_INLINE void rehashRekeyLeaveFirst () noexcept { rehash(RehashRekey|RehashLeaveFirst); }
  VVA_ALWAYS_INLINE void rehashRekeyLeaveLast () noexcept { rehash(RehashRekey|RehashLeaveLast); }

  // call this instead of `rehash()` after alot of deletions
  // if `doRealloc` is `false`, force moving all entries to top if entry pool reallocated
  void compact (bool doRealloc=true) noexcept {
    unsigned newsz = nextPOTU32(mBucketsUsed);
    if (doRealloc) {
      if (newsz >= 0x40000000u) return;
      newsz <<= 1;
      if (newsz < 64 || newsz >= mEBSize) return;
    }
    #ifdef CORE_MAP_TEST
    printf("compacting; old size=%u; new size=%u; used=%u; fe=%d; le=%d; cseed=(0x%08x); mpc=%u", mEBSize, newsz, mBucketsUsed, mFirstEntry, mLastEntry, mSeed, mMaxProbeCount);
    #endif
    bool didAnyCopy = false;
    // move all entries to top
    if (mFirstEntry >= 0) {
      unsigned didx = 0;
      while (didx < mEBSize && !mEntries[didx].isEmpty()) ++didx;
      const unsigned end = mLastEntry;
      // copy entries
      for (unsigned f = didx+1; f <= end; ++f) {
        if (!mEntries[f].isEmpty()) {
          vassert(didx < f);
          didAnyCopy = true;
          mEntries[didx].zeroEntry(); // just in case
          #if !defined(TMAP_NO_CLEAR) && !defined(TMAP_CLEAR_INB4)
          mEntries[didx].initEntry();
          #endif
          mEntries[didx] = mEntries[f];
          #if !defined(TMAP_NO_CLEAR)
          mEntries[f].destroyEntry();
          #endif
          mEntries[f].zeroEntry();
          #if !defined(TMAP_NO_CLEAR) && defined(TMAP_CLEAR_INB4)
          mEntries[f].initEntry();
          #endif
          ++didx;
          while (didx < mEBSize && !mEntries[didx].isEmpty()) ++didx;
        }
      }
      if (didAnyCopy) {
        mFirstEntry = mLastEntry = 0;
        while ((unsigned)mLastEntry+1 < mEBSize && !mEntries[mLastEntry].isEmpty()) ++mLastEntry;
        mLastEntry = (int)(mBucketsUsed-1u);
      }
    }
    if (doRealloc) {
      // shrink
      mBuckets = (TEntry **)Z_Realloc(mBuckets, newsz*sizeof(TEntry *));
      mEntries = (TEntry *)Z_Realloc((void *)mEntries, newsz*sizeof(TEntry));
      mEBSize = newsz;
      calcCurrentMaxLoad(newsz); // it will be done anyway, but...
      didAnyCopy = true; // just in case, reinsert everything
    }
    // mFreeEntryHead will be fixed in `rehash()`
    // reinsert entries
    if (didAnyCopy) rehash();
    #ifdef CORE_MAP_TEST
    printf("; newfe=%d; newle=%d; newcseed=(0x%08x)\n", mFirstEntry, mLastEntry, mSeed);
    #endif
  }

  bool has (const TK &akey) const noexcept {
    if (mBucketsUsed == 0u) return false;
    const unsigned khash = calcKeyHash(akey);
    TMAP_IMPL_CALC_BUCKET_INDEX(khash)
    if (!mBuckets[idx]) return false;
    #ifdef CORE_MAP_TEST
    unsigned probeCount = 0;
    #endif
    bool res = false;
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      #ifdef CORE_MAP_TEST
      ++probeCount;
      if (mMaxProbeCount < probeCount) mMaxProbeCount = probeCount;
      #endif
      const unsigned pdist = distToStIdxBH(idx, bhigh);
      if (dist > pdist) break;
      res = (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey);
      if (res) break;
      idx = (idx+1)&bhigh;
    }
    return res;
  }

  const TV *get (const TK &akey) const noexcept {
    if (mBucketsUsed == 0u) return nullptr;
    const unsigned khash = calcKeyHash(akey);
    TMAP_IMPL_CALC_BUCKET_INDEX(khash)
    if (!mBuckets[idx]) return nullptr;
    #ifdef CORE_MAP_TEST
    unsigned probeCount = 0;
    #endif
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      #ifdef CORE_MAP_TEST
      ++probeCount;
      if (mMaxProbeCount < probeCount) mMaxProbeCount = probeCount;
      #endif
      const unsigned pdist = distToStIdxBH(idx, bhigh);
      if (dist > pdist) break;
      if (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey) return &(mBuckets[idx]->value);
      idx = (idx+1)&bhigh;
    }
    return nullptr;
  }

  TV *get (const TK &akey) noexcept {
    if (mBucketsUsed == 0u) return nullptr;
    const unsigned khash = calcKeyHash(akey);
    TMAP_IMPL_CALC_BUCKET_INDEX(khash)
    if (!mBuckets[idx]) return nullptr;
    #ifdef CORE_MAP_TEST
    unsigned probeCount = 0;
    #endif
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      #ifdef CORE_MAP_TEST
      ++probeCount;
      if (mMaxProbeCount < probeCount) mMaxProbeCount = probeCount;
      #endif
      const unsigned pdist = distToStIdxBH(idx, bhigh);
      if (dist > pdist) break;
      if (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey) return &(mBuckets[idx]->value);
      idx = (idx+1)&bhigh;
    }
    return nullptr;
  }

  //WARNING! returned pointer will be invalidated by any map mutation
  VVA_ALWAYS_INLINE TV *Find (const TK &Key) noexcept { return get(Key); }
  VVA_ALWAYS_INLINE TV *find (const TK &Key) noexcept { return get(Key); }
  VVA_ALWAYS_INLINE const TV *Find (const TK &Key) const noexcept { return get(Key); }
  VVA_ALWAYS_INLINE const TV *find (const TK &Key) const noexcept { return get(Key); }

  VVA_ALWAYS_INLINE const TV FindPtr (const TK &Key) const noexcept {
    auto res = get(Key);
    if (res) return *res;
    return nullptr;
  }
  VVA_ALWAYS_INLINE const TV findptr (const TK &Key) const noexcept { return FindPtr(Key); }

  // see http://codecapsule.com/2013/11/17/robin-hood-hashing-backward-shift-deletion/
  VVA_ALWAYS_INLINE bool del (const TK &akey) noexcept {
    if (mBucketsUsed == 0u) return false;
    const unsigned khash = calcKeyHash(akey);
    return delInternal(akey, khash);
  }

  VVA_ALWAYS_INLINE bool Remove (const TK &Key) noexcept { return del(Key); }
  VVA_ALWAYS_INLINE bool remove (const TK &Key) noexcept { return del(Key); }

  // returns `true` if old value was replaced
  bool put (const TK &akey, const TV &aval) noexcept {
    const unsigned khash = calcKeyHash(akey);
    TMAP_IMPL_CALC_BUCKET_INDEX(khash)

    // check if we already have this key
    if (mBucketsUsed && mBuckets[idx]) {
      #ifdef CORE_MAP_TEST
      unsigned probeCount = 0;
      #endif
      for (unsigned dist = 0; dist <= bhigh; ++dist) {
        TEntry *e = mBuckets[idx];
        if (!e) break;
        #ifdef CORE_MAP_TEST
        ++probeCount;
        if (mMaxProbeCount < probeCount) mMaxProbeCount = probeCount;
        #endif
        const unsigned pdist = distToStIdxBH(idx, bhigh);
        if (dist > pdist) break;
        if (e->hash == khash && e->key == akey) {
          // replace element
          e->value = aval;
          return true;
        }
        idx = (idx+1)&bhigh;
      }
    }

    // need to resize elements table?
    // if elements table is empty, `allocEntry()` will take care of it
    if (mEBSize && mBucketsUsed >= mCurrentMaxLoad) {
      unsigned newsz = (unsigned)mEBSize;
      if (newsz >= 0x40000000u) Sys_Error("out of memory for TMap!");
      newsz <<= 1;
      #ifdef CORE_MAP_TEST
      printf("growing; old size=%u; new size=%u; used=%u; fe=%d; le=%d; cseed=(0x%08x); maxload=%u; mpc=%u\n", mEBSize, newsz, mBucketsUsed, mFirstEntry, mLastEntry, mSeed, mCurrentMaxLoad, mMaxProbeCount);
      #endif
      // resize buckets array
      mBuckets = (TEntry **)Z_Realloc(mBuckets, newsz*sizeof(TEntry *));
      memset(mBuckets+mEBSize, 0, (newsz-mEBSize)*sizeof(TEntry *));
      // resize entries array
      mEntries = (TEntry *)Z_Realloc((void *)mEntries, newsz*sizeof(TEntry));
      //memset((void *)(mEntries+mEBSize), 0, (newsz-mEBSize)*sizeof(TEntry));
      //for (unsigned f = mEBSize; f < newsz; ++f) mEntries[f].setEmpty(); //k8: no need to
      initEntries(mEBSize, newsz);
      mEBSize = newsz;
      // mFreeEntryHead will be fixed in `rehash()`
      // reinsert entries
      rehash();
    }

    // create new entry
    TEntry *swpe = allocEntry();
    swpe->key = akey;
    swpe->value = aval;
    swpe->hash = khash;

    putEntryInternal(swpe);
    return false;
  }

  VVA_ALWAYS_INLINE void Set (const TK &Key, const TV &Value) noexcept { put(Key, Value); }
  VVA_ALWAYS_INLINE void set (const TK &Key, const TV &Value) noexcept { put(Key, Value); }

  VVA_ALWAYS_INLINE int count () const noexcept { return (int)mBucketsUsed; }
  VVA_ALWAYS_INLINE int length () const noexcept { return (int)mBucketsUsed; }
  VVA_ALWAYS_INLINE int capacity () const noexcept { return (int)mEBSize; }

#undef TMAP_IMPL_CALC_BUCKET_INDEX

  #ifdef CORE_MAP_TEST
  int countItems () const noexcept {
    int res = 0;
    for (unsigned f = 0; f < mEBSize; ++f) if (!mEntries[f].isEmpty()) ++res;
    return res;
  }

  unsigned getMaxProbeCount () const noexcept { return mMaxProbeCount; }
  #endif

  VVA_ALWAYS_INLINE TIterator first () noexcept { return TIterator(this); }
};
