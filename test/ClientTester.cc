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
#include "ClientTester.h"
#include "MCSB/Client.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

namespace MCSB {

//-----------------------------------------------------------------------------
ClientTester::ClientTester(int argc, char* const argv[])
//-----------------------------------------------------------------------------
: opts(argc,argv,"usage: %s [options] [numClients]"), id(-1), numClients(4)
{
	// numClients is the first argument
	argc -= optind;
	argv += optind;
	if (argc) {
		numClients = atoi(argv[0]);
	}
	Verbosity(opts.verbosity);
	dbprintf(kInfo, "numClients %d\n", numClients);
}

//-----------------------------------------------------------------------------
ClientTester::~ClientTester(void)
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
int ClientTester::Test(void)
//-----------------------------------------------------------------------------
{
	bool runManager = 1;
	if (numClients<=0) {
		runManager = 0;
		numClients = -numClients;
	}

	int errs = 0;
	if (numClients) {
		for (int i=0; i<numClients; i++) {
			id = i;
			pid_t pid = fork();
			if (!pid) return RunClient();
		}
		id = -1;
		errs = runManager ? RunManager() : WaitForClients();
	} else {
		// run just one, don't fork, and don't spawn a Manager
		id = 0;
		errs = RunClient();
	}
	
	if (!errs) {
		dbprintf(kInfo, "==== PASS ====\n");
	}
	return errs;
}

//-----------------------------------------------------------------------------
int ClientTester::RunClient(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kInfo, "client %d starting\n", id);
	MCSB::Client client(opts);
	
	// connect and then disconnect
	while (!client.Connected()) {
		if (client.Poll(1)<0) usleep(1000);
		else break;
	}

	return 0;
}

//-----------------------------------------------------------------------------
int ClientTester::RunManager(void)
//-----------------------------------------------------------------------------
{
	dbprintf(kInfo, "Manager starting\n");
	
	int errs = 0;
	try {
		ev::default_loop loop;
		MCSB::ManagerParams mparms(opts.ManagerArgc(), opts.ManagerArgv());
		MCSB::Manager manager(mparms, loop);

		int clientsLeft = numClients;
		while (clientsLeft>0) {
			dbprintf(kInfo, "ClientTester: clientsLeft %d\n", clientsLeft);
			int status;
			pid_t res = waitpid(-1, &status, WNOHANG);
			if (!res) {
				loop.run(EVLOOP_ONESHOT);
				continue;
			}
			errs += !WIFEXITED(status) || WEXITSTATUS(status);
			clientsLeft--;
			loop.run(EVRUN_NOWAIT);
		}

	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- %s\n", err.what());
		return -1;
	}
	
	return errs;
}

//-----------------------------------------------------------------------------
int ClientTester::WaitForClients(void)
//-----------------------------------------------------------------------------
{
	int errs = 0;
	int clientsLeft = numClients;
	while (clientsLeft>0) {
		dbprintf(kInfo, "ClientTester: clientsLeft %d\n", clientsLeft);
		int status;
		wait(&status);
		errs += !WIFEXITED(status) || WEXITSTATUS(status);
		clientsLeft--;
	}
	return errs;
}

} // namespace MCSB
