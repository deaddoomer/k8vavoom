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
//**  Copyright (C) 2018-2020 Ketmar Dark
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
#include "gamedefs.h"
#include "network.h"
#include "net_message.h"

extern VCvarB net_debug_dump_recv_packets;
static VCvarB net_debug_rpc("net_debug_rpc", false, "Dump RPC info?");


//==========================================================================
//
//  VChannel::VChannel
//
//==========================================================================
VChannel::VChannel (VNetConnection *AConnection, EChannelType AType, vint32 AIndex, bool AOpenedLocally)
  : Connection(AConnection)
  , Index(AIndex)
  , Type(AType)
  , OpenedLocally(AOpenedLocally)
  , OpenAcked(AIndex >= 0 && AIndex < CHANIDX_ObjectMap) // those channels are automatically opened
  , Closing(false)
  , NumInList(0)
  , NumOutList(0)
  , InList(nullptr)
  , OutList(nullptr)
  , bSentAnyMessages(false)
{
  vassert(Index >= 0 && Index < MAX_CHANNELS);
  vassert(!Connection->Channels[Index]);
  Connection->Channels[Index] = this;
  Connection->OpenChannels.append(this);
}


//==========================================================================
//
//  VChannel::~VChannel
//
//==========================================================================
VChannel::~VChannel () {
  // free outgoung queue
  while (OutList) {
    VMessageOut *curr = OutList;
    OutList = curr->Next;
    delete curr;
  }
  // free incoming queue
  while (InList) {
    VMessageIn *curr = InList;
    InList = curr->Next;
    delete curr;
  }

  // remove from connection's channel table
  if (Connection && Index >= 0 && Index < MAX_CHANNELS) {
    vassert(Connection->Channels[Index] == this);
    Connection->OpenChannels.Remove(this);
    Connection->Channels[Index] = nullptr;
    Connection = nullptr;
  }
}


//==========================================================================
//
//  VChannel::GetName
//
//==========================================================================
VStr VChannel::GetName () const noexcept {
  return VStr(va("chan #%d(%s)", Index, GetTypeName()));
}


//==========================================================================
//
//  VChannel::GetDebugName
//
//==========================================================================
VStr VChannel::GetDebugName () const noexcept {
  return (Connection ? Connection->GetAddress() : VStr("<noip>"))+":"+(IsLocalChannel() ? "l:" : "r:")+GetName();
}


//==========================================================================
//
//  VChannel::CanSendData
//
//==========================================================================
bool VChannel::CanSendData () const noexcept {
  // keep some space for close message
  if (NumOutList >= MAX_RELIABLE_BUFFER-2) return false;
  return (Connection ? Connection->CanSendData() : false);
}


//==========================================================================
//
//  VChannel::CanSendClose
//
//==========================================================================
bool VChannel::CanSendClose () const noexcept {
  return (NumOutList < MAX_RELIABLE_BUFFER-1);
}


//==========================================================================
//
//  VChannel::SetClosing
//
//==========================================================================
void VChannel::SetClosing () {
  Closing = true;
}


//==========================================================================
//
//  VChannel::ReceivedCloseAck
//
//  this is called from `ReceivedAcks()`
//  the channel will be automatically closed and destroyed, so don't do it here
//
//==========================================================================
void VChannel::ReceivedCloseAck () {
}


//==========================================================================
//
//  VChannel::Close
//
//==========================================================================
void VChannel::Close () {
  // if this channel is already closing, do nothing
  if (Closing) return;
  // if the connection is dead, simply set the flag and get out
  if (!Connection || Connection->IsClosed()) {
    SetClosing();
    return;
  }
  vassert(Connection->Channels[Index] == this);
  if (net_debug_dump_recv_packets) GCon->Logf(NAME_DevNet, "%s: sending CLOSE %s", *GetDebugName(), (IsLocalChannel() ? "request" : "ack"));
  // send a close notify, and wait for the ack
  // we should not have any closing message in the queue (sanity check)
  for (VMessageOut *Out = OutList; Out; Out = Out->Next) vassert(!Out->bClose);
  // send closing message
  {
    VMessageOut cnotmsg(this, true); // reliable
    cnotmsg.bClose = true;
    SendMessage(&cnotmsg);
  }
  // message queuing should set closing flag, check it
  vassert(Closing);
}


