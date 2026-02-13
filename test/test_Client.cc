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

// always assert for tests, even in Release
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "ClientTester.h"
#include "MCSB/Client.h"

#include <cassert>
#include <list>

class MyTester : public MCSB::ClientTester {
  public:
	MyTester(int argc, char* const argv[])
		: MCSB::ClientTester(argc,argv), sequence(0) {}
   ~MyTester(void) {}
	int RunClient(void);
  protected:
	int sequence;
	std::list<MCSB::RecvMessageDescriptor> rmdList;
	static void HandleMessageF(const MCSB::RecvMessageDescriptor& desc, void* arg) {
		fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
		MyTester* t = (MyTester*)arg;
		t->HandleMessage1(desc);
	}
	void HandleMessage1(const MCSB::RecvMessageDescriptor& desc) {
		dbprintf(kInfo, "client[%d] %s\n", id, __PRETTY_FUNCTION__);
		HandleMessage2(desc.MessageID(), desc.Buf(), desc.Size());
		rmdList.push_back(desc);
		if (rmdList.size()>4) {
			MCSB::RecvMessageDescriptor rmd = rmdList.front();
			rmdList.pop_front();
		}
	}
	void HandleMessage2(uint32_t msgID, const void* msg, uint32_t len) {
		dbprintf(kInfo, "client[%d] %s\n", id, __PRETTY_FUNCTION__);
		assert(len>=sizeof(int));
		int recvSeq = *((const int*)msg);
		dbprintf(kInfo, "client[%d] recvSeq %d\n", id, recvSeq);
		assert(recvSeq-sequence<=1);
		sequence = recvSeq;
	}
	void HandleDropReport(uint32_t segs, uint32_t bytes) {
		dbprintf(kNotice, "client[%d] %s\n", id, __PRETTY_FUNCTION__);
	}
	void HandleConnectionEvent(int which) {
		dbprintf(kNotice, "client[%d] %s %d\n", id, __PRETTY_FUNCTION__, which);
		if (which!=MCSB::kDisconnection) return;
		// clean up the rmdList
		while (rmdList.size()) rmdList.pop_front();
	}
};

