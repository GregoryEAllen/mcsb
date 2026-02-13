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

#include "MCSB/ClientSendManager.h"

#include <assert.h>
#include <cstdio>
#include <stdexcept>
#include <vector>

typedef MCSB::SendMsgSegment SendMessageSegment;

std::vector<void*> slabPtrVec(16,(void*)0);
void** slabPtrs = &slabPtrVec[0];

//-----------------------------------------------------------------------------
int test1(void)
//-----------------------------------------------------------------------------
{
	fprintf(stderr,"=== test1 ===\n");

	MCSB::ClientSendManager sendMgr;
	
	const uint32_t blockSize = 8192;
	const uint32_t blocksPerSlab = 32;
	const uint32_t slabSize = blockSize*blocksPerSlab;
	sendMgr.SetSizeParams(blockSize,slabSize);
	
	try {
		sendMgr.GetMessageDescriptor(slabSize+1);
		assert(0);
	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- %s\n", err.what());
	}
	assert(!sendMgr.GetMessageDescriptor(blockSize));
	
	uint32_t slabIDs[] = {0,2,4};
	unsigned numSlabs = sizeof(slabIDs)/sizeof(uint32_t);
	sendMgr.AddFreeSlabs(slabIDs, slabPtrs, numSlabs);
	assert(numSlabs==sendMgr.SlabsHeld());

	for (uint32_t bid=0; bid<blocksPerSlab-1; bid++) {
		SendMessageSegment* seg = sendMgr.GetMessageDescriptor(blockSize);
		uint32_t blockID = seg->BlockID();
		uint32_t expectedBlockID = slabIDs[0]*blocksPerSlab + bid;
		fprintf(stderr,"blockID %d\n", blockID);
		assert(expectedBlockID==blockID);
		sendMgr.ReleaseMessageDescriptor(seg);
	}
	// get the last one
	SendMessageSegment* seg = sendMgr.GetMessageDescriptor(blockSize);
	uint32_t blockID = seg->BlockID();
	fprintf(stderr,"blockID %d\n", blockID);
	assert(sendMgr.NumWorkingSlabs()==0);
	sendMgr.ReleaseMessageDescriptor(seg);

	{
		uint32_t sid[4];
		unsigned count = sendMgr.GetRetiredSlabs(sid,4);
		assert(count==1);
		assert(sid[0]==slabIDs[0]);
		sendMgr.AddFreeSlabs(sid, slabPtrs, count);
	}
	assert(numSlabs==sendMgr.SlabsHeld());

	sendMgr.MaxWorkingSlabs(-1);
	unsigned blocks = 1;
	while ((seg = sendMgr.GetMessageDescriptor(blocks*blockSize))) {
		uint32_t blockID = seg->BlockID();
		fprintf(stderr,"blockID %d\n", blockID);
		sendMgr.ReleaseMessageDescriptor(seg);
		blocks *= 2;
		if (blocks>=blocksPerSlab) blocks = blocksPerSlab-1;
	}
	assert(!sendMgr.NumFreeSlabs());
	assert(sendMgr.NumWorkingSlabs()==numSlabs);

	return 0;
}

//-----------------------------------------------------------------------------
int test2(void)
// verify that the oldest workingSlab gets used
//-----------------------------------------------------------------------------
{
	fprintf(stderr,"=== test2 ===\n");

	MCSB::ClientSendManager sendMgr;
	
	const uint32_t blockSize = 8192;
	const uint32_t blocksPerSlab = 32;
	const uint32_t slabSize = blockSize*blocksPerSlab;
	sendMgr.SetSizeParams(blockSize,slabSize);

	uint32_t slabIDs[] = {0,1,2,3};
	unsigned numSlabs = sizeof(slabIDs)/sizeof(uint32_t);
	sendMgr.AddFreeSlabs(slabIDs, slabPtrs, numSlabs);
	
	sendMgr.MaxWorkingSlabs(-1);
	for (unsigned i=0; i<numSlabs; i++) {
		uint32_t nBlocks = blocksPerSlab/2+1;
		SendMessageSegment* seg = sendMgr.GetMessageDescriptor(nBlocks*blockSize);
		uint32_t blockID = seg->BlockID();
		uint32_t expectedBlockID = slabIDs[i]*blocksPerSlab;
		assert(blockID==expectedBlockID);
		fprintf(stderr,"blockID %d\n", blockID);
		sendMgr.ReleaseMessageDescriptor(seg);
	}
	SendMessageSegment* seg = sendMgr.GetMessageDescriptor(blockSize);
	uint32_t blockID = seg->BlockID();
	uint32_t expectedBlockID = slabIDs[0]*blocksPerSlab + blocksPerSlab/2+1;
	fprintf(stderr,"blockID %d\n", blockID);
	assert(blockID==expectedBlockID);

	return 0;
}