//==========================================================================
//
//  VChannel::PacketLost
//
//==========================================================================
void VChannel::PacketLost (vuint32 PacketId) {
  for (VMessageOut *Out = OutList; Out; Out = Out->Next) {
    // retransmit reliable messages in the lost packet
    if (Out->PacketId == PacketId && !Out->bReceivedAck) {
      vassert(Out->bReliable);
      Connection->SendMessage(Out);
    }
  }
}


//==========================================================================
//
//  VChannel::ReceivedAcks
//
//==========================================================================
void VChannel::ReceivedAcks () {
  vassert(Connection->Channels[Index] == this);

  // sanity check
  for (VMessageOut *Out = OutList; Out && Out->Next; Out = Out->Next) vassert(Out->Next->ChanSequence > Out->ChanSequence);

  // release all acknowledged outgoing queued messages
  bool doClose = false;
  while (OutList && OutList->bReceivedAck) {
    doClose = (doClose || OutList->bClose);
    VMessageOut *curr = OutList;
    OutList = OutList->Next;
    delete curr;
    --NumOutList;
  }

  // if a close has been acknowledged in sequence, we're done
  if (doClose) {
    // `OutList` can still contain some packets here for some reason
    // it looks like a bug in my netcode
    if (OutList) {
      GCon->Logf(NAME_DevNet, "!!!! %s: acked close message, but contains some other unacked messages (%d) !!!!", *GetDebugName(), NumOutList);
      for (VMessageOut *Out = OutList; Out; Out = Out->Next) {
        vassert(!Out->bReceivedAck);
        GCon->Logf(NAME_DevNet, "  pid=%u; csq=%u; cidx=%u; ctype=%u; open=%d; close=%d; reliable=%d; size=%d",
          Out->PacketId, Out->ChanSequence, Out->ChanType, Out->ChanIndex, (int)Out->bOpen, (int)Out->bClose,
          (int)Out->bReliable, Out->GetNumBits());
      }
    }
    vassert(!OutList);
    ReceivedCloseAck();
    delete this;
  }
}


//==========================================================================
//
//  VChannel::SendMessage
//
//==========================================================================
void VChannel::SendMessage (VMessageOut *Msg) {
  vassert(Msg);
  vassert(!Closing);
  vassert(Connection->Channels[Index] == this);
  vassert(!Msg->IsError());

  // set some additional message flags
  if (OpenedLocally && !bSentAnyMessages) {
    // first message must be reliable
    vassert(Msg->bReliable);
    Msg->bOpen = true;
  }
  bSentAnyMessages = true;

  if (Msg->bReliable) {
    // put outgoint message into send queue
    vassert(NumOutList < MAX_RELIABLE_BUFFER-1+(Msg->bClose ? 1 : 0));
    Msg->Next = nullptr;
    Msg->ChanSequence = ++Connection->OutReliable[Index];
    ++NumOutList;
    VMessageOut *OutMsg = new VMessageOut(*Msg);
    VMessageOut **OutLink;
    for (OutLink = &OutList; *OutLink; OutLink = &(*OutLink)->Next) {}
    *OutLink = OutMsg;
    Msg = OutMsg; // use this new message for sending
  }

  // send the raw message
  Msg->bReceivedAck = false;
  Connection->SendMessage(Msg);
  // if we're closing the channel, mark this channel as dying, so we can reject any new data
  // note that we can still have some fragments of the data in incoming queue, and it will be
  // processed normally
  if (Msg->bClose) SetClosing();
}


