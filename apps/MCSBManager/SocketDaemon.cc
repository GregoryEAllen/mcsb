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

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string>
#include <errno.h>
#include <exception>
#include <stdexcept>

namespace MCSB {

//-----------------------------------------------------------------------------
SocketDaemon::SocketDaemon(ev::loop_ref loop, const char* sockName_,
	int verbosity_, bool force, unsigned mxNumClients, int backlog)
//-----------------------------------------------------------------------------
:	dbprinter(verbosity_), listener(loop), sockName(sockName_),
	maxNumClients(mxNumClients), nextNewClientID(0)
{
	int fd = -1;

	try {
		fd = CreateAndListen(backlog);
	} catch (const std::runtime_error& err) {
		if (!force) throw err;
		dbprintf(kNotice,"#-- %s\n", err.what());
		dbprintf(kNotice,"--- force set, trying again\n");
		listener.fd = fd;
		CloseAndRemoveListener();
		fd = CreateAndListen(backlog); // try again
	}
	listener.set<SocketDaemon, &SocketDaemon::ReadListener>(this);
	listener.start(fd, ev::READ);
}

//-----------------------------------------------------------------------------
SocketDaemon::~SocketDaemon(void)
//-----------------------------------------------------------------------------
{
	CloseAndRemoveListener();
}

//-----------------------------------------------------------------------------
int SocketDaemon::CreateAndListen(int backlog) const
//-----------------------------------------------------------------------------
{
	int fd = -1;
	
	// create the socket
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd<0) {
		std::string err = "socket error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}

	// bind to sockName
	struct sockaddr_un local;
	memset(&local, 0, sizeof(local));
	local.sun_family = AF_UNIX;
	strncpy(local.sun_path, sockName.c_str(), sizeof(local.sun_path)-1);
	int len = sizeof(local) - sizeof(local.sun_path) + strlen(local.sun_path);
	int res = bind(fd, (struct sockaddr *)&local, len);
	if (res<0) {
		close(fd);
		std::string err = "bind " + sockName + " error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}

	// listen to sockName
	res = listen(fd, backlog);
	if (res<0) {
		close(fd);
		std::string err = "listen " + sockName + " error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
	
	return fd;
}

//-----------------------------------------------------------------------------
void SocketDaemon::CloseAndRemoveListener(void)
//-----------------------------------------------------------------------------
{
	if (listener.fd>0 && close(listener.fd)) {
		dbprintf(kError, "#-- close %s error: %s\n", sockName.c_str(), strerror(errno));
	}
	if (unlink(sockName.c_str())) {
		dbprintf(kError, "#-- shm_unlink %s error: %s\n", sockName.c_str(), strerror(errno));
	}
}

//-----------------------------------------------------------------------------
void SocketDaemon::CloseAllClients(void)
//-----------------------------------------------------------------------------
{
	for (client_iter i=clients.begin(); i!=clients.end(); ++i) {
		i->second->CloseFD();
	}
}

//-----------------------------------------------------------------------------
void SocketDaemon::ReadListener(ev::io &watcher, int revents)
//-----------------------------------------------------------------------------
{
	struct sockaddr_un remote;
	socklen_t len = sizeof(remote);
	int newSock = accept(watcher.fd, (sockaddr*)&remote, &len);
	if (newSock<0) {
		std::string err = "accept error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
	AddConnectedClient(watcher.loop, newSock);
}

//-----------------------------------------------------------------------------
void SocketDaemon::AddConnectedClient(ev::loop_ref loop, int sock)
//-----------------------------------------------------------------------------
{
	// check that we have room for another client
	if (clients.size()>=maxNumClients) {
		dbprintf(kWarning, "#-- refusing additional socket clients (%u connected)\n", maxNumClients);
		if (close(sock)) {
			dbprintf(kError, "#-- close error: %s\n", strerror(errno));
		}
		return;
	}
	
	unsigned newClientID = GetNewClientID();
	ClientProxy* newProxy = 0;
	try {
		newProxy = CreateNewClientProxy(loop,sock,newClientID,this);
	} catch(std::runtime_error err) {
		dbprintf(kError, "### error creating ClientProxy[%d]: %s\n", newClientID, err.what());
		if (close(sock)) {
			dbprintf(kError, "#-- close error: %s\n", strerror(errno));
		}
		return;
	}
	
	clients.insert(pair(newClientID,newProxy));
	const char* s = "";
	unsigned nClients = clients.size();
	if (nClients>1) s = "s";
	dbprintf(kNotice,"- new client[%d] (%d client%s connected)\n", newClientID, nClients, s);
}

//-----------------------------------------------------------------------------
SocketDaemon::ClientID SocketDaemon::GetNewClientID(void)
//-----------------------------------------------------------------------------
{
	while (1) {
		ClientID newID = nextNewClientID++;
		if (newID>=kMaxClientID)
			nextNewClientID = 0;
		if (!clients.count(newID))
			return newID;
	}
}

//-----------------------------------------------------------------------------
SocketDaemon::ClientProxy* SocketDaemon::CreateNewClientProxy(ev::loop_ref loop,
	int fd, ClientID clientID, SocketDaemon* daemon)
//-----------------------------------------------------------------------------
{
	ClientProxy* newProxy = new ClientProxy(loop,fd,clientID,daemon);
	return newProxy;
}

//-----------------------------------------------------------------------------
void SocketDaemon::DeleteClient(ClientProxy* proxy, int readResult)
//-----------------------------------------------------------------------------
{
	//	close the file descriptor after destruction of classes that use it
	int oldFD = proxy->FD();
	int clientID = proxy->ClientID();
	if (!readResult) {
		dbprintf(kNotice,"- client[%d] disconnected\n", clientID);
	} else {
		dbprintf(kError, "#-- client[%d] error: %s\n", clientID, strerror(errno));
	}

	ErasingClientHook(clientID,proxy);
	clients.erase(clientID);
	delete proxy;
	if (close(oldFD)) {
		dbprintf(kError, "#-- close error: %s\n", strerror(errno));
	}
}

//-----------------------------------------------------------------------------
SocketDaemon::ClientProxy::ClientProxy(ev::loop_ref loop, int fd_,
	SocketDaemon::ClientID clientID_, SocketDaemon* daemon_)
//-----------------------------------------------------------------------------
:	io(loop), clientID(clientID_), daemon(daemon_)
{
	io.set<SocketDaemon::ClientProxy, &SocketDaemon::ClientProxy::Readable>(this);
	io.start(fd_, ev::READ);
}

//-----------------------------------------------------------------------------
void SocketDaemon::ClientProxy::Readable(ev::io &watcher, int revents)
//-----------------------------------------------------------------------------
{
	int readResult = 0;
	try {
		readResult = Read();
	} catch(std::runtime_error err) {
		fprintf(stderr, "# client[%d] exception: %s\n", clientID, err.what());
	}
	if (readResult<=0) { // we must delete a client
		io.stop();
		daemon->DeleteClient(this, readResult);
	}
}

//-----------------------------------------------------------------------------
int SocketDaemon::ClientProxy::Read(void)
//	intended to be overridden, currently just echoes
//-----------------------------------------------------------------------------
{
	int maxLen = 1024;
	char buf[maxLen];
	int res = recv(FD(),buf,maxLen,0);
	if (res<0) {
		std::string err = "recv error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	} else if (!res) {
		return res;
	}
	
	// we received res bytes without error, echo them back
	int numToSend = res;
	char* sendPtr = buf;
	while (numToSend) {
		int sent = send(FD(),sendPtr,numToSend,0);
		if (sent<=0)
			return sent;
		numToSend -= sent;
		sendPtr += sent;
	}

	return res;
}

} // namespace MCSB
