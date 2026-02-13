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

#ifndef MCSB_ManagerParams_h
#define MCSB_ManagerParams_h
#pragma once

#include "MCSB/dbprinter.h"

#include <string>
#include <stdint.h>

namespace MCSB {

class ManagerParams {
  public:
	ManagerParams(void);
	ManagerParams(int argc, char* const argv[]);
	
	int Parse(int argc, char* const argv[]);
	void SetDefaults(void);
	void ParamCheck(void);
	void usage(const char* argv0);
	
	int verbosity;
	bool force; // delete and retry if shared resource allocation fails
	bool allowUnlockedMemory; // proceed even if memory locking fails
	bool playbackMode; // increase producer nice factor for every client
	                   // so that wanted segments don't get harvested

	// parameters for the SocketDaemon
	std::string ctrlSockName; // the socket that clients connect to
	unsigned maxNumClients; // manager will only allow this many
	unsigned backlog;       // parameter to listen(2)

	// parameters about the memory buffer
	std::string shmNameFmt; // the shared memory name format
	uint32_t blockSize;     // in bytes
	uint32_t slabSize;      // in bytes
	uint64_t bufferSize;    // in bytes
	uint32_t numBlocks;     // per buffer
	uint32_t numBuffers;    // initially allocated
	uint32_t maxNumBuffers; // that can ever be allocated
	float    nonrsrvblePct; // percent memory non-reservable by clients
	uint32_t SlabsPerBuffer(void) const { return bufferSize/slabSize; }

	const std::string& ShmNameFmt(const char* fmt); // sub %U for username

	enum { kDefaultVerbosity = kNotice };
	enum { kDefaultBlockSize = 8*1024 };
	enum { kDefaultSlabSize = kDefaultBlockSize*1024 };
	enum { kDefaultBufferSize = kDefaultSlabSize*32 };
	enum { kDefaultNumBuffers = 1 };
	enum { kDefaultMaxNumBuffers = 8 };
	enum { kDefaultNonrsrvblePct = 2 };
};

} // namespace MCSB

#endif
