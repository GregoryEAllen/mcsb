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

#include "MCSB/ClientRecvManager.h"

#include <cstdio>

namespace MCSB {

//-----------------------------------------------------------------------------
ClientRecvManager::ClientRecvManager(void)
//-----------------------------------------------------------------------------
:	pendingMsg(0), numConsSlabs(0), numSegmentsRcvd(0)
{
	AddMsgSegBlock();
}

//-----------------------------------------------------------------------------
ClientRecvManager::~ClientRecvManager(void)
//-----------------------------------------------------------------------------
{
	while (!freeSegs.empty()) freeSegs.pop_back();
	while (!pendingSegs.empty()) pendingSegs.pop_back();
	while (!busySegs.empty()) busySegs.pop_back();
	while (!retiredSegs.empty()) retiredSegs.pop_back();

	while (segs.size()) {
		delete[] segs.back();
		segs.pop_back();
	}
}

//-----------------------------------------------------------------------------
void ClientRecvManager::AddMsgSegBlock(void)
//-----------------------------------------------------------------------------
{
	const int kNumSegs = 32;
	RecvMsgSegment* newSegs = new RecvMsgSegment[kNumSegs];
	segs.push_back(newSegs);
	
	for (int i=0; i<kNumSegs; i++) {
		freeSegs.push_back(newSegs[i]);
	}
}

//-----------------------------------------------------------------------------
void ClientRecvManager::AddSegments(const uint32_t blockIDs[],
	const void* blockPtrs[], const BlockInfo* blockInfoPtrs[], unsigned count)
//-----------------------------------------------------------------------------
{
	// put each segment into pendingSegs
	for (unsigned i=0; i<count; i++) {
		if (!freeSegs.size()) AddMsgSegBlock();
		RecvMsgSegment& seg = freeSegs.front();
		freeSegs.pop_front();
		seg.buf = (void*)blockPtrs[i];
		seg.size = blockInfoPtrs[i]->size;
		seg.blockID = blockIDs[i];
		seg.next = 0; // we'll deal with this when we check pendingSegs
		seg.blockInfo = blockInfoPtrs[i];
		seg.refcnt = 1;
		pendingSegs.push_back(seg);
	}
	numSegmentsRcvd += count;
}

//-----------------------------------------------------------------------------
bool ClientRecvManager::PendingMessage(void)
//-----------------------------------------------------------------------------
{
	if (pendingMsg) return 1;
	pendingMsg = GetMessageDescriptor();
	return !!pendingMsg;
}

//-----------------------------------------------------------------------------
ClientRecvManager::MessageDescriptor ClientRecvManager::GetMessageDescriptor(void)
//-----------------------------------------------------------------------------
{
	RecvMsgSegment* result = 0;
	if (pendingMsg) {
		result = pendingMsg;
		pendingMsg = 0;
		return result;
	}
	
	while (pendingSegs.size()) {
		RecvMsgSegment& seg0 = pendingSegs.front();
		unsigned segmentNumber = seg0.blockInfo->segmentNumber;
		unsigned numSegments = seg0.blockInfo->numSegments;
		if (segmentNumber || !numSegments || numSegments>numConsSlabs) {
			// we've missed the beginning of the message,
			// or numSegments==0, or more segs than we can hold
			// so drop this seg
			pendingSegs.pop_front();
			retiredSegs.push_back(seg0);
			// FIXME: droppedSegmentOfLargeMessage++;
			continue;
		}
		if (numSegments>pendingSegs.size()) return 0; // don't yet have all segs

		// look for numSegments matching segments at the head of pendingSegs
		unsigned numSegMatches = 1;
		MsgSegList::iterator it = pendingSegs.begin();
		while (numSegMatches<numSegments) {
			++it;
			bool numSegsMatch = it->blockInfo->numSegments==numSegments;
			bool segNumMatch = it->blockInfo->segmentNumber==numSegMatches;
			bool msgidMatch = it->blockInfo->messageID==seg0.blockInfo->messageID;
			if (numSegsMatch && segNumMatch && msgidMatch) numSegMatches++;
			else break;
		}
		if (numSegMatches<numSegments) {
			// segments did not match, drop head segment
			pendingSegs.pop_front();
			retiredSegs.push_back(seg0);
			// FIXME: droppedSegmentOfLargeMessage++;
			continue;
		}

		// we have numSegments matching segments to return
		RecvMsgSegment* currSeg = &seg0;
		pendingSegs.pop_front();
		busySegs.push_back(seg0);
		for (unsigned i=1; i<numSegments; i++) {
			RecvMsgSegment& segN = pendingSegs.front();
			currSeg->next = &segN;
			currSeg = &segN;
			pendingSegs.pop_front();
			busySegs.push_back(segN);
		}
		currSeg->next = 0;
		result = &seg0;
		break;
	}
	
	return result;
}

//-----------------------------------------------------------------------------
void ClientRecvManager::ReleaseMessageDescriptor(MessageDescriptor desc)
//-----------------------------------------------------------------------------
{
	RecvMsgSegment* seg = desc;
	while (seg) {
		// move from busySegs to retiredSegs
		busySegs.erase(*seg);
		retiredSegs.push_back(*seg);
		seg = static_cast<RecvMsgSegment*>(seg->next);
	}
}

//-----------------------------------------------------------------------------
unsigned ClientRecvManager::GetRetiredSegments(uint32_t blockIDs[],
	unsigned maxCount)
//-----------------------------------------------------------------------------
{
	unsigned i=0;
	while(retiredSegs.size() && i<maxCount) {
		RecvMsgSegment& seg = retiredSegs.front();
		uint32_t blockID = seg.BlockID();
		retiredSegs.pop_front();
		freeSegs.push_back(seg);
		blockIDs[i++] = blockID;
	}
	return i;
}

//-----------------------------------------------------------------------------
void ClientRecvManager::PrintState(const char* prefix) const
//-----------------------------------------------------------------------------
{
	fprintf(stderr,"%sfreeSegs.size: %lu\n", prefix, freeSegs.size());
	fprintf(stderr,"%spendingSegs.size: %lu\n", prefix, pendingSegs.size());
	fprintf(stderr,"%sbusySegs.size: %lu\n", prefix, busySegs.size());
	fprintf(stderr,"%sretiredSegs.size: %lu\n", prefix, retiredSegs.size());
}

} // namespace MCSB
