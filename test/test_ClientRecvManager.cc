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

// always assert for tests, even in Release
#ifdef NDEBUG
#undef NDEBUG
#endif

#include "MCSB/ClientRecvManager.h"
#include "MCSB/ShmDefs.h"

#include <assert.h>
#include <cstdio>
#include <stdexcept>
#include <vector>

typedef MCSB::ClientRecvManager::MessageDescriptor RecvMessageDescriptor;

//-----------------------------------------------------------------------------
int test1(void)
//-----------------------------------------------------------------------------
{
	MCSB::ClientRecvManager recvMgr;
	
	// set up to call AddSegments
	unsigned maxMsgSlabs = 10;
	uint32_t blockIDs[] = {0,1,2,3,4,5,6,7,8,9};
	std::vector<const void*> blockPtrs(maxMsgSlabs, (void*)0);
	std::vector<MCSB::BlockInfo> blockInfo(maxMsgSlabs);
	std::vector<const MCSB::BlockInfo*> blockInfoPtrs(maxMsgSlabs);
	for (unsigned i=0; i<maxMsgSlabs; i++) {
		blockInfoPtrs[i] = &blockInfo[i];
	}
	recvMgr.NumConsSlabs(maxMsgSlabs);
	
	// verify that messages up to maxMsgSlabs gan be gotten and released
	for (unsigned count=1; count<maxMsgSlabs; count++) {
		for (unsigned i=0; i<count; i++) {
			blockInfo[i].numSegments = count;
			blockInfo[i].segmentNumber = i;
			blockInfo[i].messageID = maxMsgSlabs;
		}
		recvMgr.AddSegments(blockIDs,&blockPtrs[0],&blockInfoPtrs[0],count);
		assert(recvMgr.NumPendingSegments()==count);
		assert(recvMgr.PendingMessage());
		RecvMessageDescriptor desc = recvMgr.GetMessageDescriptor();
		assert(desc->NumSegments()==count);
		assert(recvMgr.NumBusySegments()==count);
		recvMgr.ReleaseMessageDescriptor(desc);
		assert(recvMgr.NumRetiredSegments()==count);
		uint32_t retiredBlocks[count];
		recvMgr.GetRetiredSegments(retiredBlocks,maxMsgSlabs);
		assert(!recvMgr.PendingMessage());
		assert(!recvMgr.NumPendingSegments());
		assert(!recvMgr.NumBusySegments());
		assert(!recvMgr.NumRetiredSegments());
	}
	
	// verify that messages must start with segmentNumber==0
	for (unsigned count=1; count<maxMsgSlabs; count++) {
		for (unsigned i=0; i<count; i++) {
			blockInfo[i].numSegments = count;
			blockInfo[i].segmentNumber = i+1;
			blockInfo[i].messageID = maxMsgSlabs;
		}
		recvMgr.AddSegments(blockIDs,&blockPtrs[0],&blockInfoPtrs[0],count);
		assert(!recvMgr.PendingMessage());
		assert(recvMgr.NumRetiredSegments()==count);
		uint32_t retiredBlocks[count];
		recvMgr.GetRetiredSegments(retiredBlocks,maxMsgSlabs);
	}

	// verify that numSegments must match
	for (unsigned count=1; count<maxMsgSlabs; count++) {
		for (unsigned i=0; i<count; i++) {
			blockInfo[i].numSegments = count;
			blockInfo[i].segmentNumber = i;
			blockInfo[i].messageID = maxMsgSlabs;
		}
		blockInfo[count-1].numSegments--;
		recvMgr.AddSegments(blockIDs,&blockPtrs[0],&blockInfoPtrs[0],count);
		assert(!recvMgr.PendingMessage());
		assert(recvMgr.NumRetiredSegments()==count);
		uint32_t retiredBlocks[count];
		recvMgr.GetRetiredSegments(retiredBlocks,maxMsgSlabs);
	}
	
	// verify that msgID must match
	for (unsigned count=2; count<maxMsgSlabs; count++) {
		for (unsigned i=0; i<count; i++) {
			blockInfo[i].numSegments = count;
			blockInfo[i].segmentNumber = i;
			blockInfo[i].messageID = maxMsgSlabs+i;
		}
		recvMgr.AddSegments(blockIDs,&blockPtrs[0],&blockInfoPtrs[0],count);
		assert(!recvMgr.PendingMessage());
		assert(recvMgr.NumRetiredSegments()==count);
		uint32_t retiredBlocks[count];
		recvMgr.GetRetiredSegments(retiredBlocks,maxMsgSlabs);
	}

	return 0;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	int result = test1();
	if (result) return result;
	fprintf(stderr,"=== PASS ===\n");
	return result;
}
