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

#ifndef MCSB_ShmClient_h
#define MCSB_ShmClient_h
#pragma once

#include "MCSB/ShmDefs.h"
#include <cstddef>
#include <string>
#include <vector>

namespace MCSB {

class ShmClient { // maps for a client
  public:
	// nameFmt must contain %u for bufferNumber to be used with sprintf
	ShmClient(const char* nameFmt="");
	~ShmClient(void);

	void NameFormat(const char* nameFmt); // set once after empty ctor

	unsigned MapBuffers(void); // will map additional buffers if available
	unsigned NumBuffers(void) const { return headerVec.size(); }

	const ShmHeader* GetShmHeader(unsigned bufNum=0) const { return headerVec[bufNum]; }
	uint32_t BlockSize(void) const { return blockSize; }
	uint32_t SlabSize(void) const { return slabSize; }
	uint32_t SlabsPerBuffer(void) const
		{ return numBlocks*blockSize/slabSize; }
	unsigned BlocksPerSlab(void) const { return blocksPerSlab; }
	uint32_t BlocksPerBuffer(void) const
		{ return numBlocks; }

	const BlockInfo* GetBlockInfo(unsigned blockID) const;
	const char* GetBlockPtr(unsigned blockID) const;
	char* GetWriteableBlockPtr(unsigned blockID) const;

	const BlockInfo* GetBlockInfo(unsigned bufNum, unsigned blockOffset) const
		{ return infoVec[bufNum] + blockOffset; }
	const char* GetBlockPtr(unsigned bufNum, unsigned blockOffset) const
		{ return blockVec[bufNum] + blockOffset*blockSize; }
	char* GetWriteableBlockPtr(unsigned bufNum, unsigned blockOffset) const
		{ return blockVecWrt[bufNum] + blockOffset*blockSize; }

  protected:
	std::string nameFormat;
	uint32_t blockSize;
	uint32_t slabSize;
	uint32_t numBlocks; // in a single buffer
	size_t bufSize;     // of a single buffer
	size_t blksSize;    // in a single buffer
	unsigned blocksPerSlab;
	std::vector<const ShmHeader*> headerVec; // of length numBuffers
	std::vector<const BlockInfo*> infoVec;
	std::vector<const char*> blockVec;
	std::vector<char*> blockVecWrt;

	void Initialize(void);
	void GetFirstHeader(ShmHeader& hdr);
	int OpenShm(unsigned bufNum, std::string& shmName) const;
	void Map(unsigned bufNum, const ShmHeader* refhdr);
};

} // namespace MCSB

#endif
