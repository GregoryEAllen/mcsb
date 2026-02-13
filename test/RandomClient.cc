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

#include "RandomClient.h"
#include "rand_buf.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

namespace MCSB {

//-----------------------------------------------------------------------------
RandomClient::RandomClient(Client* client_, int numMessages_, int id_,
	bool fillData_, bool contiguous_)
//-----------------------------------------------------------------------------
:	client(client_), numMessages(numMessages_), id(id_), started(0),
	fillData(fillData_), contiguous(contiguous_), totalErrs(0)
{
	Verbosity(client->Verbosity());

	msgID = getpid();
	unsigned seed = msgID;
	randSeed = rand_r(&seed);
	dbprintf(kInfo,"RandomClient[%u]: using msgID %u\n", id, msgID);
	
	// register some messageIDs
	client->RegisterForMsgID<MCSB::RandomClient,&MCSB::RandomClient::HandleMessage>(msgID, this);
	client->SetDropReportHandler<MCSB::RandomClient,&MCSB::RandomClient::HandleDropReport>(this);
}

//-----------------------------------------------------------------------------
RandomClient::~RandomClient(void)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
bool RandomClient::Done(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	
	if (started) {
		return !numMessages;
	}
	
	// try to send
	if (!client->Connected()) return 0;
	if (!client->MaxSendMessageSize()) return 0;

	uint32_t sendSz = client->MaxSendMessageSize();
	uint32_t recvSz = client->MaxRecvMessageSize();
	maxMsgSize = sendSz<recvSz ? sendSz : recvSz;
	if (contiguous) {
		maxMsgSize = client->SlabSize();
	}

	dbprintf(kInfo,"RandomClient[%u]: maxMsgSize %u\n", id, maxMsgSize);
	messageBuffer.reserve(maxMsgSize/sizeof(uint32_t));

	SendMessage();
	return 0;
}

//-----------------------------------------------------------------------------
void RandomClient::SendMessage(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);
	
	verfSeed = randSeed;
	uint32_t numElems;
	set_rand_buf(&numElems, 1, randSeed);
	numElems %= maxMsgSize/sizeof(uint32_t)+1;
	dbprintf(kInfo,"RandomClient[%u]: numMessages %d, numElems %u, len %u\n",
		id, numMessages, numElems, numElems*sizeof(uint32_t));
	
	// FIXME: eliminate messageBuffer and use zero-copy
	// FIXME: support discontiguous
	messageBuffer.resize(numElems);
	if (fillData)
		set_rand_buf(&messageBuffer[0], numElems, randSeed);
	
	int res = client->SendMessage(msgID,&messageBuffer[0],numElems*sizeof(uint32_t));
	started = res>=0;
}

//-----------------------------------------------------------------------------
void RandomClient::HandleMessage(uint32_t msgID, const void* buf, uint32_t len)
//-----------------------------------------------------------------------------
{
	dbprintf(kDebug,"%s\n", __PRETTY_FUNCTION__);

	// verify the message
	uint32_t numElems;
	set_rand_buf(&numElems, 1, verfSeed);
	numElems %= maxMsgSize/sizeof(uint32_t)+1;
	
	if (len!=numElems*sizeof(uint32_t)) {
		totalErrs++;
		dbprintf(kWarning,"len %u but numElems*4 %u\n", len, numElems*4);
	}
	if (fillData)
		totalErrs += verify_rand_buf((const uint32_t*)buf, len/sizeof(uint32_t), verfSeed);

	if (numMessages>0) if (!--numMessages) return;
	SendMessage();
}

//-----------------------------------------------------------------------------
void RandomClient::HandleDropReport(uint32_t segs, uint32_t bytes)
//-----------------------------------------------------------------------------
{
	dbprintf(kNotice,"### RandomClient[%u]: dropped %u segs, %u bytes\n", id, segs, bytes);
	//started = 0;
	numMessages = 0;
	totalErrs++;
}
} // namespace MCSB

