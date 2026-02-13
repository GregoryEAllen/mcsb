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

#ifndef MCSB_SocketDaemon_h
#define MCSB_SocketDaemon_h
#pragma once

#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <unistd.h>
#include <ev++.h>

#include "MCSB/dbprinter.h"

namespace MCSB {

class SocketDaemon : public dbprinter {
  public:
	SocketDaemon(ev::loop_ref loop, const char* sockName, int verbosity=1,
		bool force=0, unsigned mxNumClients=1000, int backlog=5);
	virtual ~SocketDaemon(void);
	
	unsigned long NumClients(void) const { return clients.size(); }
	
	class ClientProxy;
	typedef unsigned short ClientID;

  protected:
	ev::io listener;
	std::string sockName;
	unsigned maxNumClients;

	int CreateAndListen(int backlog) const;
	void CloseAndRemoveListener(void);
	void CloseAllClients(void);

	typedef std::map<ClientID,ClientProxy*> ClientMap;
	typedef std::pair<ClientID,ClientProxy*> pair;
	typedef ClientMap::iterator client_iter;
	typedef ClientMap::const_iterator client_citer;
	ClientMap clients;

	enum { kMaxClientID = 9999 };
	ClientID GetNewClientID(void);
	ClientID nextNewClientID;

	virtual ClientProxy* CreateNewClientProxy(ev::loop_ref loop,
		int fd, ClientID clientID, SocketDaemon* daemon);
	void DeleteClient(ClientProxy* proxy, int readResult);
	virtual void ErasingClientHook(ClientID clientID,
		const ClientProxy* proxy) {}

	virtual void ReadListener(ev::io &watcher, int revents);
	void AddConnectedClient(ev::loop_ref loop, int sock);
};

class SocketDaemon::ClientProxy {
  public:
	ClientProxy(ev::loop_ref loop, int fd,
		SocketDaemon::ClientID clientID, SocketDaemon* daemon);
	virtual ~ClientProxy(void) {}

	void Readable(ev::io &watcher, int revents);

	virtual int Read(void);

	int FD(void) const { return io.fd; }
	int CloseFD(void) { return close(io.fd); }
	SocketDaemon::ClientID ClientID(void) const { return clientID; }

  protected:
	ev::io io;
	SocketDaemon::ClientID clientID;
	SocketDaemon* daemon;
};

} // namespace MCSB

#endif
