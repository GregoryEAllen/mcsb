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

#ifndef MCSB_ClientProxy_h
#define MCSB_ClientProxy_h
#pragma once

#include "MCSB/Manager.h"
#include "MCSB/SocketEndpoint.h"
#include "MCSB/WantedSegment.h"
#include "MCSB/ProxyStats.h"
#include "MCSB/DropReporter.h"
#include <unistd.h>
#include <string>
#include <set>

namespace MCSB {

class Manager::ClientProxy : public SocketDaemon::ClientProxy, public SocketEndpoint {
  public:
	ClientProxy(ev::loop_ref loop, int fd, SocketDaemon::ClientID clientID, SocketDaemon* d);
	~ClientProxy(void);
	int Read(void) { return SocketEndpoint::Poll(0); }

	void CheckBufferParams(void);

	void SendSegments(const uint32_t blockIDs[], const uint32_t slabIDs[],
		const BlockInfo info[], unsigned count);
	int SendRegistration(uint32_t type, int16_t cltID, int16_t grpID, const uint32_t msgIDs[], unsigned count);
	void SendRegistrationsToFD(int fd);
	bool WantRegistrations(void) const { return wantRegistrations; }

	void EraseWantedSegment(WantedSegment& seg); // when the manager seizes wantedSlabs

	std::string GetClientName(void) const { return clientName; }
	const ProxyStats& GetProxyStats(void) const { return stats; }

	void TakeFreeSlabs(const uint32_t slabIDs[], unsigned count);

  protected:
	Manager* manager;
	pid_t clientPID;
	std::string clientName;
	class SlabTracker;
	SlabTracker* slabs;
	std::set<uint32_t> registeredMsgIDs;
	bool wantRegistrations;
	unsigned blocksPerSlab;
	char prodNiceLevel;
	unsigned pendingFreeSlabRqsts;
	ProxyStats stats;
	DropReporter dropReporter;

	WantedSegmentCProxyList wantedQueue;
	unsigned maxWantedQueueSize;
	void PopWantedQueue(void);

	int SendBlockIDs(const uint32_t blockIDs[], const uint32_t sizes[], unsigned count);
	int SendSlabIDs(const uint32_t slabIDs[], unsigned count);
	int SendDropReport(uint32_t segs=0, uint32_t bytes=0);
	void HandleClientPID(int32_t pid) { clientPID = pid; }
	void HandleCtrlString(uint32_t which, const char* str);
	void HandleNumSlabs(uint32_t prodSlbs, uint32_t consSlbs);
	void FillProducerSlabs(void);
	void HandleBlockIDs(const uint32_t blockIDs[], unsigned count);
	void HandleSlabIDs(const uint32_t slabIDs[], unsigned count);
	void HandleRegistration(uint32_t type, int16_t clientID, int16_t groupID, const uint32_t msgIDs[], unsigned count);
	void HandleGroupIDStr(const char* groupStr);
	void HandleRegistrationsWanted(bool wantNew, bool wantCurrent);
	void HandleBlocksAndInfo(const uint32_t blockIDs[], const BlockInfo info[], unsigned count);
	void HandleManagerEcho(const void* ptr, uint16_t len);
	void HandleDropReportAck(void);
	void HandleProdNiceLevel(int32_t lvl);
	int HandleSendWouldBlock(void);

	void HandleSequenceToken(uint32_t token)
		{ SendSequenceToken(token); }

	void DeregisterAllMsgIDs(void);
};

} // namespace MCSB

#endif
