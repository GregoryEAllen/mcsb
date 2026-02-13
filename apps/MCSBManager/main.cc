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

#include "MCSB/ManagerParams.h"
#include "MCSB/Manager.h"

#include <unistd.h>
#include <stdexcept>
#include <ev++.h>
#include <sys/socket.h>
#include <execinfo.h>
#include <exception>

namespace MCSB {
	int RunClientZ(MCSB::ManagerParams& params, int fd);
};

//-----------------------------------------------------------------------------
void unhandled_exception_handler(void)
//-----------------------------------------------------------------------------
{
	fprintf(stderr,"# %s printing backtrace\n",__PRETTY_FUNCTION__);
	const int kMaxStacktrace = 100;
	void* stack[kMaxStacktrace];
	size_t size = backtrace(stack, kMaxStacktrace);
	backtrace_symbols_fd(stack, size, STDERR_FILENO);
	MCSB::Manager::StaticHastyCleanup();
	static int threw = 0;
	try {
		if (!threw++) throw; // rethrow the pending exception (only once)
	} catch (const std::runtime_error &err) {
		fprintf(stderr,"# runtime_error: %s\n", err.what());
	} catch (...) {}
	exit(-1);
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	MCSB::ManagerParams params(argc,argv);

	bool forkClientZ = 0;
	int fd[2] = {-1};

	if (forkClientZ) {
		int res = socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
		if (res<0) {
			perror("socketpair error");
			return -1;
		}

		pid_t pid = fork();
		if (!pid) { // I am ClientZ, the child
			close(fd[0]);
			return MCSB::RunClientZ(params,fd[1]);
		} else if (pid==-1) {
			perror("fork error");
			return -1;
		}
		close(fd[1]);
	}

	std::set_terminate(unhandled_exception_handler);

	ev::default_loop loop;
	MCSB::Manager mgr(params, loop);
	if (fd[0]>0)
		mgr.AddConnectedClient(loop, fd[0]);
	loop.run();
	return 0;
}
