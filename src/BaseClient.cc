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

#include "MCSB/BaseClient.h"
#include "MCSB/ClientImpl.h"
#include "MCSB/SocketClient.h"
#include "MCSB/dbprinter.h"
#include "MCSB/uptimer.h"
#include "MCSB/MCSBVersion.h"

#include <unistd.h>
#include <stdexcept>
#include <assert.h>

extern const char* kHgRevision_MCSB;

namespace MCSB {

//-----------------------------------------------------------------------------
void BaseClient::Init(bool connect)
//-----------------------------------------------------------------------------
{
	Verbosity(options.verbosity);
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	cimpl = 0;
	SetConnectionEventHandler(0);
	SetDropReportHandler(0);
	SetRegistrationHandler(0);
	connecting = 0;
	if (connect)
		Connect();
}

//-----------------------------------------------------------------------------
BaseClient::~BaseClient(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	DeregisterAllMsgIDs();
	Close();
}

//-----------------------------------------------------------------------------
void BaseClient::Close(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	if (!cimpl) return;
	
	if (connectionEventHandler.first)
		(*connectionEventHandler.first)(kDisconnection,connectionEventHandler.second);

	// this is the only place we delete the cimpl, because
	// nearly anything else could be called from HandleInput(...)
	delete cimpl;
	cimpl = 0;
}

//-----------------------------------------------------------------------------
bool BaseClient::Connect(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	
	if (cimpl) {
		if (cimpl->Connected()) {
			return 1;
		} else {
			Close();
			if (cimpl) return 0; // could not delete cimpl
		}
	}
	
	if (connecting) {
		dbprintf(kWarning,"# Recursively called %s\n", __PRETTY_FUNCTION__);
		return 0;
	}
	
	try {
		int fd = OpenSocketClient(options.ctrlSockName.c_str());
		connecting = 1;
		dbprintf(kInfo,"- connected to %s on fd %d\n", options.ctrlSockName.c_str(), fd);
		if (connectionEventHandler.first)
			(*connectionEventHandler.first)(kConnection,connectionEventHandler.second);
		options.verbosity = Verbosity();
		cimpl = new ClientImpl(fd,options);
		if (groupStr.size())
			cimpl->RequestGroupID(groupStr.c_str(),0);
		cimpl->SetDropReportHandler(dropReportHandler.first,dropReportHandler.second);
		cimpl->SetConnectionEventHandler(connectionEventHandler.first,connectionEventHandler.second);

		// wait until cimpl has mmapped and gotten number of slabs
		// and gotten assigned a GroupID
		while (!cimpl->MaxSendMessageSize() ||
			(groupStr.size() && GroupID()<=0) ) {
			dbprintf(kDebug,"- BaseClient::Connect calling ClientImpl::Poll()\n");
			if (connectionEventHandler.first)
				(*connectionEventHandler.first)(kPoll,connectionEventHandler.second);
			cimpl->Poll(.1);
		}

		// send registrations to Client
		std::vector<uint32_t> msgIDs;
		MsgIdMap::iterator it = registeredMsgIDs.begin();
		for (; it!=registeredMsgIDs.end(); ++it) {
			uint32_t mid = it->first;
			msgIDs.push_back(mid);
		}
		cimpl->RegisterMsgIDs(&msgIDs[0],msgIDs.size());
		cimpl->SetRegistrationHandler(registrationHandler.first,registrationHandler.second);

	} catch (std::runtime_error err) {
		Close();
		dbprintf(kNotice, "#-- %s\n", err.what());
	}

	connecting = 0;
	return Connected();
}

//-----------------------------------------------------------------------------
bool BaseClient::Connected(void) const
//-----------------------------------------------------------------------------
{
	return cimpl && cimpl->Connected();
}

//-----------------------------------------------------------------------------
int BaseClient::Poll(float timeout)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	if (!Connected() && !connecting)
		Connect();
	int result = -1;
	try {
		if (cimpl && !connecting)
			result = cimpl->Poll(timeout);

	} catch (std::runtime_error err) {
		Close();
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return result;
}

//-----------------------------------------------------------------------------
int BaseClient::SendMessage(uint32_t msgID, const void* msg, uint32_t len)
//-----------------------------------------------------------------------------
{
	if (!cimpl)
		Connect();

	try {
		if (cimpl)
			return cimpl->SendMessage(msgID,msg,len);
	} catch (std::runtime_error err) {
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return -1;
}

//-----------------------------------------------------------------------------
int BaseClient::SendMessage(uint32_t msgID, const struct iovec msgiov[], int iovcnt)
//-----------------------------------------------------------------------------
{
	if (!cimpl)
		Connect();

	try {
		if (cimpl)
			return cimpl->SendMessage(msgID,msgiov,iovcnt);
	} catch (std::runtime_error err) {
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return -1;
}

//-----------------------------------------------------------------------------
int BaseClient::SendMessage(uint32_t msgID, SendMessageDescriptor& desc, uint32_t len)
//-----------------------------------------------------------------------------
{
	if (!cimpl)
		Connect();

	try {
		if (cimpl) {
			if (cimpl!=desc.CImpl()) {
				dbprintf(kNotice, "#-- SendMessageDescriptor ClientImpl mismatch\n");
				return -1;
			}
			return cimpl->SendMessage(msgID,desc.Release(),len);
		}
	} catch (std::runtime_error err) {
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return -1;
}

//-----------------------------------------------------------------------------
SendMessageDescriptor BaseClient::GetSendMessageDescriptor(uint32_t len,
	bool contiguous, bool poll)
//-----------------------------------------------------------------------------
{
	if (!cimpl)
		Connect();

	SendMessageDescriptor desc;
	try {
		if (cimpl) {
			SendMsgSegment* seg = cimpl->GetSendMsgDesc(len, contiguous, poll);
			desc.Reset(seg, cimpl);
		}
	} catch (std::runtime_error err) {
		Close();
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return desc;
}

//-----------------------------------------------------------------------------
int BaseClient::RegisterMsgIDs(const uint32_t msgIDs[], int count)
// returns the number that were actually registered (i.e. weren't already)
//-----------------------------------------------------------------------------
{
	int regCount = 0;
	uint32_t regIDs[count];
	
	for (int i=0; i<count; i++) {
		uint32_t msgID = msgIDs[i];
		MsgIdMap::iterator it = registeredMsgIDs.find(msgID);
		if (it==registeredMsgIDs.end()) { // not found
			regIDs[regCount++] = msgID;
			uint32_t cnt = 1;
			registeredMsgIDs.insert( std::make_pair(msgID,cnt) );
		} else {
			it->second++;
		}
	}

	try {
		if (regCount && cimpl && cimpl->Connected())
			cimpl->RegisterMsgIDs(regIDs,regCount);
	} catch (std::runtime_error err) {
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return regCount;
}

//-----------------------------------------------------------------------------
int BaseClient::DeregisterMsgIDs(const uint32_t msgIDs[], int count)
// returns the number that were actually deregistered
//-----------------------------------------------------------------------------
{
	int deregCount = 0;
	uint32_t deregIDs[count];
	
	for (int i=0; i<count; i++) {
		uint32_t msgID = msgIDs[i];
		MsgIdMap::iterator it = registeredMsgIDs.find(msgID);
		if (it==registeredMsgIDs.end()) continue; // not found
		if (--(it->second)==0) {
			deregIDs[deregCount++] = msgID;
			registeredMsgIDs.erase(it);
		}
	}

	try {
		if (deregCount && cimpl && cimpl->Connected())
			cimpl->DeregisterMsgIDs(deregIDs,deregCount);
	} catch (std::runtime_error err) {
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return deregCount;
}

//-----------------------------------------------------------------------------
bool BaseClient::PendingRecvMessage(void)
//-----------------------------------------------------------------------------
{
	return cimpl && cimpl->Connected() && cimpl->PendingRecvMessage();
}

//-----------------------------------------------------------------------------
RecvMessageDescriptor BaseClient::GetRecvMessageDescriptor(void)
//-----------------------------------------------------------------------------
{
	RecvMsgSegment* seg = 0;
	if (cimpl)
		seg = cimpl->GetRecvMsgDesc();
	return RecvMessageDescriptor(seg, cimpl);
}

//-----------------------------------------------------------------------------
void BaseClient::DeregisterAllMsgIDs(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	// clear out registeredMsgIDs
	while(!registeredMsgIDs.empty()) {
		const int kMaxCount = 64;
		uint32_t msgIDs[kMaxCount];
		int count = 0;
		MsgIdMap::iterator it = registeredMsgIDs.begin();
		for (; count<kMaxCount && it != registeredMsgIDs.end(); count++, it++) {
			msgIDs[count] = it->first;
		}
		DeregisterMsgIDs(msgIDs,count);
	}
}

//-----------------------------------------------------------------------------
void BaseClient::SetDropReportHandler(DropReportHandler func, void* arg)
//-----------------------------------------------------------------------------
{
	dropReportHandler = std::make_pair(func,arg);
	if (cimpl)
		cimpl->SetDropReportHandler(func,arg);
}

//-----------------------------------------------------------------------------
void BaseClient::SetConnectionEventHandler(ConnectionEventHandler func, void* arg)
//-----------------------------------------------------------------------------
{
	connectionEventHandler = std::make_pair(func,arg);
	if (cimpl)
		cimpl->SetConnectionEventHandler(func,arg);
}

//-----------------------------------------------------------------------------
int BaseClient::SendSequenceToken(void)
//-----------------------------------------------------------------------------
{
	if (!cimpl)
		Connect();

	try {
		if (cimpl) {
			return cimpl->SendSequenceToken();
		}
	} catch (std::runtime_error err) {
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return -1;
}

//-----------------------------------------------------------------------------
int BaseClient::SendManagerEcho(void)
//-----------------------------------------------------------------------------
{
	if (!cimpl)
		Connect();

	try {
		if (cimpl) {
			return cimpl->SendManagerEcho();
		}
	} catch (std::runtime_error err) {
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return -1;
}

//-----------------------------------------------------------------------------
int BaseClient::Flush(float timeout)
//-----------------------------------------------------------------------------
{
	if (SendSequenceToken()<0)
		return -1;
	while (PendingSequenceTokens()) {
		if (Poll(timeout)<0)
			return -1;
	}
	return 0;
}

//-----------------------------------------------------------------------------
void BaseClient::SetRegistrationHandler(RegistrationHandler func, void* arg)
//-----------------------------------------------------------------------------
{
	registrationHandler = std::make_pair(func,arg);
	try {
		if (cimpl) {
			cimpl->SetRegistrationHandler(func,arg);
		}
	} catch (std::runtime_error err) {
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
}

//-----------------------------------------------------------------------------
int BaseClient::RequestGroupID(const char* groupStr_, bool wait)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	
	try {
		if (groupStr.size()) { // was already set
			throw std::runtime_error("GroupID can only be set once");
		}
		groupStr = groupStr_;
		if (!groupStr.size()) { // empty
			throw std::runtime_error("Invalid GroupID string");
		}
		if (cimpl)
			cimpl->RequestGroupID(groupStr_,wait);
	} catch (std::runtime_error err) {
		Close();
		dbprintf(kNotice, "#-- %s\n", err.what());
	}
	return GroupID();
}

uint32_t BaseClient::BlockSize(void) const
	{ return cimpl ? cimpl->BlockSize() : 0; }
uint32_t BaseClient::SlabSize(void) const
	{ return cimpl ? cimpl->SlabSize() : 0; }
uint32_t BaseClient::NumProducerSlabs(void) const
	{ return cimpl ? cimpl->NumProducerSlabs() : 0; }
uint32_t BaseClient::NumConsumerSlabs(void) const
	{ return cimpl ? cimpl->NumConsumerSlabs() : 0; }
uint32_t BaseClient::MaxSendMessageSize(void) const
	{ return cimpl ? cimpl->MaxSendMessageSize() : 0; }
uint32_t BaseClient::MaxRecvMessageSize(void) const
	{ return cimpl ? cimpl->MaxRecvMessageSize() : 0; }
int BaseClient::FD(void) const
	{ return cimpl ? cimpl->FD() : -1; }
unsigned BaseClient::PendingSequenceTokens(void) const
	{ return cimpl ? cimpl->PendingSequenceTokens() : 0; }
int BaseClient::ClientID(void) const
	{ return cimpl ? cimpl->ClientID() : -1; }
int BaseClient::GroupID(void) const
	{ return cimpl ? cimpl->GroupID() : -1; }
unsigned BaseClient::PendingRecvSegments(void) const
	{ return (cimpl && cimpl->Connected()) ? cimpl->PendingRecvSegments() : 0; }
uint64_t BaseClient::NumSegmentsRcvd(void) const
	{ return cimpl ? cimpl->NumSegmentsRcvd() : 0; }

const char* GetVersion(void)
{	return MCSB_VERSION; }
const char* GetHgRevision(void)
{	return kHgRevision_MCSB; }

} // namespace MCSB
