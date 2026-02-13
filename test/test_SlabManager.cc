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

#include "MCSB/SlabManager.h"

#include <cstdio>
#include <stdexcept>
#include <cassert>

class SlabCounter {
	int numFree;
	int numHeld;
	int numWanted;
  public:
	SlabCounter(void):
		numFree(0), numHeld(0), numWanted(0) {}
	void HandleSlabStateChange(int which, const MCSB::SlabManager::SlabInfo& slab);
};

//-----------------------------------------------------------------------------
void SlabCounter::HandleSlabStateChange(int which, const MCSB::SlabManager::SlabInfo& slab)
//-----------------------------------------------------------------------------
{
	switch (which) {
		case MCSB::SlabManager::kFreeToHeld:
			numFree -= 1;
			numHeld += 1;
			break;
		case MCSB::SlabManager::kHeldToFree:
			numHeld -= 1;
			numFree += 1;
			break;
		case MCSB::SlabManager::kHeldToWanted:
			numHeld -= 1;
			numWanted += 1;
			break;
		case MCSB::SlabManager::kWantedToFree:
			numWanted -= 1;
			numFree += 1;
			break;
		case MCSB::SlabManager::kWantedToHeld:
			numWanted -= 1;
			numHeld += 1;
			break;
		default:
			printf("which %d\n", which);
			assert(0);
	}
	assert(numFree+numHeld+numWanted == 0);
}


