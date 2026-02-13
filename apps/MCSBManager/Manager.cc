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

#include "MCSB/Manager.h"
#include "MCSB/ClientProxy.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>


namespace MCSB {

Manager* Manager::theManager=0;

//-----------------------------------------------------------------------------
Manager::Manager(const ManagerParams& p, ev::loop_ref loop_)
//-----------------------------------------------------------------------------
:	SocketDaemon(loop_, p.ctrlSockName.c_str(),p.verbosity,p.force,p.maxNumClients,p.backlog),
	shmMapper(p.shmNameFmt.c_str(),p.force,p.blockSize,p.slabSize,p.numBlocks,p.maxNumBuffers),
	slabManager(p.SlabsPerBuffer(),p.numBuffers,p.nonrsrvblePct),
	allowUnlockedMemory(p.allowUnlockedMemory), playbackMode(p.playbackMode),
	sigintCount(0), loop(loop_), sigintWatcher(loop), sigtermWatcher(loop),
	timerWatcher(loop), idleWatcher(loop)
{
	theManager = this;
	statsIter = clients.end();
	blocksPerSlab = shmMapper.BlocksPerSlab();
	int which = SlabManager::kHeldToFree | SlabManager::kWantedToFree;
	slabManager.SetSlabStateChangeHandler<Manager,
		&Manager::HandleSlabStateChange>(this,which);
	sigintWatcher.set<Manager, &Manager::HandleSigInt>(this);
	sigtermWatcher.set<Manager, &Manager::HandleSigInt>(this);
	sigintWatcher.start(SIGINT);
	sigtermWatcher.start(SIGTERM);
	timerWatcher.set<Manager, &Manager::HandleTimer>(this);
	timerWatcher.set(1.,1.);
	timerWatcher.start();
	idleWatcher.set<Manager, &Manager::HandleIdle>(this);
	signal(SIGSEGV,HandleFatalSignal);
	signal(SIGBUS,HandleFatalSignal);
	IncreaseNumBuffers(p.numBuffers);
}

//-----------------------------------------------------------------------------
Manager::~Manager(void)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
int Manager::GetSlabReservation(unsigned numSlabs)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	int failed = 0;
	while (1) {
		failed = slabManager.GetSlabReservation(numSlabs);
		if (!failed) break;
		if (!IncreaseNumBuffers()) continue;
		dbprintf(kNotice,"# Manager::GetSlabReservation is denying a request for %d slabs\n", numSlabs);
		break;
	}
	return failed;
}

//-----------------------------------------------------------------------------
void Manager::RequestFreeSlabs(unsigned numSlabs, ClientProxy* proxy, int niceLevel)
//-----------------------------------------------------------------------------
{
	niceLevel += playbackMode;
	if (niceLevel) {
		// all nice free slabs go through the slabRqstManager
		slabRqstManager.AddRequest(niceLevel,numSlabs,proxy->ClientID(),proxy);
		ServiceSlabRequests();
		return;
	}

	// greedy slab acquisition, which can drop segments
	uint32_t freeSlabIDs[numSlabs];
	unsigned count = 0;
	if (numSlabs>NumFreeSlabs())
		count = numSlabs-NumFreeSlabs();
	// this causes dropped wanted segments
	HarvestWantedSlabs(count);
	unsigned slabsGiven = slabManager.GetFreeSlabs(freeSlabIDs,numSlabs);
	proxy->TakeFreeSlabs(freeSlabIDs,slabsGiven);
	if (slabsGiven!=numSlabs) {
		// this is supposed to be guaranteed/enforced by reservation
		dbprintf(kNotice,"slabsGiven (%u) != numSlabs (%u)\n", slabsGiven, numSlabs);
	}
}

//-----------------------------------------------------------------------------
void Manager::ServiceSlabRequests(void)
//-----------------------------------------------------------------------------
{
	// are there freeSlab requests and freeSlabs to fulfill them?
	while (slabRqstManager.NumRequests()) {
		if (!slabManager.NumFreeSlabs())
			break;
		unsigned level = slabRqstManager.LowestLevelPending();
		const SlabRequestManager::SlabRequest& req = slabRqstManager.FrontRequest(level);
		slabRqstManager.PopFrontRequest(level);
		// make sure clientID and clientProxy are still valid
		int clientID = req.Id();
		client_iter it = clients.find(clientID);
		if (it==clients.end()) continue;
		ClientProxy* proxy = dynamic_cast<ClientProxy*>(it->second);
		if (!proxy || proxy!=req.Arg()) continue;
		// fulfill one slab of the request
		uint32_t freeSlabID;
		slabManager.GetFreeSlabs(&freeSlabID,1);
		proxy->TakeFreeSlabs(&freeSlabID,1);
		// do we need another request?
		unsigned slabsPending = req.NumSlabs()-1;
		if (slabsPending)
			slabRqstManager.AddRequest(level,slabsPending,clientID,proxy);
	}
}

