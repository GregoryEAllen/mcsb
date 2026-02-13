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

#include "MCSB/SocketClient.h"

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdexcept>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace MCSB {

//-----------------------------------------------------------------------------
int OpenSocketClient(const char* sockNm)
//-----------------------------------------------------------------------------
{
	int sockFD = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockFD<0) {
		std::string err = "socket error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}

	struct sockaddr_un local;
	memset(&local, 0, sizeof(local));
	local.sun_family = AF_UNIX;
	strncpy(local.sun_path, sockNm, sizeof(local.sun_path)-1);
	int len = sizeof(local) - sizeof(local.sun_path) + strlen(local.sun_path);
	int res = connect(sockFD, (struct sockaddr *)&local, len);

	if (res) {
		close(sockFD);
		std::string err = "socket \"" + std::string(sockNm) + "\" connect error: ";
		err += strerror(errno);
		throw std::runtime_error(err);
	}
	
	return sockFD;
}

} // namespace MCSB