//-----------------------------------------------------------------------------
int main()
//-----------------------------------------------------------------------------
{
	uint32_t slabsPerBuf = 32;
	uint32_t numBufs = 4;
	uint32_t maxNumBufs = 8;
	unsigned errs = 0;

	MCSB::SlabManager slabMgr(slabsPerBuf, numBufs);
	SlabCounter counter;
	int which = MCSB::SlabManager::kFreeToHeld | MCSB::SlabManager::kHeldToFree
		| MCSB::SlabManager::kHeldToWanted | MCSB::SlabManager::kWantedToFree
		| MCSB::SlabManager::kWantedToHeld;
	slabMgr.SetSlabStateChangeHandler<SlabCounter,&SlabCounter::HandleSlabStateChange>(&counter,which);

	try {
		
		for (unsigned i=slabMgr.NumBuffers(); i<=maxNumBufs; i++) {
			slabMgr.NumBuffers(i);
			if (slabMgr.NumBuffers()!=i) return -1;
			while(!slabMgr.GetSlabReservation(slabsPerBuf/2))
				;
		}

		slabMgr.NumBuffers(maxNumBufs-1);
		if (slabMgr.NumBuffers()!=maxNumBufs) return -1;
		
		unsigned numSlabs = slabMgr.NumBuffers() * slabsPerBuf;
		for (unsigned i=0; i<numSlabs; i++) {
			if (slabMgr.GetSlabInfo(i).SlabID() != i)
				errs++;
		}

		if (slabMgr.NumSlabsReserved()!=numSlabs) return -1;
		while (slabMgr.NumSlabsReserved()) {
			if (slabMgr.ReleaseSlabReservation(slabsPerBuf/2)) return -1;
		}
		if (!slabMgr.ReleaseSlabReservation(1)) return -1;

		if (errs) return errs;

	} catch (std::runtime_error err) {
		fprintf(stderr,"#-- %s\n", err.what());
		return -1;
	}
	
	try { // invalid slabID should throw
		uint32_t slabID = -1;
		slabMgr.IncrementHeldRefcnt(&slabID,1);
		return -1;
	} catch (std::runtime_error err) {
		fprintf(stderr,"- %s\n", err.what());
	}

	try { // IncrementHeldRefcnt before GetFreeSlabs should throw
		uint32_t slabID = 0;
		slabMgr.IncrementHeldRefcnt(&slabID,1);
		return -1;
	} catch (std::runtime_error err) {
		fprintf(stderr,"- %s\n", err.what());
	}

	unsigned count = 10;
	uint32_t slabIDs[count];
	try { // double DecrementHeldRefcnt should throw
		slabMgr.GetFreeSlabs(slabIDs,count);
		slabMgr.DecrementHeldRefcnt(slabIDs,count);
		slabMgr.DecrementHeldRefcnt(slabIDs,count);
		return -1;
	} catch (std::runtime_error err) {
		fprintf(stderr,"- %s\n", err.what());
	}
	if (slabMgr.NumHeldSlabs()) return -1;
	if (slabMgr.NumWantedSlabs()) return -1;
	if (slabMgr.NumFreeSlabs()!=slabsPerBuf*maxNumBufs) return -1;

	try { // getting WantedSegment from unheld slabs should throw
		slabMgr.GetWantedSegment(0,0,0);
		return -1;
	} catch (std::runtime_error err) {
		fprintf(stderr,"- %s\n", err.what());
	}

	try { // ensure free->held->wanted->free
		slabMgr.GetFreeSlabs(slabIDs,count);
		if (slabMgr.NumHeldSlabs()!=count) return -1;
		std::vector<MCSB::WantedSegment*> segv;
		for (unsigned i=0; i<count; i++) {
			MCSB::WantedSegment& seg = 
				slabMgr.GetWantedSegment(0,slabIDs[i],slabIDs[i]);
			segv.push_back(&seg);
		}
		if (slabMgr.NumHeldSlabs()!=count) return -1;
		slabMgr.DecrementHeldRefcnt(slabIDs,count);
		if (slabMgr.NumHeldSlabs()) return -1;;
		if (slabMgr.NumWantedSlabs()!=count) return -1;
		for (unsigned i=0; i<count; i++) {
			MCSB::WantedSegment& seg = *segv[i];
			slabMgr.ReleaseWantedSegment(seg,slabIDs[i]);
		}
		if (slabMgr.NumWantedSlabs()) return -1;
		if (slabMgr.NumHeldSlabs()) return -1;
	} catch (std::runtime_error err) {
		fprintf(stderr,"- %s\n", err.what());
		return -1;
	}

	try { // double release should throw
		slabMgr.GetFreeSlabs(slabIDs,count);
		if (slabMgr.NumHeldSlabs()!=count) return -1;
		for (unsigned i=0; i<count; i++) {
			MCSB::WantedSegment& seg =
				slabMgr.GetWantedSegment(0,slabIDs[i],slabIDs[i]);
			slabMgr.ReleaseWantedSegment(seg,slabIDs[i]);
			slabMgr.ReleaseWantedSegment(seg,slabIDs[i]);
		}
		return -1;
	} catch (std::runtime_error err) {
		fprintf(stderr,"- %s\n", err.what());
	}
	slabMgr.DecrementHeldRefcnt(slabIDs,count);
	if (slabMgr.NumFreeSlabs()!=slabsPerBuf*maxNumBufs) return -1;

	try { // FrontWantedSegment should throw when there are no wantedSlabs
		slabMgr.FrontWantedSegment();
		return -1;
	} catch (std::runtime_error err) {
		fprintf(stderr,"- %s\n", err.what());
	}

	try { // verify dropping of wanted segments
		slabMgr.GetFreeSlabs(slabIDs,count);
		if (slabMgr.NumHeldSlabs()!=count) return -1;
		for (unsigned i=0; i<count; i++) {
			slabMgr.GetWantedSegment(slabIDs[i]+10,slabIDs[i],slabIDs[i]);
		}
		slabMgr.DecrementHeldRefcnt(slabIDs,count);
		if (slabMgr.NumWantedSlabs()!=count) return -1;
		unsigned i=0;
		while (slabMgr.NumWantedSlabs()) {
			MCSB::WantedSegment& seg = slabMgr.FrontWantedSegment();
			slabMgr.ReleaseWantedSegment(seg,seg.BlockID());
			// check we get back what was passed to GetWantedSegment
			if (seg.ClientID()!=slabIDs[i]+10) return -1;
			if (seg.BlockID()!=slabIDs[i++]) return -1;
		}
	} catch (std::runtime_error err) {
		fprintf(stderr,"- %s\n", err.what());
		return -1;
	}

	fprintf(stderr,"=== PASS ===\n");
	return 0;
}
