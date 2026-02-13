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

#include "MCSB/SlabRequestManager.h"
#include <cassert>

namespace MCSB {

//-----------------------------------------------------------------------------
SlabRequestManager::SlabRequestManager(unsigned maxLvl)
//-----------------------------------------------------------------------------
:	maxLevel(maxLvl), numRequests(0)
{
	assert(maxLevel>0);
	for (unsigned lvl=0; lvl<=maxLevel; lvl++) {
		requestLists.push_back( new SlabRequestList() );
	}
	AddSlabRequestBlock();
}

//-----------------------------------------------------------------------------
SlabRequestManager::~SlabRequestManager(void)
//-----------------------------------------------------------------------------
{
	// empty the lists so we can delete the memory
	while (!requestLists.empty()) {
		SlabRequestList* list = requestLists.back();
		while (!list->empty()) {
			list->pop_back();
		}
		requestLists.pop_back();
		delete list;
	}
	while(!emptyRequests.empty())
		emptyRequests.pop_back();

	// now delete the memory
	while(!slabRequestBlocks.empty()) {
		SlabRequest* req = slabRequestBlocks.back();
		slabRequestBlocks.pop_back();
		delete[] req;
	}
}

//-----------------------------------------------------------------------------
void SlabRequestManager::Reset(void)
//-----------------------------------------------------------------------------
{
	for (unsigned lvl=0; lvl<=maxLevel; lvl++) {
		while (requestLists[lvl]->size()) {
			PopFrontRequest(lvl);
		}
	}
	assert(!numRequests);
}

//-----------------------------------------------------------------------------
void SlabRequestManager::AddSlabRequestBlock(void)
//-----------------------------------------------------------------------------
{
	const int kNumReqs = 256;
	SlabRequest* newReqs = new SlabRequest[kNumReqs];
	slabRequestBlocks.push_back(newReqs);
	
	for (int i=0; i<kNumReqs; i++) {
		emptyRequests.push_back(newReqs[i]);
	}
}

//-----------------------------------------------------------------------------
void SlabRequestManager::AddRequest(int level, unsigned numSlabs, unsigned id, const void* arg)
//-----------------------------------------------------------------------------
{
	if (emptyRequests.empty())
		AddSlabRequestBlock();

	level = SafeLevel(level);
	SlabRequest& req = emptyRequests.front();
	emptyRequests.pop_front();
	req.id = id;
	req.numSlabs = numSlabs;
	req.arg = arg;
	requestLists[level]->push_back(req);
	numRequests++;
}

//-----------------------------------------------------------------------------
int SlabRequestManager::LowestLevelPending(void) const
//-----------------------------------------------------------------------------
{
	if (!numRequests)
		return -1;
	for (unsigned lvl=0; lvl<=maxLevel; lvl++) {
		if (requestLists[lvl]->size())
			return lvl;
	}
	return -1;
}

//-----------------------------------------------------------------------------
unsigned SlabRequestManager::NumRequests(int level) const
//-----------------------------------------------------------------------------
{
	if (level<0 || unsigned(level)>maxLevel)
		return 0;
	return requestLists[level]->size();
}

//-----------------------------------------------------------------------------
const SlabRequestManager::SlabRequest&
	SlabRequestManager::FrontRequest(int level) const
//-----------------------------------------------------------------------------
{
	assert(level>=0 && unsigned(level)<=maxLevel);
	if (level<0 || unsigned(level)>maxLevel) {
		const SlabRequest* sr = (const SlabRequest*)0; // ref to null pointer
		return *sr;
	}
	return requestLists[level]->front();
}

//-----------------------------------------------------------------------------
void SlabRequestManager::PopFrontRequest(int level)
//-----------------------------------------------------------------------------
{
	assert(level>=0 && unsigned(level)<=maxLevel);
	if (level<0 || unsigned(level)>maxLevel) return;
	assert(requestLists[level]->size());
	if (!requestLists[level]->size()) return;
	SlabRequest& req = requestLists[level]->front();
	requestLists[level]->pop_front();
	numRequests--;
	emptyRequests.push_back(req);
}

} // namespace MCSB
