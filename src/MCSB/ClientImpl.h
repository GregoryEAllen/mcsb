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

#ifndef MCSB_ClientImpl_h
#define MCSB_ClientImpl_h
#pragma once

#include "MCSB/SocketEndpoint.h"
#include "MCSB/ShmClient.h"
#include "MCSB/ClientOptions.h"
#include "MCSB/ClientSendManager.h"
#include "MCSB/ClientRecvManager.h"
#include "MCSB/ClientCallbacks.h"

namespace MCSB {

class ClientImpl : public SocketEndpoint {
  public:
	ClientImpl(int fd, const ClientOptions& opts);
	~ClientImpl(void);

	int16_t ClientID(void) const { return clientID; }
	int16_t RequestGroupID(const char* groupStr, bool wait=1);
	
	int RegisterMsgIDs(uint32_t msgIDs[], unsigned count)
		{ return SendRegistration(kRegType_RegisterList,msgIDs,count); }
	int DeregisterMsgIDs(uint32_t msgIDs[], unsigned count)
		{ return SendRegistration(kRegType_DeregisterList,msgIDs,count); }

	// copying sends
	int SendMessage(uint32_t msgID, const void* msg, uint32_t len);
	int SendMessage(uint32_t msgID, const struct iovec iov[], int iovcnt);

	typedef ClientSendManager::MessageDescriptor SendMsgDesc;
	typedef ClientRecvManager::MessageDescriptor RecvMsgDesc;
	
	// get a descriptor where we can place the message data for sending
	SendMsgDesc GetSendMsgDesc(uint32_t len, bool contiguous=1, bool poll=1);
	// send the zero-copy message and release the descriptor
	int SendMessage(uint32_t msgID, SendMsgDesc desc, uint32_t len);
	// release the descriptor without sending
	void ReleaseSendMsgDesc(SendMsgDesc desc);

	// methods for receiving messages via descriptors
	bool PendingRecvMessage(void);
	RecvMsgDesc GetRecvMsgDesc(void);
	void ReleaseRecvMsgDesc(RecvMsgDesc desc);

	unsigned PendingRecvSegments(void) const
		{ return recvMgr.NumPendingSegments(); }
	uint64_t NumSegmentsRcvd(void) const
		{ return recvMgr.NumSegmentsRcvd(); }

	uint32_t BlockSize(void) const { return shm.BlockSize(); }
	uint32_t SlabSize(void) const { return shm.SlabSize(); }
	uint32_t NumProducerSlabs(void) const { return numProdSlabs; }
	uint32_t NumConsumerSlabs(void) const { return numConsSlabs; }
	uint32_t MaxSendMessageSize(void) const { return numProdSlabs*SlabSize(); }
	uint32_t MaxRecvMessageSize(void) const { return numConsSlabs*SlabSize(); }

	int SendSequenceToken(void)
		{ return SocketEndpoint::SendSequenceToken(++sequenceTokenSent); }
	unsigned PendingSequenceTokens(void) const;
	
	// handling dropped segment handler (arg is user data)
	void SetDropReportHandler(DropReportHandler func, void* arg=0)
		{ dropReportHandler = std::make_pair(func,arg); }

	// notification of a connection event (arg is user data)
	void SetConnectionEventHandler(ConnectionEventHandler func, void* arg=0) {
		connectionEventHandler = std::make_pair(func,arg);
	}

	// handling registration events (arg is user data)
	void SetRegistrationHandler(RegistrationHandler func, void* arg=0)
		{ registrationHandler = std::make_pair(func,arg);
			SendRegistrationsWanted(!!func); }

	// CCI interfaces
	int SendCCI(uint32_t cciMsgID, const void* msg, uint32_t len);

	void PrintState(void) const;

  protected:
	ShmClient shm;
	ClientOptions opts;
	ClientSendManager sendMgr;
	ClientRecvManager recvMgr;
	int16_t clientID;
	uint32_t numProdSlabs, numProdSlabsRqstd;
	uint32_t numConsSlabs, numConsSlabsRqstd;
	uint32_t sequenceTokenSent;
	uint32_t sequenceTokenRcvd;
	std::vector<BlockInfo> blockInfo;
	std::pair<DropReportHandler,void*> dropReportHandler;
	std::pair<ConnectionEventHandler,void*> connectionEventHandler;
	std::pair<RegistrationHandler,void*> registrationHandler;

	uint64_t crcErrors;
	uint64_t sendCallingPoll;

	int SendRegistration(uint32_t type, const uint32_t msgIDs[], unsigned count);
	int SendRetiredSlabs(void);
	int SendRetiredSegments(void);
	int SendSequenceToken(uint32_t token); // make protected

	void HandleClientID(int16_t id);
	void HandleCtrlString(uint32_t which, const char* str);
	int ComputeAndSendNumSlabs(void);
	void HandleNumSlabs(uint32_t prodSlabs, uint32_t consSlabs);
	void HandleDropReport(uint32_t segs, uint32_t bytes);
	void HandleSlabIDs(const uint32_t slabIDs[], unsigned count);
	void HandleBlockIDs(const uint32_t blockIDs[], unsigned count);
	void HandleManagerEcho(const void* ptr, uint16_t len) {} // do nothing
	void HandleRegistration(uint32_t type, int16_t clientID, int16_t groupID, const uint32_t msgIDs[], unsigned count);

	void HandleSequenceToken(uint32_t token)
		{ sequenceTokenRcvd = token; }
};

} // namespace MCSB

#endif
