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

#include "MCSB/ClientSendManager.h"

#include <assert.h>
#include <stdexcept>

namespace MCSB {

// this class holds a pool of producer slabs and manages the policy
// for allocating space in the pool for sending messages

//-----------------------------------------------------------------------------
ClientSendManager::ClientSendManager(void)
//-----------------------------------------------------------------------------
:	blockSize(0), slabSize(0), blocksPerSlab(0),
	maxWorkingSlabs(1), slabsHeld(0), segs(0)
{
	AddMsgSegBlock();
}

//-----------------------------------------------------------------------------
ClientSendManager::~ClientSendManager(void)
//-----------------------------------------------------------------------------
{
	while (!freeSegs.empty()) freeSegs.pop_back();
	while (!busySegs.empty()) busySegs.pop_back();

	while (segs.size()) {
		delete[] segs.back();
		segs.pop_back();
	}

	while (!freeSlabs.empty()) freeSlabs.pop_back();
	while (!workingSlabs.empty()) workingSlabs.pop_back();
	while (!fullSlabs.empty()) fullSlabs.pop_back();
	while (!retiredSlabs.empty()) retiredSlabs.pop_back();

	while (slabInfoVec.size()) {
		delete slabInfoVec.back();
		slabInfoVec.pop_back();
	}
}

//-----------------------------------------------------------------------------
void ClientSendManager::SetSizeParams(uint32_t blockSz, uint32_t slabSz)
//-----------------------------------------------------------------------------
{
	blockSize = blockSz;
	slabSize = slabSz;
	blocksPerSlab = slabSize/blockSize;
	assert(slabSize==blocksPerSlab*blockSize);
}

//-----------------------------------------------------------------------------
void ClientSendManager::SetNumTotalSlabs(uint32_t numTotalSlabs)
//-----------------------------------------------------------------------------
{
	if (slabInfoVec.size()>=numTotalSlabs) return;

	slabInfoVec.resize(numTotalSlabs,0);
	for (unsigned i=0; i<slabInfoVec.size(); i++) {
		if (slabInfoVec[i]) continue;
		slabInfoVec[i] = new SlabInfo(i);
	}
}

//-----------------------------------------------------------------------------
void ClientSendManager::AddFreeSlabs(const uint32_t slabIDs[], void* slabPtrs[],
	unsigned count)
//-----------------------------------------------------------------------------
{
	for (unsigned i=0; i<count; i++) {
		uint32_t slabID = slabIDs[i];
		if (slabID>=slabInfoVec.size()) SetNumTotalSlabs(slabID+1);
		SlabInfo& slab = *slabInfoVec[slabID];
		freeSlabs.push_back(slab);
		slab.Reset(slabPtrs[i]);
	}
	slabsHeld += count;
}

//-----------------------------------------------------------------------------
unsigned ClientSendManager::GetRetiredSlabs(uint32_t slabIDs[], unsigned maxCount)
//-----------------------------------------------------------------------------
{
	unsigned i=0;
	while(retiredSlabs.size() && i<maxCount) {
		uint32_t slabID = retiredSlabs.front().SlabID();
		retiredSlabs.pop_front();
		slabIDs[i++] = slabID;
	}
	slabsHeld -= i;
	return i;
}

//-----------------------------------------------------------------------------
unsigned ClientSendManager::SlabInfo::Alloc(unsigned blocks)
//-----------------------------------------------------------------------------
{
	assert(working);
	unsigned idx = blocksUsed;
	blocksUsed += blocks;
	refcount++;
	return idx;
}

//-----------------------------------------------------------------------------
SendMsgSegment*
	ClientSendManager::GetSegment(unsigned nBlocks, bool noFreeSlabs)
// get a single contiguous segment, optionally disallow harvesting freeSlabs
//-----------------------------------------------------------------------------
{
	if (nBlocks>blocksPerSlab) {
		throw std::runtime_error("ClientSendManager::GetSegment: nBlocks exceeds blocksPerSlab");
	}
	
	SlabList::iterator it;
	bool found = 0;
	
	if (nBlocks<blocksPerSlab) {
		// look for room in workingSlabs to allocate nBlocks
		for (it = workingSlabs.begin(); it!=workingSlabs.end(); ++it) {
			unsigned blocksLeft = blocksPerSlab - it->BlocksUsed();
			if (blocksLeft>=nBlocks) { // we found a place
				found = 1;
				break;
			}
		}
	}

	if (!found) {
		// there was no room in workingSlabs, so harvest a freeSlab
		if (!freeSlabs.size() || noFreeSlabs) {
			return 0;
		}
		SlabInfo& info = freeSlabs.front();
		freeSlabs.pop_front();
		workingSlabs.push_back(info);
		info.Working(1);
		it = SlabList::iterator(&workingSlabs.back()); // point at the new element
	}

	// --- we will allocate a segment from the slab at *it
	
	// get a segment descriptor
	if (!freeSegs.size()) AddMsgSegBlock();
	SendMsgSegment& seg = freeSegs.front();
	freeSegs.pop_front();
	busySegs.push_back(seg);

	// do the allocation
	SlabInfo& slab = *it;
	unsigned blockIndex = slab.Alloc(nBlocks);

	// fill out seg
	seg.buf = (char*)slab.Buf() + blockIndex*blockSize;
	seg.size = nBlocks*blockSize;
	seg.blockID = slab.SlabID()*blocksPerSlab + blockIndex;
	seg.next = 0;

	// if full, remove from workingSlabs
	unsigned blocksLeft = blocksPerSlab-slab.BlocksUsed();
	if (!blocksLeft) {
		// this block is full
		workingSlabs.erase(it);
		slab.Working(0);
		fullSlabs.push_back(slab); // not retired because we just alloc'd
	}

	// check whether we need to reduce workingSlabs
	if (workingSlabs.size()>maxWorkingSlabs)
		MaxWorkingSlabs(maxWorkingSlabs);

	return &seg;
}

//-----------------------------------------------------------------------------
ClientSendManager::MessageDescriptor
	ClientSendManager::GetMessageDescriptor(uint32_t len, bool contiguous)
//-----------------------------------------------------------------------------
{
	if (!blockSize) return 0; // we're not sufficiently initialized

	unsigned nBlocks = (len-1+blockSize)/blockSize;
	if (!nBlocks) nBlocks = 1; // even empty messages need a block
	if (contiguous || nBlocks<=blocksPerSlab)
		return GetSegment(nBlocks);

	// try for a multi-slab message
	unsigned wholeSlabs = nBlocks/blocksPerSlab;
	nBlocks -= wholeSlabs*blocksPerSlab;

	if (wholeSlabs>freeSlabs.size()) return 0; // just not enough
	
	// success hinges on whether we can get the partial slab
	SendMsgSegment* seg0 = 0;
	if (nBlocks) {
		bool noFreeSlabs = (wholeSlabs==freeSlabs.size());
		seg0 = GetSegment(nBlocks, noFreeSlabs);
		if (!seg0) return seg0;
	}
	
	// the remainder are whole slabs, and we have enough
	SendMsgSegment* segN = seg0;
	while (wholeSlabs--) {
		SendMsgSegment* next = GetSegment(blocksPerSlab);
		if (!seg0) { segN = seg0 = next; }
		segN->next = next;
		segN = static_cast<SendMsgSegment*>(segN->next);
	}
	
	return seg0;
}

//-----------------------------------------------------------------------------
unsigned ClientSendManager::MaxWorkingSlabs(unsigned maxSlabs)
//	possibly reduces workingSlabs by removing the oldest
//-----------------------------------------------------------------------------
{
	maxWorkingSlabs = maxSlabs;
	while (workingSlabs.size()>maxWorkingSlabs) {
		SlabInfo& info = workingSlabs.front();
		workingSlabs.pop_front();
		info.Working(0);
		if (info.Refcount()) {
			fullSlabs.push_back(info);
		} else {
			retiredSlabs.push_back(info);
		}
	}
	return maxWorkingSlabs;
}

//-----------------------------------------------------------------------------
void ClientSendManager::FlushWorkingSlabs(void)
//-----------------------------------------------------------------------------
{
	unsigned slabs = maxWorkingSlabs;
	MaxWorkingSlabs(0);
	MaxWorkingSlabs(slabs);
}

//-----------------------------------------------------------------------------
void ClientSendManager::AddMsgSegBlock(void)
//-----------------------------------------------------------------------------
{
	const int kNumSegs = 32;
	SendMsgSegment* newSegs = new SendMsgSegment[kNumSegs];
	segs.push_back(newSegs);
	
	for (int i=0; i<kNumSegs; i++) {
		freeSegs.push_back(newSegs[i]);
	}
}

//-----------------------------------------------------------------------------
unsigned ClientSendManager::SlabInfo::DecRef(void)
//-----------------------------------------------------------------------------
{
	assert(refcount>=0);
	return --refcount;
}

//-----------------------------------------------------------------------------
void ClientSendManager::ReleaseMessageDescriptor(MessageDescriptor desc)
//-----------------------------------------------------------------------------
{
	SendMsgSegment* seg = desc;
	while (seg) {
		// decrement refcount
		uint32_t slabID = seg->blockID/blocksPerSlab;
		SlabInfo& info = *slabInfoVec[slabID];
		unsigned newRefcount = info.DecRef();

		// move from busySegs to freeSegs
		busySegs.erase(*seg);
		freeSegs.push_back(*seg);

		// check retirement eligibility
		if (!info.Working() && !newRefcount) {
			fullSlabs.erase(info);
			retiredSlabs.push_back(info);
		}
		
		seg = static_cast<SendMsgSegment*>(seg->next);
	}
}

} // namespace MCSB
