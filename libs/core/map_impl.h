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
//**  Template for mapping kays to values.
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
  static inline unsigned nextPOTU32 (unsigned x) noexcept {
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

  inline static unsigned hashU32 (unsigned a) noexcept {
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

    inline bool isEmpty () const noexcept { return (hash == 0u); }
    inline void setEmpty () noexcept { hash = 0u; }

    inline void initEntry () noexcept {
      new(&key, E_ArrayNew, E_ArrayNew)TK;
      new(&value, E_ArrayNew, E_ArrayNew)TV;
    }

    inline void zeroEntry () noexcept {
      memset((void *)this, 0, sizeof(*this));
    }

    inline void destroyEntry () noexcept {
      key.~TK();
      value.~TV();
    }
  };

private:
  unsigned mEBSize;
  TEntry *mEntries;
  TEntry **mBuckets;
  int mBucketsUsed;
  TEntry *mFreeEntryHead;
  int mFirstEntry, mLastEntry;
  unsigned mSeed;
  unsigned mSeedCount;

private:
  // up to, but not including last
  void initEntries (unsigned first, unsigned last) {
    if (first < last) {
      TEntry *ee = &mEntries[first];
      memset((void *)ee, 0, (last-first)*sizeof(TEntry));
      #if !defined(TMAP_NO_CLEAR) && defined(TMAP_CLEAR_INB4)
      for (unsigned f = first; f < last; ++f, ++ee) ee->initEntry();
      #endif
    }
  }

public:
  class TIterator {
  private:
    TMap_Class_Name *map;
    unsigned index;

  public:
    // ctor
    inline TIterator (TMap_Class_Name *amap) noexcept : map(amap), index(0) {
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
    inline TIterator (const TIterator &src, bool /*dummy*/) noexcept : map(src.map), index(src.map->mEBSize) {}

    inline TIterator (const TIterator &src) noexcept : map(src.map), index(src.index) {}
    inline TIterator &operator = (const TIterator &src) noexcept { if (&src != this) { map = src.map; index = src.index; } return *this; }

    // convert to bool
    inline operator bool () const noexcept { return ((int)index <= map->mLastEntry); }

    // next (prefix increment)
    inline void operator ++ () noexcept {
      if (index < map->mEBSize) {
        ++index;
        while ((int)index <= map->mLastEntry && index < map->mEBSize && map->mEntries[index].isEmpty()) ++index;
        if ((int)index > map->mLastEntry) index = map->mEBSize;
      }
    }

    // `foreach` interface
    inline TIterator begin () noexcept { return TIterator(*this); }
    inline TIterator end () noexcept { return TIterator(*this, true); }
    inline bool operator != (const TIterator &b) const noexcept { return (map != b.map || index != b.index); } /* used to compare with end */
    inline TIterator operator * () const noexcept { return TIterator(*this); } /* required for iterator */

    // key/value getters
    inline const TK &GetKey () const noexcept { return map->mEntries[index].key; }
    inline const TV &GetValue () const noexcept { return map->mEntries[index].value; }
    inline TV &GetValue () noexcept { return map->mEntries[index].value; }
    inline const TK &getKey () const noexcept { return map->mEntries[index].key; }
    inline const TV &getValue () const noexcept { return map->mEntries[index].value; }
    inline TV &getValue () noexcept { return map->mEntries[index].value; }

    inline const TK &key () const noexcept { return map->mEntries[index].key; }
    inline const TV &value () const noexcept { return map->mEntries[index].value; }
    inline TV &value () noexcept { return map->mEntries[index].value; }

    inline void removeCurrent () noexcept {
      if ((int)index <= map->mLastEntry && index < map->mEBSize) {
        if (!map->mEntries[index].isEmpty()) map->del(map->mEntries[index].key);
        operator++();
      }
    }
    inline void RemoveCurrent () noexcept { removeCurrent(); }

    inline void resetToFirst () noexcept {
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
  inline bool isValidIIdx (int index) const noexcept {
    return (index >= 0 && index <= mLastEntry);
  }

  // this is for VavoomC VM
  inline int getFirstIIdx () const noexcept {
    if (mFirstEntry < 0) return -1;
    int index = mFirstEntry;
    while (index <= mLastEntry && index < (int)mEBSize && mEntries[index].isEmpty()) ++index;
    return (int)(index <= mLastEntry ? index : -1);
  }

  // <0: done
  inline int getNextIIdx (int index) const noexcept {
    if (index >= 0 && index <= mLastEntry) {
      ++index;
      while (index <= mLastEntry && mEntries[index].isEmpty()) ++index;
      return (index <= mLastEntry ? index : -1);
    }
    return -1;
  }

  inline int removeCurrAndGetNextIIdx (int index) noexcept {
    if (index >= 0 && index <= mLastEntry) {
      if (!mEntries[index].isEmpty()) del(mEntries[index].key);
      return getNextIIdx(index);
    }
    return -1;
  }

  inline const TK *getKeyIIdx (int index) const noexcept {
    return (isValidIIdx(index) && !mEntries[index].isEmpty() ? &mEntries[index].key : nullptr);
  }

  inline TV *getValueIIdx (int index) const noexcept {
    return (isValidIIdx(index) && !mEntries[index].isEmpty() ? &mEntries[index].value : nullptr);
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
        mSeedCount = (unsigned)(uintptr_t)mEntries;
        mSeed = hashU32(mSeedCount);
      }
      // it is guaranteed to have a room for at least one entry here (see `put()`)
      ++mLastEntry;
      if (mFirstEntry == -1) mFirstEntry = 0;
      res = &mEntries[mLastEntry];
    } else {
      res = mFreeEntryHead;
      mFreeEntryHead = res->nextFree;
      // fix mFirstEntry and mLastEntry
      int idx = (int)(res-&mEntries[0]);
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
    const int idx = (int)(e-&mEntries[0]);
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

  inline unsigned distToStIdx (unsigned idx) const noexcept {
    #ifndef TMAP_USE_MULTIPLY
    unsigned res = (mBuckets[idx]->hash^mSeed)&(unsigned)(mEBSize-1);
    #else
    unsigned res = (unsigned)(((uint64_t)(mBuckets[idx]->hash^mSeed)*(uint64_t)(mEBSize-1))>>32);
    #endif
    res = (res <= idx ? idx-res : idx+(mEBSize-res));
    return res;
  }

  void putEntryInternal (TEntry *swpe) noexcept {
    const unsigned bhigh = (unsigned)(mEBSize-1);
    #ifndef TMAP_USE_MULTIPLY
    unsigned idx = (swpe->hash^mSeed)&bhigh;
    #else
    unsigned idx = (unsigned)(((uint64_t)(swpe->hash^mSeed)*(uint64_t)bhigh)>>32);
    #endif
    unsigned pcur = 0;
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) {
        // put entry
        mBuckets[idx] = swpe;
        ++mBucketsUsed;
        return;
      }
      unsigned pdist = distToStIdx(idx);
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

public:
  inline TMap_Class_Name () noexcept : mEBSize(0), mEntries(nullptr), mBuckets(nullptr), mBucketsUsed(0), mFreeEntryHead(nullptr), mFirstEntry(-1), mLastEntry(-1), mSeed(0), mSeedCount(0) {}

  inline TMap_Class_Name (TMap_Class_Name &other) noexcept : mEBSize(0), mEntries(nullptr), mBuckets(nullptr), mBucketsUsed(0), mFreeEntryHead(nullptr), mFirstEntry(-1), mLastEntry(-1), mSeed(0), mSeedCount(0) {
    operator=(other);
  }

  inline ~TMap_Class_Name () noexcept { clear(); }

  TMap_Class_Name &operator = (const TMap_Class_Name &other) noexcept {
    if (&other != this) {
      clear();
      // copy entries
      if (other.mBucketsUsed > 0) {
        // has some entries
        mEBSize = nextPOTU32(unsigned(mBucketsUsed));
        mBuckets = (TEntry **)Z_Malloc(mEBSize*sizeof(TEntry *));
        memset(&mBuckets[0], 0, mEBSize*sizeof(TEntry *));
        mEntries = (TEntry *)Z_Malloc(mEBSize*sizeof(TEntry));
        initEntries(0, mEBSize);
        if (!mSeedCount) mSeedCount = (unsigned)(uintptr_t)mEntries;
        mFirstEntry = mLastEntry = -1;
        if (other.mLastEntry >= 0) {
          const unsigned end = (unsigned)other.mLastEntry;
          unsigned didx = 0;
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
    mBucketsUsed = 0;
    mBuckets = nullptr;
    Z_Free((void *)mEntries);
    mEBSize = 0;
    mEntries = nullptr;
    mFreeEntryHead = nullptr;
    mFirstEntry = mLastEntry = -1;
  }

  // won't shrink buckets
  void reset () noexcept {
    freeEntries();
    if (mBucketsUsed > 0) {
      memset(mBuckets, 0, mEBSize*sizeof(TEntry *));
      mBucketsUsed = 0;
    }
  }

  void rehash () noexcept {
    // clear buckets
    memset(mBuckets, 0, mEBSize*sizeof(TEntry *));
    mBucketsUsed = 0;
    // reinsert entries
    mFreeEntryHead = nullptr;
    TEntry *lastfree = nullptr;
    if (mLastEntry >= 0) {
      // change seed, to minimize pathological cases
      //TODO: use prng to generate new hash
      if (++mSeedCount == 0) mSeedCount = 1;
      mSeed = hashU32(mSeedCount);
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
      for (unsigned idx = stx; idx <= /*mEBSize*/end; ++idx, ++e) {
        if (!e->isEmpty()) {
          // no need to recalculate hash
          putEntryInternal(e);
        } else {
          if (lastfree) lastfree->nextFree = e; else mFreeEntryHead = e;
          lastfree = e;
        }
      }
      if (lastfree) lastfree->nextFree = nullptr;
    }
  }

  // call this instead of `rehash()` after alot of deletions
  // if `doRealloc` is `false`, force moving all entries to top if entry pool reallocated
  void compact (bool doRealloc=true) noexcept {
    unsigned newsz = nextPOTU32((unsigned)mBucketsUsed);
    if (doRealloc) {
      if (newsz >= 0x40000000u) return;
      newsz <<= 1;
      if (newsz < 64 || newsz >= mEBSize) return;
    }
    #ifdef CORE_MAP_TEST
    printf("compacting; old size=%u; new size=%u; used=%d; fe=%d; le=%d; cseed=(%u:0x%08x)", mEBSize, newsz, mBucketsUsed, mFirstEntry, mLastEntry, mSeedCount, mSeed);
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
        mLastEntry = mBucketsUsed-1;
      }
    }
    if (doRealloc) {
      // shrink
      mBuckets = (TEntry **)Z_Realloc(mBuckets, newsz*sizeof(TEntry *));
      mEntries = (TEntry *)Z_Realloc((void *)mEntries, newsz*sizeof(TEntry));
      mEBSize = newsz;
      didAnyCopy = true; // just in case, reinsert everything
    }
    // mFreeEntryHead will be fixed in `rehash()`
    // reinsert entries
    if (didAnyCopy) rehash();
    #ifdef CORE_MAP_TEST
    printf("; newfe=%d; newle=%d; newcseed=(%u:0x%08x)\n", mFirstEntry, mLastEntry, mSeedCount, mSeed);
    #endif
  }

  bool has (const TK &akey) const noexcept {
    if (mBucketsUsed == 0) return false;
    const unsigned bhigh = (unsigned)(mEBSize-1);
    unsigned khash = GetTypeHash(akey);
    khash += !khash; // avoid zero hash value
    //if (!khash) khash = 1; // avoid zero hash value
    #ifndef TMAP_USE_MULTIPLY
    unsigned idx = (khash^mSeed)&bhigh;
    #else
    unsigned idx = (unsigned)(((uint64_t)(khash^mSeed)*(uint64_t)bhigh)>>32);
    #endif
    if (!mBuckets[idx]) return false;
    bool res = false;
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      unsigned pdist = distToStIdx(idx);
      if (dist > pdist) break;
      res = (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey);
      if (res) break;
      idx = (idx+1)&bhigh;
    }
    return res;
  }

  const TV *get (const TK &akey) const noexcept {
    if (mBucketsUsed == 0) return nullptr;
    const unsigned bhigh = (unsigned)(mEBSize-1);
    unsigned khash = GetTypeHash(akey);
    khash += !khash; // avoid zero hash value
    //if (!khash) khash = 1; // avoid zero hash value
    #ifndef TMAP_USE_MULTIPLY
    unsigned idx = (khash^mSeed)&bhigh;
    #else
    unsigned idx = (unsigned)(((uint64_t)(khash^mSeed)*(uint64_t)bhigh)>>32);
    #endif
    if (!mBuckets[idx]) return nullptr;
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      unsigned pdist = distToStIdx(idx);
      if (dist > pdist) break;
      if (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey) return &(mBuckets[idx]->value);
      idx = (idx+1)&bhigh;
    }
    return nullptr;
  }

  TV *get (const TK &akey) noexcept {
    if (mBucketsUsed == 0) return nullptr;
    const unsigned bhigh = (unsigned)(mEBSize-1);
    unsigned khash = GetTypeHash(akey);
    khash += !khash; // avoid zero hash value
    //if (!khash) khash = 1; // avoid zero hash value
    #ifndef TMAP_USE_MULTIPLY
    unsigned idx = (khash^mSeed)&bhigh;
    #else
    unsigned idx = (unsigned)(((uint64_t)(khash^mSeed)*(uint64_t)bhigh)>>32);
    #endif
    if (!mBuckets[idx]) return nullptr;
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      unsigned pdist = distToStIdx(idx);
      if (dist > pdist) break;
      if (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey) return &(mBuckets[idx]->value);
      idx = (idx+1)&bhigh;
    }
    return nullptr;
  }

  //WARNING! returned pointer will be invalidated by any map mutation
  inline TV *Find (const TK &Key) noexcept { return get(Key); }
  inline TV *find (const TK &Key) noexcept { return get(Key); }
  inline const TV *Find (const TK &Key) const noexcept { return get(Key); }
  inline const TV *find (const TK &Key) const noexcept { return get(Key); }

  inline const TV FindPtr (const TK &Key) const noexcept {
    auto res = get(Key);
    if (res) return *res;
    return nullptr;
  }
  inline const TV findptr (const TK &Key) const noexcept { return FindPtr(Key); }

  // see http://codecapsule.com/2013/11/17/robin-hood-hashing-backward-shift-deletion/
  bool del (const TK &akey) noexcept {
    if (mBucketsUsed == 0) return false;

    const unsigned bhigh = (unsigned)(mEBSize-1);
    unsigned khash = GetTypeHash(akey);
    khash += !khash; // avoid zero hash value
    //if (!khash) khash = 1; // avoid zero hash value
    #ifndef TMAP_USE_MULTIPLY
    unsigned idx = (khash^mSeed)&bhigh;
    #else
    unsigned idx = (unsigned)(((uint64_t)(khash^mSeed)*(uint64_t)bhigh)>>32);
    #endif

    // find key
    if (!mBuckets[idx]) return false; // no key
    bool res = false;
    for (unsigned dist = 0; dist <= bhigh; ++dist) {
      if (!mBuckets[idx]) break;
      unsigned pdist = distToStIdx(idx);
      if (dist > pdist) break;
      res = (mBuckets[idx]->hash == khash && mBuckets[idx]->key == akey);
      if (res) break;
      idx = (idx+1)&bhigh;
    }

    if (!res) return false; // key not found

    releaseEntry(mBuckets[idx]);

    if (mBucketsUsed > 1) {
      unsigned idxnext = (idx+1)&bhigh;
      for (unsigned dist = 0; dist <= bhigh; ++dist) {
        if (!mBuckets[idxnext]) { mBuckets[idx] = nullptr; break; }
        unsigned pdist = distToStIdx(idxnext);
        if (pdist == 0) { mBuckets[idx] = nullptr; break; }
        mBuckets[idx] = mBuckets[idxnext];
        idx = (idx+1)&bhigh;
        idxnext = (idxnext+1)&bhigh;
      }
    } else {
      mBuckets[idx] = nullptr;
    }

    --mBucketsUsed;

    return true;
  }

  inline bool Remove (const TK &Key) noexcept { return del(Key); }
  inline bool remove (const TK &Key) noexcept { return del(Key); }

  // returns `true` if old value was replaced
  bool put (const TK &akey, const TV &aval) noexcept {
    const unsigned bhigh = (unsigned)(mEBSize-1);
    unsigned khash = GetTypeHash(akey);
    khash += !khash; // avoid zero hash value
    //if (!khash) khash = 1; // avoid zero hash value
    #ifndef TMAP_USE_MULTIPLY
    unsigned idx = (khash^mSeed)&bhigh;
    #else
    unsigned idx = (unsigned)(((uint64_t)(khash^mSeed)*(uint64_t)bhigh)>>32);
    #endif

    // check if we already have this key
    if (mBucketsUsed != 0 && mBuckets[idx]) {
      for (unsigned dist = 0; dist <= bhigh; ++dist) {
        TEntry *e = mBuckets[idx];
        if (!e) break;
        unsigned pdist = distToStIdx(idx);
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
    if (mEBSize && (unsigned)mBucketsUsed >= (bhigh+1)*LoadFactorPrc/100u) {
      unsigned newsz = (unsigned)mEBSize;
      if (newsz >= 0x40000000u) Sys_Error("out of memory for TMap!");
      newsz <<= 1;
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

  inline void Set (const TK &Key, const TV &Value) noexcept { put(Key, Value); }
  inline void set (const TK &Key, const TV &Value) noexcept { put(Key, Value); }

  inline int count () const noexcept { return (int)mBucketsUsed; }
  inline int length () const noexcept { return (int)mBucketsUsed; }
  inline int capacity () const noexcept { return (int)mEBSize; }

  #ifdef CORE_MAP_TEST
  int countItems () const noexcept {
    int res = 0;
    for (unsigned f = 0; f < mEBSize; ++f) if (!mEntries[f].isEmpty()) ++res;
    return res;
  }
  #endif

  inline TIterator first () noexcept { return TIterator(this); }
};
