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

#ifndef MCSB_SlabManager_h
#define MCSB_SlabManager_h
#pragma once

#include "MCSB/IntrusiveList.h"
#include "MCSB/WantedSegment.h"
#include <vector>

namespace MCSB {

class SlabManager { // keeps track of slabs and slab states for the manager
  public:
	SlabManager(uint32_t slabsPerBuf, uint32_t numBufs, float pctNonreservable=0);
   ~SlabManager(void);

	unsigned NumBuffers(unsigned n); // increase the number of buffers
	unsigned NumBuffers(void) const { return infoVec.size(); }
	unsigned NumSlabs(void) const { return slabsPerBuf*NumBuffers(); }

	int GetSlabReservation(unsigned numSlabs);
	int ReleaseSlabReservation(unsigned numSlabs);
	uint32_t NumSlabsReserved(void) const { return numSlabsReserved; }
	uint32_t NumSlabsNonreservable(void) const { return numSlabsNonreservable; }

	class SlabInfo;
	SlabInfo& GetSlabInfo(unsigned slabID) const;

	unsigned GetFreeSlabs(uint32_t slabIDs[], unsigned mxcount,
		int prodClientID=-1, void* prodArg=0);
	void IncrementHeldRefcnt(const uint32_t slabIDs[], unsigned count);
	void DecrementHeldRefcnt(const uint32_t slabIDs[], unsigned count,
		const unsigned amounts[] = 0); // if null, decrement by 1
	WantedSegment& GetWantedSegment(uint16_t clientID, uint32_t blockID,
		uint32_t slabID);
	void ReleaseWantedSegment(WantedSegment& seg, uint32_t slabID);

	WantedSegment& FrontWantedSegment(void); // to free wanted slabs, see impl

	size_t NumFreeSlabs(void) const { return freeSlabs.size(); }
	size_t NumHeldSlabs(void) const { return heldSlabs.size(); }
	size_t NumWantedSlabs(void) const { return wantedSlabs.size(); }

	enum { kFreeToHeld=1, /*kFreeToWanted=2,*/ kHeldToFree=4,
		kHeldToWanted=8, kWantedToFree=16, kWantedToHeld=32 };
	typedef void (*SlabStateChangeHandler)(int which, const SlabInfo& slab, void* arg);
	// install a bare function
	// called immediately after the change has occurred
	void SetSlabStateChangeHandler(int which, SlabStateChangeHandler func, void* arg=0)
		{ slabStateChangeHandler = std::make_pair(func,arg); whichStateChange = which; }
	// install a class method
	template <class T, void (T::*method)(int which, const SlabInfo& slab)>
	void SetSlabStateChangeHandler(T* object, int which) {
		SetSlabStateChangeHandler(which, SlabStateChangeHandlerThunk<T,method>, reinterpret_cast<void*>(object));
	}

  protected:
	uint32_t slabsPerBuf;
	uint32_t numSlabsReserved;
	uint64_t wantedSlabsHarvested;
	float pctNonreservable;
	uint32_t numSlabsNonreservable;
	std::vector<SlabInfo*> infoVec; // of length numBuffers

	typedef IntrusiveList<SlabInfo> SlabList;
	SlabList freeSlabs;
	SlabList heldSlabs;
	SlabList wantedSlabs;
	WantedSegmentMgrList freeWantedSegs;

	std::pair<SlabStateChangeHandler,void*> slabStateChangeHandler;
	int whichStateChange;
	template <class T, void (T::*SlabStateChangeHandlerMethod)(int, const SlabInfo&)>
	static void SlabStateChangeHandlerThunk(int which, const SlabInfo& slab, void* arg)
	{
		(reinterpret_cast<T*>(arg)->*SlabStateChangeHandlerMethod)(which,slab);
	}
	void CallStateChangeHandler(int which, const SlabInfo& slab) {
		if (slabStateChangeHandler.first && (whichStateChange&which))
			(*slabStateChangeHandler.first)(which,slab,slabStateChangeHandler.second);
	}

	std::vector<WantedSegment*> wantedSegmentBlocks;
	void AddWantedSegmentBlock(void);
};

class SlabManager::SlabInfo : public IntrusiveList<SlabInfo>::Hook {
	uint32_t slabID;
	uint32_t heldRefcnt; // held by either a producer or consumer
	int prodClientID;    // assigned when slab is given to a producer
	void* prodArg;
	WantedSegmentMgrList wantedSegs;
  public:
	SlabInfo(unsigned id):
		slabID(id), heldRefcnt(0) { SetProducerParams(); }
	unsigned SlabID(void) const { return slabID; }
	void GetFreeSlab(void);
	uint32_t Held(void) const { return heldRefcnt; }
	uint32_t IncrementHeld(void) { return heldRefcnt++; }
	uint32_t DecrementHeld(unsigned amount=1);
	uint32_t Wanted(void) const { return wantedSegs.size(); }
	void Erase(WantedSegment& seg);
	void Push(WantedSegment& seg) { wantedSegs.push_back(seg); };
	WantedSegment& FrontWantedSegment(void) { return wantedSegs.front(); }
	void SetProducerParams(int clientID=-1, void* arg=0);
};

} // namespace MCSB

#endif