//-----------------------------------------------------------------------------
void Manager::HandleSlabStateChange(int which, const SlabManager::SlabInfo& slab)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	if (which==SlabManager::kHeldToFree || which==SlabManager::kWantedToFree) {
		// a slab became free
		ServiceSlabRequests();
	}
}

//-----------------------------------------------------------------------------
unsigned Manager::HarvestWantedSlabs(unsigned count)
// this causes dropped wanted segments/messages
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	if (count>slabManager.NumWantedSlabs())
		count = slabManager.NumWantedSlabs();
	
	unsigned origNumFreeSlabs = NumFreeSlabs();
	while (NumFreeSlabs()-origNumFreeSlabs<count) {
		// drop wantedSegments to increase freeSlabs
		WantedSegment& seg = slabManager.FrontWantedSegment();
		// erase from client's wantedQueue
		client_iter it = clients.find(seg.ClientID());
		if (it==clients.end()) {
			dbprintf(kWarning,"%s: bad clientID %u\n",__PRETTY_FUNCTION__, seg.ClientID());
			ReleaseWantedSegment(seg);
			continue;
		}
		ClientProxy* proxy = dynamic_cast<ClientProxy*>(it->second);
		if (!proxy) continue;
		proxy->EraseWantedSegment(seg);
	}

	return count;
}

//-----------------------------------------------------------------------------
int Manager::IncreaseNumBuffers(unsigned newNumBuffers)
// try to allocate another buffer (unless newNumBuffers is specified)
// returns 0 on success
//-----------------------------------------------------------------------------
{
	if (newNumBuffers==shmMapper.NumBuffers())
		return 0; // nothing to do
	if (!newNumBuffers)
		newNumBuffers = shmMapper.NumBuffers()+1;

	shmMapper.NumBuffers(newNumBuffers);
	if (shmMapper.NumBuffers()!=newNumBuffers) return -1;

	// check for memory lock errors
	if (shmMapper.LockErrors() && !allowUnlockedMemory) {
		dbprintf(kError,"# Error: failed to lock MCSB buffer memory (override with -F)\n");
		throw std::runtime_error("failed to lock MCSB buffer memory");
		return -1;
	}

	dbprintf(kNotice,"- Manager now using %d buffer%s\n", newNumBuffers,
		newNumBuffers>1?"s":"");
	slabManager.NumBuffers(newNumBuffers);
	// tell each client
	for (client_iter i=clients.begin(); i!=clients.end(); ++i) {
		ClientProxy* proxy = dynamic_cast<ClientProxy*>(i->second);
		if (!proxy) continue;
		proxy->CheckBufferParams();
	}
	return 0;
}

//-----------------------------------------------------------------------------
SocketDaemon::ClientProxy* Manager::CreateNewClientProxy(ev::loop_ref loop_,
	int fd, ClientID clientID, SocketDaemon* daemon)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug, "%s\n", __PRETTY_FUNCTION__);
	Manager::ClientProxy* newProxy = new Manager::ClientProxy(loop_,fd,clientID,daemon);
	newProxy->Verbosity(Verbosity());
	return newProxy;
}

//-----------------------------------------------------------------------------
int Manager::SendRegistration(uint32_t type, int16_t clientID, int16_t groupID,
		const uint32_t msgIDs[], unsigned count)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);

	// loop over the clients and send
	int result = 0;
	for (client_iter i=clients.begin(); i!=clients.end(); ++i) {
		ClientProxy* proxy = dynamic_cast<ClientProxy*>(i->second);
		if (!proxy || !proxy->WantRegistrations()) continue;
		result += proxy->SendRegistration(type,clientID,groupID,msgIDs,count);
	}
	return result;
}

//-----------------------------------------------------------------------------
void Manager::SendRegistrationsToFD(int fd)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	// loop over the clients and send
	for (client_iter i=clients.begin(); i!=clients.end(); ++i) {
		ClientProxy* proxy = dynamic_cast<ClientProxy*>(i->second);
		if (!proxy) continue;
		proxy->SendRegistrationsToFD(fd);
	}
}

//-----------------------------------------------------------------------------
void Manager::TakeBlocksAndInfo(const uint32_t blockIDs[], const BlockInfo info[],
	unsigned count, int16_t srcGroupID)
// somebody sent these blocks as segments/messages
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	uint32_t slabIDs[count];
	unsigned totalNumBlocks = shmMapper.TotalNumBlocks();
	for (unsigned blk=0; blk<count; blk++) {
		uint32_t blockID = blockIDs[blk];
		if (blockID>=totalNumBlocks) {
			throw std::runtime_error("client sent blockID>=totalNumBlocks");
		}
		slabIDs[blk] = blockID/blocksPerSlab;
		BlockInfo* blockInfo = shmMapper.GetBlockInfo(blockID);
		*blockInfo = info[blk];
		stats.rcvdBytes += info[blk].size;
	}
	stats.rcvdSegs += count;
	
	// loop over the clients and send the segments
	for (client_iter i=clients.begin(); i!=clients.end(); ++i) {
		ClientProxy* proxy = dynamic_cast<ClientProxy*>(i->second);
		if (!proxy || !proxy->Connected()) continue;
		// don't send back to the same non-zero groupID
		if (srcGroupID && srcGroupID==proxy->GroupID()) continue;
		proxy->SendSegments(blockIDs,slabIDs,info,count);
	}
}

