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
#ifndef VCCRUN_HEADER_FILE
#define VCCRUN_HEADER_FILE

#include <stdio.h>

//#include "../libs/core/core.h"

//#define Random()  ((float)(rand()&0x7fff)/(float)0x8000)
/*
float Random () {
  unsigned int rn;
  ed25519_randombytes(&rn, sizeof(rn));
  fprintf(stderr, "rn=0x%08x\n", rn);
  return (rn&0x3ffff)/(float)0x3ffff;
}
*/


//#define OPCODE_STATS

//#include "convars.h"
//#include "filesys/fsys.h"

//#include "vcc_netobj.h"
#include "vcc_run_vc.h"
#include "../libs/vavoomc/vc_public.h"
#include "../source/utils/scripts.h"
//#include "../source/utils/misc.h"


//extern VStream *OpenFile (const VStr &Name);


// callback should be thread-safe (it may be called from several different threads)
// event handler *SHOULD* call `sockmodAckEvent()` for each such event with appropriate flags
// calls should be made in order
//extern void (*sockmodPostEventCB) (int code, int sockid, int data);

// this callback will be called... ah, see above
void sockmodAckEvent (int code, int sockid, int data, bool eaten, bool cancelled);


// called to notify `VCC_WaitEvent()` that some new event was posted
// if you will called this WITHOUT events in the queue, the event loop *MAY* abort with fatal error
typedef void (*VCC_PingEventLoopFn) ();
extern VCC_PingEventLoopFn VCC_PingEventLoop;

// if `quitev` is not `nullptr`, it will receive the copy of the `ev_quit` message
// returns `data1` of `ev_quit`
int VCC_RunEventLoop (event_t *quitev);


// `exitcode` will be put to `data1`
extern void PostQuitEvent (int exitcode);


// should wait for incoming events, and them into queue
typedef void (*VCC_WaitEventsFn) ();
extern VCC_WaitEventsFn VCC_WaitEvents;


// process received event, called by the event loop
// will stop processing events if the event was eaten or cancelled
typedef void (*VCC_ProcessEventFn) (event_t &ev, void *udata);


// different `udata` means different handlers!
extern void RegisterEventProcessor (VCC_ProcessEventFn handler, void *udata);
// `udata` must match!
extern void UnregisterEventProcessor (VCC_ProcessEventFn handler, void *udata);


// timer API for non-SDL apps
// returns timer id, or 0 on error
extern int VCC_SetTimer (int mstimeout);

// timer API for non-SDL apps
// returns timer id, or 0 on error
extern int VCC_SetInterval (int mstimeout);

extern void VCC_CancelInterval (int iid);

#endif
