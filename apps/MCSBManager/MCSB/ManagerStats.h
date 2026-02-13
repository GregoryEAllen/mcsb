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

#ifndef MCSB_ManagerStats_h
#define MCSB_ManagerStats_h
#pragma once

#include <cstring>
#include <stdint.h>

namespace MCSB {

struct ManagerStats {
	ManagerStats(void) { memset(this,0,sizeof(ManagerStats)); }
	void Print(FILE* file) const;

	double time;
	double uptime;

	uint32_t numClients;

	uint32_t numBuffers;
	uint32_t numSlabs;
	uint32_t numSlabsReserved;
	uint32_t numSlabsNonreservable;
	uint32_t numFreeSlabs;
	uint32_t numHeldSlabs;
	uint32_t numWantedSlabs;

	uint64_t rcvdSegs; // from any Client
	uint64_t rcvdBytes;
	uint64_t sentSegs; // to any Client
	uint64_t sentBytes;
	uint64_t droppedSegs; // to any Client
	uint64_t droppedBytes;
};

} // namespace MCSB

#endif
