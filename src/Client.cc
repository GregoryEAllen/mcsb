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

#include "MCSB/Client.h"
#include "MCSB/ClientImpl.h"
#include "MCSB/SocketClient.h"
#include "MCSB/dbprinter.h"

#include <unistd.h>
#include <stdexcept>
#include <assert.h>

namespace MCSB {

//-----------------------------------------------------------------------------
void Client::Init(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	handlingRecvMessage = 0;
}

//-----------------------------------------------------------------------------
Client::~Client(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	DeregisterForAllMsgs();
}

//-----------------------------------------------------------------------------
void Client::Close(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);

	if (handlingRecvMessage) {
		// We can't call BaseClient::Close because that will delete cimpl,
		// which is in use by functions below us on the stack!
		// Instead we can close just the socket, which will later get
		// detected and cause BaseClient::Close() to get called.
		close(FD());
		return;
	}
	BaseClient::Close();
}

//-----------------------------------------------------------------------------
void Client::Run(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	while (true) {
		if (Poll()<0)
			sleep(1);
	}
}

//-----------------------------------------------------------------------------
int Client::Poll(float timeout, bool serviceRecvMessages)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	int result = BaseClient::Poll(timeout);
	
	if (serviceRecvMessages && !handlingRecvMessage) {
		// avoid servicing too long, lest I starve the main event loop
		uint64_t segsAtEntry = NumSegmentsRcvd();
		while (1) {
			bool entrySegsDone =
				segsAtEntry+PendingRecvSegments()<NumSegmentsRcvd();
			if (entrySegsDone || !PendingRecvMessage()) break;
			RecvMessageDescriptor rmd = GetRecvMessageDescriptor();
			HandleRecvMessage(rmd);
		}
		if (PendingRecvSegments() || PendingRecvMessage()) {
			 // I'm leaving work undone, so make me readable in the future
			SendManagerEcho();
		}
	}
	if (!Connected()) return -1;
	return result;
}

//-----------------------------------------------------------------------------
void Client::RegisterForMsgID(uint32_t msgID, MessageHandlerFunction mhf, void* arg)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	assert(mhf);
	messageHandlers.insert( std::make_pair(msgID,std::make_pair(mhf,arg)) );
	RegisterMsgIDs(&msgID,1);
}

//-----------------------------------------------------------------------------
bool Client::DeregisterForMsgID(uint32_t msgID, MessageHandlerFunction mhf, void* arg)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	MessageHandlerMultiMap::iterator it = messageHandlers.lower_bound(msgID);
	MessageHandlerMultiMap::iterator upper = messageHandlers.upper_bound(msgID);
	bool found = false;
	for (; it!=upper; ++it) {
		if (it->second.first == mhf && it->second.second == arg) {
			messageHandlers.erase(it);
			found = true;
			break;
		}
	}
	if (found)
		DeregisterMsgIDs(&msgID,1);
	return found;
}

//-----------------------------------------------------------------------------
void* Client::DeregisterForMsgID(uint32_t msgID, MessageHandlerFunction mhf,
	void* arg, bool (*argsEqualFunc)(void* arg1, void* arg2) )
//	argsEqualFunc compares two args and returns true if they are equal
//	It will be called to compare the passed arg to the arg inserted into the map
//	on success, returns the arg that was in the map
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	MessageHandlerMultiMap::iterator it = messageHandlers.lower_bound(msgID);
	MessageHandlerMultiMap::iterator upper = messageHandlers.upper_bound(msgID);
	bool found = false;
	void* result = 0;
	for (; it!=upper; ++it) {
		if (it->second.first == mhf && (*argsEqualFunc)(it->second.second, arg)) {
			messageHandlers.erase(it);
			found = true;
			result = it->second.second;
			break;
		}
	}
	if (found)
		DeregisterMsgIDs(&msgID,1);
	return result;
}

class IntIncr {
	int& i;
	public:
		IntIncr(int& i_): i(i_) { ++i; }
		~IntIncr(void) { --i; }
};

//-----------------------------------------------------------------------------
void Client::HandleRecvMessage(const RecvMessageDescriptor& rmd)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);

	IntIncr intIncr(handlingRecvMessage);

	// NOTE: if the message handlers were to register/deregister, any iterators
	//	would be invalidated. Therefore, we build a list of handlers and then
	//	call them all

	uint32_t msgID = rmd.MessageID();

	// build an array of handlers to call
	unsigned numHandlers = messageHandlers.count(msgID);
	MessageHandlerFunction funcs[numHandlers];
	void* args[numHandlers];
	
	unsigned i = 0;
	
	// add the messageHandlers with matching msgID
	MessageHandlerMultiMap::iterator it = messageHandlers.lower_bound(msgID);
	MessageHandlerMultiMap::iterator upper = messageHandlers.upper_bound(msgID);
	for (; it!=upper; ++it, ++i) {
		funcs[i] = it->second.first;
		args[i] = it->second.second;
	}
	assert(i==numHandlers);

	// now we call the handlers, and they can't change the data structure under us
	for (i=0; i<numHandlers; i++) {
		(*funcs[i])(rmd,args[i]);
	}

	// warn in case we got an unhandled message
	if (!numHandlers)
		dbprintf(kWarning,"# unhandled msgID %u in %s\n", msgID, __PRETTY_FUNCTION__);
}

//-----------------------------------------------------------------------------
void Client::DeregisterForAllMsgs(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	
	// clear out messageHandlers
	while (!messageHandlers.empty()) {
		MessageHandlerMultiMap::iterator begin = messageHandlers.begin();
		DeregisterForMsgID( begin->first, begin->second.first, begin->second.second );
	}
}

} // namespace MCSB