//-----------------------------------------------------------------------------
int test3(void)
// verify MaxWorkingSlabs reduces workingSlabs
//-----------------------------------------------------------------------------
{
	fprintf(stderr,"=== test3 ===\n");

	MCSB::ClientSendManager sendMgr;
	
	const uint32_t blockSize = 8192;
	const uint32_t blocksPerSlab = 32;
	const uint32_t slabSize = blockSize*blocksPerSlab;
	sendMgr.SetSizeParams(blockSize,slabSize);

	uint32_t slabIDs[] = {0,1,2,3};
	unsigned numSlabs = sizeof(slabIDs)/sizeof(uint32_t);
	sendMgr.AddFreeSlabs(slabIDs, slabPtrs, numSlabs);

	sendMgr.MaxWorkingSlabs(-1);
	for (unsigned i=0; i<numSlabs; i++) {
		uint32_t nBlocks = blocksPerSlab/2+1;
		SendMessageSegment* seg = sendMgr.GetMessageDescriptor(nBlocks*blockSize);
		uint32_t blockID = seg->BlockID();
		fprintf(stderr,"blockID %d\n", blockID);
		sendMgr.ReleaseMessageDescriptor(seg);
	}
	SendMessageSegment* seg = sendMgr.GetMessageDescriptor(blockSize);
	uint32_t blockID = seg->BlockID();
	fprintf(stderr,"blockID %d\n", blockID);
	assert(sendMgr.NumWorkingSlabs()==numSlabs);
	
	sendMgr.MaxWorkingSlabs(2);
	assert(sendMgr.NumWorkingSlabs()==2);
	sendMgr.ReleaseMessageDescriptor(seg);
	sendMgr.FlushWorkingSlabs();
	assert(sendMgr.NumWorkingSlabs()==0);

	{
		uint32_t sid[4];
		unsigned count = sendMgr.GetRetiredSlabs(sid,4);
		assert(count==4);
	}

	return 0;
}

//-----------------------------------------------------------------------------
int test4(void)
// verify multi-slab messages
//-----------------------------------------------------------------------------
{
	fprintf(stderr,"=== test4 ===\n");

	MCSB::ClientSendManager sendMgr;
	
	const uint32_t blockSize = 8192;
	const uint32_t blocksPerSlab = 32;
	const uint32_t slabSize = blockSize*blocksPerSlab;
	sendMgr.SetSizeParams(blockSize,slabSize);

	uint32_t slabIDs[] = {0,1,2,3};
	const unsigned numSlabs = sizeof(slabIDs)/sizeof(uint32_t);
	sendMgr.AddFreeSlabs(slabIDs, slabPtrs, numSlabs);

	SendMessageSegment* seg = sendMgr.GetMessageDescriptor(numSlabs*slabSize, 0);
	assert(seg);
	assert(!seg->Contiguous());
	assert(seg->NumSegments()==numSlabs);
	assert(seg->TotalSize()==numSlabs*slabSize);
	struct iovec iov[numSlabs];
	if (seg->GetIovec(iov,numSlabs-1)>0) return -1;
	seg->GetIovec(iov,numSlabs);
	sendMgr.ReleaseMessageDescriptor(seg);

	{
		uint32_t sid[4];
		unsigned count = sendMgr.GetRetiredSlabs(sid,4);
		assert(count==4);
	}
	sendMgr.AddFreeSlabs(slabIDs, slabPtrs, numSlabs);

	seg = sendMgr.GetMessageDescriptor(0);
	sendMgr.ReleaseMessageDescriptor(seg);

	unsigned allocSize = (unsigned)(3.5*slabSize);
	seg = sendMgr.GetMessageDescriptor(allocSize,0);
	sendMgr.ReleaseMessageDescriptor(seg);
	assert(seg->NumSegments()==numSlabs);
	assert(seg->TotalSize()==allocSize);

	return 0;
}

//-----------------------------------------------------------------------------
int main(int argc, char* const argv[])
//-----------------------------------------------------------------------------
{
	int result = test1();
	if (result) return result;
	result = test2();
	if (result) return result;
	result = test3();
	if (result) return result;
	result = test4();
	if (result) return result;
	fprintf(stderr,"=== PASS ===\n");
	return result;
}
