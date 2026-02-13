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

#ifndef MCSB_ProxyStats_h
#define MCSB_ProxyStats_h
#pragma once

#include <cstring>
#include <cstdio>
#include <stdint.h>

namespace MCSB {

struct ProxyStats {
	ProxyStats(void) { memset(this,0,sizeof(ProxyStats)); }
	void Print(FILE* file, const char* clientName=0) const;

	short clientID;

	uint64_t sentSegs; // to the Client
	uint64_t sentBytes;
	uint64_t rcvdSegs; // from the Client
	uint64_t rcvdBytes;

	uint64_t wantedSegsIn; // pushed into wantedQueue
	uint64_t wantedBytesIn;
	uint64_t wantedSegsOut; // popped out and consumed
	uint64_t wantedBytesOut;
	uint64_t wantedSegsDropped; // free slabs harvested, dropped
	uint64_t wantedBytesDropped;
	uint64_t wantedSegsOverage; // wantedQueue size overage, dropped
	uint64_t wantedBytesOverage;
};

} // namespace MCSB

#endif
