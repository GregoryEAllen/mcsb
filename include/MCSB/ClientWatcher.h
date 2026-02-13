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

/// \file ClientWatcher.h
/// \brief A simple watcher class for using an MCSB Client with \libev.

#ifndef MCSB_ClientWatcher_h
#define MCSB_ClientWatcher_h
#pragma once

#include "MCSB/Client.h"
#include <ev++.h>

namespace MCSB {

/// A simple watcher class for using an MCSB Client with \libev.

/// A ClientWatcher watches a Client and calls Client::Poll() as needed.
/// If the Client is connected, ClientWatcher will watch its file descriptor
/// and call Poll whenever it is readable.
/// If the Client is not connected, ClientWatcher will periodically call Poll,
/// which will attempt to reconnect the Client to the Manager.
class ClientWatcher {
  public:
	/// \param client  The MCSB Client to be watched and serviced.
	/// \param loop    The \libev event loop in use (where 0 is default_loop).
	/// \param period  Waiting period to try reconnecting when disconnected.
	ClientWatcher(Client& client, ev::loop_ref loop=0, float period=1.);

  private:
	Client& client;
	ev::io readable;
	ev::timer timer;

	void HandleEvent(void);
};

} // namespace MCSB

#endif
