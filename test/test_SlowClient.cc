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
#include "MCSB/Client.h"

#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <ev++.h>
#include <vector>

// globals
std::vector<int> done;
MCSB::ClientOptions gopts;
int msgSize = 8192;
int numMessages = 10000;
int msgWait = 1000;

//-----------------------------------------------------------------------------
void* ProdThread(void* arg)
//-----------------------------------------------------------------------------
{
	unsigned id = *((int*)arg);
	fprintf(stderr, "- prod[%d] in %s\n", id, __PRETTY_FUNCTION__);

	MCSB::ClientOptions opts(gopts);
	opts.clientName += "-prod";
	MCSB::BaseClient client(opts);

	while (!client.MaxSendMessageSize()) {
		if (client.Poll()<0)
			sleep(1);
	}

	uint32_t msgID = getpid() + (id>>1);
	int pctDone = 0;
	int msgsSent = 0;
	while (1) {
		MCSB::SendMessageDescriptor smd = client.GetSendMessageDescriptor(msgSize);
		if (!smd.Valid()) {
			fprintf(stderr,"p");
			continue;
		}
		if (smd.SendMessage(msgID,msgSize)>0)
			msgsSent++;
	//	fprintf(stderr, "msgsSent %d\n", msgsSent);
		float pct = msgsSent*100./numMessages;
		if (int(pct)!=pctDone) {
			pctDone = int(pct);
			fprintf(stderr, "s%02d ", pctDone);
		}
		if (msgsSent>=numMessages) break;
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "- prod[%d] sent %d messages\n", id, numMessages);

	done[id] = 1;
	return (void*)0;
}

//-----------------------------------------------------------------------------
void* ConsThread(void* arg)
//-----------------------------------------------------------------------------
{
	unsigned id = *((int*)arg);
	fprintf(stderr, "- cons[%d] in %s\n", id, __PRETTY_FUNCTION__);

	MCSB::ClientOptions opts(gopts);
	opts.clientName += "-cons";
	MCSB::BaseClient client(opts);

	while (!client.MaxSendMessageSize()) {
		client.Poll(); // to initialize
	}

	uint32_t msgID = getpid() + (id>>1);
	client.RegisterMsgIDs(&msgID,1);
	int consecutivePolls = 0;
	int msgsRcvd = 0;
	while (1) {
		if (!client.PendingRecvMessage()) {
			client.Poll(.1);
			fprintf(stderr, "c");
			if (++consecutivePolls>10) break;
		}
		MCSB::RecvMessageDescriptor rmd = client.GetRecvMessageDescriptor();
		if (!rmd.Valid()) continue;
		consecutivePolls = 0;
		msgsRcvd++;
//		fprintf(stderr, "- client[%d] nbytes %d\n", id, rmd.Nbytes());
		usleep(msgWait);
	}

	fprintf(stderr, "- cons[%d] rcvd %d messages\n", id, msgsRcvd);

	done[id] = 1;
	return (void*)0;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	const char* usage = "usage: %s [options] [msgSize [numMessages]]";
	MCSB::TestingClientOptions opts(argc,argv,usage);
	gopts = opts;

	// parse the args for this program
	argc -= optind;
	argv += optind;
	if (argc) {
		msgSize = atoi(argv[0]);
	}
	if (argc>1) {
		numMessages = atoi(argv[1]);
	}
	fprintf(stderr, "msgSize %d\n", msgSize);
	fprintf(stderr, "numMessages %d\n", numMessages);

	ev::default_loop loop;
	MCSB::ManagerParams mparms(opts.ManagerArgc(), opts.ManagerArgv());
	MCSB::Manager manager(mparms, loop);

	loop.run(EVRUN_NOWAIT);

	int numClients = 2;
	done.resize(numClients);
	pthread_t threads[numClients];
	unsigned ids[numClients];
	for (int i=0; i<numClients; i++) {
		done[i] = 0;
		ids[i] = i;
		void* (*func)(void*) = i&1 ? &ConsThread : &ProdThread;
		int err = pthread_create(&threads[i], 0, func, &ids[i]);
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
