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

#ifndef MCSB_SocketEndpoint_h
#define MCSB_SocketEndpoint_h
#pragma once

#include "MCSB/SocketProtocol.h"
#include "MCSB/dbprinter.h"
#include <stdint.h>
#include <vector>

namespace MCSB {

class BlockInfo;

class SocketEndpoint : public dbprinter {
  public:
	SocketEndpoint(int fd, int verbosity=0, unsigned recvBufCapacity=kMaxCtrlMsgSize);
	virtual ~SocketEndpoint(void);

	int Poll(float timeout=-1.);
	int FD(void) const { return sockFD; }

	int SendCtrlMsg(uint16_t msgID, const void* ptr, uint16_t len);
	int SendCtrlMsg(uint16_t msgID, const void* ptrs[], uint16_t lens[]);
	int SendBlockIDs(const uint32_t blockIDs[], unsigned count);
	int SendSlabIDs(const uint32_t slabIDs[], unsigned count);
	int SendBlocksAndInfo(const uint32_t blockIDs[], const BlockInfo blockInfo[], unsigned count);
	int SendNumSlabs(uint32_t prodSlabs, uint32_t consSlabs);
	int SendCtrlString(uint32_t which, const char* str);
	int SendDropReport(uint32_t segs, uint32_t bytes);
	int SendDropReportAck(void)
		{ return SendCtrlMsg(kCtrlMsgID_DropReportAck,(void*)0,0); }
	int SendRegistration(uint32_t type, int16_t cltID, int16_t grpID, const uint32_t msgIDs[], unsigned count);
	int SendSequenceToken(uint32_t token)
		{ return SendCtrlMsg(kCtrlMsgID_SequenceToken,&token,sizeof(token)); }
	int SendClientPID(int32_t pid)
		{ return SendCtrlMsg(kCtrlMsgID_ClientPID,&pid,sizeof(pid)); }
	int SendClientID(int16_t id);
	int SendGroupID(int16_t id, bool setLocal=true);
	int SendRegistrationsWanted(bool wanted=true);
	int SendManagerEcho(const void* ptr=0, uint16_t len=0)
		{ return SendCtrlMsg(kCtrlMsgID_ManagerEcho,ptr,len); }
	int SendProdNiceLevel(int lvl)
		{ return SendCtrlMsg(kCtrlMsgID_ProdNiceLevel,&lvl,sizeof(lvl)); }

	unsigned GetSendSockBufSize(void) { return GetSockBufSize(1); }
	unsigned GetRecvSockBufSize(void) { return GetSockBufSize(0); }
	void SetSendSockBufSize(unsigned size) { return SetSockBufSize(size,1); }
	void SetRecvSockBufSize(unsigned size) { return SetSockBufSize(size,0); }

	int16_t GroupID(void) const { return groupID; }

	bool Connected(void) const { return !sendFailed; }

  protected:
	int sockFD;
	std::vector<char> recvBuf;
	unsigned recvBufLen;
	bool sendFailed;
	bool validPeer;
	bool throwOnPeerDisconnect;
	int16_t groupID;

	unsigned GetSockBufSize(bool send);
	void SetSockBufSize(unsigned size, bool send);
	int SetSocketOptions(void);
	static int PollFD(int fd, float timeout=-1, short events=0);
	int Parse(void);
	int SendValidatePeer(void);

	// all called from inside of Poll()
	void LocalHandleCtrlMsg(uint16_t msgID, const void* ptr, uint16_t len);
	virtual void HandleCtrlMsg(uint16_t msgID, const void* ptr, uint16_t len);
	virtual void HandleCtrlString(uint32_t which, const char* str);
	virtual void HandleBlockIDs(const uint32_t blockIDs[], unsigned count);
	virtual void HandleSlabIDs(const uint32_t slabIDs[], unsigned count);
	virtual void HandleBlocksAndInfo(const uint32_t blockIDs[], const BlockInfo info[], unsigned count);
	virtual void HandleManagerEcho(const void* ptr, uint16_t len);
	virtual void HandleNumSlabs(uint32_t prodSlabs, uint32_t consSlabs);
	virtual void HandleDropReport(uint32_t segs, uint32_t bytes);
	virtual void HandleDropReportAck(void);
	virtual void HandleSequenceToken(uint32_t token);
	virtual void HandleClientPID(int32_t pid);
	virtual void HandleClientID(int16_t cid);
	virtual void HandleGroupID(int16_t gid);
	virtual void HandleRegistrationsWanted(bool wantNew, bool wantCurrent);
	virtual void HandleRegistration(uint32_t type, int16_t clientID, int16_t groupID, const uint32_t msgIDs[], unsigned count);
	virtual void HandleValidatePeer(uint32_t magic, uint32_t protoVersion);
	virtual void HandleProdNiceLevel(int32_t lvl);

	virtual int HandleSendWouldBlock(void);

};

} // namespace MCSB

#endif
