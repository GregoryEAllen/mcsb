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

#ifndef MCSB_ShmDefs_h
#define MCSB_ShmDefs_h
#pragma once

// A single shared memory buffer consists of:
//  - a ShmHeader (padded up to kMappingAlignment)
//  - a vector of BlockInfo structures of length blocksPerBuffer (also padded)
//  - a vector of blocks of length blocksPerBuffer

// The entire buffer is mapped read-write by the manager
// Each client maps the entire buffer read-only (for consuming data), and
// then optionally remaps (elsewhere) the blocks as writeable (for producing)

#include <stdint.h>
#include <stddef.h>

namespace MCSB {

//-----------------------------------------------------------------------------
// ShmHeader is the header of a shared memory buffer, maintained by the
// manager and accessed (read only) by clients
//-----------------------------------------------------------------------------

struct ShmHeader {
	uint32_t syncWord;
	uint32_t shmVersion;
	uint32_t infoOffset;      // from this to the first BlockInfo
	uint32_t blockOffset;     // from this to the first block
	
	uint32_t bufferNumber;    // of this buffer (0 to numBuffers-1)
	uint32_t numBuffers;      // total currenly mapped
	uint32_t maxNumBuffers;   // maximum allowed to be mapped

	uint32_t blockSize;       // of a single block
	uint32_t slabSize;        // a multiple of a block
	uint32_t numBlocks;       // in this buffer
	uint32_t infoSize;        // of a BlockInfo

	enum { kSyncWord = 0x4D435342 }; // 'MCSB'
	enum { kVersion = 1 };
	
	ShmHeader(void) : numBuffers(0) {}
	ShmHeader(uint32_t blkSz, uint32_t slbSz, uint32_t numBlks,
		uint32_t numBufs=1, uint32_t mxNumBufs=1);
	
	static uint32_t PaddedSize(void);
	bool ValidHeader(void) const;
	size_t TotalBlocksSize(void) const;
	size_t TotalBufferSize(void) const;
	bool IsContiguous(unsigned startBlock, unsigned runLen) const;
};


//-----------------------------------------------------------------------------
// BlockInfo is in shared memory, maintained by the manager and accessed
// (read only) by clients
//-----------------------------------------------------------------------------

class BlockInfo {
  public:
	uint32_t messageID;
	uint32_t size;    // possibly spanning multiple contiguous blocks
	uint32_t crc32c;    // of the segment
	uint16_t segmentNumber; // 0 to numSegments-1
	uint16_t numSegments;   // when segmenting larger messages
	double   sendTime;  // in seconds

	BlockInfo(void);
	static uint32_t PaddedSize(unsigned numBlockInfos);
};


//-----------------------------------------------------------------------------
// each block is simply an array of blockSize bytes
//-----------------------------------------------------------------------------

//char block[blockSize];

} // namespace MCSB

#endif
