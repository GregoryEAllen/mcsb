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

#ifndef MCSB_Manager_h
#define MCSB_Manager_h
#pragma once

#include "MCSB/ManagerParams.h"
#include "MCSB/SocketDaemon.h"
#include "MCSB/ShmMapper.h"
#include "MCSB/SlabManager.h"
#include "MCSB/GroupManager.h"
#include "MCSB/SlabRequestManager.h"
#include "MCSB/ManagerStats.h"
#include "MCSB/uptimer.h"

#include <ev++.h>

namespace MCSB {

class Manager : public SocketDaemon, public uptimer {
  public:
	Manager(const ManagerParams& params, ev::loop_ref loop);
   ~Manager(void);
	
	using SocketDaemon::AddConnectedClient;
	class ClientProxy;

	const char* ShmNameFormat(void) const { return shmMapper.ShmNameFormat(); }
	int GetSlabReservation(unsigned numSlabs);
	int ReleaseSlabReservation(unsigned numSlabs)
		{ return slabManager.ReleaseSlabReservation(numSlabs); }
	
	void RequestFreeSlabs(unsigned numSlabs, ClientProxy* proxy, int niceLevel=0);
	size_t NumFreeSlabs(void) const { return slabManager.NumFreeSlabs(); }
	void IncrementHeldRefcnt(const uint32_t slabIDs[], unsigned count)
		{ return slabManager.IncrementHeldRefcnt(slabIDs,count); }
	void DecrementHeldRefcnt(const uint32_t slabIDs[], unsigned count,
		const unsigned amounts[] = 0)
		{ return slabManager.DecrementHeldRefcnt(slabIDs,count,amounts); }
	WantedSegment& GetWantedSegment(uint16_t clientID, uint32_t blockID)
		{ return slabManager.GetWantedSegment(clientID,blockID,
			blockID/blocksPerSlab); }
	void ReleaseWantedSegment(WantedSegment& seg)
		{ slabManager.ReleaseWantedSegment(seg,seg.BlockID()/blocksPerSlab); }

	int16_t GetGroupID(const char* groupStr) { return groupManager.GetGroupID(groupStr); }
	int DecGroupID(short gid) { return groupManager.DecGroupID(gid); }

	int SendRegistration(uint32_t type, int16_t clientID, int16_t groupID,
		const uint32_t msgIDs[], unsigned count);
	void SendRegistrationsToFD(int fd);

	void TakeBlocksAndInfo(const uint32_t blocks[], const BlockInfo info[],
		unsigned count, int16_t srcGroupID);

	uint32_t TotalNumSlabs(void) const { return shmMapper.TotalNumSlabs(); }
	unsigned BlocksPerSlab(void) const { return blocksPerSlab; }
	const BlockInfo* GetBlockInfo(uint32_t blockID) const
		{ return shmMapper.GetBlockInfo(blockID); }
	const char* GetBlockPtr(uint32_t blockID) const
		{ return shmMapper.GetBlockPtr(blockID); }

	void SentStats(uint64_t segs, uint64_t bytes)
		{ stats.sentSegs += segs; stats.sentBytes += bytes; }
	void DroppedStats(uint64_t segs, uint64_t bytes)
		{ stats.droppedSegs += segs; stats.droppedBytes += bytes; }
	
	bool PlaybackMode(void) const { return playbackMode; }
	
	void HastyCleanup(void);
	static void StaticHastyCleanup(void);

  protected:
	ShmMapper shmMapper;
	SlabManager slabManager;
	GroupManager groupManager;
	ManagerStats stats;
	SlabRequestManager slabRqstManager;
	bool allowUnlockedMemory;
	bool playbackMode;
	SocketDaemon::ClientProxy* CreateNewClientProxy(ev::loop_ref loop,
		int fd, ClientID clientID, SocketDaemon* daemon);
	void HandleSigInt(ev::sig &signal, int revents);
	void HandleTimer(void);
	void HandleIdle(void);
	unsigned sigintCount;
	ev::loop_ref loop;
	ev::sig sigintWatcher;
	ev::sig sigtermWatcher;
	ev::timer timerWatcher;
	ev::idle idleWatcher;
	unsigned blocksPerSlab;
	static Manager* theManager;
	static void HandleFatalSignal(int sig);
	client_citer statsIter;

	int IncreaseNumBuffers(unsigned newNumBuffers=0);
	void ErasingClientHook(ClientID clientID,
		const SocketDaemon::ClientProxy* proxy);
	unsigned HarvestWantedSlabs(unsigned count);
	void ServiceSlabRequests(void);
	void HandleSlabStateChange(int which, const SlabManager::SlabInfo& slab);
};

} // namespace MCSB

#endif
