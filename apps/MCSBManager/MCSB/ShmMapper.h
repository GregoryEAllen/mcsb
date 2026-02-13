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

#ifndef MCSB_ShmMapper_h
#define MCSB_ShmMapper_h
#pragma once

#include "MCSB/ShmDefs.h"
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

namespace MCSB {

class ShmMapper { // allocates and maps for the manager
  public:
	// nameFmt must contain %u for bufferNumber to be used with sprintf
	// force will remove and reallocate the shared memory if it already exists
	// the remaining parameters describe the shared memory to map
	ShmMapper(const char* nameFmt, bool force, uint32_t blkSz, uint32_t slbSz,
		uint32_t numBlks, uint32_t mxNumBufs);
   ~ShmMapper(void);

	unsigned NumBuffers(unsigned n); // increase the number of buffers allocated and mapped
	unsigned NumBuffers(void) const { return headerVec.size(); }

	ShmHeader* GetShmHeader(unsigned bufNum) const { return headerVec[bufNum]; }
	BlockInfo* GetBlockInfo(unsigned blockID) const {
		ldiv_t result = ldiv(blockID,numBlocks);
		return infoVec[result.quot] + result.rem;
	}
	char* GetBlockPtr(unsigned blockID) const;

	const char* ShmNameFormat(void) const { return nameFormat.c_str(); }

	uint32_t TotalNumBlocks(void) const
		{ return headerVec.size()*numBlocks; }
	uint32_t TotalNumSlabs(void) const
		{ return TotalNumBlocks()/BlocksPerSlab(); }
	uint32_t BlocksPerSlab(void) const
		{ return slabSize/blockSize; }
	unsigned LockErrors(void) const
		{ return lockErrors; }

  protected:
	std::string nameFormat;
	bool force;
	uint32_t blockSize;
	uint32_t slabSize;
	uint32_t numBlocks; // in a single buffer
	size_t bufSize;     // of a single buffer
	uint32_t maxNumBuffers;
	std::vector<ShmHeader*> headerVec; // of length numBuffers
	std::vector<BlockInfo*> infoVec;
	std::vector<char*> blockVec;
	unsigned lockErrors;
	
	int OpenShm(unsigned bufNum, std::string& shmName) const;
	void CreateAndMap(unsigned bufNum);
	int LockOrTouch(const void* addr, size_t len) const;
};

} // namespace MCSB

#endif
