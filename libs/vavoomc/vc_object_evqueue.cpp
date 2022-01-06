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
static event_t *events = nullptr;
static unsigned eventMax = 2048u; // default size; why not?
static unsigned eventLast = 0u; // points past the last stored event
static unsigned eventFirst = 0u; // first stored event
// that is, new event will be posted at [head+1], and
// to read events, get [tail] while (tail != head)

static atomic_int queueSpinLock = 0;


//==========================================================================
//
//  lockQueue
//
//==========================================================================
static VVA_FORCEINLINE void lockQueue () noexcept {
  while (atomic_cmp_xchg(&queueSpinLock, 0, 1) != 0) {}
}


//==========================================================================
//
//  unlockQueue
//
//==========================================================================
static VVA_FORCEINLINE void unlockQueue () noexcept {
  atomic_store(&queueSpinLock, 0);
}


struct QueueLocker {
  VVA_FORCEINLINE QueueLocker () noexcept { lockQueue(); }
  VVA_FORCEINLINE ~QueueLocker () noexcept { unlockQueue(); }
  QueueLocker (const QueueLocker &) = delete;
  QueueLocker &operator = (const QueueLocker &) = delete;
};


//==========================================================================
//
//  VObject::ClearEventQueue
//
//==========================================================================
void VObject::ClearEventQueue () noexcept {
  QueueLocker lock;
  eventLast = eventFirst = 0u;
}


//==========================================================================
//
//  VObject::CountQueuedEventsNoLock
//
//==========================================================================
int VObject::CountQueuedEventsNoLock () noexcept {
  return (int)(eventLast+(eventLast < eventFirst ? eventMax : 0u)-eventFirst);
}


//==========================================================================
//
//  VObject::CountQueuedEvents
//
//==========================================================================
int VObject::CountQueuedEvents () noexcept {
  QueueLocker lock;
  return CountQueuedEventsNoLock();
}


//==========================================================================
//
//  VObject::GetEventQueueSize
//
//==========================================================================
int VObject::GetEventQueueSize () noexcept {
  QueueLocker lock;
  return (int)eventMax;
}


//==========================================================================
//
//  VObject::SetEventQueueSize
//
//  set new maximum size of the queue
//  invalid newsize values (negative or zero) will be ignored
//  if the queue currently contanis more than `newsize` events, the API is noop
//  returns success flag (i.e. `true` when the queue was resized)
//
//==========================================================================
bool VObject::SetEventQueueSize (int newsize) noexcept {
  if (newsize < 1) return false; // oops
  QueueLocker lock;
  if (newsize < CountQueuedEventsNoLock()) return false;
  // allocate new buffer, copy events
  event_t *newbuf = (event_t *)Z_Malloc(newsize*sizeof(event_t));
  unsigned npos = 0u;
  while (eventFirst != eventLast) {
    newbuf[npos++] = events[eventFirst++];
    eventFirst %= eventMax;
  }
  vassert(npos <= (unsigned)newsize);
  Z_Free(events);
  events = newbuf;
  eventFirst = 0u;
  eventLast = npos;
  eventMax = (unsigned)newsize;
  return true;
}


//==========================================================================
//
//  VObject::PostEvent
//
//==========================================================================
bool VObject::PostEvent (const event_t &ev) noexcept {
  QueueLocker lock;
  unsigned nextFree = (eventLast+1)%eventMax;
  if (nextFree == eventFirst) return false; // queue overflow
  // if this is first ever event, allocate queue
  if (!events) events = (event_t *)Z_Malloc(eventMax*sizeof(event_t));
  events[eventLast] = ev;
  eventLast = nextFree;
  return true;
}


//==========================================================================
//
//  VObject::InsertEvent
//
//==========================================================================
bool VObject::InsertEvent (const event_t &ev) noexcept {
  QueueLocker lock;
  unsigned prevFirst = (eventFirst+eventMax-1)%eventMax;
  if (prevFirst == eventLast) return false; // queue overflow
  // if this is first ever event, allocate queue
  if (!events) events = (event_t *)Z_Malloc(eventMax*sizeof(event_t));
  events[prevFirst] = ev;
  eventFirst = prevFirst;
  return true;
}


//==========================================================================
//
//  VObject::PeekEvent
//
//==========================================================================
bool VObject::PeekEvent (int idx, event_t *ev) noexcept {
  QueueLocker lock;
  if (idx < 0 || idx >= CountQueuedEventsNoLock()) {
    if (ev) ev->clear();
    return false;
  }
  if (ev) *ev = events[(idx+eventFirst)%eventMax];
  return true;
}


//==========================================================================
//
//  VObject::GetEvent
//
//==========================================================================
bool VObject::GetEvent (event_t *ev) noexcept {
  QueueLocker lock;
  if (eventFirst == eventLast) {
    if (ev) ev->clear();
    return false;
  }
  if (ev) *ev = events[eventFirst++];
  eventFirst %= eventMax;
  return true;
}