//-----------------------------------------------------------------------------
void Manager::HandleSigInt(ev::sig &signal, int revents)
//-----------------------------------------------------------------------------
{
	fprintf(stderr, "# received %s, terminating\n", strsignal(signal.signum));

	slabRqstManager.Reset();
	SocketDaemon::CloseAllClients();
	loop.break_loop(ev::ALL);

	// multiple SIGINTs/SIGTERMs will exit immediately
	if (sigintCount++) {
		HastyCleanup();
		exit(-1);
	}
}

//-----------------------------------------------------------------------------
void Manager::HandleFatalSignal(int signum)
//-----------------------------------------------------------------------------
{
	fprintf(stderr, "# received %s, terminating\n", strsignal(signum));
	StaticHastyCleanup();
	exit(-1);
}

//-----------------------------------------------------------------------------
void Manager::HastyCleanup(void)
//	only for hasty shutdown, otherwise the dtors should do the job
//-----------------------------------------------------------------------------
{
	shmMapper.~ShmMapper();
	SocketDaemon::CloseAndRemoveListener();
}

//-----------------------------------------------------------------------------
void Manager::StaticHastyCleanup(void)
//	only for hasty shutdown, otherwise the dtors should do the job
//-----------------------------------------------------------------------------
{
	if (theManager)
		theManager->HastyCleanup();
}

//-----------------------------------------------------------------------------
void Manager::HandleTimer(void)
//-----------------------------------------------------------------------------
{
	if (Verbosity()<=kNotice) return;
	
	stats.uptime = uptime();
	stats.numClients = clients.size();
	stats.numBuffers = shmMapper.NumBuffers();
	stats.numSlabs = slabManager.NumSlabs();
	stats.numSlabsReserved = slabManager.NumSlabsReserved();
	stats.numSlabsNonreservable = slabManager.NumSlabsNonreservable();
	stats.numFreeSlabs = slabManager.NumFreeSlabs();
	stats.numHeldSlabs = slabManager.NumHeldSlabs();
	stats.numWantedSlabs = slabManager.NumWantedSlabs();
	stats.Print(stderr);
	
	statsIter = clients.begin();
	idleWatcher.start();
}

//-----------------------------------------------------------------------------
void Manager::HandleIdle(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	
	if (statsIter==clients.end()) {
		idleWatcher.stop();
		return;
	}

	ClientProxy* proxy = dynamic_cast<ClientProxy*>(statsIter->second);
	++statsIter;
	if (!proxy) return;
	
	std::string clientName = proxy->GetClientName();
	const ProxyStats& proxyStats =  proxy->GetProxyStats();
	proxyStats.Print(stderr,clientName.c_str());
}

//-----------------------------------------------------------------------------
void Manager::ErasingClientHook(ClientID clientID,
		const SocketDaemon::ClientProxy* proxy)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n",__PRETTY_FUNCTION__);
	if (statsIter==clients.end())
		return;
	// increment statsIter before the client gets erased
	if (clientID == statsIter->first)
		++statsIter;
}

//-----------------------------------------------------------------------------
void ManagerStats::Print(FILE* file) const
//-----------------------------------------------------------------------------
{
	fprintf(file,"---\n");
	fprintf(file,"ManagerStats:\n");
	fprintf(file,"  uptime: %f\n", uptime);

	fprintf(file,"  numClients: %u\n", numClients);

	fprintf(file,"  numBuffers: %u\n", numBuffers);
	fprintf(file,"  numSlabs: %u\n", numSlabs);
	fprintf(file,"  numSlabsReserved: %u\n", numSlabsReserved);
	fprintf(file,"  numSlabsNonreservable: %u\n", numSlabsNonreservable);
	fprintf(file,"  numFreeSlabs: %u\n", numFreeSlabs);
	fprintf(file,"  numHeldSlabs: %u\n", numHeldSlabs);
	fprintf(file,"  numWantedSlabs: %u\n", numWantedSlabs);

	fprintf(file,"  rcvdSegs: %llu\n", (unsigned long long)rcvdSegs);
	fprintf(file,"  rcvdBytes: %llu\n", (unsigned long long)rcvdBytes);
	fprintf(file,"  sentSegs: %llu\n", (unsigned long long)sentSegs);
	fprintf(file,"  sentBytes: %llu\n", (unsigned long long)sentBytes);
	fprintf(file,"  droppedSegs: %llu\n", (unsigned long long)droppedSegs);
	fprintf(file,"  droppedBytes: %llu\n", (unsigned long long)droppedBytes);
	fprintf(file,"...\n");
}

} // namespace MCSB
