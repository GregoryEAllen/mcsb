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

#include "MCSB/SocketDaemon.h"
#include "MCSB/SocketClient.h"
#include "MCSB/TestingClientOptions.h"

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <stdexcept>
#include <deque>
#include <ev++.h>

//-----------------------------------------------------------------------------
int main()
//-----------------------------------------------------------------------------
{
	MCSB::TestingClientOptions opts;
	std::string sockName = opts.ctrlSockName;

	int verbosity = 2;
	bool force = 1;
	ev::default_loop loop;
	MCSB::SocketDaemon daemon(loop, sockName.c_str(), verbosity, force);
	
	unsigned maxNewClients = 1000; // use 20000 to test MID wrap-around
	std::deque<int> fds;
	signal(SIGPIPE, SIG_IGN);
	
	for (unsigned newClients=0; newClients<maxNewClients;) {
		loop.run(EVRUN_NOWAIT);
		
		float val = float(rand()) / RAND_MAX;
		
		// chance of creating
		if (val>0.45) {
			if (fds.size()>=100) continue; // prevent too many open files
			try {
				int fd = MCSB::OpenSocketClient(sockName.c_str());
				fds.push_back(fd);
				newClients++;
				printf("- %u clients created\n", newClients);
			} catch (std::runtime_error err) {
				fprintf(stderr,"#-- %s\n", err.what());
				return -1;
			}
		} else {
			if (!fds.size()) continue;
			// delete a random client
			unsigned idx = rand() % fds.size();
			std::deque<int>::iterator it = fds.begin() + idx;
			close(*it);
			fds.erase(it);
			printf("- %lu clients deleted\n", newClients-daemon.NumClients()+1);
		}
	}

	while (fds.size()) {
		loop.run(EVRUN_NOWAIT);
		unsigned idx = rand() % fds.size();
		std::deque<int>::iterator it = fds.begin() + idx;
		close(*it);
		fds.erase(it);
		printf("- %lu clients deleted\n", maxNewClients-daemon.NumClients()+1);
	}

	return 0;
}
