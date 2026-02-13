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

#include "MCSB/TestingClientOptions.h"
#include "MCSB/Manager.h"
#include "MCSB/SocketClient.h"
#include "MCSB/ClientImpl.h"
#include "rand_buf.h"

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <ev++.h>
#include <vector>

// globals
std::vector<int> done;
MCSB::ClientOptions gopts;
int numMessages = 100;
int doLfsr = 1;

//-----------------------------------------------------------------------------
void* ClientThread(void* arg)
//-----------------------------------------------------------------------------
{
	unsigned id = *((int*)arg);
	fprintf(stderr, "%s id: %u\n", __PRETTY_FUNCTION__, id);
	
	try {
		MCSB::ClientOptions opts(gopts);
		int fd = MCSB::OpenSocketClient(opts.ctrlSockName.c_str());
		MCSB::ClientImpl cb(fd, opts);
	
		uint32_t msgID = id;
		cb.RegisterMsgIDs(&msgID, 1);

		while (!cb.MaxSendMessageSize()) {
			cb.Poll(); // to initialize
		}
		if (!id) {
			fprintf(stderr,"- maxSendMessageSize %u\n", cb.MaxSendMessageSize());
			fprintf(stderr,"- maxRecvMessageSize %u\n", cb.MaxRecvMessageSize());
		}
		unsigned maxMsgSize = cb.SlabSize();

		unsigned seed = 42 + id;
		unsigned vseed = seed;
		unsigned rseed = seed + getpid();
		for (int i=0; i<numMessages; i++) {
			unsigned msgSize = rand_r(&rseed) % (maxMsgSize+1);
			unsigned len = msgSize/sizeof(uint32_t);
			MCSB::ClientImpl::SendMsgDesc smd;
			smd = cb.GetSendMsgDesc(msgSize);
			if (doLfsr)
				MCSB::set_rand_buf((unsigned*)(smd->Buf()), len, seed);
			cb.SendMessage(msgID,smd,msgSize);
			while (!cb.PendingRecvMessage()) {
				cb.Poll();
			}
			MCSB::ClientImpl::RecvMsgDesc rmd;
			rmd = cb.GetRecvMsgDesc();
			uint32_t recvSize = rmd->Size();
			unsigned errs = 0;
			if (doLfsr)
				errs += MCSB::verify_rand_buf((unsigned*)(rmd->Buf()), len, vseed);
			if (recvSize != msgSize) errs++;
			cb.ReleaseRecvMsgDesc(rmd);
			if (errs) {
				throw std::runtime_error("verify_rand_buf reports error");
			}
		}
		
		cb.SendSequenceToken();
		while (cb.PendingSequenceTokens())
			cb.Poll();

	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- ClientThread[%u] %s\n", id, err.what());
		done[id] = 1;
		return (void*)1;
	}

	done[id] = 1;
	return (void*)0;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	MCSB::TestingClientOptions opts(argc,argv);
	gopts = opts;

	int numClients = 4;
	// parse the args for this program
	argc -= optind;
	argv += optind;
	if (argc) {
		numClients = atoi(argv[0]);
	}
	if (argc>1) {
		numMessages = atoi(argv[1]);
	}
	if (argc>2) {
		doLfsr = atoi(argv[2]);
	}
	fprintf(stderr, "numClients %u\n", numClients);
	fprintf(stderr, "numMessages %u\n", numMessages);
	fprintf(stderr, "doLfsr %u\n", doLfsr);

	ev::default_loop loop;
	MCSB::ManagerParams mparms(opts.ManagerArgc(), opts.ManagerArgv());
	MCSB::Manager manager(mparms, loop);

	loop.run(EVRUN_NOWAIT);

	done.resize(numClients);
	pthread_t threads[numClients];
	unsigned ids[numClients];
	for (int i=0; i<numClients; i++) {
		done[i] = 0;
		ids[i] = i;
		int err = pthread_create(&threads[i], 0, &ClientThread, &ids[i]);
		if (err) {
			perror("pthread_create error");
			return err;
		}
		loop.run(EVRUN_NOWAIT);
	}
	int i=0;
	long errs = 0;
	while (i<numClients) {
		if (done[i]) {
			void* err;
			pthread_join(threads[i++], &err);
			errs += (long)err;
		}
		else usleep(1);
		loop.run(EVRUN_NOWAIT);
	}
	
	if (!errs) {
		fprintf(stderr, "==== PASS ====\n");
	}
	
	return errs;
}
