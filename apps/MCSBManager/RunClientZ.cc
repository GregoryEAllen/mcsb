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
#include "MCSB/ClientOptions.h"
#include "MCSB/SocketClient.h"
#include "MCSB/ClientZ.h"

#include <stdio.h>
#include <stdexcept>

namespace MCSB {

//-----------------------------------------------------------------------------
int RunClientZ(MCSB::ManagerParams& params, int fd)
//-----------------------------------------------------------------------------
{
	MCSB::ClientOptions opts("ClientZ");
	opts.ctrlSockName = params.ctrlSockName;

	if (fd<0) {
		try {
			fd = MCSB::OpenSocketClient(opts.ctrlSockName.c_str());
		} catch(std::runtime_error err) {
			fprintf(stderr,"### %s\n", err.what());
			return -1;
		}
	}

	try {
		MCSB::ClientZ cz(fd, opts);
		cz.Run();
	} catch(std::runtime_error err) {
		fprintf(stderr,"### %s\n", err.what());
		return -1;
	}
	return 0;
}

} // namespace MCSB
