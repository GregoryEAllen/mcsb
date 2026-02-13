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
#include "MCSB/ClientWatcher.h"
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
	void HandleMessage(const MCSB::RecvMessageDescriptor& desc) {
		dbprintf(kInfo, "client[%d] %s\n", id, __PRETTY_FUNCTION__);
		uint32_t len = desc.Size();
		assert(len>=sizeof(int));
		int recvSeq = *((const int*)desc.Buf());
		dbprintf(kInfo, "client[%d] recvSeq %d\n", id, recvSeq);
		assert(recvSeq==sequence+1 || recvSeq==sequence);
		sequence = recvSeq;
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

	ev::default_loop loop;
	float period = 0.01; // just so the test goes fast
	MCSB::ClientWatcher watcher(client,loop,period);

	client.RegisterForMsgID<MyTester,&MyTester::HandleMessage>(msgID,this);

	// wait for connection
	while(!client.Connected()) {
		loop.run(EVLOOP_ONESHOT);
	}

	// send with ptr,len
	int sendSeq = 1;
	client.SendMessage(msgID,&sendSeq,sizeof(sendSeq));
	sendSeq++;
	client.SendMessage(msgID,&sendSeq,sizeof(sendSeq));

	while(sequence!=sendSeq) {
		loop.run(EVLOOP_ONESHOT);
	}

	// disconnect so ClientWatcher will reconnect
	client.Close();
	while(!client.Connected()) {
		loop.run(EVLOOP_ONESHOT);
	}

	sendSeq++;
	client.SendMessage(msgID,&sendSeq,sizeof(sendSeq));
	sendSeq++;
	client.SendMessage(msgID,&sendSeq,sizeof(sendSeq));

	while(sequence!=sendSeq) {
		loop.run(EVLOOP_ONESHOT);
	}
	
	return 0;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	MyTester tester(argc,argv);
	return tester.Test();
}
