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

#include "MCSB/ClientWatcher.h"

namespace MCSB {

//-----------------------------------------------------------------------------
ClientWatcher::ClientWatcher(Client& client_, ev::loop_ref loop, float period)
//-----------------------------------------------------------------------------
:	client(client_)
{
	if (loop==0) {
		loop = ev::get_default_loop();
	}

	readable.loop = loop;
	timer.loop = loop;

	readable.set<ClientWatcher, &ClientWatcher::HandleEvent>(this);
	timer.set<ClientWatcher, &ClientWatcher::HandleEvent>(this);
	timer.set(period,period);

	int fd = client.FD();
	if (fd>=0) {
		readable.start(fd, ev::READ);
	} else {
		timer.start();
	}
}

//-----------------------------------------------------------------------------
void ClientWatcher::HandleEvent(void)
//-----------------------------------------------------------------------------
{
	int result = client.Poll(0);
	if (result<0) {
		// not connected, switch to timer
		readable.stop();
		timer.start();
	} else {
		// connected, switch to readable
		int fd = client.FD();
		timer.stop();
		readable.start(fd, ev::READ);
	}
}

} // namespace MCSB
