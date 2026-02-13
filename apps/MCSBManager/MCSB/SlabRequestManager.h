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

#ifndef MCSB_SlabRequestManager_h
#define MCSB_SlabRequestManager_h
#pragma once

#include "MCSB/IntrusiveList.h"
#include <vector>

namespace MCSB {

// requests for a future free slab
class SlabRequestManager {
  public:
	SlabRequestManager(unsigned maxLvl=3);
   ~SlabRequestManager(void);

	void Reset(void); // empty outstanding requests

	void AddRequest(int level, unsigned numSlabs, unsigned id, const void* arg=0);

	int LowestLevelPending(void) const;

	unsigned MaxLevel(void) const { return maxLevel; }
	unsigned NumRequests(void) const { return numRequests; }
	unsigned NumRequests(int level) const;
	class SlabRequest;
	const SlabRequest& FrontRequest(int level) const;
	void PopFrontRequest(int level);

	// for testing
	size_t EmptyRequestsSize(void) const { return emptyRequests.size(); }

  protected:
	unsigned maxLevel;
	unsigned numRequests;
	typedef IntrusiveList<SlabRequest> SlabRequestList;
	std::vector<SlabRequestList*> requestLists;
	std::vector<SlabRequest*> slabRequestBlocks;
	void AddSlabRequestBlock(void);
	SlabRequestList emptyRequests;
	unsigned SafeLevel(int level) const {
		if (level<0 || unsigned(level)>maxLevel)
			level = maxLevel;
		return level;
	}
};

class SlabRequestManager::SlabRequest
	: public IntrusiveList<SlabRequest>::Hook {
	unsigned id;
	unsigned numSlabs;
	const void* arg;
	friend class SlabRequestManager;
  public:
	SlabRequest(void)
		: id(0), numSlabs(0), arg(0) {}
	unsigned Id(void) const { return id; }
	unsigned NumSlabs(void) const { return numSlabs; }
	const void* Arg(void) const { return arg; }
};

} // namespace MCSB

#endif
