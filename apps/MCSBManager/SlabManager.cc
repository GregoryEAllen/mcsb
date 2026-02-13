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

#include "MCSB/SlabManager.h"
#include <cassert>
#include <stdexcept>
#include <cstdio>
#include <cmath>

namespace MCSB {

//-----------------------------------------------------------------------------
SlabManager::SlabManager(uint32_t slabsPerBuf_, uint32_t numBufs,
	float pctNonreservable_)
//-----------------------------------------------------------------------------
:	slabsPerBuf(slabsPerBuf_), numSlabsReserved(0), wantedSlabsHarvested(0),
	pctNonreservable(pctNonreservable_), numSlabsNonreservable(0)
{
	assert(pctNonreservable>=0);
	assert(pctNonreservable<100);
	NumBuffers(numBufs);
}

//-----------------------------------------------------------------------------
SlabManager::~SlabManager(void)
//-----------------------------------------------------------------------------
{
	// empty the lists so we can delete the memory
	while(!freeSlabs.empty())
		freeSlabs.pop_back();
	while(!heldSlabs.empty())
		heldSlabs.pop_back();
	while(!wantedSlabs.empty())
		wantedSlabs.pop_back();
	// now delete the memory
	for (unsigned buf=0; buf<NumBuffers(); buf++) {
		free(infoVec[buf]);
		infoVec[buf] = 0;
	}
	// do the same for wantedSegs
	while(!freeWantedSegs.empty())
		freeWantedSegs.pop_back();
	while(!wantedSegmentBlocks.empty()) {
		WantedSegment* seg = wantedSegmentBlocks.back();
		wantedSegmentBlocks.pop_back();
		delete[] seg;
	}
}

//-----------------------------------------------------------------------------
unsigned SlabManager::NumBuffers(unsigned n)
//-----------------------------------------------------------------------------
{
	if (n==NumBuffers())
		return n; // no change
	if (n<NumBuffers()) {
		fprintf(stderr,"#-- refusing to reduce the number of buffers from %u to %u\n", NumBuffers(), n);
		return n;
	}
	for (unsigned buf=NumBuffers(); buf<n; buf++) {
		fprintf(stderr,"- SlabManager creating buffer %u\n", buf);
		SlabInfo* newBuf = (SlabInfo*)malloc(sizeof(SlabInfo)*slabsPerBuf);
		infoVec.push_back(newBuf);
		for (unsigned slab=0; slab<slabsPerBuf; slab++) {
			unsigned slabID = buf*slabsPerBuf+slab;
			new (newBuf+slab) SlabInfo(slabID); // placement new
			freeSlabs.push_back(newBuf[slab]);
		}
	}
	// recompute based on new NumSlabs, take ceiling
	numSlabsNonreservable = (unsigned)ceil(pctNonreservable/100*NumSlabs());
	return n;
}

//-----------------------------------------------------------------------------
int SlabManager::GetSlabReservation(unsigned nSlabs)
//-----------------------------------------------------------------------------
{
	if (nSlabs<=0) return 0;
	if (numSlabsReserved+numSlabsNonreservable+nSlabs>NumSlabs())
		return -1;
	numSlabsReserved += nSlabs;
	return 0;
}

//-----------------------------------------------------------------------------
int SlabManager::ReleaseSlabReservation(unsigned nSlabs)
//-----------------------------------------------------------------------------
{
	if (nSlabs<=0) return 0;
	if (nSlabs>numSlabsReserved)
		return -1;
	numSlabsReserved -= nSlabs;
	return 0;
}

//-----------------------------------------------------------------------------
SlabManager::SlabInfo& SlabManager::GetSlabInfo(unsigned slabID) const
//-----------------------------------------------------------------------------
{
	ldiv_t result = ldiv(slabID,slabsPerBuf);
	if ((unsigned)result.quot>=infoVec.size() || result.rem<0) {
		throw std::runtime_error("SlabManager::GetSlabInfo invalid slabID");
	}
	return infoVec[result.quot][result.rem];
}

//-----------------------------------------------------------------------------
unsigned SlabManager::GetFreeSlabs(uint32_t slabIDs[], unsigned mxcount,
	int prodClientID, void* prodArg)
//	returns number of slabs actually returned
//-----------------------------------------------------------------------------
{
	unsigned count = 0;
	// harvest freeSlabs
	while (count<mxcount && freeSlabs.size()) {
		SlabInfo& slab = freeSlabs.front();
		freeSlabs.pop_front();
		slabIDs[count++] = slab.SlabID();
		slab.GetFreeSlab();
		slab.SetProducerParams(prodClientID,prodArg);
		heldSlabs.push_back(slab);
		CallStateChangeHandler(kFreeToHeld,slab);
	}
	return count;
}

//-----------------------------------------------------------------------------
void SlabManager::IncrementHeldRefcnt(const uint32_t slabIDs[], unsigned count)
// this should never happen for freeSlabs, only held or wanted
//-----------------------------------------------------------------------------
{
	for (unsigned i=0; i<count; i++) {
		SlabInfo& slab = GetSlabInfo(slabIDs[i]);
		if (!slab.Held()) {
			if (!slab.Wanted()) {
				throw std::runtime_error("IncrementHeldRefcnt but slab not held or wanted");
			} else {
				// move from wanted to held
				wantedSlabs.erase(slab);
				heldSlabs.push_back(slab);
				CallStateChangeHandler(kWantedToHeld,slab);
			}
		}
		slab.IncrementHeld();
	}
}

//-----------------------------------------------------------------------------
void SlabManager::DecrementHeldRefcnt(const uint32_t slabIDs[], unsigned count,
	const unsigned amounts[])
//-----------------------------------------------------------------------------
{
	for (unsigned i=0; i<count; i++) {
		SlabInfo& slab = GetSlabInfo(slabIDs[i]);
		unsigned amount = amounts ? amounts[i] : 1;
		if (slab.DecrementHeld(amount)) {
			continue; // still held
		}
		// not held, move from held to either wanted or free
		heldSlabs.erase(slab);
		if (slab.Wanted()) {
			wantedSlabs.push_back(slab);
			CallStateChangeHandler(kHeldToWanted,slab);
		} else {
			slab.SetProducerParams();
			freeSlabs.push_back(slab);
			CallStateChangeHandler(kHeldToFree,slab);
		}
	}
}

//-----------------------------------------------------------------------------
void SlabManager::AddWantedSegmentBlock(void)
//-----------------------------------------------------------------------------
{
	const int kNumSegs = 256;
	WantedSegment* newSegs = new WantedSegment[kNumSegs];
	wantedSegmentBlocks.push_back(newSegs);
	
	for (int i=0; i<kNumSegs; i++) {
		freeWantedSegs.push_back(newSegs[i]);
	}
}

//-----------------------------------------------------------------------------
WantedSegment& SlabManager::GetWantedSegment(uint16_t clientID, uint32_t blockID,
	uint32_t slabID)
// this should never happen for freeSlabs, only held or wanted
//-----------------------------------------------------------------------------
{
	SlabInfo& slab = GetSlabInfo(slabID);
	if (!slab.Held() && !slab.Wanted()) {
		throw std::runtime_error("GetWantedSegment but slab not held or wanted");
	}

	if (!freeWantedSegs.size()) AddWantedSegmentBlock();
	
	WantedSegment& seg = freeWantedSegs.front();
	
	// friends are nice
	seg.clientID = clientID;
	seg.blockID = blockID;

	freeWantedSegs.pop_front();
	slab.Push(seg);
	return seg;
}

//-----------------------------------------------------------------------------
void SlabManager::ReleaseWantedSegment(WantedSegment& seg, uint32_t slabID)
//-----------------------------------------------------------------------------
{
	SlabInfo& slab = GetSlabInfo(slabID);
	slab.Erase(seg);
	freeWantedSegs.push_back(seg);
	// put the slab into the right list
	if (!slab.Wanted() && !slab.Held()) {
		// not held and not wanted, move from wanted to free
		wantedSlabs.erase(slab);
		slab.SetProducerParams();
		freeSlabs.push_back(slab);
		CallStateChangeHandler(kWantedToFree,slab);
	}
}

//-----------------------------------------------------------------------------
WantedSegment& SlabManager::FrontWantedSegment(void)
// from the head of the wantedSlabs, the front WantedSegment
// so a user can drop wanted segments and create more freeSlabs
// (and causing message/segment loss)
//-----------------------------------------------------------------------------
{
	if (wantedSlabs.empty()) {
		throw std::runtime_error("SlabManager::FrontWantedSegment with no wanted slabs");
	}
	SlabInfo& slab = wantedSlabs.front();
	if (!slab.Wanted()) {
		// this would indicate a deeper problem
		throw std::runtime_error("SlabManager::FrontWantedSegment slab has no WantedSegments");
	}
	WantedSegment& seg = slab.FrontWantedSegment();
	return seg;
}

//-----------------------------------------------------------------------------
void SlabManager::SlabInfo::GetFreeSlab(void)
//	see SlabManager::GetFreeSlabs
//	asserts if proper conditions not met
//	returns wantedSlabsHarvested
//-----------------------------------------------------------------------------
{
	if (Held()) {
		throw std::runtime_error("GetFreeSlab encountered non-zero heldRefcnt");
	}
	if (Wanted()) {
		throw std::runtime_error("GetFreeSlab encountered a wanted slab");
	}
	++heldRefcnt;
}

//-----------------------------------------------------------------------------
uint32_t SlabManager::SlabInfo::DecrementHeld(unsigned amount)
//-----------------------------------------------------------------------------
{
	if (heldRefcnt<amount) {
		heldRefcnt = 0;
		throw std::runtime_error("SlabInfo::DecrementHeld encountered heldRefcnt==0");
	}
	heldRefcnt -= amount;
	return heldRefcnt;
}

//-----------------------------------------------------------------------------
void SlabManager::SlabInfo::Erase(WantedSegment& seg)
//-----------------------------------------------------------------------------
{
#if 0
	// it's expensive to walk wantedSegs and verify that seg is contained
	WantedSegmentMgrList::iterator it;
	for (it=wantedSegs.begin(); it!=wantedSegs.end(); ++it) {
		if (&*it==&seg) break;
	}
	if (it==wantedSegs.end()) {
		throw std::runtime_error("SlabInfo::Erase seg not in wantedSegs");
	}
#endif
	// testing for empty should eventually catch problems
	if (wantedSegs.empty()) {
		throw std::runtime_error("SlabInfo::Erase on empty list");
	}
	wantedSegs.erase(seg);
}

//-----------------------------------------------------------------------------
void SlabManager::SlabInfo::SetProducerParams(int clientID, void* arg)
//-----------------------------------------------------------------------------
{
	prodClientID = clientID;
	prodArg = arg;
}

} // namespace MCSB
