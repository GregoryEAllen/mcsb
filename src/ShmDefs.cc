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

#include "MCSB/ShmDefs.h"

#include <unistd.h>
#include <cstdio>
#include <cstring>


namespace MCSB {

//-----------------------------------------------------------------------------
ShmHeader::ShmHeader(uint32_t blkSz, uint32_t slbSz, uint32_t numBlks,
	uint32_t numBufs, uint32_t mxNumBufs)
//-----------------------------------------------------------------------------
:	syncWord(0), shmVersion(kVersion), bufferNumber(0), numBuffers(numBufs),
	maxNumBuffers(mxNumBufs), blockSize(blkSz), slabSize(slbSz),
	numBlocks(numBlks), infoSize(sizeof(BlockInfo))
{
	infoOffset = PaddedSize();
	blockOffset = infoOffset + BlockInfo::PaddedSize(numBlocks);
	if (numBuffers>maxNumBuffers) {
		fprintf(stderr, "#- increasing maxNumBuffers (%u) to specified numBuffers (%u)\n", maxNumBuffers, numBuffers);
		maxNumBuffers = numBuffers;
	}
	syncWord = kSyncWord;
}


//-----------------------------------------------------------------------------
bool ShmHeader::ValidHeader(void) const
//-----------------------------------------------------------------------------
{
	if (syncWord != kSyncWord) return 0;
	if (shmVersion != kVersion) return 0;
	return 1;
}


//-----------------------------------------------------------------------------
uint32_t ShmHeader::PaddedSize(void)
//-----------------------------------------------------------------------------
{
	int pagesize = getpagesize();
	uint32_t paddedSize = (sizeof(ShmHeader) + pagesize-1) & (-pagesize); // round to pagesize
	return paddedSize;
}


//-----------------------------------------------------------------------------
size_t ShmHeader::TotalBlocksSize(void) const
//-----------------------------------------------------------------------------
{
	int pagesize = getpagesize();
	return (size_t(blockSize) * numBlocks + pagesize-1) & (-pagesize);;
}


//-----------------------------------------------------------------------------
size_t ShmHeader::TotalBufferSize(void) const
//-----------------------------------------------------------------------------
{
	size_t totSize = PaddedSize() + BlockInfo::PaddedSize(numBlocks);
	totSize += TotalBlocksSize();
	return totSize;
}


//-----------------------------------------------------------------------------
bool ShmHeader::IsContiguous(unsigned startBlock, unsigned runLen) const
//-----------------------------------------------------------------------------
{
	unsigned firstBufNum = startBlock / numBlocks;
	unsigned lastBufNum = (startBlock + runLen-1) / numBlocks;
	return firstBufNum==lastBufNum;
}


//-----------------------------------------------------------------------------
BlockInfo::BlockInfo(void)
//-----------------------------------------------------------------------------
{
	memset(this,0,sizeof(BlockInfo));
}


//-----------------------------------------------------------------------------
uint32_t BlockInfo::PaddedSize(unsigned numBlockInfos)
//-----------------------------------------------------------------------------
{
	int pagesize = getpagesize();
	uint32_t paddedSize = (sizeof(BlockInfo)*numBlockInfos + pagesize-1) & (-pagesize); // round to pagesize
	return paddedSize;
}


} // namespace MCSB
