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

// The MCSB client allows access to an underlying file descriptor with
// client.FD(). When the file descriptor is readable, that indicates the client
// needs to be serviced by calling client.Poll().
// With this paradigm, one can control his own event loop using select or
// poll, or use one of several event loop libraries that are freely available.

// libev [http://software.schmorp.de/pkg/libev.html] is a powerful, easy to use
// event loop library. The MCSB Manager uses internally libev internally, but
// Client-side use of libev is entirely optional.

// MCSB includes a ClientWatcher class for trivial integration of a libev loop
// with one or more MCSB Clients.

// Recall that an MCSB Manager process must be running for this example.

#include "MCSB/ClientWatcher.h"
#include <ev++.h>

// This is the same file used in ex2
#include "DescriptorHello.h"

// Use libev to control our main loop
int main(int argc, char* const argv[])
{
	// these lines are the same as before
	MCSB::Client client(argc,argv);
	uint32_t messageID = 12345;
	DescriptorHello descHello(client, messageID);

	// create a default libev event loop
	ev::default_loop loop;

	// A ClientWatcher allows libev to watch an MCSB client and
	// service it whenever it is needed.
	MCSB::ClientWatcher watcher(client, loop);

	// give control to the libev event loop
	loop.run();

	return 0;
}