//-----------------------------------------------------------------------------
int MyTester::RunClient(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kInfo, "client[%d] starting\n", id);
	uint32_t msgID = id;
	dbprintf(kInfo, "client[%d] using msgID %d\n", id, msgID);
	
	MCSB::Client client(opts);

	// several ways to handle messages
	client.RegisterForMsgID(msgID,&MyTester::HandleMessageF,this);
	client.RegisterForMsgID<MyTester,&MyTester::HandleMessage1>(msgID,this);
	client.RegisterForMsgID<MyTester,&MyTester::HandleMessage2>(msgID,this);

	// other handlers
	client.SetDropReportHandler<MyTester,&MyTester::HandleDropReport>(this);
	client.SetConnectionEventHandler<MyTester,&MyTester::HandleConnectionEvent>(this);

	while (!client.Connected()) {
		if (!client.Connect()) usleep(100000);
	}

	// send with ptr,len
	int sendSeq = 1;
	client.SendMessage(msgID,&sendSeq,sizeof(sendSeq));
	
	// send with iov
	sendSeq++;
	const int nvec = 4;
	struct iovec msgiov[nvec];
	msgiov[0].iov_base = (void*)&sendSeq;
	msgiov[0].iov_len = sizeof(sendSeq);
	client.SendMessage(msgID,msgiov,1);
	
	// send with a bigger iov
	sendSeq++;
	for (int i=0; i<nvec; i++) {
		msgiov[i].iov_base = (void*)&sendSeq;
		msgiov[i].iov_len = sizeof(sendSeq);
	}
	client.SendMessage(msgID,msgiov,nvec);
	
	MCSB::SendMessageDescriptor smd1 = client.GetSendMessageDescriptor(sizeof(int));
	assert(smd1.Valid());
	MCSB::SendMessageDescriptor smd2(client.GetSendMessageDescriptor(sizeof(int)));
	assert(smd2.Valid());
	MCSB::SendMessageDescriptor smd3 = smd1;
	assert(!smd1.Valid());
	assert(smd3.Valid());

	// send using a descriptor
	sendSeq++;
	int* ptr = (int*)smd2.Buf();
	*ptr = sendSeq;
	client.SendMessage(msgID,smd2,sizeof(sendSeq));
	assert(!smd2.Valid()); // sending invalidates descriptor

	// send through a descriptor (it remembers the client)
	sendSeq++;
	ptr = (int*)smd3.Buf();
	*ptr = sendSeq;
	smd3.SendMessage(msgID,sizeof(sendSeq));
	assert(!smd3.Valid()); // sending invalidates descriptor

	// send with invalid descriptors!
	assert(client.SendMessage(msgID,smd3,sizeof(sendSeq))<0);
	assert(smd3.SendMessage(msgID,sizeof(sendSeq))<0);

	while(sequence!=sendSeq) {
		if (client.Poll(.1)<0) usleep(10000);
	}

	// make sure SMD ownership is working as expected
	MCSB::SendMessageDescriptor smd;
	if (smd.Valid()) return -1;
	{
		MCSB::SendMessageDescriptor smd2 = client.GetSendMessageDescriptor(1);
		smd = smd2;
	}
	if (!smd.Valid()) return -1;
	{
		MCSB::SendMessageDescriptor smd2 = smd;
	}
	if (smd.Valid()) return -1;

	// verify assignment of an already-constructed SMD
	smd = client.GetSendMessageDescriptor(1);
	{
		MCSB::SendMessageDescriptor smd2;
		smd2 = smd;
		smd2.Reset();
	}

	// use an array of send descriptors
	// note that an STL container is explicitly disallowed!
	const int ndesc = 4;
	MCSB::SendMessageDescriptor smdArray[ndesc];

	for (int i=0; i<ndesc; i++) {
		MCSB::SendMessageDescriptor smd = client.GetSendMessageDescriptor(sizeof(int));
		sendSeq++;
		int* ptr = (int*)smd.Buf();
		*ptr = sendSeq;
		smdArray[i] = smd;
		assert(!smd.Valid());
		assert(smdArray[i].Valid());
	}

	// and send them
	for (int i=0; i<ndesc; i++) {
		smdArray[i].SendMessage(msgID,sizeof(sendSeq));
		assert(!smdArray[i].Valid());
	}

	// we can implement our own loop with RMDs
	while(sequence!=sendSeq) {
		while (!client.PendingRecvMessage()) {
			bool serviceRecvMessages = 0;
			if (client.Poll(.1,serviceRecvMessages)<0) usleep(10000);
		}
		MCSB::RecvMessageDescriptor rmd = client.GetRecvMessageDescriptor();
		client.HandleRecvMessage(rmd);
	}

	int lvl = kInfo;
	dbprintf(lvl, "client[%d] BlockSize %u\n", id, client.BlockSize());
	dbprintf(lvl, "client[%d] SlabSize %u\n", id, client.SlabSize());
	dbprintf(lvl, "client[%d] NumProducerSlabs %u\n", id, client.NumProducerSlabs());
	dbprintf(lvl, "client[%d] NumConsumerSlabs %u\n", id, client.NumConsumerSlabs());
	dbprintf(lvl, "client[%d] MaxSendMessageSize %u\n", id, client.MaxSendMessageSize());
	dbprintf(lvl, "client[%d] MaxRecvMessageSize %u\n", id, client.MaxRecvMessageSize());
	dbprintf(lvl, "client[%d] FD %u\n", id, client.FD());
	dbprintf(lvl, "client[%d] ClientID %u\n", id, client.ClientID());
	
	client.SendSequenceToken();
	while (client.PendingSequenceTokens()) {
		client.Poll();
	}
	client.Flush();

	// lets disconnect and reconnect several times
	for (int i=0; i<100; i++) {
		client.Close();
		client.Connect();
		sendSeq++;
		int res = client.SendMessage(msgID,&sendSeq,sizeof(sendSeq));
		assert(res==4);
		while(sequence!=sendSeq) client.Poll();
	}

	client.DeregisterForMsgID(msgID,&MyTester::HandleMessageF,this);
	client.DeregisterForMsgID<MyTester,&MyTester::HandleMessage1>(msgID,this);
	client.DeregisterForMsgID<MyTester,&MyTester::HandleMessage2>(msgID,this);

	return 0;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	MyTester tester(argc,argv);
	return tester.Test();
}
