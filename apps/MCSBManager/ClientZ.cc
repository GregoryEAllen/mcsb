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

#include "MCSB/ClientZ.h"

#include <stdio.h>
#include <stdexcept>

namespace MCSB {

//-----------------------------------------------------------------------------
ClientZ::ClientZ(int fd, const ClientOptions& opts)
//-----------------------------------------------------------------------------
:	ClientImpl(fd,opts)
{
	fprintf(stderr, "%s\n", __PRETTY_FUNCTION__);
}

//-----------------------------------------------------------------------------
void ClientZ::Run(void)
//-----------------------------------------------------------------------------
{
	while (1) {
		if (Poll()<0) break;
	}
}

} // namespace MCSB
