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

#ifndef MCSB_ClientImplWatcher_h
#define MCSB_ClientImplWatcher_h
#pragma once

#include "MCSB/ClientImpl.h"

#include <ev++.h>

namespace MCSB {

class ClientImplWatcher {
	ClientImpl* cimpl;
	ev::io io;
  public:
	ClientImplWatcher(ClientImpl* c, ev::loop_ref loop)
	:	cimpl(c), io(loop)
	{
		io.set<ClientImplWatcher, &ClientImplWatcher::Readable>(this);
		io.start(cimpl->FD(), ev::READ);
	}
	void Readable(void) {
		cimpl->Poll(0);
	}
};

} // namespace MCSB

#endif
