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

#include "MCSB/TestingClientOptions.h"
#include "MCSB/Manager.h"
#include "MCSB/SocketClient.h"
#include "MCSB/ClientImpl.h"
#include "MCSB/ClientImplWatcher.h"
#include "rand_buf.h"

#include <ev++.h>
#include <unistd.h>

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	MCSB::TestingClientOptions opts(argc,argv);

	ev::default_loop loop;
	MCSB::ManagerParams mparms(opts.ManagerArgc(), opts.ManagerArgv());
	MCSB::Manager manager(mparms, loop);
	loop.run(EVRUN_NOWAIT);

	try {
		int fd = MCSB::OpenSocketClient(opts.ctrlSockName.c_str());
		MCSB::ClientImpl cb(fd, opts);
		MCSB::ClientImplWatcher watcher(&cb,loop);
		loop.run(EVRUN_NOWAIT);

		cb.RequestGroupID("",0);
		while (cb.GroupID()<0)
			loop.run(EVRUN_NOWAIT);
		loop.run(EVRUN_NOWAIT);

		uint32_t msgIDs[] = { 1, 2, 3, 4, 5 };
		unsigned numMsgIDs = sizeof(msgIDs)/sizeof(uint32_t);
		cb.RegisterMsgIDs(msgIDs, numMsgIDs);
		
		for (int i=0; i<10; i++)
			loop.run(EVRUN_NOWAIT);
		cb.DeregisterMsgIDs(msgIDs, numMsgIDs);
		for (int i=0; i<10; i++)
			loop.run(EVRUN_NOWAIT);

		uint32_t seed = 42;
		uint32_t vseed = seed;
		for (uint32_t mid=0; mid<10; mid++) {
			loop.run(EVRUN_NOWAIT);
			fprintf(stderr,"==== mid %u ====\n", mid);
			cb.RegisterMsgIDs(&mid, 1);
			loop.run(EVRUN_NOWAIT);
			unsigned msgSize = rand() % (mparms.slabSize+1);
			MCSB::ClientImpl::SendMsgDesc smd;
			smd = cb.GetSendMsgDesc(msgSize,0);
			assert(smd->Contiguous());
			MCSB::set_rand_buf((unsigned*)(smd->Buf()), msgSize/sizeof(uint32_t), seed);
			cb.SendMessage(mid,smd,msgSize);
			loop.run(EVRUN_NOWAIT); // manager handles and returns msg
			loop.run(EVRUN_NOWAIT); // client gets msg
			MCSB::ClientImpl::RecvMsgDesc rmd;
			rmd = cb.GetRecvMsgDesc();
			assert(rmd);
			uint32_t recvSize = rmd->Size();
			unsigned errs = MCSB::verify_rand_buf((unsigned*)(rmd->Buf()),
				recvSize/sizeof(uint32_t), vseed);
			assert(recvSize==msgSize);
			assert(!errs);
			cb.ReleaseRecvMsgDesc(rmd);
			cb.DeregisterMsgIDs(&mid, 1);
			loop.run(EVRUN_NOWAIT);
		}

		for (int i=0; i<10; i++)
			loop.run(EVRUN_NOWAIT);

	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- %s\n", err.what());
		return -1;
	}

	// allow manager to service the client disconnection
	for (int i=0; i<10; i++)
		loop.run(EVRUN_NOWAIT);

	// verify that all slabs are free
	size_t freeSlabs = manager.NumFreeSlabs();
	size_t numSlabs = manager.TotalNumSlabs();
	if (freeSlabs!=numSlabs) {
		fprintf(stderr,"### freeSlabs %lu, numSlabs %lu\n",
			(unsigned long)freeSlabs, (unsigned long)numSlabs);
		assert(freeSlabs==numSlabs);
	}

	return 0;
}
