//=============================================================================
//	This file is part of MCSB, the Multi-Client Shared Buffer
//	Copyright (C) 2014  Gregory E. Allen and
//		Applied Research Laboratories: The University of Texas at Austin
//
//	MCSB is free software: you can redistribute it and/or modify it
//	under the terms of the GNU Lesser General Public License as published
//	by the Free Software Foundation, either version 2.1 of the License, or
//	(at your option) any later version.
//
//	MCSB is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//	Lesser General Public License <http://www.gnu.org/licenses/lgpl.html>
//	for more details.
//=============================================================================

#ifndef MCSB_SocketProtocol_h
#define MCSB_SocketProtocol_h
#pragma once

#include <stdint.h>

namespace MCSB {

//-----------------------------------------------------------------------------
//	This file describes the protocol for messages between the manager and its
//	clients. There are two types of messages:  a 4-byte (unsigned) blockID
//	(or slabID), and control messages. A control messages is marked by
//	CtrlMsgHdr::kSyncWord, and contains a CtrlMsgHdr and its payload.
//-----------------------------------------------------------------------------
//	A blockID is the index of a block, with the most significant bit always 0.
//	A slabID always has the most significant bit set.
//-----------------------------------------------------------------------------
//	During normal communications:
//	Manager -> Client
//		A slabID indicates that the manager is giving the specified slab to
//		the client so that it can be filled with message data to send.
//		A blockID indicates that the given block begins a run that contains
//		part or all of a message that the client is registered for.
//	Client -> Manager
//		A slabID indicates that the client is relinquishing the given slab
//		(and slabIDs may be retired out of order).
//		A blockID indicates that the client is done with the run message it
//		previously received (and blockIDs must be retired in order).
//		To send a run, a client sends kCtrlMsgID_BlocksAndInfo.
//-----------------------------------------------------------------------------

enum { kProtocolMagic = 0x4253434D }; // little endian 'MCSB'
enum { kProtocolVersion = 1 };

enum { kMaxNumBlocks = 0x7FFFFFFF };
enum { kSlabMask = 0x80000000 };

struct CtrlMsgHdr {
	enum { kToken = 0xFFFFFFFF };
	enum { kMaxPayloadSize = 1024 };
	uint32_t token;	// = kToken
	uint16_t msgID;
	uint16_t length;
	CtrlMsgHdr(void) : token(kToken) {}
	CtrlMsgHdr(uint16_t msgID_, uint16_t length_)
	: token(kToken), msgID(msgID_), length(length_) {}
};

enum { kMaxCtrlMsgSize = sizeof(CtrlMsgHdr)+CtrlMsgHdr::kMaxPayloadSize };

enum {	// these are the message IDs used in control messages
	kCtrlMsgID_ValidatePeer = 0,	// exchanged both ways on startup
	kCtrlMsgID_CtrlString,
	kCtrlMsgID_BlocksAndInfo,		// Producer tells Manager to enqueue blocks
	kCtrlMsgID_NumSlabs,			// number of producer and consumer slabs
									// Client requests, Manager responds
	kCtrlMsgID_SequenceToken,		// sent by Client, echoed by Manager
	kCtrlMsgID_ClientPID,			// Client tells Manager
	kCtrlMsgID_ClientID,			// Manager tells Client
	kCtrlMsgID_GroupID,				// Client requests, Manager tells
	kCtrlMsgID_ManagerEcho,			// Client tells Manager, Manager echos
	kCtrlMsgID_DropReport,			// Manager tells Client it has dropped a segment
	kCtrlMsgID_DropReportAck,		// Client responds to Manager
	kCtrlMsgID_RegistrationsWanted,	// Client tells Manager
	kCtrlMsgID_Registration=40,		// Client requests reg/dreg of list of mids
									// Manager gives client updates (when requested)
	kCtrlMsgID_MaxRegistration=43,
	kCtrlMsgID_ProdNiceLevel,		// Client tells Manager
};

enum {	// these are the "which" parameters for CtrlString
	kCtrlString_ShmName = 1,		// Manager tells Client on connection
	kCtrlString_ClientName,			// Client tells Manager on connection
	kCtrlString_GroupID				// Client tells Manager
};

enum {	// these are the "type" parameters for Registration messages
	kRegType_RegisterList = 0,		// register a list of msgIDs
	kRegType_DeregisterList,		// deregister a list of msgIDs
	kRegType_RegisterAllMsgs,		// de/register for all msgIDs (bool param)
};

struct RegistrationMsgHdr {
	int16_t clientID;
	int16_t groupID;
	RegistrationMsgHdr(int16_t cid, int16_t gid): clientID(cid), groupID(gid) {}
};

struct RegistrationMsg : public RegistrationMsgHdr {
	enum { kMaxMsgIDs =
		(CtrlMsgHdr::kMaxPayloadSize-sizeof(RegistrationMsgHdr))/sizeof(uint32_t) };
	uint32_t msgIDs[kMaxMsgIDs];
	RegistrationMsg(int16_t cid, int16_t gid): RegistrationMsgHdr(cid,gid) {}
};

} // namespace MCSB

#endif
