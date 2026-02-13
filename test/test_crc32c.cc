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

#include "MCSB/crc32c.h"
#include "MCSB/uptimer.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	bool isAccelerated = MCSB::crc32c_is_accelerated();
	printf("CRC-32C is%s accelerated\n", isAccelerated? "" : " NOT");

	std::vector<uint8_t> data(8192,0);
	uint32_t crc32c = MCSB::crc32c(&data[0], data.size());
	//printf("crc32c 0x%08x\n", crc32c);
	if (crc32c!=0x90444623) return -1;

	// fill with random data
	for (unsigned i=0; i<data.size(); i++) {
		data[i] = rand();
	}

	// check that all the edge cases work in the accelerated implementation
	for (size_t len=data.size()-6; len<data.size(); len++) {
		for (int offset=0; offset<6; offset++) {
			MCSB::crc32c_is_accelerated(0);
			uint32_t crc_norm = MCSB::crc32c(&data[offset], len);;
			MCSB::crc32c_is_accelerated();
			uint32_t crc_accel  = MCSB::crc32c(&data[offset], len);;
			if (crc_norm!=crc_accel) return -1;
		}
	}

	// compare normal vs accelerated
	unsigned numTimes = 1000;

	MCSB::crc32c_is_accelerated(0);
	MCSB::uptimer normTimer;
	for (unsigned n=0; n<numTimes; n++) {
		volatile uint32_t crc32c = MCSB::crc32c(&data[0], data.size());
		(void)crc32c;
	}
	double normTime = normTimer.uptime();

	MCSB::crc32c_is_accelerated();
	MCSB::uptimer accelTimer;
	for (unsigned n=0; n<numTimes; n++) {
		volatile uint32_t crc32c = MCSB::crc32c(&data[0], data.size());
		(void)crc32c;
	}
	double accelTime = accelTimer.uptime();

	printf("normTime %g, accelTime %g\n", normTime, accelTime);
	printf("acceleration speedup is %g\n", normTime/accelTime);

	printf("PASS\n");
	return 0;
}