//==========================================================================
//
//  VChannel::ProcessInMessage
//
//==========================================================================
bool VChannel::ProcessInMessage (VMessageIn &Msg) {
  // fix channel incoming sequence
  if (Msg.bReliable) Connection->InReliable[Index] = Msg.ChanSequence;

  // parse a message
  const bool isCloseMsg = Msg.bClose;
  if (!Closing) ParseMessage(Msg);

  // handle a close notify
  if (isCloseMsg) {
    if (InList) Sys_Error("ERROR: %s: closing channel #%d with unprocessed incoming queue", *GetDebugName(), Index);
    delete this;
    return true;
  }
  return false;
}


//==========================================================================
//
//  VChannel::ReceivedMessage
//
//  process a raw, possibly out-of-sequence message
//  either queue it or dispatch it
//  the message won't be discarded
//
//==========================================================================
void VChannel::ReceivedMessage (VMessageIn &Msg) {
  vassert(Connection->Channels[Index] == this);

  if (Msg.bReliable && Msg.ChanSequence != Connection->InReliable[Index]+1) {
    // if this message is not in a sqeuence, buffer it
    // out-of-sequence message cannot be open message
    // actually, we should show channel error, and block all further messaging on it
    // (or even close the connection, as it looks like broken/malicious)
    vassert(!Msg.bOpen);

    // invariant
    vassert(Msg.ChanSequence > Connection->InReliable[Index]);

    // put this into incoming queue, keeping the queue ordered
    VMessageIn *prev = nullptr, *curr = InList;
    while (curr) {
      if (Msg.ChanSequence == curr->ChanSequence) return; // duplicate message, ignore it
      if (Msg.ChanSequence < curr->ChanSequence) break; // insert before `curr`
      prev = curr;
      curr = curr->Next;
    }
    // copy message
    VMessageIn *newmsg = new VMessageIn(Msg);
    if (prev) prev->Next = newmsg; else InList = newmsg;
    newmsg->Next = curr;
    ++NumInList;
    vassert(NumInList <= MAX_RELIABLE_BUFFER); //FIXME: signal error here!
    // just in case
    for (VMessageIn *m = InList; m && m->Next; m = m->Next) vassert(m->ChanSequence < m->Next->ChanSequence);
  } else {
    // this is "in sequence" message, process it
    bool removed = ProcessInMessage(Msg);
    if (removed) return;

    // dispatch any waiting messages
    while (InList) {
      if (InList->ChanSequence != Connection->InReliable[Index]+1) break;
      VMessageIn *curr = InList;
      InList = InList->Next;
      --NumInList;
      removed = ProcessInMessage(*curr);
      delete curr;
      if (removed) return;
    }
  }
}


//==========================================================================
//
//  VChannel::Tick
//
//==========================================================================
void VChannel::Tick () {
}


//==========================================================================
//
//  VChannel::WillOverflowMsg
//
//==========================================================================
bool VChannel::WillOverflowMsg (const VMessageOut *msg, const VBitStreamWriter &strm) const noexcept {
  vassert(msg);
  return msg->WillOverflow(strm);
}


//==========================================================================
//
//  VChannel::PutStream
//
//  moves steam to msg (sending previous msg if necessary)
//
//==========================================================================
void VChannel::PutStream (VMessageOut *msg, VBitStreamWriter &strm) {
  vassert(msg);
  if (strm.GetNumBits() == 0) return;
  if (WillOverflowMsg(msg, strm)) {
    SendMessage(msg);
    msg->Reset(this, msg->bReliable);
  }
  vassert(!WillOverflowMsg(msg, strm));
  msg->CopyFromWS(strm);
  strm.Clear();
}


//==========================================================================
//
//  VChannel::FlushMsg
//
//  sends message if it is not empty
//
//==========================================================================
void VChannel::FlushMsg (VMessageOut *msg) {
  vassert(msg);
  if (msg->GetNumBits()) {
    SendMessage(msg);
    msg->Reset(this, msg->bReliable);
  }
}


