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

#include "MCSB/ClientProxy.h"
#include "MCSB/ProxySlabTracker.h"
#include "MCSB/CCIHeader.h"

#include <stdexcept>
#include <sys/socket.h>

namespace MCSB {

//-----------------------------------------------------------------------------
Manager::ClientProxy::ClientProxy(ev::loop_ref loop, int fd,
	SocketDaemon::ClientID clientID, SocketDaemon* daemon)
//-----------------------------------------------------------------------------
:	SocketDaemon::ClientProxy(loop,fd,clientID,daemon), SocketEndpoint(fd),
	manager(0), clientPID(-1), wantRegistrations(0), blocksPerSlab(0),
	prodNiceLevel(0), pendingFreeSlabRqsts(0), maxWantedQueueSize(1024)
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	throwOnPeerDisconnect = 0;
	stats.clientID = clientID;
	
	manager = dynamic_cast<Manager*>(daemon);
	if (!manager) {
		throw std::runtime_error("failed dynamic_cast<Manager*>");
	}
	slabs = new SlabTracker();

	SendClientID(clientID);
	CheckBufferParams(); // tells client how to map the shared memory
	FillProducerSlabs();
//	Poll(0);
}

//-----------------------------------------------------------------------------
Manager::ClientProxy::~ClientProxy(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	while (wantedQueue.size()) { // empty out wantedQueue
		WantedSegment& seg = wantedQueue.front();
		wantedQueue.pop_front();
		manager->ReleaseWantedSegment(seg);
	}

	unsigned reservedSlabs = slabs->NumProdSlabs() + slabs->NumConsSlabs();
	if (manager->ReleaseSlabReservation(reservedSlabs)) {
		dbprintf(kNotice,"Manager::ReleaseSlabReservation() returned an error");
	}

	// release held prodSlabs
	unsigned nProdSlabs = slabs->ProdSlabsHeld();
	if (nProdSlabs) {
		uint32_t prodSlabIDs[nProdSlabs];
		slabs->GetAndEraseProdSlabs(prodSlabIDs,nProdSlabs);
		try {
			manager->DecrementHeldRefcnt(prodSlabIDs,nProdSlabs);
		} catch(std::runtime_error err) {
			dbprintf(kNotice,"Manager::DecrementHeldRefcnt() threw: %s", err.what());
		}
	}

	// release held consSlabs
	unsigned nConsSlabs = slabs->ConsSlabsHeld();
	if (nConsSlabs) {
		uint32_t consSlabIDs[nConsSlabs];
		unsigned refcnts[nConsSlabs];
		slabs->GetAndEraseConsSlabs(consSlabIDs,refcnts,nConsSlabs);
		try {
			manager->DecrementHeldRefcnt(consSlabIDs,nConsSlabs,refcnts);
		} catch(std::runtime_error err) {
			dbprintf(kNotice,"Manager::DecrementHeldRefcnt() threw: %s", err.what());
		}
	}

	// dec groupID if we have one
	if (groupID) {
		manager->DecGroupID(groupID);
	}
	
	try {
		DeregisterAllMsgIDs();
	} catch(std::runtime_error err) {
		dbprintf(kNotice,"ClientProxy::DeregisterAllMsgIDs() threw: %s", err.what());
	}

	delete slabs;
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::DeregisterAllMsgIDs(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	unsigned maxCount = (CtrlMsgHdr::kMaxPayloadSize-sizeof(RegistrationMsgHdr))
		/sizeof(uint32_t);
	uint32_t erasedMsgIDs[maxCount];

	while (registeredMsgIDs.size()) {
		unsigned erasedNum = 0;
		std::set<uint32_t>::iterator it = registeredMsgIDs.begin();
		while (it!=registeredMsgIDs.end() && erasedNum<maxCount) {
			erasedMsgIDs[erasedNum++] = *it;
			std::set<uint32_t>::iterator rmit = it++;
			registeredMsgIDs.erase(rmit);
		}
		manager->SendRegistration(kRegType_DeregisterList,clientID,groupID,
			erasedMsgIDs,erasedNum);
	}
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::CheckBufferParams(void)
//-----------------------------------------------------------------------------
{
	uint32_t totalNumSlabs = manager->TotalNumSlabs();
	slabs->TotalNumSlabs(totalNumSlabs);
	blocksPerSlab = manager->BlocksPerSlab();
	try {
		// tell the client to check the number of buffers
		SendCtrlString(kCtrlString_ShmName,manager->ShmNameFormat());
	} catch(std::runtime_error ex) {
		dbprintf(kNotice, "# while sending to client[%d]: %s\n", clientID, ex.what());
	}
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleCtrlString(uint32_t which, const char* str)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	switch (which) {
		case kCtrlString_ClientName:
			clientName = str;
			dbprintf(kNotice,"- client[%d] name: \"%s\"\n", ClientID(), str);
			break;
		case kCtrlString_GroupID:
			HandleGroupIDStr(str);
			break;
		default:
			dbprintf(kNotice,"# unknown CtrlString type %u in %s\n", which, __PRETTY_FUNCTION__);
	}
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleNumSlabs(uint32_t prodSlbs, uint32_t consSlbs)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	uint32_t numProdSlabs = slabs->NumProdSlabs();
	uint32_t numConsSlabs = slabs->NumConsSlabs();
	int dProdSlabs = prodSlbs-numProdSlabs;
	int dConsSlabs = consSlbs-numConsSlabs;
	if (dProdSlabs<0) {
		dbprintf(kWarning,"# client[%u] tried to shrink numProdSlabs, refused\n", ClientID());
		dProdSlabs = 0;
	}
	if (dConsSlabs<0) {
		dbprintf(kWarning,"# client[%u] tried to shrink numConsSlabs, refused\n", ClientID());
		dConsSlabs = 0;
	}

	int failed = manager->GetSlabReservation(dProdSlabs+dConsSlabs);
	if (!failed) {
		numProdSlabs += dProdSlabs;
		numConsSlabs += dConsSlabs;
		slabs->NumProdSlabs(numProdSlabs);
		slabs->NumConsSlabs(numConsSlabs);
	}
	SendNumSlabs(numProdSlabs, numConsSlabs);
	if (failed) {
		char str[100];
		sprintf(str,"Manager::ReserveSlabs(%d) was denied", dProdSlabs+dConsSlabs);
		throw std::runtime_error(str);
	}

	// FIXME: set socket buffer sizes?
	SetSendSockBufSize(1024*1024);
	SetRecvSockBufSize(1024*1024);
	dbprintf(kInfo,"- client[%d]: sendBuf %u, recvBuf %u\n",
		clientID, GetSendSockBufSize(), GetRecvSockBufSize());

	FillProducerSlabs();
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::FillProducerSlabs(void)
//-----------------------------------------------------------------------------
{
	if (slabs->ProdSlabsNeeded()<=pendingFreeSlabRqsts) return;

	unsigned slabsToRequest = slabs->ProdSlabsNeeded() - pendingFreeSlabRqsts;
	pendingFreeSlabRqsts += slabsToRequest;
	manager->RequestFreeSlabs(slabsToRequest,this,prodNiceLevel);
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::TakeFreeSlabs(const uint32_t slabIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	slabs->InsertProdSlabs(slabIDs,count);
	dbprintf(kDebug,"- client[%d] getting %u free slabs\n", ClientID(), count);
	SendSlabIDs(slabIDs, count);

	if (count>pendingFreeSlabRqsts) {
		dbprintf(kNotice,"- client[%d] took %u prod slabs, but had only %u requests pending\n",
			ClientID(),count,pendingFreeSlabRqsts);
		pendingFreeSlabRqsts = 0;
	} else {
		pendingFreeSlabRqsts -= count;
	}

	unsigned slabsRqstd = slabs->NumProdSlabs();
	unsigned slabsHeld = slabs->ProdSlabsHeld();
	if (slabsHeld>slabsRqstd) {
		dbprintf(kNotice,"- client[%d] holds %u prod slabs, but only wanted %u\n",
			ClientID(),slabsHeld,slabsRqstd);
	}
}

//-----------------------------------------------------------------------------
int Manager::ClientProxy::SendBlockIDs(const uint32_t blockIDs[],
	const uint32_t sizes[], unsigned count)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	int result = -1;
	try {
		// this is ultimately called by a sending client;
		// he shouldn't get disconnect because I am backed up
		result = SocketEndpoint::SendBlockIDs(blockIDs,count);
		uint64_t sentBytes = 0;
		for (unsigned i=0; i<count; i++) {
			sentBytes += sizes[i];
		}
		stats.sentSegs += count;
		stats.sentBytes += sentBytes;
		manager->SentStats(count,sentBytes);
	} catch(std::runtime_error ex) {
		dbprintf(kNotice, "# while sending to client[%d]: %s\n", clientID, ex.what());
	}
	return result;
}

//-----------------------------------------------------------------------------
int Manager::ClientProxy::SendSlabIDs(const uint32_t slabIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	int result = -1;
	try {
		// this may be called by a client retiring a segment or a slab;
		// he shouldn't get disconnected because of me
		result = SocketEndpoint::SendSlabIDs(slabIDs,count);
	} catch(std::runtime_error ex) {
		dbprintf(kNotice, "# while sending to client[%d]: %s\n", clientID, ex.what());
	}
	return result;
}

//-----------------------------------------------------------------------------
int Manager::ClientProxy::SendDropReport(uint32_t segs, uint32_t bytes)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	dropReporter.Dropped(segs,bytes);
	
	int result = -1;
	uint32_t dropSegs, dropBytes;
	if (dropReporter.GetReport(dropSegs,dropBytes)) {
		try {
			// this is ultimately called by a sending client;
			// he shouldn't get disconnect because I am backed up
			result = SocketEndpoint::SendDropReport(dropSegs,dropBytes);
		} catch(std::runtime_error ex) {
			dbprintf(kNotice, "# while sending to client[%d]: %s\n", clientID, ex.what());
		}
	}

	if (segs||bytes)
		manager->DroppedStats(segs,bytes);
	return result;
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleDropReportAck(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	dropReporter.Ack();
	SendDropReport();
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleBlockIDs(const uint32_t blockIDs[], unsigned count)
// the client is done with these segments/messages
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	uint32_t slabIDs[count]; // the slabIDs corresponding to the blockIDs
	for (unsigned i=0; i<count; i++) {
		slabIDs[i] = blockIDs[i]/blocksPerSlab; // convert to slabID
	}

	slabs->DecrementConsSlabs(slabIDs,count);
	manager->DecrementHeldRefcnt(slabIDs,count);

	PopWantedQueue();
}

//-----------------------------------------------------------------------------
// when blockID gets taken by consSlabs
//    manager->IncrementHeldRefcnt(slabID)
// when blockID gets removed from consSlabs
//    manager->DecrementHeldRefcnt(slabID)
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
void Manager::ClientProxy::PopWantedQueue(void)
// send segments/messages from the wantedQueue as we are able
// also enforce maxWantedQueueSize
//-----------------------------------------------------------------------------
{
	while (wantedQueue.size()) {
		WantedSegment& seg = wantedQueue.front();
		uint32_t blockID = seg.BlockID();
		uint32_t slabID = blockID/blocksPerSlab;
		const BlockInfo* info = manager->GetBlockInfo(blockID);
		uint32_t messageID = info->messageID;
		uint32_t segSize = info->size;
		if (registeredMsgIDs.count(messageID)) {
			// try to take the slab
			bool taken = slabs->TakeConsSlab(slabID);
			if (taken) {
				manager->IncrementHeldRefcnt(&slabID,1);
				SendBlockIDs(&blockID,&segSize,1);
			} else {
				if (wantedQueue.size()<=maxWantedQueueSize ||
					manager->PlaybackMode()) break;
				// queue size overage, dropping
				stats.wantedSegsOverage++;
				stats.wantedBytesOverage += segSize;
				SendDropReport(1,segSize);
			}
		}
		stats.wantedSegsOut++;
		stats.wantedBytesOut += segSize;
		wantedQueue.pop_front();
		manager->ReleaseWantedSegment(seg);
	}
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::EraseWantedSegment(WantedSegment& seg)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	uint32_t blockID = seg.BlockID();
	uint32_t segSize = manager->GetBlockInfo(blockID)->size;
	if (wantedQueue.empty()) {
		dbprintf(kError,"### error: %s wantedQueue is empty\n",__PRETTY_FUNCTION__);
		return;
	}
#if 0
	// it's expensive to walk wantedQueue and verify that seg is contained
	WantedSegmentCProxyList::iterator it;
	for (it=wantedQueue.begin(); it!=wantedQueue.end(); ++it) {
		if (&*it==&seg) break;
	}
	if (it==wantedQueue.end()) {
		throw std::runtime_error("ClientProxy::EraseWantedSegment seg not in wantedQueue");
	}
#endif
	stats.wantedSegsDropped++;
	stats.wantedBytesDropped += segSize;
	wantedQueue.erase(seg);
	manager->ReleaseWantedSegment(seg);
	SendDropReport(1,segSize);
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleSlabIDs(const uint32_t slabIDs[], unsigned count)
// the client released these slabs from its producer pool
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	slabs->EraseProdSlabs(slabIDs,count);
	manager->DecrementHeldRefcnt(slabIDs,count);

	// keep it full
	FillProducerSlabs();
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleManagerEcho(const void* ptr, uint16_t len)
//-----------------------------------------------------------------------------
{
	SendManagerEcho(ptr,len);
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleRegistration(uint32_t type, int16_t cid, int16_t gid, const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	if (cid != clientID) {
		throw std::runtime_error("error: clientID mismatch in registration message");
	}
	if (gid != groupID) {
		throw std::runtime_error("error: groupID mismatch in registration message");
	}
	
	uint32_t messageIDs[count];
	unsigned num = 0;
	
	switch (type) {
		case kRegType_RegisterList: {
			for (unsigned i=0; i<count; i++) {
				if (msgIDs[i]==kCCIMessageID) {
					throw std::runtime_error("Cannot register for reserved msgID (kCCIMessageID)");
				}
				if (registeredMsgIDs.insert(msgIDs[i]).second) {
					messageIDs[num++] = msgIDs[i];
				} else {
					dbprintf(kWarning,"# warning: client[%d] attempted to re-register msgID %u\n",clientID,msgIDs[i]);
				}
			}
			if (num)
				manager->SendRegistration(kRegType_RegisterList,cid,gid,messageIDs,num);
		} break;

		case kRegType_DeregisterList: {
			for (unsigned i=0; i<count; i++) {
				if (registeredMsgIDs.erase(msgIDs[i])) {
					messageIDs[num++] = msgIDs[i];
				} else {
					dbprintf(kWarning,"# warning: client[%d] attempted to deregister unregistered msgID %u\n",clientID,msgIDs[i]);
				}
			}
			if (num) {
				manager->SendRegistration(type,cid,gid,messageIDs,num);
			}
		} break;

		default:
			dbprintf(kWarning,"# client[%d]: unknown HandleRegistration(type=%u)\n",clientID, type);
			break;
	}
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleGroupIDStr(const char* groupStr)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	char str[100];
	
	// a client must not change its groupID while it has registrations, because
	// previously sent registration messages would now be incorrect
	if (registeredMsgIDs.size()) {
		sprintf(str,"error: client[%d] changing groupID while registered for messages",clientID);
		throw std::runtime_error(str);
	}
	
	// a client may change its groupID only once
	if (groupID) {
		sprintf(str,"error: client[%d] changing groupID after already set",clientID);
		throw std::runtime_error(str);
	}
	
	short gid = manager->GetGroupID(groupStr);
	dbprintf(kNotice,"- client[%d]: assigned to groupID %d\n",clientID, gid);
	SendGroupID(gid);
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleRegistrationsWanted(bool wantNew, bool wantCurrent)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	wantRegistrations = wantNew;
	if (wantCurrent) {
		manager->SendRegistrationsToFD(sockFD);
	}
}

//-----------------------------------------------------------------------------
int Manager::ClientProxy::SendRegistration(uint32_t type, int16_t cltID,
	int16_t grpID, const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	try {
		return SocketEndpoint::SendRegistration(type,cltID,grpID,msgIDs,count);
	} catch(std::runtime_error ex) {
		dbprintf(kNotice, "# while sending to client[%d]: %s\n", this->clientID, ex.what());
	}
	return 0;
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::SendRegistrationsToFD(int fd)
//	send our current registration state to the requesting filedes
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	// send every registeredMsgIDs to the fd using SocketEndpoint

	unsigned midsToSend = registeredMsgIDs.size();
	std::set<uint32_t>::iterator it = registeredMsgIDs.begin();
	unsigned maxMsgIDsPerLoop = (CtrlMsgHdr::kMaxPayloadSize-sizeof(RegistrationMsgHdr))/sizeof(uint32_t);
	uint32_t msgIDs[maxMsgIDsPerLoop];

	while (midsToSend) {
		// determine number of msgIDs to send
		unsigned count = midsToSend;
		if (count>maxMsgIDsPerLoop)
			count = maxMsgIDsPerLoop;
		
		// copy msgIDs from the set
		for (unsigned i=0; i<count; i++) {
			msgIDs[i] = *it;
			++it;
		}
		
		// call send, faking out the SocketEndpoint with the passed sockFD
		int mySockFD = sockFD;
		sockFD = fd;
		try {
			SocketEndpoint::SendRegistration(kRegType_RegisterList,clientID,
				groupID,msgIDs,count);
		} catch(std::runtime_error ex) {
			dbprintf(kNotice, "# while sending to FD %d error: %s\n", fd, ex.what());
		}
		// restore our own sockFD
		sockFD = mySockFD;
		midsToSend -= count;
	}
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleBlocksAndInfo(const uint32_t blocks[],
	const BlockInfo info[], unsigned count)
// segments/messages rcvd from the client
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	char str[100];
	uint64_t rcvdBytes = 0;
	for (unsigned i=0; i<count; i++) {
		rcvdBytes += info[i].size;
		if (info[i].messageID!=kCCIMessageID) continue;
		const char* blockPtr = manager->GetBlockPtr(blocks[i]);
		uint32_t srcClientID = ((const CCIHeader*)blockPtr)->srcClientID;
		if (srcClientID!=clientID) {
			sprintf(str,"client[%d] clientID mismatch for CCI message",clientID);
			throw std::runtime_error(str);
		}
		uint32_t dstClientID = ((const CCIHeader*)blockPtr)->dstClientID;
		if (clientID && !dstClientID) {
			sprintf(str,"client[%d] sending CCI message to %d",clientID, dstClientID);
			throw std::runtime_error(str);
		}
	}
	manager->TakeBlocksAndInfo(blocks,info,count,groupID);
	stats.rcvdSegs += count;
	stats.rcvdBytes += rcvdBytes;
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::SendSegments(const uint32_t blockIDs[],
	const uint32_t slabIDs[], const BlockInfo info[], unsigned count)
// segments/messages to send to the client (or put in the wanted queue)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	bool wantedSegsPending = wantedQueue.size();

	unsigned sendCount = 0;
	uint32_t sendBlockIDs[count]; // messages we will send
	uint32_t sendSlabIDs[count];
	uint32_t sendSegSizes[count];
	unsigned wantCount = 0;
	uint32_t wantBlockIDs[count]; // messages we want but can't take
	//uint32_t wantSlabIDs[count];
	uint32_t wantSegSizes[count];

	// sort the segments into send vs wanted (vs unwanted)
	for (unsigned i=0; i<count; i++) {
		uint32_t msgID = info[i].messageID;
		bool wantMsg = registeredMsgIDs.count(msgID); // registered
		if (msgID==kCCIMessageID) {
			const char* blockPtr = manager->GetBlockPtr(blockIDs[i]);
			uint32_t dstClientID = ((const CCIHeader*)blockPtr)->dstClientID;
			wantMsg = (dstClientID==clientID) || (dstClientID==kCCIBcastID);
		}
		if (!wantMsg) continue;
		bool tookSlab = !wantCount && !wantedSegsPending &&
			slabs->TakeConsSlab(slabIDs[i]);
		if (tookSlab) {
			sendBlockIDs[sendCount] = blockIDs[i];
			sendSlabIDs[sendCount] = slabIDs[i];
			sendSegSizes[sendCount] = info[i].size;
			sendCount++;
		} else {
			wantBlockIDs[wantCount] = blockIDs[i];
			//wantSlabIDs[wantCount] = slabIDs[i];
			wantSegSizes[wantCount] = info[i].size;
			wantCount++;
		}
	}

	if (sendCount) {
		// tell the manager we're holding sendSlabIDs and send to client
		manager->IncrementHeldRefcnt(sendSlabIDs,sendCount);
		SendBlockIDs(sendBlockIDs,sendSegSizes,sendCount);
	}

	for (unsigned i=0; i<wantCount; i++) {
		// put wantBlockIDs into the wantedQueue
		uint32_t blockID = wantBlockIDs[i];
		WantedSegment& seg = manager->GetWantedSegment(clientID,blockID);
		wantedQueue.push_back(seg);
		stats.wantedBytesIn += wantSegSizes[i];
	}
	stats.wantedSegsIn += wantCount;

	// enforce maxWantedQueueSize
	PopWantedQueue();
}

//-----------------------------------------------------------------------------
void Manager::ClientProxy::HandleProdNiceLevel(int32_t level)
//-----------------------------------------------------------------------------
{
	dbprintf(kNotice,"%s\n", __PRETTY_FUNCTION__);
	prodNiceLevel = level;
}

//-----------------------------------------------------------------------------
int Manager::ClientProxy::HandleSendWouldBlock(void)
//	return true to try again, or false to fail
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	shutdown(sockFD,SHUT_RD);
	return 0; // send will fail
}

//-----------------------------------------------------------------------------
void ProxyStats::Print(FILE* file, const char* clientName) const
//-----------------------------------------------------------------------------
{
	fprintf(file,"---\n");
	fprintf(file,"ProxyStats:\n");
	fprintf(file,"  clientID: %d\n", clientID);
	if (clientName)
		fprintf(file,"  clientName: %s\n", clientName);
	fprintf(file,"  sentSegs: %llu\n", (unsigned long long)sentSegs);
	fprintf(file,"  sentBytes: %llu\n", (unsigned long long)sentBytes);
	fprintf(file,"  rcvdSegs: %llu\n", (unsigned long long)rcvdSegs);
	fprintf(file,"  rcvdBytes: %llu\n", (unsigned long long)rcvdBytes);
	fprintf(file,"  wantedSegsIn: %llu\n", (unsigned long long)wantedSegsIn);
	fprintf(file,"  wantedBytesIn: %llu\n", (unsigned long long)wantedBytesIn);
	fprintf(file,"  wantedSegsOut: %llu\n", (unsigned long long)wantedSegsOut);
	fprintf(file,"  wantedBytesOut: %llu\n", (unsigned long long)wantedBytesOut);
	fprintf(file,"  wantedSegsDropped: %llu\n", (unsigned long long)wantedSegsDropped);
	fprintf(file,"  wantedBytesDropped: %llu\n", (unsigned long long)wantedBytesDropped);
	fprintf(file,"  wantedSegsOverage: %llu\n", (unsigned long long)wantedSegsOverage);
	fprintf(file,"  wantedBytesOverage: %llu\n", (unsigned long long)wantedBytesOverage);
	fprintf(file,"...\n");
}

} // namespace MCSB