//==========================================================================
//
//  VChannel::SendRpc
//
//==========================================================================
void VChannel::SendRpc (VMethod *Func, VObject *Owner) {
  VMessageOut Msg(this, !!(Func->Flags&FUNC_NetReliable));
  //GCon->Logf(NAME_DevNet, "%s: creating RPC: %s", *GetDebugName(), *Func->GetFullName());
  Msg.WriteInt((unsigned)Func->NetIndex);

  // serialise arguments
  VStack *Param = VObject::VMGetStackPtr()-Func->ParamsSize+1; // skip self
  for (int i = 0; i < Func->NumParams; ++i) {
    switch (Func->ParamTypes[i].Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Bool:
      case TYPE_Name:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Param->i, Func->ParamTypes[i]);
        ++Param;
        break;
      case TYPE_Float:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Param->f, Func->ParamTypes[i]);
        ++Param;
        break;
      case TYPE_String:
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Param->p, Func->ParamTypes[i]);
        ++Param;
        break;
      case TYPE_Vector:
        {
          TVec Vec;
          Vec.x = Param[0].f;
          Vec.y = Param[1].f;
          Vec.z = Param[2].f;
          VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Vec, Func->ParamTypes[i]);
          Param += 3;
        }
        break;
      default:
        Sys_Error("Bad method argument type %d", Func->ParamTypes[i].Type);
    }
    if (Func->ParamFlags[i]&FPARM_Optional) {
      Msg.WriteBit(!!Param->i);
      ++Param;
    }
  }

  // send it
  SendMessage(&Msg);
  if (net_debug_rpc) GCon->Logf(NAME_DevNet, "%s: created RPC: %s (%d bits)", *GetDebugName(), *Func->GetFullName(), Msg.GetNumBits());
}


//==========================================================================
//
//  VChannel::ReadRpc
//
//==========================================================================
bool VChannel::ReadRpc (VMessageIn &Msg, int FldIdx, VObject *Owner) {
  VMethod *Func = nullptr;
  for (VMethod *CM = Owner->GetClass()->NetMethods; CM; CM = CM->NextNetMethod) {
    if (CM->NetIndex == FldIdx) {
      Func = CM;
      break;
    }
  }
  if (!Func) return false;
  if (net_debug_rpc) GCon->Logf(NAME_DevNet, "%s: ...received RPC (%s); method %s", *GetDebugName(), (Connection->IsClient() ? "client" : "server"), *Func->GetFullName());

  //memset(pr_stackPtr, 0, Func->ParamsSize*sizeof(VStack));
  VObject::VMCheckAndClearStack(Func->ParamsSize);
  // push self pointer
  VObject::PR_PushPtr(Owner);
  // get arguments
  for (int i = 0; i < Func->NumParams; ++i) {
    switch (Func->ParamTypes[i].Type) {
      case TYPE_Int:
      case TYPE_Byte:
      case TYPE_Bool:
      case TYPE_Name:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&VObject::VMGetStackPtr()->i, Func->ParamTypes[i]);
        VObject::VMIncStackPtr();
        break;
      case TYPE_Float:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&VObject::VMGetStackPtr()->f, Func->ParamTypes[i]);
        VObject::VMIncStackPtr();
        break;
      case TYPE_String:
        VObject::VMGetStackPtr()->p = nullptr;
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&VObject::VMGetStackPtr()->p, Func->ParamTypes[i]);
        VObject::VMIncStackPtr();
        break;
      case TYPE_Pointer:
      case TYPE_Reference:
      case TYPE_Class:
      case TYPE_State:
        VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&VObject::VMGetStackPtr()->p, Func->ParamTypes[i]);
        VObject::VMIncStackPtr();
        break;
      case TYPE_Vector:
        {
          TVec Vec;
          VField::NetSerialiseValue(Msg, Connection->ObjMap, (vuint8 *)&Vec, Func->ParamTypes[i]);
          VObject::PR_Pushv(Vec);
        }
        break;
      default:
        Sys_Error("Bad method argument type `%s` for RPC method call `%s`", *Func->ParamTypes[i].GetName(), *Func->GetFullName());
    }
    if (Func->ParamFlags[i]&FPARM_Optional) {
      VObject::VMGetStackPtr()->i = Msg.ReadBit();
      VObject::VMIncStackPtr();
    }
  }
  // execute it
  (void)VObject::ExecuteFunction(Func);
  return true;
}
